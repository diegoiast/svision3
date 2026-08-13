// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Exercises svision3::dbus::Service against the real session bus daemon --
// there is no mock/fake here, this is deliberately an integration test. If
// no session bus is reachable (e.g. a headless CI container with no
// dbus-daemon at all) every test skips via REQUIRE-free early return rather
// than failing the suite.

#include "svision3/linux/dbus_service.hpp"
#include <catch2/catch_test_macros.hpp>
#include <poll.h>

using namespace svision3::dbus;

namespace {

// Pumps Service::dispatch() via real poll() until `done` becomes true or
// `timeout_ms` elapses. Returns whether `done` was actually reached.
bool pump_until(Service &svc, bool const &done, int timeout_ms) {
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (!done) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                             deadline - std::chrono::steady_clock::now())
                             .count();
        if (remaining <= 0) {
            return false;
        }
        auto fds = svc.poll_fds();
        std::vector<pollfd> pfds;
        pfds.reserve(fds.size());
        for (auto const &f : fds) {
            short events = 0;
            if (f.want_read) {
                events |= POLLIN;
            }
            if (f.want_write) {
                events |= POLLOUT;
            }
            pfds.push_back({f.fd, events, 0});
        }
        // Cap each individual wait short so we still notice `done` flipping
        // from work dispatch() itself just did, not only from fresh fd activity.
        ::poll(pfds.empty() ? nullptr : pfds.data(), pfds.size(),
              static_cast<int>(std::min<long long>(remaining, 50)));
        svc.dispatch();
    }
    return true;
}

} // namespace

TEST_CASE("dbus: connect to session bus", "[dbus]") {
    auto svc = Service::connect_session_bus();
    if (!svc) {
        WARN("no session bus reachable, skipping");
        return;
    }
    REQUIRE(svc != nullptr);
}

TEST_CASE("dbus: call_async gets a real reply from the bus daemon", "[dbus]") {
    auto svc = Service::connect_session_bus();
    if (!svc) {
        WARN("no session bus reachable, skipping");
        return;
    }

    bool done = false;
    std::string id;
    std::string error_name;
    svc->call_async(
        "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetId", {},
        [&](MethodReply const &reply) {
            done = true;
            if (!reply.args.empty()) {
                if (auto const *s = std::get_if<std::string>(&reply.args[0].base())) {
                    id = *s;
                }
            }
        },
        [&](std::string const &name, std::string const &) {
            done = true;
            error_name = name;
        });

    REQUIRE(pump_until(*svc, done, 2000));
    REQUIRE(error_name.empty());
    REQUIRE_FALSE(id.empty());
}

TEST_CASE("dbus: exported object round-trips a method call through the bus", "[dbus]") {
    auto svc = Service::connect_session_bus();
    if (!svc) {
        WARN("no session bus reachable, skipping");
        return;
    }

    auto &obj = svc->export_object("/toolkit/test/Object");
    obj.add_method("com.example.ToolkitTest", "Add", [](MethodCall const &call) -> MethodResult {
        if (call.args.size() != 2) {
            return MethodError{"org.freedesktop.DBus.Error.InvalidArgs", "expected 2 args"};
        }
        auto const *a = std::get_if<int32_t>(&call.args[0].base());
        auto const *b = std::get_if<int32_t>(&call.args[1].base());
        if (!a || !b) {
            return MethodError{"org.freedesktop.DBus.Error.InvalidArgs", "expected two int32"};
        }
        return MethodReply{{Value{*a + *b}}};
    });
    obj.add_property("com.example.ToolkitTest", "Answer", [] { return Value{uint32_t{42}}; });

    // Real SNI-style bus names are per-process; a well-known name works just
    // as well here and is easier to target from the same process.
    REQUIRE(svc->request_name("com.example.ToolkitTest"));

    bool call_done = false;
    int32_t sum = 0;
    std::string call_error;
    svc->call_async(
        "com.example.ToolkitTest", "/toolkit/test/Object", "com.example.ToolkitTest", "Add",
        {Value{int32_t{2}}, Value{int32_t{3}}},
        [&](MethodReply const &reply) {
            call_done = true;
            if (!reply.args.empty()) {
                if (auto const *v = std::get_if<int32_t>(&reply.args[0].base())) {
                    sum = *v;
                }
            }
        },
        [&](std::string const &name, std::string const &) {
            call_done = true;
            call_error = name;
        });

    REQUIRE(pump_until(*svc, call_done, 2000));
    REQUIRE(call_error.empty());
    REQUIRE(sum == 5);

    bool prop_done = false;
    uint32_t answer = 0;
    std::string prop_error;
    svc->call_async(
        "com.example.ToolkitTest", "/toolkit/test/Object", "org.freedesktop.DBus.Properties", "Get",
        {Value{std::string("com.example.ToolkitTest")}, Value{std::string("Answer")}},
        [&](MethodReply const &reply) {
            prop_done = true;
            if (!reply.args.empty()) {
                if (auto const *variant = std::get_if<Variant>(&reply.args[0].base());
                    variant && variant->value) {
                    if (auto const *v = std::get_if<uint32_t>(&variant->value->base())) {
                        answer = *v;
                    }
                }
            }
        },
        [&](std::string const &name, std::string const &) {
            prop_done = true;
            prop_error = name;
        });

    REQUIRE(pump_until(*svc, prop_done, 2000));
    REQUIRE(prop_error.empty());
    REQUIRE(answer == 42);
}

