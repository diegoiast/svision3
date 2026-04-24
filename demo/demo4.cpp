// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "BeatesSongs.hpp"
#include "declarative.hpp"
#include "nfd.h"
#include "toolkit/application.hpp"
#include "toolkit/image_loader.hpp"
#include "toolkit/line_input.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
#include "toolkit/xdg_icons.hpp"
#include <fmt/format.h>
#include <fstream>
#include <regex>
#include <spdlog/spdlog.h>

static const char *LOREM_IPSUM =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor "
    "incididunt ut labore et dolore magna aliqua.";

static const char *EDITOR_DEFAULT_TEXT = R"(#include <stdio.h>

int main() {
    printf("Hello world\n");
    return 0;
}
)";

static toolkit::ThemeStyle current_style = toolkit::ThemeStyle::MacOS;
static toolkit::ColorScheme current_scheme = toolkit::ColorScheme::Light;

static void apply_theme(toolkit::Application &app, toolkit::Window *window) {
    toolkit::Theme::set_current(toolkit::ThemeFactory::create(current_style, current_scheme));
    app.notify_theme_changed();
    window->request_redraw();
}

int main(int argc, char *argv[]) {
    toolkit::Application app;
    app.set_icon_provider(std::make_unique<toolkit::XdgImageLoader>("Faenza"));
    current_style = toolkit::Theme::detect_system_style();

    auto style_names = std::vector<std::string>{};
    for (int i = 0; i < toolkit::theme_style_count; i++) {
        style_names.push_back(toolkit::Theme::style_name(static_cast<toolkit::ThemeStyle>(i)));
    }

    auto window = app.create_window("Declarative Kitchen sink v4", {600, 400});
    auto platformText =
        fmt::format("Platform: {} | Painter: {}", app.platform_name(), window->painter_name());

    // Widgets whose raw pointers are captured by lambdas or timers must be
    // declared before the UI tree so .get() is called before ownership moves.

    auto editor = ui::text_edit(EDITOR_DEFAULT_TEXT);
    auto open_action = [editor = editor.get()]() {
        NFD_Init();
        nfdu8char_t *out_path = nullptr;
        if (NFD_OpenDialogU8(&out_path, nullptr, 0, nullptr) == NFD_OKAY && out_path) {
            std::ifstream f(out_path);
            if (f) {
                std::string contents((std::istreambuf_iterator<char>(f)),
                                     std::istreambuf_iterator<char>());
                editor->set_text(contents);
            }
            NFD_FreePathU8(out_path);
        }
        NFD_Quit();
    };

    auto theme_group = ui::radio_group().on_change([&app, &window](int index) {
        current_scheme = (index == 0) ? toolkit::ColorScheme::Light : toolkit::ColorScheme::Dark;
        apply_theme(app, window);
    });

    auto progressBar = ui::progress_bar();
    auto auto_progress_timer = window->start_timer(0.25f, [progressBar = progressBar.get()] {
        auto v = progressBar->value() + 0.01f;
        if (v > 1.0f) {
            v = 0.0f;
        }
        progressBar->set_value(v);
        progressBar->set_tooltip(std::to_string(static_cast<int>(v * 100)) + "%");
    });

    auto repeat_label = ui::label("Count: 0");
    auto repeat_action = [l = repeat_label.get()]() {
        static int count = 0;
        count++;
        l->set_text(fmt::format("Count: {}", count));
    };

    auto open_icon = app.load_icon(XDG::IconActions::documentOpen, 16, XDG::IconContexts::actions);

    auto email_status = ui::label("Empty");

    auto songs_adapter = std::make_shared<toolkit::StringListAdapter>(beatlesSongs);
    auto filter_adapter = std::make_shared<toolkit::FilterAdapter>(songs_adapter);
    auto filter_progress = ui::progress_bar();
    filter_adapter->set_simulated_delay_ms(10);
    filter_adapter->on_progress = [fp = filter_progress.get(), &window](float progress) {
        fp->set_visible(progress >= 0.0f);
        if (progress >= 0.0f) {
            fp->set_value(progress);
        }
        window->request_redraw();
    };

    auto table_model = std::make_shared<toolkit::StringTableModel>(
        std::vector<std::string>{"Song", "Album", "Year", "Duration"}, beatlesSongsLength);

    using STM = toolkit::SimpleTreeModel;
    auto tree_model = std::make_shared<STM>(std::vector<toolkit::TreeNode>{
        {.text = "Documents",
         .children =
             {
                 {.text = "resume.pdf"},
                 {.text = "cover_letter.pdf"},
                 {.text = "Projects",
                  .children = {{.text = "svision"}, {.text = "toolkit"}, {.text = "web"}},
                  .expanded = true},
             },
         .expanded = true},
        {.text = "Music",
         .children =
             {
                 {.text = "Beatles",
                  .children =
                      {
                          {.text = "Abbey Road"},
                          {.text = "Revolver"},
                          {.text = "Sgt. Pepper's"},
                      }},
                 {.text = "Pink Floyd"},
                 {.text = "Queen"},
             }},
        {.text = "Pictures",
         .children =
             {
                 {.text = "2024", .children = {{.text = "vacation.jpg"}, {.text = "family.png"}}},
                 {.text = "2025"},
             }},
        {.text = "Downloads", .children = {{.text = "archive.tar.gz"}, {.text = "installer.dmg"}}},
    });

    auto make_plus = [] {
        return ui::button("+").flat(true).focusable(false).padding({2, 8, 2, 8});
    };
    auto make_close = [] {
        return ui::button("x")
            .flat(true)
            .focusable(false)
            .padding({2, 8, 2, 8})
            .background_color(toolkit::Color::rgb(1.0f, 0.8f, 0.8f));
    };

    // ── Commands ──────────────────────────────────────────────────────────────
    auto new_cmd = ui::command("New", [] { spdlog::info("Menu: New"); }).shortcut("Std+N");
    auto open_cmd = ui::command("Open...", open_action).shortcut("F3");
    auto save_cmd = ui::command("Save", [] { spdlog::info("Menu: Save"); }).shortcut("Std+S");
    auto exit_cmd =
        ui::command("Exit", [&window] { window->close(); }).icon(XDG::IconActions::applicationExit);
    auto undo_cmd = ui::command("Undo", [] { spdlog::info("Menu: Undo"); }).shortcut("Std+Z");
    auto redo_cmd = ui::command("Redo", [] { spdlog::info("Menu: Redo"); }).shortcut("Std+Y");

    window->add_command(new_cmd);
    window->add_command(open_cmd);
    window->add_command(save_cmd);
    window->add_command(exit_cmd);
    window->add_command(undo_cmd);
    window->add_command(redo_cmd);

    // ── UI tree ───────────────────────────────────────────────────────────────
    // clang-format off
    window->set_root(
        ui::vbox().margins(ui::no_margins()).spacing(ui::no_spacing)
            | ui::menubar()
                  .add_menu(ui::menu("&File")
                      .action(new_cmd)
                      .action(open_cmd)
                      .action(save_cmd)
                      .separator()
                      .submenu("Recent &Files", ui::menu()
                          .action("&File1.txt", [] { spdlog::info("Opening File1"); })
                          .action("&File2.txt", [] { spdlog::info("Opening File2"); }))
                      .separator()
                      .action(exit_cmd))
                  .add_menu(ui::menu("&Edit")
                      .action(undo_cmd)
                      .action(redo_cmd)
                      .separator()
                      .action("Cut",   [] { spdlog::info("Menu: Cut"); })
                      .action("Copy",  [] { spdlog::info("Menu: Copy"); })
                      .action("Paste", [] { spdlog::info("Menu: Paste"); }))
                  .add_menu(ui::menu("&Help")
                      .action("About", [] { spdlog::info("Menu: About"); }))
            | ui::toolbar()
                  .command("OK", [] { spdlog::info("Toolbar: OK"); })
                  .command(exit_cmd)
                  .separator()
                  .command("Disabled", "This command is disabled").disable()
                  .command("Increase count", "Increase the counter", {}, repeat_action)
            | ui::tab_widget()
                  .add_tab("Main", ui::vbox()
                      | ui::label(LOREM_IPSUM).tooltip(LOREM_IPSUM).shrinkable(true)
                      | ui::checkbox("Enable notifications")
                            .tooltip("Toggle desktop notifications").checked(true)
                      | ui::checkbox("Tri-state option")
                            .tri_state(true)
                            .tooltip("This checkbox cycles through 3 states")
                            .checked(true)
                      | ui::combobox(style_names)
                            .selected(static_cast<int>(current_style))
                            .on_change([&app, &window](int index) {
                                current_style = static_cast<toolkit::ThemeStyle>(index);
                                apply_theme(app, window);
                            })
                      | ui::radio_button("Light", theme_group).selected(true)
                      | ui::radio_button("Dark",  theme_group)
                      | progressBar
                      | (ui::hbox().margins(ui::no_margins())
                          | ui::button("Auto repeat").auto_repeat(true).on_click(repeat_action)
                          | ui::button("Open").icon(open_icon).on_click(open_action)
                          | repeat_label)
                      | ui::label(platformText))
                  .add_tab("Inputs", ui::vbox()
                      | ui::line_input("Regular input")
                      | ui::line_input("Password").password_mode(true)
                      | ui::line_input().text("This text cannot be edited").read_only(true)
                      | ui::line_input("Numbers only (blocking)")
                            .validation_mode(toolkit::LineInput::ValidationMode::Block)
                            .validator([](auto &text, auto &) {
                                return std::regex_match(text, std::regex("[0-9]*"));
                            })
                      | (ui::hbox().margins(ui::no_margins())
                          | ui::line_input("Email address (visual)")
                                .validation_mode(toolkit::LineInput::ValidationMode::Notify)
                                .validator([](auto &text, auto &) {
                                    static auto re = std::regex(
                                        R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
                                    return text.empty() || std::regex_match(text, re);
                                })
                                .on_change([l = email_status.get()](auto &s, auto &w) {
                                    auto const &p = toolkit::Theme::current().palette;
                                    if (s.empty()) {
                                        l->set_text("Empty");
                                        l->set_background_color(p.warning);
                                    } else if (w.is_valid()) {
                                        l->set_text("Valid");
                                        l->set_background_color(p.success);
                                    } else {
                                        l->set_text("Invalid");
                                        l->set_background_color(p.error);
                                    }
                                })
                                .expand()
                          | email_status)
                      | (ui::hbox()
                          | ui::slider().expand()
                          | ui::label("0")))
                  .add_tab("Songs", ui::vbox()
                      | (ui::hbox().margins(ui::no_margins())
                          | ui::line_input("Filter songs...")
                                .on_change([filter_adapter](auto &text, auto &) {
                                    filter_adapter->set_filter(text);
                                })
                                .expand()
                          | ui::label("Delay (ms)").tooltip("Simulated delay per item (ms)")
                          | ui::spin_box(10).on_change([filter_adapter](auto v, auto &) {
                                filter_adapter->set_simulated_delay_ms(v);
                            }))
                      | filter_progress
                      | ui::list_view(filter_adapter).alternate_row_colors(true).expand())
                  .add_tab("Table", ui::vbox()
                      | ui::table_view(table_model).alternate_row_colors(true).expand())
                  .add_tab("Editor", ui::vbox()
                      | (ui::hbox().margins(ui::no_margins())
                          | ui::button("Open...").on_click(open_action)
                          | ui::spacer().expand())
                      | editor.expand())
                  .add_tab("Tree", ui::vbox()
                      | ui::tree_view(tree_model).alternate_row_colors(true).expand())
                  .add_tab("Tabs", ui::vbox().margins(ui::no_margins()).spacing(0)
                      | (ui::hbox().margins(ui::no_margins()).spacing(0)
                          | ui::tab_widget()
                                .orientation(toolkit::TabOrientation::West)
                                .leading_widget(make_plus())
                                .trailing_widget(make_close())
                                .add_tab("West 1", ui::label("West 1 Content").alignment(toolkit::Alignment::Center).background_color(toolkit::Color::rgb(0.9, 1.0, 0.9)))
                                .add_tab("West 2", ui::label("West 2 Content").alignment(toolkit::Alignment::Center).background_color(toolkit::Color::rgb(0.8, 1.0, 0.8)))
                                .add_tab("West 3", ui::label("West 3 Content").alignment(toolkit::Alignment::Center).background_color(toolkit::Color::rgb(0.7, 1.0, 0.7)))
                                .add_tab("West 4", ui::label("West 4 Content").alignment(toolkit::Alignment::Center).background_color(toolkit::Color::rgb(0.6, 1.0, 0.6)))
                                .add_tab("West 5", ui::label("West 5 Content").alignment(toolkit::Alignment::Center).background_color(toolkit::Color::rgb(0.5, 1.0, 0.5)))
                                .expand()
                          | ui::tab_widget()
                                .orientation(toolkit::TabOrientation::East)
                                .leading_widget(make_plus())
                                .trailing_widget(make_close())
                                .add_tab("East 1", ui::label("East 1 Content").alignment(toolkit::Alignment::Center).background_color(toolkit::Color::rgb(0.9, 0.9, 1.0)))
                                .add_tab("East 2", ui::label("East 2 Content").alignment(toolkit::Alignment::Center).background_color(toolkit::Color::rgb(0.8, 0.8, 1.0)))
                                .add_tab("East 3", ui::label("East 3 Content").alignment(toolkit::Alignment::Center).background_color(toolkit::Color::rgb(0.7, 0.7, 1.0)))
                                .add_tab("East 4", ui::label("East 4 Content").alignment(toolkit::Alignment::Center).background_color(toolkit::Color::rgb(0.6, 0.6, 1.0)))
                                .add_tab("East 5", ui::label("East 5 Content").alignment(toolkit::Alignment::Center).background_color(toolkit::Color::rgb(0.5, 0.5, 1.0)))
                                .expand())
                          .expand()
                      | ui::tab_widget()
                            .orientation(toolkit::TabOrientation::South)
                            .leading_widget(make_plus())
                            .trailing_widget(make_close())
                            .add_tab("South 1", ui::label("South 1 Content").alignment(toolkit::Alignment::Center).background_color(toolkit::Color::rgb(1.0, 0.9, 0.9)))
                            .add_tab("South 2", ui::label("South 2 Content").alignment(toolkit::Alignment::Center).background_color(toolkit::Color::rgb(1.0, 0.8, 0.8)))
                            .add_tab("South 3", ui::label("South 3 Content").alignment(toolkit::Alignment::Center).background_color(toolkit::Color::rgb(1.0, 0.7, 0.7)))
                            .add_tab("South 4", ui::label("South 4 Content").alignment(toolkit::Alignment::Center).background_color(toolkit::Color::rgb(1.0, 0.6, 0.6)))
                            .add_tab("South 5", ui::label("South 5 Content").alignment(toolkit::Alignment::Center).background_color(toolkit::Color::rgb(1.0, 0.5, 0.5))))
                  .expand()
            | ui::spacer()
            | (ui::vbox()
                | ui::checkbox("Show debug frames").on_toggle([&window](auto checked) {
                      toolkit::Widget::debug_show_frames = checked;
                      window->request_redraw();
                  })
                | ui::checkbox("Show performance stats").on_toggle([&window](auto checked) {
                      if (checked) {
                          window->reset_statistics();
                          spdlog::set_level(spdlog::level::trace);
                      } else {
                          spdlog::set_level(spdlog::level::info);
                      }
                      window->set_statistics_logging_enabled(checked);
                  }))
            | (ui::hbox()
                | ui::button("About").disable()
                | ui::spacer().expand()
                | ui::button("&Quit").on_click([&window] { window->close(); }))
    );
    // clang-format on

    window->resize_to_fit();
    window->show();
    return app.run();
}
