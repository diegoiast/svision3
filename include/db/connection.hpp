// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace db {

class Error : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class Result {
  public:
    virtual ~Result() = default;

    virtual bool next() = 0;
    virtual int column_count() const = 0;
    virtual std::string column_name(int col) const = 0;

    virtual int get_int(int col) const = 0;
    virtual int64_t get_int64(int col) const = 0;
    virtual double get_double(int col) const = 0;
    virtual std::string get_string(int col) const = 0;
    virtual bool is_null(int col) const = 0;
};

class Statement {
  public:
    virtual ~Statement() = default;

    virtual Statement &bind(int index, int value) = 0;
    virtual Statement &bind(int index, int64_t value) = 0;
    virtual Statement &bind(int index, double value) = 0;
    virtual Statement &bind(int index, std::string_view value) = 0;
    virtual Statement &bind_null(int index) = 0;

    virtual std::unique_ptr<Result> execute() = 0;
    virtual int execute_update() = 0;
    virtual void reset() = 0;
};

class Connection {
  public:
    virtual ~Connection() = default;

    virtual std::unique_ptr<Statement> prepare(std::string_view sql) = 0;
    virtual void execute(std::string_view sql) = 0;

    virtual void begin() = 0;
    virtual void commit() = 0;
    virtual void rollback() = 0;

    virtual int64_t last_insert_id() = 0;
};

using ConnectionFactory = std::unique_ptr<Connection> (*)(std::string_view params);

void register_driver(std::string_view scheme, ConnectionFactory factory);

// Opens a connection. The scheme is extracted from the connection string:
//   "sqlite:mydb.db"                      → sqlite driver, params "mydb.db"
//   "mysql://user:pass@host:3306/mydb"    → mysql driver, params as-is
//   ":memory:" or plain path              → sqlite (default)
std::unique_ptr<Connection> open(std::string_view connection_string);

} // namespace db
