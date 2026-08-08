// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// First platform-specific header exposed under include/toolkit/ (rather than
// hidden behind the Platform/PlatformWindow abstraction). Linux desktop
// integration (system tray, portal settings, ...) has no cross-platform
// equivalent to abstract behind, so there is nothing to hide this behind --
// callers on other platforms simply don't include it. Only built when
// TOOLKIT_HAS_DBUS is defined (see CMakeLists.txt).
//
// Thin wrapper around raw libdbus (no GDBus, no sd-bus). Covers both
// directions: hosting an object (e.g. org.kde.StatusNotifierItem for the
// system tray) and consuming another service's methods/signals (e.g.
// org.freedesktop.portal.Settings, for desktop dark-mode change
// notifications). Still not a general D-Bus RPC framework: no introspection
// XML generation, no generic proxy-object codegen, no support for D-Bus
// types beyond what Value models -- every method/property/signal has to be
// registered or subscribed to explicitly by name. dbus.h itself never leaks
// into this header or its callers -- see DBusRawBridge below.
//
// EVENTUALLY (not implemented, revisit if/when actually needed):
//  - Pending-call timeouts are not enforced. call_async()'s on_error fires
//    on an explicit D-Bus error reply, but a peer that never replies at all
//    leaves the call pending forever -- next_timeout_ms() always returns -1.
//    Fixing this needs the full dbus_connection_set_timeout_functions()
//    integration instead of the current single-fd shortcut (see dispatch()'s
//    implementation comment in dbus_service.cpp for why that shortcut works
//    at all).
//  - No automatic reconnect/re-registration if a depended-on service
//    (StatusNotifierWatcher, xdg-desktop-portal) restarts, or wasn't running
//    yet at startup. The building block already exists -- subscribe to
//    NameOwnerChanged on org.freedesktop.DBus -- callers just have to do it
//    themselves for now.
//  - No org.freedesktop.DBus.Introspectable support. Skipped deliberately:
//    nothing we talk to (SNI hosts, the portal) requires it.
//  - Struct/Array/Dict reading (as opposed to writing) is best-effort:
//    Struct and homogeneous primitive Array read back fine, but a Dict
//    entry's value is read via the generic path (so it comes back as
//    whatever read_value() produces for that wire type, generally a
//    Variant), and deeply nested compound element types inside an Array
//    (e.g. an array of structs that isn't the one IconPixmap shape we
//    special-case) are logged and dropped rather than reconstructed.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace toolkit::dbus {

// SNI's IconPixmap struct: (width, height, ARGB32 bytes, network byte order).
struct IconPixmap {
    int32_t width = 0;
    int32_t height = 0;
    std::vector<uint8_t> argb32;
};

struct Value;

// D-Bus variants ("v") wrap a value of a type not known until you unmarshal
// it -- e.g. the settings portal sends a setting's value as a variant, and
// the type inside differs per setting (uint32 for color-scheme, could be
// string/bool for others). Value has to be able to hold one of itself to
// represent that; the shared_ptr indirection is what makes that legal (a
// variant can't directly hold itself -- incomplete type).
struct Variant {
    std::shared_ptr<Value> value;
};

// D-Bus object-path type ("o"), distinct from a plain string ("s") -- e.g.
// StatusNotifierItem's "Menu" property must be wire-typed 'o' for hosts that
// validate it, not 's'.
struct ObjectPath {
    std::string path;
};

// Ordered, heterogeneous fields -- marshaled as a D-Bus struct "(...)". Used
// for e.g. DBusMenu's GetLayout, whose reply nests (id, properties, children)
// tuples recursively.
struct Struct {
    std::vector<Value> fields;
};

// Homogeneous elements -- marshaled as a D-Bus array "a<element_signature>".
// element_signature has to be provided explicitly since it can't be inferred
// from an empty vector (an empty items list is legal and common).
struct Array {
    std::string element_signature;
    std::vector<Value> items;
};

// String-keyed dict of variants ("a{sv}") -- the only dict shape D-Bus's core
// interfaces (Properties, DBusMenu) actually use; not a generic a{s<T>}.
struct Dict {
    std::vector<std::pair<std::string, Value>> entries;
};

// Covers the argument/property types SNI, the settings portal, and DBusMenu
// actually use. Deliberately not a fully generic D-Bus type system -- see
// the EVENTUALLY note above for what's still missing.
struct Value : std::variant<std::string, int32_t, uint32_t, bool, std::vector<IconPixmap>, Variant,
                            ObjectPath, Struct, Array, Dict> {
    using Base = std::variant<std::string, int32_t, uint32_t, bool, std::vector<IconPixmap>, Variant,
                              ObjectPath, Struct, Array, Dict>;
    using Base::Base;

    // Named accessor rather than relying on std::visit/std::get_if working
    // directly on a type derived from std::variant -- that behavior has
    // historically varied across standard library versions, so call sites
    // upcast explicitly instead of depending on it.
    Base const &base() const { return *this; }
};

struct MethodCall {
    std::string interface;
    std::string member;
    std::vector<Value> args;
};

struct MethodReply {
    // empty = void reply
    std::vector<Value> args;
};

struct MethodError {
    // e.g. "org.freedesktop.DBus.Error.InvalidArgs"
    std::string name;
    std::string message;
};

using MethodResult = std::variant<MethodReply, MethodError>;

// Same shape as MethodCall -- interface/member/args -- but delivered to a
// signal subscription instead of an exported method's dispatch table.
using Signal = MethodCall;

