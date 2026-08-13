// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/linux/dbus_service.hpp"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <dbus/dbus.h>

namespace svision3::dbus {

// ── Value <-> DBusMessageIter marshaling ─────────────────────────────────────
// Free functions only, no access to Service/ExportedObject internals needed.
namespace {

std::string signature_of(Value const &v) {
    return std::visit(
        [](auto const &val) -> std::string {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return "s";
            } else if constexpr (std::is_same_v<T, int32_t>) {
                return "i";
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                return "u";
            } else if constexpr (std::is_same_v<T, bool>) {
                return "b";
            } else if constexpr (std::is_same_v<T, std::vector<IconPixmap>>) {
                return "a(iiay)";
            } else if constexpr (std::is_same_v<T, Variant>) {
                return "v";
            } else if constexpr (std::is_same_v<T, ObjectPath>) {
                return "o";
            } else if constexpr (std::is_same_v<T, Struct>) {
                std::string sig = "(";
                for (auto const &f : val.fields) {
                    sig += signature_of(f);
                }
                sig += ")";
                return sig;
            } else if constexpr (std::is_same_v<T, Array>) {
                return "a" + val.element_signature;
            } else if constexpr (std::is_same_v<T, Dict>) {
                return "a{sv}";
            }
        },
        v.base());
}

void append_value(DBusMessageIter *iter, Value const &v) {
    std::visit(
        [&](auto const &val) {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::string>) {
                char const *p = val.c_str();
                dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &p);
            } else if constexpr (std::is_same_v<T, int32_t>) {
                dbus_message_iter_append_basic(iter, DBUS_TYPE_INT32, &val);
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &val);
            } else if constexpr (std::is_same_v<T, bool>) {
                dbus_bool_t b = val ? TRUE : FALSE;
                dbus_message_iter_append_basic(iter, DBUS_TYPE_BOOLEAN, &b);
            } else if constexpr (std::is_same_v<T, std::vector<IconPixmap>>) {
                DBusMessageIter arr;
                dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "(iiay)", &arr);
                for (auto const &px : val) {
                    DBusMessageIter st;
                    dbus_message_iter_open_container(&arr, DBUS_TYPE_STRUCT, nullptr, &st);
                    dbus_message_iter_append_basic(&st, DBUS_TYPE_INT32, &px.width);
                    dbus_message_iter_append_basic(&st, DBUS_TYPE_INT32, &px.height);
                    DBusMessageIter bytes;
                    dbus_message_iter_open_container(&st, DBUS_TYPE_ARRAY, "y", &bytes);
                    if (!px.argb32.empty()) {
                        uint8_t const *ptr = px.argb32.data();
                        dbus_message_iter_append_fixed_array(&bytes, DBUS_TYPE_BYTE, &ptr,
                                                             static_cast<int>(px.argb32.size()));
                    }
                    dbus_message_iter_close_container(&st, &bytes);
                    dbus_message_iter_close_container(&arr, &st);
                }
                dbus_message_iter_close_container(iter, &arr);
            } else if constexpr (std::is_same_v<T, Variant>) {
                auto sig = val.value ? signature_of(*val.value) : std::string("s");
                DBusMessageIter sub;
                dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, sig.c_str(), &sub);
                if (val.value) {
                    append_value(&sub, *val.value);
                }
                dbus_message_iter_close_container(iter, &sub);
            } else if constexpr (std::is_same_v<T, ObjectPath>) {
                char const *p = val.path.c_str();
                dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH, &p);
            } else if constexpr (std::is_same_v<T, Struct>) {
                DBusMessageIter st;
                dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, nullptr, &st);
                for (auto const &f : val.fields) {
                    append_value(&st, f);
                }
                dbus_message_iter_close_container(iter, &st);
            } else if constexpr (std::is_same_v<T, Array>) {
                DBusMessageIter arr;
                dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, val.element_signature.c_str(),
                                                 &arr);
                for (auto const &item : val.items) {
                    append_value(&arr, item);
                }
                dbus_message_iter_close_container(iter, &arr);
            } else if constexpr (std::is_same_v<T, Dict>) {
                DBusMessageIter arr;
                dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "{sv}", &arr);
                for (auto const &[key, value] : val.entries) {
                    DBusMessageIter entry;
                    dbus_message_iter_open_container(&arr, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
                    char const *key_ptr = key.c_str();
                    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key_ptr);
                    append_value(&entry, Value{Variant{std::make_shared<Value>(value)}});
                    dbus_message_iter_close_container(&arr, &entry);
                }
                dbus_message_iter_close_container(iter, &arr);
            }
        },
        v.base());
}

