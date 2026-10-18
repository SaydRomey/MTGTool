#include "core/sqlite.hpp"

#include <filesystem>
#include <limits>

namespace mtg {
namespace {

[[noreturn]] void throw_sqlite(sqlite3* db, std::string_view prefix) {
    const char* msg = db ? sqlite3_errmsg(db) : "unknown sqlite error";
    throw SqliteError(std::string(prefix) + ": " + msg);
}

} // namespace

SqliteDb::SqliteDb(const std::string& path) {
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    const int rc = sqlite3_open_v2(path.c_str(), &db_, flags, nullptr);
    if (rc != SQLITE_OK) {
        const std::string msg = db_ ? sqlite3_errmsg(db_) : "unable to allocate sqlite handle";
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        throw SqliteError("sqlite open failed: " + msg);
    }

    sqlite3_busy_timeout(db_, 5000);
    exec("PRAGMA foreign_keys=ON;");
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA synchronous=NORMAL;");
}

SqliteDb::~SqliteDb() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void SqliteDb::exec(std::string_view sql) {
    char* err = nullptr;
    const std::string owned(sql);
    const int rc = sqlite3_exec(db_, owned.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        const std::string msg = err ? err : sqlite3_errmsg(db_);
        sqlite3_free(err);
        throw SqliteError("sqlite exec failed: " + msg);
    }
}

bool SqliteDb::table_exists(std::string_view name) const {
    sqlite3_stmt* stmt = nullptr;
    constexpr const char* sql =
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw_sqlite(db_, "sqlite prepare failed");
    }
    const std::string owned(name);
    sqlite3_bind_text(stmt, 1, owned.c_str(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw_sqlite(db_, "sqlite step failed");
}

SqliteStmt::SqliteStmt(SqliteDb& db, std::string_view sql) : db_(db.handle()) {
    const std::string owned(sql);
    if (sqlite3_prepare_v2(db_, owned.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
        throw_sqlite(db_, "sqlite prepare failed");
    }
}

SqliteStmt::~SqliteStmt() {
    if (stmt_) sqlite3_finalize(stmt_);
}

void SqliteStmt::reset() {
    const int rc = sqlite3_reset(stmt_);
    if (rc != SQLITE_OK) throw_sqlite(db_, "sqlite reset failed");
}

void SqliteStmt::clear_bindings() {
    const int rc = sqlite3_clear_bindings(stmt_);
    if (rc != SQLITE_OK) throw_sqlite(db_, "sqlite clear bindings failed");
}

void SqliteStmt::bind(int index, std::int64_t value) {
    if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) {
        throw_sqlite(db_, "sqlite bind int64 failed");
    }
}

void SqliteStmt::bind(int index, int value) {
    if (sqlite3_bind_int(stmt_, index, value) != SQLITE_OK) {
        throw_sqlite(db_, "sqlite bind int failed");
    }
}

void SqliteStmt::bind(int index, double value) {
    if (sqlite3_bind_double(stmt_, index, value) != SQLITE_OK) {
        throw_sqlite(db_, "sqlite bind double failed");
    }
}

void SqliteStmt::bind(int index, std::string_view value) {
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw SqliteError("string too large for sqlite bind");
    }
    if (sqlite3_bind_text(stmt_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
        throw_sqlite(db_, "sqlite bind text failed");
    }
}

void SqliteStmt::bind_null(int index) {
    if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) {
        throw_sqlite(db_, "sqlite bind null failed");
    }
}

bool SqliteStmt::step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw_sqlite(db_, "sqlite step failed");
}

std::int64_t SqliteStmt::col_i64(int column) const {
    return sqlite3_column_int64(stmt_, column);
}

int SqliteStmt::col_int(int column) const {
    return sqlite3_column_int(stmt_, column);
}

double SqliteStmt::col_double(int column) const {
    return sqlite3_column_double(stmt_, column);
}

std::string SqliteStmt::col_text(int column) const {
    const unsigned char* text = sqlite3_column_text(stmt_, column);
    if (!text) return {};
    return reinterpret_cast<const char*>(text);
}

bool SqliteStmt::col_is_null(int column) const {
    return sqlite3_column_type(stmt_, column) == SQLITE_NULL;
}

SqliteTransaction::SqliteTransaction(SqliteDb& db) : db_(&db) {
    db_->exec("BEGIN IMMEDIATE;");
}

SqliteTransaction::~SqliteTransaction() {
    if (active_ && db_) {
        try {
            db_->exec("ROLLBACK;");
        } catch (...) {
        }
    }
}

void SqliteTransaction::commit() {
    if (!active_) return;
    db_->exec("COMMIT;");
    active_ = false;
}

void SqliteTransaction::rollback() {
    if (!active_) return;
    db_->exec("ROLLBACK;");
    active_ = false;
}

} // namespace mtg