TEST_CASE("dbus: Struct/Array/Dict/ObjectPath round-trip through a real call, "
         "matching DBusMenu's GetLayout shape",
         "[dbus]") {
    auto svc = Service::connect_session_bus();
    if (!svc) {
        WARN("no session bus reachable, skipping");
        return;
    }

    auto &obj = svc->export_object("/toolkit/test/Layout");
    obj.add_method(
        "com.example.ToolkitTest", "GetLayout", [](MethodCall const &) -> MethodResult {
            // (id:i, properties:a{sv}, children:av) -- one root with one
            // child, exactly DBusMenu's GetLayout item shape.
            Struct child{{Value{int32_t{1}}, Value{Dict{{{"label", Value{std::string("Action 1")}}}}},
                         Value{Array{"v", {}}}}};
            Struct root{{Value{int32_t{0}}, Value{Dict{{{"label", Value{std::string("root")}}}}},
                        Value{Array{"v", {Value{Variant{std::make_shared<Value>(Value{child})}}}}}}};
            return MethodReply{{Value{uint32_t{1}}, Value{root},
                               Value{ObjectPath{"/toolkit/test/Layout"}}}};
        });
    REQUIRE(svc->request_name("com.example.ToolkitTest.Layout"));

    bool done = false;
    std::string error;
    uint32_t revision = 0;
    std::string root_label;
    std::string child_label;
    std::string path_back;
    svc->call_async(
        "com.example.ToolkitTest.Layout", "/toolkit/test/Layout", "com.example.ToolkitTest",
        "GetLayout", {},
        [&](MethodReply const &reply) {
            done = true;
            REQUIRE(reply.args.size() == 3);
            if (auto const *rev = std::get_if<uint32_t>(&reply.args[0].base())) {
                revision = *rev;
            }
            if (auto const *root = std::get_if<Struct>(&reply.args[1].base())) {
                REQUIRE(root->fields.size() == 3);
                if (auto const *props = std::get_if<Dict>(&root->fields[1].base())) {
                    REQUIRE(props->entries.size() == 1);
                    REQUIRE(props->entries[0].first == "label");
                    if (auto const *v = std::get_if<Variant>(&props->entries[0].second.base());
                        v && v->value) {
                        if (auto const *s = std::get_if<std::string>(&v->value->base())) {
                            root_label = *s;
                        }
                    }
                }
                if (auto const *children = std::get_if<Array>(&root->fields[2].base())) {
                    REQUIRE(children->items.size() == 1);
                    if (auto const *cv = std::get_if<Variant>(&children->items[0].base());
                        cv && cv->value) {
                        if (auto const *child = std::get_if<Struct>(&cv->value->base())) {
                            REQUIRE(child->fields.size() == 3);
                            if (auto const *cprops = std::get_if<Dict>(&child->fields[1].base())) {
                                if (auto const *cvv = std::get_if<Variant>(&cprops->entries[0].second.base());
                                    cvv && cvv->value) {
                                    if (auto const *s = std::get_if<std::string>(&cvv->value->base())) {
                                        child_label = *s;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (auto const *p = std::get_if<ObjectPath>(&reply.args[2].base())) {
                path_back = p->path;
            }
        },
        [&](std::string const &name, std::string const &) {
            done = true;
            error = name;
        });

    REQUIRE(pump_until(*svc, done, 2000));
    REQUIRE(error.empty());
    REQUIRE(revision == 1);
    REQUIRE(root_label == "root");
    REQUIRE(child_label == "Action 1");
    REQUIRE(path_back == "/toolkit/test/Layout");
}

TEST_CASE("dbus: subscribe receives NameOwnerChanged for our own name request", "[dbus]") {
    auto svc = Service::connect_session_bus();
    if (!svc) {
        WARN("no session bus reachable, skipping");
        return;
    }

    bool got_signal = false;
    std::string seen_name;
    svc->subscribe("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                   "NameOwnerChanged", [&](Signal const &sig) {
                       if (!sig.args.empty()) {
                           if (auto const *s = std::get_if<std::string>(&sig.args[0].base())) {
                               if (*s == "com.example.ToolkitTest.Subscribe") {
                                   got_signal = true;
                                   seen_name = *s;
                               }
                           }
                       }
                   });

    REQUIRE(svc->request_name("com.example.ToolkitTest.Subscribe"));
    REQUIRE(pump_until(*svc, got_signal, 2000));
    REQUIRE(seen_name == "com.example.ToolkitTest.Subscribe");
}
