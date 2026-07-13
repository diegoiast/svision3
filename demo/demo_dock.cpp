// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "declarative.hpp"
#include "toolkit/application.hpp"
#include "toolkit/dock_area.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
#include "toolkit/window.hpp"
#include "toolkit/xdg_image_loader.hpp"
#include <array>
#include <filesystem>
#include <fstream>

using namespace toolkit;

static auto make_outline() {
    auto model = std::make_shared<StringListModel>(std::vector<std::string>{
        "class Application",
        "  Application()",
        "  ~Application()",
        "  run() -> int",
        "class Widget",
        "  paint(Painter&)",
        "  handle_mouse(MouseEvent&)",
        "  set_rect(Rect&)",
    });
    return ui::list_view(model);
}

static std::unique_ptr<Widget> make_inspector() {
    auto form = std::make_unique<FormLayout>();
    form->set_margins({8, 8, 8, 8});
    form->set_spacing(6);
    form->set_label_spacing(8);

    auto lbl = [](std::string_view t) { return std::make_unique<Label>(std::string(t)); };
    auto val = [](std::string_view t) { return std::make_unique<Label>(std::string(t)); };

    form->add_row(lbl("Type:"), val("TextEdit"));
    form->add_row(lbl("X:"), val("220"));
    form->add_row(lbl("Y:"), val("0"));
    form->add_row(lbl("Width:"), val("560"));
    form->add_row(lbl("Height:"), val("600"));
    form->add_row(lbl("Visible:"), val("true"));
    form->add_row(lbl("Enabled:"), val("true"));

    return form;
}

static auto make_console() {
    // FIXME: use raw strgins
    return ui::text_edit(
        "[build] Scanning dependencies of target toolkit\n"
        "[build] Building CXX object CMakeFiles/toolkit.dir/src/toolkit/layout.cpp.o\n"
        "[build] Building CXX object CMakeFiles/toolkit.dir/src/toolkit/dock_area.cpp.o\n"
        "[build] Linking CXX static library libtoolkit.a\n"
        "[build] Building CXX executable demo_dock\n"
        "[build] Build finished successfully.\n");
}

static auto make_problems() {
    auto model = std::make_shared<StringListModel>(std::vector<std::string>{"No problems found."});
    return ui::list_view(model);
}

static auto make_center_editor() {
    // FIXME: use raw strgins
    return ui::text_edit(
        "// SPDX-License-Identifier: MIT\n"
        "\n"
        "#include \"toolkit/dock_area.hpp\"\n"
        "#include \"toolkit/application.hpp\"\n"
        "\n"
        "int main(int argc, char *argv[]) {\n"
        "    auto app = toolkit::Application{};\n"
        "\n"
        "    auto *win = app.create_window(\"My App\", {1200, 800});\n"
        "\n"
        "    auto dock = std::make_unique<toolkit::DockArea>();\n"
        "    dock->set_center<toolkit::TextEdit>();\n"
        "    dock->add_dock<toolkit::ListView>(toolkit::DockPosition::Left, \"Files\");\n"
        "    dock->add_dock<toolkit::TextEdit>(toolkit::DockPosition::Bottom, \"Console\");\n"
        "\n"
        "    win->set_root(std::move(dock));\n"
        "    return app.run();\n"
        "}\n");
}

