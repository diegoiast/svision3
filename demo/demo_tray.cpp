// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/application.hpp"
#include "toolkit/tray_icon.hpp"
#include "toolkit/rich_label.hpp"
#include "toolkit/window.hpp"
#include "toolkit/xdg_image_loader.hpp"
#include <spdlog/spdlog.h>

using namespace toolkit;

int main() {
    auto app = Application{};
    // "breeze" is a real system theme (unlike demo1.cpp/demo_declarative.cpp's bundled "Faenza",
    // which only resolves when run from the repo root) -- needed so the menu icons below
    // (document-new, edit-copy, ...) actually load instead of failing silently.
    app.set_icon_provider(std::make_unique<toolkit::XdgImageLoader>("breeze"));

    auto *win = app.create_window("Tray Demo", {400, 150});
    auto label = std::make_unique<RichLabel>(
        "**This window exists only to keep the app running.**\n\n"
        "Left-click the tray icon to hide/show this window.\nRight-click it for a menu.");
    auto *label_ptr = label.get();
    win->set_root(std::move(label));

    std::vector<Command::Ptr> right_click_actions;
    auto add_action = [label_ptr, &app](std::vector<Command::Ptr> &list, std::string name,
                                        std::string icon) {
        auto cmd = Command::create(name, [label_ptr, &app, name] {
            spdlog::info("Tray menu: {} clicked", name);
            if (name == "Quit") {
                app.quit();
                return;
            }
            label_ptr->set_markdown("Tray menu: **" + name + "** clicked");
        });
        cmd->set_icon(std::move(icon));
        list.push_back(std::move(cmd));
    };
    add_action(right_click_actions, "New", "document-new");
    add_action(right_click_actions, "Open", "document-open");
    add_action(right_click_actions, "Save", "document-save");
    add_action(right_click_actions, "Copy", "edit-copy");
    add_action(right_click_actions, "Cut", "edit-cut");
    add_action(right_click_actions, "Paste", "edit-paste");
    add_action(right_click_actions, "Quit", "application-exit");

    auto tray = TrayIcon::create("utilities-terminal", "svision3 tray demo", "svision3-tray-demo", win,
                                 std::move(right_click_actions));
    if (!tray) {
        spdlog::error("Failed to create tray icon (no D-Bus session bus?)");
    }

    win->show();
    return app.run();
}
