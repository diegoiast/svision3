#include "declarative.hpp"
#include "nfd.h"
#include "toolkit/application.hpp"
#include "toolkit/line_input.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
#include <fmt/format.h>
#include <fstream>
#include <regex>
#include <spdlog/spdlog.h>

auto LOREM_IPSUM = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor "
                   "incididunt ut labore et dolore magna aliqua.";

auto EDITOR_DEFAULT_TEXT = R"(#include <stdio.h>

int main() {
    printf("Hello world\n");
    return 0;
}
)";
#include "BeatesSongs.hpp"

static toolkit::ThemeStyle current_style = toolkit::ThemeStyle::MacOS;
static toolkit::ColorScheme current_scheme = toolkit::ColorScheme::Light;
static void apply_theme(toolkit::Application &app, toolkit::Window *window) {
    toolkit::Theme::set_current(toolkit::ThemeFactory::create(current_style, current_scheme));
    app.notify_theme_changed();
    window->request_redraw();
}

int main(int argc, char *argv[]) {
    toolkit::Application app;
    current_style = toolkit::Theme::detect_system_style();

    auto style_names = std::vector<std::string>{};
    for (int i = 0; i < toolkit::theme_style_count; i++) {
        style_names.push_back(toolkit::Theme::style_name(static_cast<toolkit::ThemeStyle>(i)));
    }
    auto window = app.create_window("Declarative Kitchen sink", {800, 600});
    auto open_icon_btn = std::make_unique<toolkit::Button>("&Open");
    auto platformText =
        fmt::format("Platform: {} | Painter: {}", app.platform_name(), window->painter_name());
    auto group = ui::radio_group().on_change([&app, &window](int index) {
        current_scheme = (index == 0) ? toolkit::ColorScheme::Light : toolkit::ColorScheme::Dark;
        apply_theme(app, window);
    });

    // Progress bar needs to be accesed from the lambda, and inserted into the root widget
    // If you move this to be bellow the root widget, code will crash
    auto progressBar = ui::progress_bar();
    auto auto_progress_timer = window->start_timer(0.25f, [progressBar = progressBar.get()] {
        auto v = progressBar->value() + 0.01f;
        if (v > 1.0f) {
            v = 0.0f;
        }
        progressBar->set_value(v);
        progressBar->set_tooltip(std::to_string(static_cast<int>(v * 100)) + "%");
    });

    auto editor = ui::text_edit(EDITOR_DEFAULT_TEXT);
    auto table_model = std::make_shared<toolkit::StringTableModel>(
        std::vector<std::string>{"Song", "Album", "Year", "Duration"}, beatlesSongsLength);

    auto songs_adapter = std::make_shared<toolkit::StringListAdapter>(beatlesSongs);
    auto filter_adapter = std::make_shared<toolkit::FilterAdapter>(songs_adapter);
    auto email_stats_label = ui::label("Empty");
    auto filter_progress = ui::progress_bar();
    filter_adapter->set_simulated_delay_ms(10);
    filter_adapter->on_progress = [progress_view = filter_progress.get(), &window](float progress) {
        if (progress < 0.0f) {
            progress_view->set_visible(false);
        } else {
            progress_view->set_visible(true);
            progress_view->set_value(progress);
        }
        window->request_redraw();
    };

    // Just to make the code prettier after clang-format :)
    using STM = toolkit::SimpleTreeModel;
    auto tree_model = std::make_shared<STM>(std::vector<toolkit::TreeNode>{
        {.text = "Documents",
         .children =
             {
                 {.text = "resume.pdf"},
                 {.text = "cover_letter.pdf"},
                 {.text = "Projects",
                  .children =
                      {
                          {.text = "svision"},
                          {.text = "toolkit"},
                          {.text = "web"},
                      },
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
                 {.text = "2024",
                  .children =
                      {
                          {.text = "vacation.jpg"},
                          {.text = "family.png"},
                      }},
                 {.text = "2025"},
             }},
        {.text = "Downloads",
         .children =
             {
                 {.text = "archive.tar.gz"},
                 {.text = "installer.dmg"},
             }},
    });

    auto make_plus = []() {
        return ui::button("+").flat(true).focusable(false).padding({2, 8, 2, 8});
    };
    auto make_close = []() {
        return ui::button("x")
            .flat(true)
            .focusable(false)
            .padding({2, 8, 2, 8})
            .background_color(toolkit::Color::rgb(1.0f, 0.8f, 0.8f));
    };

    auto rootWidget =
        ui::tab_widget()
            .add_tab(
                "Main",
                ui::vbox()
                    .add(ui::label(LOREM_IPSUM).tooltip(LOREM_IPSUM).shrinkable(true))
                    .add(ui::checkbox("Enable notifications")
                             .tooltip("Toggle desktop notifications")
                             .checked(true))
                    .add(ui::checkbox("Tri-state option")
                             .tri_state(true)
                             .tooltip("This checkbox cycles trough 3 state")
                             .checked(true))
                    .add(ui::combobox(style_names)
                             .selected(static_cast<int>(current_style))
                             .on_change([&app, &window](int index) {
                                 current_style = static_cast<toolkit::ThemeStyle>(index);
                                 apply_theme(app, window);
                             }))
                    .add(ui::radio_button("Light", group).selected(true))
                    .add(ui::radio_button("Dark", group))
                    .add(progressBar)
                    .add([&]() {
                        auto repeat_label = ui::label("Count: 0");
                        auto repeat_action = [l = repeat_label.get()]() {
                            static int count = 0;
                            count++;
                            l->set_text(fmt::format("Count: {}", count));
                        };
                        return ui::hbox()
                            .margins(ui::no_margins())
                            .add(
                                ui::button("Auto repeat").auto_repeat(true).on_click(repeat_action))
                            // WIP - icon is not supported yet.
                            .add(ui::button("Open") /*.icon(open_icon_btn) */)
                            .add(repeat_label);
                    }())
                    .add(ui::label(platformText)))
            .add_tab(
                "Inputs",
                ui::vbox()
                    .add(ui::line_input("Regular input"))
                    .add(ui::line_input("Password").password_mode(true))
                    .add(ui::line_input().text("This text cannot be edited").read_only(true))
                    .add(ui::line_input("Numbers only (blocking)")
                             .validation_mode(toolkit::LineInput::ValidationMode::Block)
                             .validator([](auto &text, auto &) {
                                 return std::regex_match(text, std::regex("[0-9]*"));
                             }))
                    .add(ui::hbox()
                             .margins(ui::no_margins())
                             .add(ui::line_input("Email address (visual)")
                                      .validation_mode(toolkit::LineInput::ValidationMode::Notify)
                                      .validator([](auto &text, auto &) {
                                          auto static email_regex = std::regex(
                                              R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
                                          if (text.empty()) {
                                              return true;
                                          }
                                          return std::regex_match(text, email_regex);
                                      })
                                      .on_change([l = email_stats_label.get()](auto &s, auto &w) {
                                          auto const &palette = toolkit::Theme::current().palette;
                                          if (s.empty()) {
                                              l->set_text("Empty");
                                              l->set_background_color(palette.warning);
                                          } else if (w.is_valid()) {
                                              l->set_text("Valid");
                                              l->set_background_color(palette.success);
                                          } else {
                                              l->set_text("Invalid");
                                              l->set_background_color(palette.error);
                                          }
                                      }),
                                  ui::expand)
                             .add(email_stats_label))
                    .add(ui::hbox().add(ui::slider(), ui::expand).add(ui::label("0"))))
            .add_tab(
                "Songs",
                ui::vbox()
                    .add(ui::hbox()
                             .margins(ui::no_margins())
                             .add(ui::line_input("Filter songs...")
                                      .on_change([filter_adapter](auto &text, auto &) {
                                          filter_adapter->set_filter(text);
                                      }),
                                  ui::expand)
                             .add(ui::label("Delay (ms)").tooltip("Simulated delay per item (ms)"))
                             .add(ui::spin_box(10).on_change([filter_adapter](auto v, auto &w) {
                                 filter_adapter->set_simulated_delay_ms(v);
                             })))
                    .add(filter_progress)
                    .add(ui::list_view(filter_adapter).alternate_row_colors(true), ui::expand))
            .add_tab("Table", ui::vbox().add(ui::table_view(table_model).alternate_row_colors(true),
                                             ui::expand))
            .add_tab("Editor",
                     ui::vbox()
                         .add(ui::hbox()
                                  .margins(ui::no_margins())
                                  .add(ui::button("Open...").on_click([editor = editor.get()]() {
                                      NFD_Init();
                                      nfdu8char_t *out_path = nullptr;
                                      nfdresult_t result =
                                          NFD_OpenDialogU8(&out_path, nullptr, 0, nullptr);
                                      if (result == NFD_OKAY && out_path) {
                                          std::ifstream f(out_path);
                                          if (f) {
                                              std::string contents(
                                                  (std::istreambuf_iterator<char>(f)),
                                                  std::istreambuf_iterator<char>());
                                              editor->set_text(contents);
                                          }
                                          NFD_FreePathU8(out_path);
                                      }
                                      NFD_Quit();
                                  }))
                                  .add(ui::spacer(), ui::expand))
                         .add(editor, ui::expand))
            .add_tab("Tree", ui::vbox().add(ui::tree_view(tree_model).alternate_row_colors(true),
                                            ui::expand))
            .add_tab(
                "Tabs",
                ui::vbox()
                    .margins(ui::no_margins())
                    .spacing(0)
                    .add(
                        ui::hbox()
                            .margins(ui::no_margins())
                            .spacing(0)
                            .add(ui::tab_widget()
                                     .orientation(toolkit::TabOrientation::West)
                                     .leading_widget(make_plus())
                                     .trailing_widget(make_close())
                                     .add_tab("West 1", ui::label("West 1 Content")
                                                            .alignment(toolkit::Alignment::Center)
                                                            .background_color(
                                                                toolkit::Color::rgb(0.9, 1.0, 0.9)))
                                     .add_tab("West 2", ui::label("West 2 Content")
                                                            .alignment(toolkit::Alignment::Center)
                                                            .background_color(
                                                                toolkit::Color::rgb(0.8, 1.0, 0.8)))
                                     .add_tab("West 3", ui::label("West 3 Content")
                                                            .alignment(toolkit::Alignment::Center)
                                                            .background_color(
                                                                toolkit::Color::rgb(0.7, 1.0, 0.7)))
                                     .add_tab("West 4", ui::label("West 4 Content")
                                                            .alignment(toolkit::Alignment::Center)
                                                            .background_color(
                                                                toolkit::Color::rgb(0.6, 1.0, 0.6)))
                                     .add_tab("West 5", ui::label("West 5 Content")
                                                            .alignment(toolkit::Alignment::Center)
                                                            .background_color(toolkit::Color::rgb(
                                                                0.5, 1.0, 0.5))),
                                 ui::expand)
                            .add(ui::tab_widget()
                                     .orientation(toolkit::TabOrientation::East)
                                     .leading_widget(make_plus())
                                     .trailing_widget(make_close())
                                     .add_tab("East 1", ui::label("East 1 Content")
                                                            .alignment(toolkit::Alignment::Center)
                                                            .background_color(
                                                                toolkit::Color::rgb(0.9, 0.9, 1.0)))
                                     .add_tab("East 2", ui::label("East 2 Content")
                                                            .alignment(toolkit::Alignment::Center)
                                                            .background_color(
                                                                toolkit::Color::rgb(0.8, 0.8, 1.0)))
                                     .add_tab("East 3", ui::label("East 3 Content")
                                                            .alignment(toolkit::Alignment::Center)
                                                            .background_color(
                                                                toolkit::Color::rgb(0.7, 0.7, 1.0)))
                                     .add_tab("East 4", ui::label("East 4 Content")
                                                            .alignment(toolkit::Alignment::Center)
                                                            .background_color(
                                                                toolkit::Color::rgb(0.6, 0.6, 1.0)))
                                     .add_tab("East 5", ui::label("East 5 Content")
                                                            .alignment(toolkit::Alignment::Center)
                                                            .background_color(toolkit::Color::rgb(
                                                                0.5, 0.5, 1.0))),
                                 ui::expand),
                        ui::expand)
                    .add(ui::tab_widget()
                             .orientation(toolkit::TabOrientation::South)
                             .leading_widget(make_plus())
                             .trailing_widget(make_close())
                             .add_tab("South 1",
                                      ui::label("South 1 Content")
                                          .alignment(toolkit::Alignment::Center)
                                          .background_color(toolkit::Color::rgb(1.0, 0.9, 0.9)))
                             .add_tab("South 2",
                                      ui::label("South 2 Content")
                                          .alignment(toolkit::Alignment::Center)
                                          .background_color(toolkit::Color::rgb(1.0, 0.8, 0.8)))
                             .add_tab("South 3",
                                      ui::label("South 3 Content")
                                          .alignment(toolkit::Alignment::Center)
                                          .background_color(toolkit::Color::rgb(1.0, 0.7, 0.7)))
                             .add_tab("South 4",
                                      ui::label("South 4 Content")
                                          .alignment(toolkit::Alignment::Center)
                                          .background_color(toolkit::Color::rgb(1.0, 0.6, 0.6)))
                             .add_tab("South 5",
                                      ui::label("South 5 Content")
                                          .alignment(toolkit::Alignment::Center)
                                          .background_color(toolkit::Color::rgb(1.0, 0.5, 0.5)))));

    window->set_root(
        ui::vbox()
            .margins(ui::no_margins())
            .spacing(0)
            .add(rootWidget, ui::expand)
            .add(ui::spacer())
            .add(ui::vbox()
                     .add(ui::checkbox("Show debug frames").on_toggle([&window](auto checked) {
                         toolkit::Widget::debug_show_frames = checked;
                         window->request_redraw();
                     }))
                     .add(ui::checkbox("Show performace stats").on_toggle([&window](auto checked) {
                         if (checked) {
                             window->reset_statistics();
                             spdlog::set_level(spdlog::level::trace);
                         } else {
                             spdlog::set_level(spdlog::level::info);
                         }
                         window->set_statistics_logging_enabled(checked);
                     })))
            .add(ui::hbox()
                     .add(ui::button("About").enabled(false))
                     .add(ui::spacer(), ui::expand)
                     .add(ui::button("&Quit").on_click([&window] { window->close(); }))));

    window->show();
    return app.run();
}
