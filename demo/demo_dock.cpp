// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "declarative.hpp"

#include <array>
#include <filesystem>
#include <fstream>

#include <spdlog/spdlog.h>
#include <svision3/application.hpp>
#include <svision3/theme.hpp>
#include <svision3/theme_factory.hpp>
#include <svision3/window.hpp>
#include <svision3/xdg_image_loader.hpp>

using namespace svision3;

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
        "[build] Scanning dependencies of target svision3\n"
        "[build] Building CXX object CMakeFiles/svision3.dir/src/svision3/layout.cpp.o\n"
        "[build] Building CXX object CMakeFiles/svision3.dir/src/svision3/dock_area.cpp.o\n"
        "[build] Linking CXX static library libsvision3.a\n"
        "[build] Building CXX executable demo_dock\n"
        "[build] Build finished successfully.\n")
        .highlight_current_line();
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
        "#include \"svision3/dock_area.hpp\"\n"
        "#include \"svision3/application.hpp\"\n"
        "\n"
        "int main(int argc, char *argv[]) {\n"
        "    auto app = svision3::Application{};\n"
        "\n"
        "    auto *win = app.create_window(\"My App\", {1200, 800});\n"
        "\n"
        "    auto dock = std::make_unique<svision3::DockArea>();\n"
        "    dock->set_center<svision3::TextEdit>();\n"
        "    dock->add_dock<svision3::ListView>(svision3::DockPosition::Left, \"Files\");\n"
        "    dock->add_dock<svision3::TextEdit>(svision3::DockPosition::Bottom, \"Console\");\n"
        "\n"
        "    win->set_root(std::move(dock));\n"
        "    return app.run();\n"
        "}\n")
        .highlight_current_line();
}