Value read_value(DBusMessageIter *iter) {
    switch (dbus_message_iter_get_arg_type(iter)) {
    case DBUS_TYPE_STRING: {
        char const *s = nullptr;
        dbus_message_iter_get_basic(iter, &s);
        return Value{std::string(s ? s : "")};
    }
    case DBUS_TYPE_OBJECT_PATH: {
        char const *s = nullptr;
        dbus_message_iter_get_basic(iter, &s);
        return Value{ObjectPath{std::string(s ? s : "")}};
    }
    case DBUS_TYPE_INT32: {
        int32_t v = 0;
        dbus_message_iter_get_basic(iter, &v);
        return Value{v};
    }
    case DBUS_TYPE_UINT32: {
        uint32_t v = 0;
        dbus_message_iter_get_basic(iter, &v);
        return Value{v};
    }
    case DBUS_TYPE_BOOLEAN: {
        dbus_bool_t v = FALSE;
        dbus_message_iter_get_basic(iter, &v);
        return Value{v != FALSE};
    }
    case DBUS_TYPE_VARIANT: {
        DBusMessageIter sub;
        dbus_message_iter_recurse(iter, &sub);
        return Value{Variant{std::make_shared<Value>(read_value(&sub))}};
    }
    case DBUS_TYPE_ARRAY: {
        if (dbus_message_iter_get_element_type(iter) == DBUS_TYPE_STRUCT) {
            std::vector<IconPixmap> pixmaps;
            DBusMessageIter arr;
            dbus_message_iter_recurse(iter, &arr);
            while (dbus_message_iter_get_arg_type(&arr) == DBUS_TYPE_STRUCT) {
                DBusMessageIter st;
                dbus_message_iter_recurse(&arr, &st);
                IconPixmap px;
                dbus_message_iter_get_basic(&st, &px.width);
                dbus_message_iter_next(&st);
                dbus_message_iter_get_basic(&st, &px.height);
                dbus_message_iter_next(&st);
                if (dbus_message_iter_get_arg_type(&st) == DBUS_TYPE_ARRAY) {
                    DBusMessageIter bytes;
                    dbus_message_iter_recurse(&st, &bytes);
                    if (dbus_message_iter_get_arg_type(&bytes) == DBUS_TYPE_BYTE) {
                        uint8_t *data = nullptr;
                        int len = 0;
                        dbus_message_iter_get_fixed_array(&bytes, &data, &len);
                        px.argb32.assign(data, data + len);
                    }
                }
                pixmaps.push_back(std::move(px));
                dbus_message_iter_next(&arr);
            }
            return Value{std::move(pixmaps)};
        }
        if (dbus_message_iter_get_element_type(iter) == DBUS_TYPE_DICT_ENTRY) {
            Dict dict;
            DBusMessageIter arr;
            dbus_message_iter_recurse(iter, &arr);
            while (dbus_message_iter_get_arg_type(&arr) == DBUS_TYPE_DICT_ENTRY) {
                DBusMessageIter entry;
                dbus_message_iter_recurse(&arr, &entry);
                char const *key = nullptr;
                dbus_message_iter_get_basic(&entry, &key);
                dbus_message_iter_next(&entry);
                dict.entries.emplace_back(key ? key : "", read_value(&entry));
                dbus_message_iter_next(&arr);
            }
            return Value{std::move(dict)};
        }
        switch (dbus_message_iter_get_element_type(iter)) {
        case DBUS_TYPE_STRING:
        case DBUS_TYPE_OBJECT_PATH:
        case DBUS_TYPE_INT32:
        case DBUS_TYPE_UINT32:
        case DBUS_TYPE_BOOLEAN:
        case DBUS_TYPE_VARIANT: {
            // Homogeneous primitive array -- best effort, see the header's
            // EVENTUALLY note for what compound element types aren't covered.
            Array result{std::string(1, static_cast<char>(dbus_message_iter_get_element_type(iter))),
                        {}};
            DBusMessageIter arr;
            dbus_message_iter_recurse(iter, &arr);
            while (dbus_message_iter_get_arg_type(&arr) != DBUS_TYPE_INVALID) {
                result.items.push_back(read_value(&arr));
                dbus_message_iter_next(&arr);
            }
            return Value{std::move(result)};
        }
        default:
            break;
        }
        spdlog::warn("dbus: unsupported array element type {} in reply/signal, skipping",
                     dbus_message_iter_get_element_type(iter));
        return Value{std::string{}};
    }
    case DBUS_TYPE_STRUCT: {
        Struct s;
        DBusMessageIter sub;
        dbus_message_iter_recurse(iter, &sub);
        while (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INVALID) {
            s.fields.push_back(read_value(&sub));
            dbus_message_iter_next(&sub);
        }
        return Value{std::move(s)};
    }
    default:
        spdlog::warn("dbus: unsupported argument type '{}', skipping",
                     static_cast<char>(dbus_message_iter_get_arg_type(iter)));
        return Value{std::string{}};
    }
}

