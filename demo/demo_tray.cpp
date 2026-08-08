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
    // Prefer whatever icon theme the desktop is actually configured with, since "breeze" and
    // friends only exist on a Linux box. Falling back to the repo's bundled "Faenza" is what
    // makes the icons below (utilities-terminal, document-new, edit-copy, ...) resolve on
    // Windows -- but, like demo1.cpp, only when run from the repo root: the bundled theme is
    // looked up as themes/<name> relative to the working directory.
    if (!app.use_xdg_icons()) {
        app.set_icon_provider(std::make_unique<toolkit::XdgImageLoader>("Faenza"));
    }

    auto win = app.create_window("Tray Demo", {400, 150});
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

    // icon() is what every backend draws; icon_name() is the extra hint that lets a Linux tray
    // host theme it and size it to the panel instead of scaling our bitmap.
    auto tray = TrayIcon::builder()
                    .icon(app.load_icon("utilities-terminal", 22, ""))
                    .icon_name("utilities-terminal")
                    .tooltip("svision3 tray demo")
                    .id("svision3-tray-demo")
                    .owner_window(win)
                    .actions(std::move(right_click_actions))
                    .build();
    if (!tray) {
        spdlog::error("Failed to create tray icon (no tray backend on this platform, or no "
                      "D-Bus session bus on Linux)");
    }

    win->show();
    return app.run();
}