int main(int argc, char *argv[]) {
    std::string screenshot_path;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.starts_with("--screenshot=")) {
            screenshot_path = arg.substr(13);
        }
    }

    auto app = Application{};
    app.set_force_csd(true);
    if (!app.use_xdg_icons()) {
        auto loader = std::make_unique<XdgImageLoader>();
        loader->set_theme_path("./themes/Faenza");
        app.set_icon_provider(std::move(loader));
    }

    auto style = Theme::detect_system_style();
    Theme::set_current(ThemeFactory::create(style));

    auto win = app.create_window("Dock Demo", {1200, 800});

    // Center: TabWidget with one initial editor tab
    auto center = ui::tab_widget().tabs_closable(true).add_tab("Welcome", make_center_editor());
    auto center_ref = center.ref();
    center->on_tab_close = [center_ref](int index, std::string const &) {
        auto center_ptr = center_ref.lock();
        if (center_ptr && center_ptr->get_tab_count() > 1) {
            center_ptr->remove_tab(index);
        }
    };

    auto dock =
        ui::dock_area()
            .center(std::move(center))
            .dock_size(DockPosition::East, 200.0f)
            .dock_size(DockPosition::West, 200.0f)
            .dock_size(DockPosition::South, 200.0f)
            .add_dock(
                DockPosition::East, "Files",
                ui::file_browser()
                    .margins({0, 0, 0, 0})
                    .browser_mode(true)
                    .on_file_activated([center_ref](std::string const &path) {
                        auto center_ptr = center_ref.lock();
                        if (!center_ptr) {
                            return;
                        }
                        auto name = std::filesystem::path(path).filename().string();
                        for (auto i = 0; i < static_cast<int>(center_ptr->get_tab_count()); ++i) {
                            if (center_ptr->get_tab_title(i) == name) {
                                center_ptr->set_current(i);
                                return;
                            }
                        }
                        auto editor = ui::text_edit().highlight_current_line();
                        std::ifstream f(path);
                        if (f) {
                            editor->set_text(std::string(std::istreambuf_iterator<char>(f), {}));
                        }
                        auto canonical = std::filesystem::weakly_canonical(path).string();
                        auto tab_index = static_cast<int>(center_ptr->get_tab_count());
                        center_ptr->add_tab(name, std::move(editor.w));
                        center_ptr->set_tab_tooltip(tab_index, canonical);
                        center_ptr->set_current(tab_index);
                    }))
            .add_dock(DockPosition::East, "Outline", make_outline())
            .add_dock(DockPosition::West, "Inspector", make_inspector())
            .add_dock(DockPosition::South, "Console", make_console())
            .add_dock(DockPosition::South, "Problems", make_problems());

    auto dock_ref = dock.ref();
    auto vertical = std::make_shared<bool>(true);
    auto toggle_vertical = [dock_ref, vertical]() {
        auto dock_ptr = dock_ref.lock();
        if (!dock_ptr) {
            return;
        }
        *vertical = !*vertical;
        auto left = dock_ptr->dock_tab_widget(DockPosition::East).lock();
        if (left) {
            left->set_orientation(*vertical ? TabOrientation::West : TabOrientation::North);
        }
        auto right = dock_ptr->dock_tab_widget(DockPosition::West).lock();
        if (right) {
            right->set_orientation(*vertical ? TabOrientation::East : TabOrientation::North);
        }
    };

    auto make_theme = [](ThemeStyle style, ColorScheme scheme) {
        return [style, scheme] { Theme::set_current(ThemeFactory::create(style, scheme)); };
    };

    auto cmd_toggle_docks = Command::create("Toggle Docks", [dock_ref]() {
        auto dock_ptr = dock_ref.lock();
        if (!dock_ptr) {
            return;
        }
        std::array positions = {DockPosition::East, DockPosition::West, DockPosition::South};
        bool any_open = false;
        for (auto pos : positions) {
            auto tab = dock_ptr->dock_tab_widget(pos).lock();
            if (tab && !tab->is_collapsed()) {
                any_open = true;
                break;
            }
        }
        for (auto pos : positions) {
            auto tab = dock_ptr->dock_tab_widget(pos).lock();
            if (tab) {
                tab->set_collapsed(any_open);
            }
        }
    });
    cmd_toggle_docks->set_shortcut("ctrl+e");
    win->add_command(cmd_toggle_docks);

    // Weak: this Command is stored on the window, so a shared capture is a cycle.
    auto cmd_inspector = Command::create("Toggle Inspector", [weak_win = std::weak_ptr(win)]() {
        Widget::debug_show_inspector = !Widget::debug_show_inspector;
        if (auto win = weak_win.lock()) {
            win->request_redraw("inspector toggle");
        }
    });
    cmd_inspector->set_shortcut("super+f12");
    win->add_command(cmd_inspector);

    center_ref.lock()->set_leading_widget(
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

    win->set_root(std::move(dock));
    // Weak: this callback lives on the window, so capturing the shared_ptr
    // would be a cycle that never frees.
    win->on_key = [weak_win = std::weak_ptr(win), dock_ref](KeyEvent const &e) {
        if (e.key == Key::Escape) {
            auto win = weak_win.lock();
            auto dock_ptr = dock_ref.lock();
            if (!win || !dock_ptr) {
                return false;
            }
            auto t = std::dynamic_pointer_cast<TabWidget>(dock_ptr->get_center().lock());
            if (t) {
                spdlog::info("Focusing main widget");
                win->set_focused_widget(t.get());
                t->get_current_widget()->set_focused(true);
                win->set_focused_widget(t->get_current_widget());
                return true;
            } else {
                spdlog::error("Focusing main widget FAILED");
            }
        }
        return false;
    };

    if (!screenshot_path.empty()) {
        win->relayout();
        auto ok = win->save_to_png(screenshot_path);
        spdlog::info("Screenshot saved to '{}': {}", screenshot_path, ok ? "success" : "failed");

        // Also capture a "maximized" screenshot at a larger size, to check that
        // docks keep their configured pixel size and the centre widget absorbs
        // the extra space instead of everything scaling proportionally.
        auto path = std::filesystem::path(screenshot_path);
        auto maximized_path =
            (path.parent_path() / (path.stem().string() + "_maximized" + path.extension().string()))
                .string();
        win->handle_resize({1800, 1100});
        auto ok2 = win->save_to_png(maximized_path);
        spdlog::info("Maximized screenshot saved to '{}': {}", maximized_path,
                     ok2 ? "success" : "failed");

        return (ok && ok2) ? 0 : 1;
    }

    win->show();
    return app.run();
}