std::vector<Value> read_args(DBusMessage *msg) {
    std::vector<Value> args;
    DBusMessageIter it;
    if (dbus_message_iter_init(msg, &it)) {
        do {
            args.push_back(read_value(&it));
        } while (dbus_message_iter_next(&it));
    }
    return args;
}

struct CallContext {
    Service::ReplyHandler on_reply;
    Service::ErrorHandler on_error;
};

} // namespace

// ── ExportedObject::Impl ─────────────────────────────────────────────────────

struct ExportedObject::Impl {
    struct MethodEntry {
        std::string interface;
        std::string member;
        MethodHandler handler;
    };
    struct PropertyEntry {
        std::string interface;
        std::string name;
        PropertyGetter getter;
        PropertySetter setter;
    };
    std::vector<MethodEntry> methods;
    std::vector<PropertyEntry> properties;
};

ExportedObject::ExportedObject(Service *owner, std::string path)
    : owner_(owner), path_(std::move(path)), impl_(std::make_unique<Impl>()) {}

ExportedObject &ExportedObject::add_method(std::string interface, std::string member,
                                           MethodHandler handler) {
    impl_->methods.push_back({std::move(interface), std::move(member), std::move(handler)});
    return *this;
}

ExportedObject &ExportedObject::add_property(std::string interface, std::string name,
                                             PropertyGetter getter, PropertySetter setter) {
    impl_->properties.push_back(
        {std::move(interface), std::move(name), std::move(getter), std::move(setter)});
    return *this;
}

void ExportedObject::notify_properties_changed(std::string interface,
                                               std::vector<std::string> names) {
    std::vector<std::pair<std::string, Value>> changed;
    for (auto const &name : names) {
        auto it = std::find_if(impl_->properties.begin(), impl_->properties.end(),
                               [&](Impl::PropertyEntry const &p) {
                                   return p.interface == interface && p.name == name;
                               });
        if (it != impl_->properties.end()) {
            changed.emplace_back(name, it->getter());
        }
    }
    if (!changed.empty()) {
        owner_->emit_properties_changed(path_, interface, std::move(changed));
    }
}

// ── Service::Impl ────────────────────────────────────────────────────────────

struct Service::Impl {
    DBusConnection *conn = nullptr;
    std::vector<std::unique_ptr<ExportedObject>> objects;

