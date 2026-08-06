// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/tray_icon.hpp"
#include "toolkit/linux/dbus_service.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/window.hpp"
#include <spdlog/spdlog.h>

namespace toolkit {

using namespace toolkit::dbus;

struct TrayIcon::Impl {
    std::vector<Command::Ptr> right_click_actions;
    Window *owner_window = nullptr;
    // Tracks what the *last* Activate()-driven toggle did, not the window's
    // actual state -- there is no is_visible() query on Window to read that
    // back. Can drift from reality if the window is also closed/reopened
    // some other way; good enough for a left-click toggle, not a substitute
    // for real visibility tracking.
    bool window_shown = true;
    std::unique_ptr<Service> service;
    std::vector<int> registered_fds;

    // Unregisters fd sources from the platform's event loop *before*
    // `service` (and the ExportedObject method/property handlers it owns,
    // all of which capture `this`) get destroyed -- otherwise a later
    // poll() wakeup could invoke a callback pointing at a freed Impl.
    ~Impl() {
        if (auto *app = detail::current_platform()) {
            for (auto fd : registered_fds) {
                app->remove_fd_source(fd);
            }
        }
    }

    // Mirrors dbus_service.hpp's own poll_fds()/dispatch() integration
    // pattern -- add_fd_source is idempotent (updates in place), so this is
    // cheap to call after every dispatch() even though in practice the
    // session bus connection is always exactly one fd for its lifetime and
    // only its want_write flag actually changes.
    void sync_fd_sources() {
        auto *app = detail::current_platform();
        if (!app) {
            return;
        }
        auto current = service->poll_fds();
        registered_fds.clear();
        for (auto const &pfd : current) {
            registered_fds.push_back(pfd.fd);
            app->add_fd_source(pfd.fd, pfd.want_read, pfd.want_write, [this] {
                service->dispatch();
                sync_fd_sources();
            });
        }
    }
};

TrayIcon::TrayIcon() : impl_(std::make_unique<Impl>()) {}
TrayIcon::~TrayIcon() = default;

std::unique_ptr<TrayIcon> TrayIcon::create(std::string icon_name, std::string tooltip, std::string id,
                                           Window *owner_window,
                                           std::vector<Command::Ptr> right_click_actions) {
    auto service = Service::connect_session_bus();
    if (!service) {
        spdlog::warn("TrayIcon: no D-Bus session bus reachable, tray icon not created");
        return nullptr;
    }

    auto tray = std::unique_ptr<TrayIcon>(new TrayIcon());
    auto *self = tray->impl_.get();
    self->right_click_actions = std::move(right_click_actions);
    self->owner_window = owner_window;
    self->service = std::move(service);
    auto *svc = self->service.get();

    // ── com.canonical.dbusmenu, so the host renders the right-click menu ───
    // natively.
    auto &menu = svc->export_object("/MenuBar");
    menu.add_property("com.canonical.dbusmenu", "Version", [] { return Value{uint32_t{3}}; });
    menu.add_property("com.canonical.dbusmenu", "TextDirection",
                      [] { return Value{std::string("ltr")}; });
    menu.add_property("com.canonical.dbusmenu", "Status", [] { return Value{std::string("normal")}; });

    menu.add_method("com.canonical.dbusmenu", "GetLayout",
                    [self](MethodCall const &) -> MethodResult {
                        Array children{"v", {}};
                        for (size_t i = 0; i < self->right_click_actions.size(); ++i) {
                            auto const &cmd = self->right_click_actions[i];
                            Dict props{{{"label", Value{cmd->name()}},
                                       {"enabled", Value{cmd->is_enabled()}}}};
                            if (!cmd->icon().empty()) {
                                props.entries.emplace_back("icon-name", Value{cmd->icon()});
                            }
                            Struct child{{Value{static_cast<int32_t>(i + 1)}, Value{props},
                                         Value{Array{"v", {}}}}};
                            children.items.push_back(
                                Value{Variant{std::make_shared<Value>(Value{child})}});
                        }
                        Struct root{{Value{int32_t{0}}, Value{Dict{}}, Value{children}}};
                        return MethodReply{{Value{uint32_t{1}}, Value{root}}};
                    });
    menu.add_method("com.canonical.dbusmenu", "Event",
                    [self](MethodCall const &call) -> MethodResult {
                        if (call.args.size() >= 2) {
                            auto const *id_arg = std::get_if<int32_t>(&call.args[0].base());
                            auto const *event_id = std::get_if<std::string>(&call.args[1].base());
                            if (id_arg && event_id && *event_id == "clicked" && *id_arg >= 1) {
                                auto index = static_cast<size_t>(*id_arg - 1);
                                if (index < self->right_click_actions.size()) {
                                    self->right_click_actions[index]->execute();
                                }
                            }
                        }
                        return MethodReply{};
                    });
    menu.add_method("com.canonical.dbusmenu", "AboutToShow",
                    [](MethodCall const &) -> MethodResult { return MethodReply{{Value{false}}}; });

    // ── org.kde.StatusNotifierItem ───────────────────────────────────────────
    auto &item = svc->export_object("/StatusNotifierItem");
    item.add_property("org.kde.StatusNotifierItem", "Category",
                      [] { return Value{std::string("ApplicationStatus")}; });
    item.add_property("org.kde.StatusNotifierItem", "Id", [id] { return Value{id}; });
    item.add_property("org.kde.StatusNotifierItem", "Title", [tooltip] { return Value{tooltip}; });
    item.add_property("org.kde.StatusNotifierItem", "Status",
                      [] { return Value{std::string("Active")}; });
    item.add_property("org.kde.StatusNotifierItem", "WindowId", [] { return Value{uint32_t{0}}; });
    item.add_property("org.kde.StatusNotifierItem", "IconName", [icon_name] { return Value{icon_name}; });
    // false: an item with ItemIsMenu=true has *only* a menu -- hosts open
    // /MenuBar on left-click too and never call Activate() at all. Keeping
    // it false is what makes left-click (Activate) and right-click (the
    // Menu property) distinguishable in the first place.
    item.add_property("org.kde.StatusNotifierItem", "ItemIsMenu", [] { return Value{false}; });
    item.add_property("org.kde.StatusNotifierItem", "Menu",
                      [] { return Value{ObjectPath{"/MenuBar"}}; });
    item.add_property("org.kde.StatusNotifierItem", "ToolTip", [tooltip, icon_name] {
        return Value{Struct{{Value{icon_name}, Value{Array{"(iiay)", {}}}, Value{tooltip},
                            Value{std::string("")}}}};
    });

    auto noop = [](MethodCall const &) -> MethodResult { return MethodReply{}; };
    item.add_method("org.kde.StatusNotifierItem", "Activate",
                    [self](MethodCall const &) -> MethodResult {
                        if (self->owner_window) {
                            if (self->window_shown) {
                                self->owner_window->hide();
                            } else {
                                self->owner_window->show();
                            }
                            self->window_shown = !self->window_shown;
                        }
                        return MethodReply{};
                    });
    item.add_method("org.kde.StatusNotifierItem", "SecondaryActivate", noop);
    item.add_method("org.kde.StatusNotifierItem", "ContextMenu", noop);
    item.add_method("org.kde.StatusNotifierItem", "Scroll", noop);

    // Our own unique bus name always works and never collides -- no need to
    // race for a well-known org.kde.StatusNotifierItem-<pid>-<n> name.
    svc->call("org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
             "org.kde.StatusNotifierWatcher", "RegisterStatusNotifierItem",
             {Value{svc->unique_name()}});

    self->sync_fd_sources();
    return tray;
}

} // namespace toolkit