int main(int, char *[]) {
    auto app = Application{};
    app.set_force_csd(true);
    if (!app.use_xdg_icons()) {
        auto loader = std::make_unique<XdgImageLoader>();
        loader->set_theme_path("./themes/Faenza");
        app.set_icon_provider(std::move(loader));
    }

    auto style = Theme::detect_system_style();
    Theme::set_current(ThemeFactory::create(style));

    auto *win = app.create_window("Dock Demo", {1200, 800});

    auto dock = std::make_unique<DockArea>();
    dock->set_margins({0, 0, 0, 0});

    // Center: TabWidget with one initial editor tab
    auto center = ui::tab_widget().tabs_closable(true).add_tab("Welcome", make_center_editor());
    auto *center_ptr = center.get();
    center->on_tab_close = [center_ptr](int index, std::string const &) {
        if (center_ptr->tab_count() > 1) {
            center_ptr->remove_tab(index);
        }
    };
    dock->set_center(std::move(center.w));

    // Left dock: file browser + outline — vertical rotated tabs (West)
    dock->add_dock(DockPosition::Left, "Files",
                   ui::file_browser().on_file_activated([center_ptr](std::string const &path) {
                       auto name = std::filesystem::path(path).filename().string();
                       for (int i = 0; i < static_cast<int>(center_ptr->tab_count()); ++i) {
                           if (center_ptr->tab_title(i) == name) {
                               center_ptr->set_current(i);
                               return;
                           }
                       }
                       auto editor = ui::text_edit();
                       std::ifstream f(path);
                       if (f) {
                           editor->set_text(std::string(std::istreambuf_iterator<char>(f), {}));
                       }
                       auto canonical = std::filesystem::weakly_canonical(path).string();
                       auto tab_index = static_cast<int>(center_ptr->tab_count());
                       center_ptr->add_tab(name, std::move(editor.w));
                       center_ptr->set_tab_tooltip(tab_index, canonical);
                       center_ptr->set_current(tab_index);
                   }));
    dock->add_dock(DockPosition::Left, "Outline", make_outline());
    dock->dock_tab_widget(DockPosition::Left)->set_orientation(TabOrientation::West);

    // Right dock: inspector — vertical rotated tabs (East)
    dock->add_dock(DockPosition::Right, "Inspector", make_inspector());
    dock->set_dock_size(DockPosition::Right, 200.0f);
    dock->dock_tab_widget(DockPosition::Right)->set_orientation(TabOrientation::East);

    // Bottom dock: console + problems — tabs on bottom edge
    dock->add_dock(DockPosition::Bottom, "Console", make_console());
    dock->add_dock(DockPosition::Bottom, "Problems", make_problems());
    dock->set_dock_size(DockPosition::Bottom, 180.0f);
    dock->dock_tab_widget(DockPosition::Bottom)->set_orientation(TabOrientation::South);
    dock->dock_tab_widget(DockPosition::Bottom)->set_collapsible(true);

    auto *dock_ptr = dock.get();

    auto vertical = std::make_shared<bool>(true);
    auto toggle_vertical = [dock_ptr, vertical]() {
        *vertical = !*vertical;
        auto *left = dock_ptr->dock_tab_widget(DockPosition::Left);
        if (left) {
            left->set_orientation(*vertical ? TabOrientation::West : TabOrientation::North);
        }
        auto *right = dock_ptr->dock_tab_widget(DockPosition::Right);
        if (right) {
            right->set_orientation(*vertical ? TabOrientation::East : TabOrientation::North);
        }
    };

    auto make_theme = [](ThemeStyle style, ColorScheme scheme) {
        return [style, scheme] { Theme::set_current(ThemeFactory::create(style, scheme)); };
    };

    auto cmd_toggle_docks = Command::create("Toggle Docks", [dock_ptr]() {
        std::array positions = {DockPosition::Left, DockPosition::Right, DockPosition::Bottom};
        bool any_open = false;
        for (auto pos : positions) {
            auto *tab = dock_ptr->dock_tab_widget(pos);
            if (tab && !tab->is_collapsed()) {
                any_open = true;
                break;
            }
        }
        for (auto pos : positions) {
            auto *tab = dock_ptr->dock_tab_widget(pos);
            if (tab) {
                tab->set_collapsed(any_open);
            }
        }
    });
    cmd_toggle_docks->set_shortcut("ctrl+e");
    win->add_command(cmd_toggle_docks);

    auto cmd_inspector = Command::create("Toggle Inspector", [win]() {
        Widget::debug_show_inspector = !Widget::debug_show_inspector;
        win->request_redraw("inspector toggle");
    });
    cmd_inspector->set_shortcut("super+f12");
    win->add_command(cmd_inspector);

    center_ptr->set_leading_widget(
        ui::button("Menu")
            //.flat(true)
            .focusable(false)
            .background_color(Color::from_rgb(0x41cd52))
            .menu(ui::menu()
                      .action("Vertical Tabs", toggle_vertical)
                      .action(cmd_toggle_docks)
                      .separator()
                      .submenu("Theme",
                               ui::menu()
                                   .action("Material Light",
                                           make_theme(ThemeStyle::Material, ColorScheme::Light))
                                   .action("Material Dark",
                                           make_theme(ThemeStyle::Material, ColorScheme::Dark))
                                   .separator()
                                   .action("GNOME Light",
                                           make_theme(ThemeStyle::GNOME, ColorScheme::Light))
                                   .action("GNOME Dark",
                                           make_theme(ThemeStyle::GNOME, ColorScheme::Dark))
                                   .separator()
                                   .action("Plasma Light",
                                           make_theme(ThemeStyle::Plasma6, ColorScheme::Light))
                                   .action("Plasma Dark",
                                           make_theme(ThemeStyle::Plasma6, ColorScheme::Dark))
                                   .separator()
                                   .action("Windows 11 Light",
                                           make_theme(ThemeStyle::Win11, ColorScheme::Light))
                                   .action("Windows 11 Dark",
                                           make_theme(ThemeStyle::Win11, ColorScheme::Dark))
                                   .separator()
                                   .action("Windows 95",
                                           make_theme(ThemeStyle::Win95, ColorScheme::Light)))
                      .separator()
                      .action("Quit", [] { Application::instance().quit(); }))
            .w);

    auto root = ui::vbox().margins({0, 0, 0, 0}).spacing(0).add(std::move(dock), 1);

    win->set_root(std::move(root.w));
    win->show();
    return app.run();
}
