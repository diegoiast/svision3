// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "db/connection.hpp"
#include "sqlite/sqlite3.h"

#include <cstring>
#include <vector>

namespace db {

static void throw_sqlite(sqlite3 *db, std::string_view context) {
    std::string msg(context);
    msg += ": ";
    msg += sqlite3_errmsg(db);
    throw Error(msg);
}

// ── Result ──────────────────────────────────────────────────────────────────

class SqliteResult final : public Result {
  public:
    SqliteResult(sqlite3_stmt *stmt, sqlite3 *db) : stmt_(stmt), db_(db) {}

    ~SqliteResult() override { sqlite3_finalize(stmt_); }

    bool next() override {
        int rc = sqlite3_step(stmt_);
        if (rc == SQLITE_ROW) {
            return true;
        }
        if (rc == SQLITE_DONE) {
            return false;
        }
        throw_sqlite(db_, "step");
        return false;
    }

    int column_count() const override { return sqlite3_column_count(stmt_); }

    std::string column_name(int col) const override {
        auto name = sqlite3_column_name(stmt_, col);
        return name ? name : "";
    }

    int get_int(int col) const override { return sqlite3_column_int(stmt_, col); }

    int64_t get_int64(int col) const override { return sqlite3_column_int64(stmt_, col); }

    double get_double(int col) const override { return sqlite3_column_double(stmt_, col); }

    std::string get_string(int col) const override {
        auto text = reinterpret_cast<char const *>(sqlite3_column_text(stmt_, col));
        if (!text) {
            return {};
        }
        auto len = sqlite3_column_bytes(stmt_, col);
        return {text, static_cast<size_t>(len)};
    }

    bool is_null(int col) const override { return sqlite3_column_type(stmt_, col) == SQLITE_NULL; }

  private:
    sqlite3_stmt *stmt_;
    sqlite3 *db_;
};

// ── Statement ───────────────────────────────────────────────────────────────

class SqliteStatement final : public Statement {
  public:
    SqliteStatement(sqlite3_stmt *stmt, sqlite3 *db) : stmt_(stmt), db_(db) {}

    ~SqliteStatement() override { sqlite3_finalize(stmt_); }

    Statement &bind(int index, int value) override {
        if (sqlite3_bind_int(stmt_, index, value) != SQLITE_OK) {
            throw_sqlite(db_, "bind int");
        }
        return *this;
    }

    Statement &bind(int index, int64_t value) override {
        if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) {
            throw_sqlite(db_, "bind int64");
        }
        return *this;
    }

    Statement &bind(int index, double value) override {
        if (sqlite3_bind_double(stmt_, index, value) != SQLITE_OK) {
            throw_sqlite(db_, "bind double");
        }
        return *this;
    }

    Statement &bind(int index, std::string_view value) override {
        if (sqlite3_bind_text(stmt_, index, value.data(), static_cast<int>(value.size()),
                              SQLITE_TRANSIENT) != SQLITE_OK) {
            throw_sqlite(db_, "bind text");
        }
        return *this;
    }

    Statement &bind_null(int index) override {
        if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) {
            throw_sqlite(db_, "bind null");
        }
        return *this;
    }

    std::unique_ptr<Result> execute() override {
        // Transfer ownership of stmt_ to the Result — the Result will finalize
        auto *s = stmt_;
        stmt_ = nullptr;
        return std::make_unique<SqliteResult>(s, db_);
    }

    int execute_update() override {
        int rc = sqlite3_step(stmt_);
        if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
            throw_sqlite(db_, "execute_update");
        }
        int changes = sqlite3_changes(db_);
        sqlite3_reset(stmt_);
        sqlite3_clear_bindings(stmt_);
        return changes;
    }

    void reset() override {
        sqlite3_reset(stmt_);
        sqlite3_clear_bindings(stmt_);
    }

  private:
    sqlite3_stmt *stmt_;
    sqlite3 *db_;
};

// ── Connection ──────────────────────────────────────────────────────────────

class SqliteConnection final : public Connection {
  public:
    explicit SqliteConnection(std::string_view path) {
        int rc = sqlite3_open(std::string(path).c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::string msg = "open: ";
            msg += sqlite3_errmsg(db_);
            sqlite3_close(db_);
            db_ = nullptr;
            throw Error(msg);
        }
        execute("PRAGMA journal_mode=WAL");
        execute("PRAGMA foreign_keys=ON");
    }

    ~SqliteConnection() override {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    SqliteConnection(SqliteConnection const &) = delete;
    SqliteConnection &operator=(SqliteConnection const &) = delete;

    std::unique_ptr<Statement> prepare(std::string_view sql) override {
        sqlite3_stmt *stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql.data(), static_cast<int>(sql.size()), &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw_sqlite(db_, "prepare");
        }
        return std::make_unique<SqliteStatement>(stmt, db_);
    }

    void execute(std::string_view sql) override {
        char *err = nullptr;
        int rc = sqlite3_exec(db_, std::string(sql).c_str(), nullptr, nullptr, &err);
        if (rc != SQLITE_OK) {
            std::string msg = "exec: ";
            if (err) {
                msg += err;
                sqlite3_free(err);
            }
            throw Error(msg);
        }
    }

    void begin() override { execute("BEGIN"); }
    void commit() override { execute("COMMIT"); }
    void rollback() override { execute("ROLLBACK"); }

    int64_t last_insert_id() override { return sqlite3_last_insert_rowid(db_); }

  private:
    sqlite3 *db_ = nullptr;
};

// ── Registration ────────────────────────────────────────────────────────────

static std::unique_ptr<Connection> create_sqlite(std::string_view params) {
    return std::make_unique<SqliteConnection>(params);
}

namespace drivers {
void register_sqlite() { register_driver("sqlite", create_sqlite); }
} // namespace drivers

} // namespace db
