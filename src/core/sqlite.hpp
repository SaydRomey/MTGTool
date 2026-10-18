#pragma once

#include <sqlite3.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mtg {

class SqliteError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class SqliteDb {
public:
    explicit SqliteDb(const std::string& path);
    ~SqliteDb();

    SqliteDb(const SqliteDb&) = delete;
    SqliteDb& operator=(const SqliteDb&) = delete;
    SqliteDb(SqliteDb&&) = delete;
    SqliteDb& operator=(SqliteDb&&) = delete;

    sqlite3* handle() const noexcept { return db_; }
    void exec(std::string_view sql);
    bool table_exists(std::string_view name) const;

private:
    sqlite3* db_ = nullptr;
};

class SqliteStmt {
public:
    SqliteStmt(SqliteDb& db, std::string_view sql);
    ~SqliteStmt();

    SqliteStmt(const SqliteStmt&) = delete;
    SqliteStmt& operator=(const SqliteStmt&) = delete;

    void reset();
    void clear_bindings();

    void bind(int index, std::int64_t value);
    void bind(int index, int value);
    void bind(int index, double value);
    void bind(int index, std::string_view value);
    void bind_null(int index);

    bool step();

    std::int64_t col_i64(int column) const;
    int col_int(int column) const;
    double col_double(int column) const;
    std::string col_text(int column) const;
    bool col_is_null(int column) const;

private:
    sqlite3* db_ = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
};

class SqliteTransaction {
public:
    explicit SqliteTransaction(SqliteDb& db);
    ~SqliteTransaction();

    SqliteTransaction(const SqliteTransaction&) = delete;
    SqliteTransaction& operator=(const SqliteTransaction&) = delete;

    void commit();
    void rollback();

private:
    SqliteDb* db_ = nullptr;
    bool active_ = true;
};

} // namespace mtg
