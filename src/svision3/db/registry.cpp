// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "db/connection.hpp"

#include <mutex>
#include <unordered_map>

namespace db {

// Forward-declare built-in driver registration functions.
// Each driver .cpp defines its own; add new ones here.
namespace drivers {
void register_sqlite();
} // namespace drivers

static auto &driver_map() {
    static std::unordered_map<std::string, ConnectionFactory> map;
    return map;
}

static std::mutex &driver_mutex() {
    static std::mutex m;
    return m;
}

static void ensure_builtin_drivers() {
    static std::once_flag flag;
    std::call_once(flag, [] { drivers::register_sqlite(); });
}

void register_driver(std::string_view scheme, ConnectionFactory factory) {
    std::lock_guard lock(driver_mutex());
    driver_map()[std::string(scheme)] = factory;
}

std::unique_ptr<Connection> open(std::string_view connection_string) {
    ensure_builtin_drivers();

    auto colon = connection_string.find(':');
    if (colon == std::string_view::npos || colon == 0) {
        std::lock_guard lock(driver_mutex());
        auto it = driver_map().find("sqlite");
        if (it == driver_map().end()) {
            throw Error("no sqlite driver registered");
        }
        return it->second(connection_string);
    }

    auto scheme = std::string(connection_string.substr(0, colon));

    std::lock_guard lock(driver_mutex());
    auto it = driver_map().find(scheme);
    if (it == driver_map().end()) {
        throw Error("unknown database driver: " + scheme);
    }

    auto params = connection_string.substr(colon + 1);
    if (params.starts_with("//")) {
        params.remove_prefix(2);
    }

    return it->second(params);
}

} // namespace db