    struct Subscription {
        uint64_t id;
        std::string path;
        std::string interface;
        std::string member;
        std::string rule;
        SignalHandler handler;
    };
    std::vector<Subscription> subscriptions;
    uint64_t next_token = 1;
};

// ── DBusRawBridge: the only code in the toolkit that touches raw libdbus ────
// callback types (DBusHandlerResult, DBusMessage, ...) while also reaching
// into Service/ExportedObject private state. See the header comment on
// DBusRawBridge for why this indirection exists.

struct DBusRawBridge {
    static DBusHandlerResult handle_properties(DBusConnection *conn, DBusMessage *msg,
                                                std::string const &member, ExportedObject &obj) {
        DBusMessageIter args;
        dbus_message_iter_init(msg, &args);

        auto send_error = [&](char const *name, char const *text) {
            DBusMessage *err = dbus_message_new_error(msg, name, text);
            dbus_connection_send(conn, err, nullptr);
            dbus_message_unref(err);
        };

        if (member == "Get") {
            char const *iface_c = nullptr;
            char const *name_c = nullptr;
            dbus_message_iter_get_basic(&args, &iface_c);
            dbus_message_iter_next(&args);
            dbus_message_iter_get_basic(&args, &name_c);
            std::string iface = iface_c ? iface_c : "", name = name_c ? name_c : "";

            for (auto const &prop : obj.impl_->properties) {
                if (prop.interface != iface || prop.name != name) {
                    continue;
                }
                DBusMessage *reply = dbus_message_new_method_return(msg);
                DBusMessageIter it;
                dbus_message_iter_init_append(reply, &it);
                append_value(&it, Value{Variant{std::make_shared<Value>(prop.getter())}});
                dbus_connection_send(conn, reply, nullptr);
                dbus_message_unref(reply);
                return DBUS_HANDLER_RESULT_HANDLED;
            }
            send_error(DBUS_ERROR_UNKNOWN_PROPERTY, "no such property");
            return DBUS_HANDLER_RESULT_HANDLED;
        }

        if (member == "GetAll") {
            char const *iface_c = nullptr;
            dbus_message_iter_get_basic(&args, &iface_c);
            std::string iface = iface_c ? iface_c : "";

            DBusMessage *reply = dbus_message_new_method_return(msg);
            DBusMessageIter it;
            dbus_message_iter_init_append(reply, &it);
            DBusMessageIter arr;
            dbus_message_iter_open_container(&it, DBUS_TYPE_ARRAY, "{sv}", &arr);
            for (auto const &prop : obj.impl_->properties) {
                if (prop.interface != iface) {
                    continue;
                }
                DBusMessageIter entry;
                dbus_message_iter_open_container(&arr, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
                char const *name_ptr = prop.name.c_str();
                dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &name_ptr);
                append_value(&entry, Value{Variant{std::make_shared<Value>(prop.getter())}});
                dbus_message_iter_close_container(&arr, &entry);
            }
            dbus_message_iter_close_container(&it, &arr);
            dbus_connection_send(conn, reply, nullptr);
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_HANDLED;
        }

        if (member == "Set") {
            char const *iface_c = nullptr;
            char const *name_c = nullptr;
            dbus_message_iter_get_basic(&args, &iface_c);
            dbus_message_iter_next(&args);
            dbus_message_iter_get_basic(&args, &name_c);
            dbus_message_iter_next(&args);
            std::string iface = iface_c ? iface_c : "", name = name_c ? name_c : "";

            for (auto const &prop : obj.impl_->properties) {
                if (prop.interface != iface || prop.name != name) {
                    continue;
                }
                if (!prop.setter) {
                    send_error(DBUS_ERROR_PROPERTY_READ_ONLY, "read-only property");
                    return DBUS_HANDLER_RESULT_HANDLED;
                }
                Value v = read_value(&args);
                if (auto const *variant = std::get_if<Variant>(&v.base()); variant && variant->value) {
                    prop.setter(*variant->value);
                } else {
                    prop.setter(v);
                }
                DBusMessage *reply = dbus_message_new_method_return(msg);
                dbus_connection_send(conn, reply, nullptr);
                dbus_message_unref(reply);
                return DBUS_HANDLER_RESULT_HANDLED;
            }
            send_error(DBUS_ERROR_UNKNOWN_PROPERTY, "no such property");
            return DBUS_HANDLER_RESULT_HANDLED;
        }

        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    static DBusHandlerResult object_path_handler(DBusConnection *conn, DBusMessage *msg,
                                                 void *user_data) {
        auto *obj = static_cast<ExportedObject *>(user_data);
        if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        char const *iface_c = dbus_message_get_interface(msg);
        char const *member_c = dbus_message_get_member(msg);
        if (!member_c) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        std::string iface = iface_c ? iface_c : "";
        std::string member = member_c;

        if (iface == "org.freedesktop.DBus.Properties") {
            return handle_properties(conn, msg, member, *obj);
        }

        for (auto const &entry : obj->impl_->methods) {
            if (entry.interface != iface || entry.member != member) {
                continue;
            }
            MethodCall call{iface, member, read_args(msg)};
            MethodResult result = entry.handler(call);
            DBusMessage *reply = nullptr;
            if (auto const *ok = std::get_if<MethodReply>(&result)) {
                reply = dbus_message_new_method_return(msg);
                DBusMessageIter it;
                dbus_message_iter_init_append(reply, &it);
                for (auto const &a : ok->args) {
                    append_value(&it, a);
                }
            } else {
                auto const &e = std::get<MethodError>(result);
                reply = dbus_message_new_error(msg, e.name.c_str(), e.message.c_str());
            }
            dbus_connection_send(conn, reply, nullptr);
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    static DBusHandlerResult connection_filter(DBusConnection *, DBusMessage *msg, void *user_data) {
        auto *svc = static_cast<Service *>(user_data);
        if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_SIGNAL) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        char const *iface_c = dbus_message_get_interface(msg);
        char const *member_c = dbus_message_get_member(msg);
        char const *path_c = dbus_message_get_path(msg);
        if (!iface_c || !member_c) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }

        // Copy first: a handler that itself calls subscribe()/unsubscribe()
        // would otherwise invalidate the vector while we're iterating it.
        auto subs = svc->impl_->subscriptions;
        for (auto const &sub : subs) {
            if (sub.interface != iface_c || sub.member != member_c) {
                continue;
            }
            if (!sub.path.empty() && (!path_c || sub.path != path_c)) {
                continue;
            }
            Signal sig{iface_c, member_c, read_args(msg)};
            sub.handler(sig);
        }
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    static void pending_call_notify(DBusPendingCall *pending, void *user_data) {
        auto *ctx = static_cast<CallContext *>(user_data);
        DBusMessage *reply = dbus_pending_call_steal_reply(pending);
        if (!reply) {
            return;
        }
        if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
            if (ctx->on_error) {
                DBusError err;
                dbus_error_init(&err);
                dbus_set_error_from_message(&err, reply);
                ctx->on_error(err.name ? err.name : "", err.message ? err.message : "");
                dbus_error_free(&err);
            }
        } else if (ctx->on_reply) {
            MethodReply r{read_args(reply)};
            ctx->on_reply(r);
        }
        dbus_message_unref(reply);
    }

    static void free_call_context(void *user_data) { delete static_cast<CallContext *>(user_data); }
};

// ── Service ───────────────────────────────────────────────────────────────────

Service::Service() : impl_(std::make_unique<Impl>()) {}

std::unique_ptr<Service> Service::connect_session_bus() {
    DBusError err;
    dbus_error_init(&err);
    DBusConnection *conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err) || !conn) {
        spdlog::warn("dbus: failed to connect to session bus: {}", err.message ? err.message : "?");
        dbus_error_free(&err);
        return nullptr;
    }
    dbus_error_free(&err);

    // Never let libdbus exit(1) the whole process just because the bus
    // connection dropped -- that's the (surprising) default.
    dbus_connection_set_exit_on_disconnect(conn, FALSE);

    auto svc = std::unique_ptr<Service>(new Service());
    svc->impl_->conn = conn;
    dbus_connection_add_filter(conn, &DBusRawBridge::connection_filter, svc.get(), nullptr);
    return svc;
}

