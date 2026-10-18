#include "core/migrations.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mtg {
namespace {

std::string read_all(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open migration: " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int current_version(SqliteDb& db) {
    SqliteStmt stmt(db, "SELECT COALESCE(MAX(version), 0) FROM schema_migrations;");
    if (!stmt.step()) return 0;
    return stmt.col_int(0);
}

} // namespace

std::vector<MigrationRunner::Migration> MigrationRunner::discover(const std::filesystem::path& directory) {
    if (!std::filesystem::exists(directory)) {
        throw std::runtime_error("migration directory does not exist: " + directory.string());
    }

    std::vector<Migration> migrations;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".sql") continue;
        const std::string stem = entry.path().stem().string();
        const auto underscore = stem.find('_');
        const std::string number = stem.substr(0, underscore);
        if (number.empty() || !std::all_of(number.begin(), number.end(), [](unsigned char c) { return c >= '0' && c <= '9'; })) {
            continue;
        }
        migrations.push_back(Migration{std::stoi(number), entry.path()});
    }

    std::sort(migrations.begin(), migrations.end(), [](const Migration& a, const Migration& b) {
        return a.version < b.version;
    });
    return migrations;
}

void MigrationRunner::apply(SqliteDb& db, const std::filesystem::path& directory) {
    db.exec(
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "version INTEGER PRIMARY KEY,"
        "applied_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");");

    int version = current_version(db);
    for (const auto& migration : discover(directory)) {
        if (migration.version <= version) continue;

        SqliteTransaction tx(db);
        db.exec(read_all(migration.path));
        SqliteStmt record(db, "INSERT INTO schema_migrations(version) VALUES(?);");
        record.bind(1, migration.version);
        record.step();
        tx.commit();
        version = migration.version;
    }
}

} // namespace mtg
