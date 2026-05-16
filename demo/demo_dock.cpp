// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/application.hpp"
#include "toolkit/dock_area.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/text_edit.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
#include "toolkit/window.hpp"
#include <spdlog/spdlog.h>

int main() {
    auto app = toolkit::Application{};
    auto window = app.make_window(800, 600, "Dock Area Demo");

    auto dock_area = std::make_unique<toolkit::DockArea>();

    // Top panel — toolbar area
    {
        auto label = std::make_unique<toolkit::Label>("Toolbar area - top of the window");
        label->set_shrinkable(true);
        auto panel = std::make_unique<toolkit::DockPanel>("Top Panel", std::move(label));
        dock_area->set_top(std::move(panel));
    }

    // Bottom panel — status bar
    {
        auto label = std::make_unique<toolkit::Label>("Ready | Line 1, Col 1");
        label->set_shrinkable(true);
        auto panel = std::make_unique<toolkit::DockPanel>("Status Bar", std::move(label));
        dock_area->set_bottom(std::move(panel));
    }

    // Left panel — project / file tree
    {
        auto label = std::make_unique<toolkit::Label>("File tree goes here");
        label->set_shrinkable(true);
        auto panel = std::make_unique<toolkit::DockPanel>("Project", std::move(label));
        panel->set_min_size({160, 0});
        dock_area->set_left(std::move(panel));
    }

    // Right panel — properties
    {
        auto label = std::make_unique<toolkit::Label>("Properties panel");
        label->set_shrinkable(true);
        auto panel = std::make_unique<toolkit::DockPanel>("Properties", std::move(label));
        panel->set_min_size({160, 0});
        dock_area->set_right(std::move(panel));
    }

    // Center — main text editor
    {
        auto editor = std::make_unique<toolkit::TextEdit>();
        editor->set_text(
            "// Main editing area\n"
            "// This is the center content\n"
            "\n"
            "int main() {\n"
            "    return 0;\n"
            "}\n");
        dock_area->set_center(std::move(editor));
    }

    window->set_root(std::move(dock_area));
    return app.run();
}