Service::~Service() {
    if (impl_ && impl_->conn) {
        dbus_connection_close(impl_->conn);
        dbus_connection_unref(impl_->conn);
    }
}

bool Service::request_name(std::string const &name) {
    DBusError err;
    dbus_error_init(&err);
    int ret = dbus_bus_request_name(impl_->conn, name.c_str(), DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    if (dbus_error_is_set(&err)) {
        spdlog::warn("dbus: request_name('{}') failed: {}", name, err.message ? err.message : "?");
        dbus_error_free(&err);
        return false;
    }
    return ret == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER;
}

std::string Service::unique_name() const {
    char const *n = dbus_bus_get_unique_name(impl_->conn);
    return n ? n : "";
}

ExportedObject &Service::export_object(std::string const &path) {
    auto obj = std::unique_ptr<ExportedObject>(new ExportedObject(this, path));
    static DBusObjectPathVTable const vtable = {
        nullptr, &DBusRawBridge::object_path_handler, nullptr, nullptr, nullptr, nullptr,
    };
    DBusError err;
    dbus_error_init(&err);
    if (!dbus_connection_try_register_object_path(impl_->conn, path.c_str(), &vtable, obj.get(),
                                                  &err)) {
        spdlog::error("dbus: failed to register object path '{}': {}", path,
                     err.message ? err.message : "?");
    }
    dbus_error_free(&err);
    auto &ref = *obj;
    impl_->objects.push_back(std::move(obj));
    return ref;
}

void Service::emit_signal(std::string const &path, std::string const &interface,
                          std::string const &signal, std::vector<Value> args) {
    DBusMessage *msg =
        dbus_message_new_signal(path.c_str(), interface.c_str(), signal.c_str());
    DBusMessageIter it;
    dbus_message_iter_init_append(msg, &it);
    for (auto const &a : args) {
        append_value(&it, a);
    }
    dbus_connection_send(impl_->conn, msg, nullptr);
    dbus_message_unref(msg);
}

void Service::emit_properties_changed(std::string const &path, std::string const &interface,
                                      std::vector<std::pair<std::string, Value>> changed) {
    DBusMessage *sig =
        dbus_message_new_signal(path.c_str(), "org.freedesktop.DBus.Properties", "PropertiesChanged");
    DBusMessageIter it;
    dbus_message_iter_init_append(sig, &it);

    char const *iface_ptr = interface.c_str();
    dbus_message_iter_append_basic(&it, DBUS_TYPE_STRING, &iface_ptr);

    DBusMessageIter arr;
    dbus_message_iter_open_container(&it, DBUS_TYPE_ARRAY, "{sv}", &arr);
    for (auto const &[name, value] : changed) {
        DBusMessageIter entry;
        dbus_message_iter_open_container(&arr, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        char const *name_ptr = name.c_str();
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &name_ptr);
        append_value(&entry, Value{Variant{std::make_shared<Value>(value)}});
        dbus_message_iter_close_container(&arr, &entry);
    }
    dbus_message_iter_close_container(&it, &arr);

    // invalidated_properties: always empty -- we always send the new value
    // directly rather than asking listeners to re-fetch it.
    DBusMessageIter empty;
    dbus_message_iter_open_container(&it, DBUS_TYPE_ARRAY, "s", &empty);
    dbus_message_iter_close_container(&it, &empty);

    dbus_connection_send(impl_->conn, sig, nullptr);
    dbus_message_unref(sig);
}

void Service::call(std::string const &destination, std::string const &path,
                   std::string const &interface, std::string const &method,
                   std::vector<Value> args) {
    DBusMessage *msg = dbus_message_new_method_call(destination.c_str(), path.c_str(),
                                                    interface.c_str(), method.c_str());
    DBusMessageIter it;
    dbus_message_iter_init_append(msg, &it);
    for (auto const &a : args) {
        append_value(&it, a);
    }
    dbus_message_set_no_reply(msg, TRUE);
    dbus_connection_send(impl_->conn, msg, nullptr);
    dbus_message_unref(msg);
}

void Service::call_async(std::string const &destination, std::string const &path,
                         std::string const &interface, std::string const &method,
                         std::vector<Value> args, ReplyHandler on_reply, ErrorHandler on_error) {
    DBusMessage *msg = dbus_message_new_method_call(destination.c_str(), path.c_str(),
                                                    interface.c_str(), method.c_str());
    DBusMessageIter it;
    dbus_message_iter_init_append(msg, &it);
    for (auto const &a : args) {
        append_value(&it, a);
    }

    DBusPendingCall *pending = nullptr;
    dbus_connection_send_with_reply(impl_->conn, msg, &pending, -1 /* default timeout */);
    dbus_message_unref(msg);

    if (!pending) {
        if (on_error) {
            on_error("org.freedesktop.DBus.Error.Disconnected", "not connected to the bus");
        }
        return;
    }

    auto *ctx = new CallContext{std::move(on_reply), std::move(on_error)};
    dbus_pending_call_set_notify(pending, &DBusRawBridge::pending_call_notify, ctx,
                                 &DBusRawBridge::free_call_context);
    dbus_pending_call_unref(pending);
}

Service::SubscriptionToken Service::subscribe(std::string const &sender, std::string const &path,
                                              std::string const &interface,
                                              std::string const &member, SignalHandler handler) {
    std::string rule = "type='signal'";
    if (!sender.empty()) {
        rule += ",sender='" + sender + "'";
    }
    if (!path.empty()) {
        rule += ",path='" + path + "'";
    }
    if (!interface.empty()) {
        rule += ",interface='" + interface + "'";
    }
    if (!member.empty()) {
        rule += ",member='" + member + "'";
    }

    DBusError err;
    dbus_error_init(&err);
    dbus_bus_add_match(impl_->conn, rule.c_str(), &err);
    if (dbus_error_is_set(&err)) {
        spdlog::warn("dbus: subscribe('{}') failed: {}", rule, err.message ? err.message : "?");
        dbus_error_free(&err);
    }

    auto id = impl_->next_token++;
    impl_->subscriptions.push_back({id, path, interface, member, std::move(rule), std::move(handler)});
    return {id};
}

void Service::unsubscribe(SubscriptionToken token) {
    auto &subs = impl_->subscriptions;
    auto it = std::find_if(subs.begin(), subs.end(),
                           [&](Impl::Subscription const &s) { return s.id == token.id; });
    if (it == subs.end()) {
        return;
    }
    dbus_bus_remove_match(impl_->conn, it->rule.c_str(), nullptr);
    subs.erase(it);
}

std::vector<Service::PollFd> Service::poll_fds() const {
    int fd = -1;
    if (!impl_->conn || !dbus_connection_get_unix_fd(impl_->conn, &fd) || fd < 0) {
        return {};
    }
    // Single-fd shortcut: relies on the session/system bus transport always
    // being one UNIX domain socket for the connection's lifetime, which is
    // true for the standard transports libdbus uses for DBUS_BUS_SESSION.
    // The fully "correct" way per libdbus's own docs is
    // dbus_connection_set_watch_functions(), which supports transports with
    // more than one fd/watch -- not needed for what this wraps, but see the
    // header's EVENTUALLY note: it's also what real pending-call timeout
    // enforcement would need.
    bool want_write = dbus_connection_has_messages_to_send(impl_->conn);
    return {PollFd{fd, true, want_write}};
}

int Service::next_timeout_ms() const { return -1; }

void Service::dispatch() {
    if (!impl_->conn) {
        return;
    }
    dbus_connection_read_write(impl_->conn, 0);
    DBusDispatchStatus status;
    do {
        status = dbus_connection_dispatch(impl_->conn);
    } while (status == DBUS_DISPATCH_DATA_REMAINS);
}

} // namespace svision3::dbus