class Service;

// Implemented entirely in dbus_service.cpp, where the real dbus.h types
// (DBusConnection, DBusMessage, DBusHandlerResult, ...) are available. This
// friend-only bridge is what lets Service/ExportedObject's private state be
// reached from the raw libdbus callbacks without either of those types
// showing up here.
struct DBusRawBridge;

// One exported object (e.g. "/StatusNotifierItem"). Owned by Service;
// obtained via Service::export_object().
class ExportedObject {
  public:
    using MethodHandler = std::function<MethodResult(MethodCall const &)>;
    using PropertyGetter = std::function<Value()>;
    using PropertySetter = std::function<void(Value const &)>;

    // interface/member dispatch is a flat vector scan, not introspection --
    // callers register exactly the handful of methods/properties SNI (or
    // whatever else is exported) defines. Fine at this scale (single digits
    // of entries per object).
    ExportedObject &add_method(std::string interface, std::string member, MethodHandler handler);
    ExportedObject &add_property(std::string interface, std::string name, PropertyGetter getter,
                                 PropertySetter setter = nullptr);

    // Emits org.freedesktop.DBus.Properties.PropertiesChanged for `names`,
    // reading each current value from its registered getter. Names not
    // registered via add_property are silently skipped.
    void notify_properties_changed(std::string interface, std::vector<std::string> names);

  private:
    friend class Service;
    friend struct DBusRawBridge;
    ExportedObject(Service *owner, std::string path);

    Service *owner_ = nullptr;
    std::string path_;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// One session-bus connection. Does NOT run its own loop -- the owning
// platform backend (X11/Wayland/...) polls poll_fds() alongside its own fds
// and calls dispatch() when one of them is readable/writable, the same way
// it already integrates the X11/wl_display socket.
class Service {
  public:
    static std::unique_ptr<Service> connect_session_bus();
    ~Service();

    Service(Service const &) = delete;
    Service &operator=(Service const &) = delete;

    // Per SNI spec: try "org.kde.StatusNotifierItem-<pid>-1", bump the
    // suffix and retry if taken. Returns false only on a real connection
    // error, not on name-already-taken (caller decides the retry policy).
    bool request_name(std::string const &name);

    // This connection's own unique bus name (e.g. ":1.42"), assigned by the
    // bus daemon at connect time. Useful for registering with a well-known
    // watcher service (e.g. org.kde.StatusNotifierWatcher) without needing
    // to win a well-known-name race first -- the unique name always works
    // and never collides.
    std::string unique_name() const;

    ExportedObject &export_object(std::string const &path);

    void emit_signal(std::string const &path, std::string const &interface,
                     std::string const &signal, std::vector<Value> args = {});

    // Fire-and-forget: used for StatusNotifierWatcher.RegisterStatusNotifierItem,
    // where we don't care about (or need to react to) the reply.
    void call(std::string const &destination, std::string const &path, std::string const &interface,
              std::string const &method, std::vector<Value> args = {});

    using ReplyHandler = std::function<void(MethodReply const &)>;
    using ErrorHandler = std::function<void(std::string const &error_name, std::string const &message)>;

    // Async call with a reply: used for things like
    // org.freedesktop.portal.Settings.ReadOne, where we need the current
    // value back, not just to fire the request. Backed by
    // dbus_connection_send_with_reply + DBusPendingCall -- never blocks the
    // caller; on_reply/on_error fire later, from inside dispatch(). See the
    // EVENTUALLY note above: a peer that never replies leaves this pending
    // forever, there is no timeout enforcement yet.
    void call_async(std::string const &destination, std::string const &path,
                    std::string const &interface, std::string const &method,
                    std::vector<Value> args, ReplyHandler on_reply, ErrorHandler on_error = nullptr);

    using SignalHandler = std::function<void(Signal const &)>;
    struct SubscriptionToken {
        uint64_t id = 0;
    };

    // Adds a bus match rule and routes matching signals to `handler`. Used
    // for org.freedesktop.portal.Settings.SettingChanged to detect dark-mode
    // changes, but not tied to that -- any (sender, path, interface, member)
    // signal can be subscribed to. `sender` may be a well-known name (e.g.
    // "org.freedesktop.portal.Desktop") or empty to match any sender; `path`
    // may be empty to match any path.
    SubscriptionToken subscribe(std::string const &sender, std::string const &path,
                                std::string const &interface, std::string const &member,
                                SignalHandler handler);
    void unsubscribe(SubscriptionToken token);

    struct PollFd {
        int fd;
        bool want_read;
        bool want_write;
    };
    // Changes whenever libdbus's outgoing-queue state changes (want_write
    // toggles). Backends should re-fetch this after each dispatch() rather
    // than caching it across iterations.
    std::vector<PollFd> poll_fds() const;

    // Milliseconds until the next scheduled internal action, or -1 if
    // nothing is scheduled. Always -1 today -- see the EVENTUALLY note above
    // on pending-call timeouts. Present in the interface now so callers
    // don't have to change their poll-loop integration when that lands.
    int next_timeout_ms() const;

    // Non-blocking: drains and dispatches whatever is pending, then returns.
    void dispatch();

  private:
    Service();
    friend struct DBusRawBridge;
    friend class ExportedObject;

    void emit_properties_changed(std::string const &path, std::string const &interface,
                                 std::vector<std::pair<std::string, Value>> changed);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace toolkit::dbus
