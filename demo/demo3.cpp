#include <chrono>
#include <fmt/format.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <regex>
#include <spdlog/spdlog.h>

#include "declarative.hpp"
#include "github_markdown_css.hpp"
// #include "nfd.h"
#include "toolkit/application.hpp"
#include "toolkit/directory_dialog.hpp"
#include "toolkit/file_dialog.hpp"
#include "toolkit/line_input.hpp"
#include "toolkit/message_box.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
#include "toolkit/xdg_icons.hpp"
#include "toolkit/xdg_image_loader.hpp"

auto LOREM_IPSUM = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
                   "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
auto LOREM_IPSUM_MD =
    "**Lorem** _ipsum_ dolor sit amet, [consectetur](https://example.com) adipiscing elit, "
    "sed do eiusmod `tempor incididunt ut labore` et dolore magna ***aliqua***.";

static constexpr auto PREVIEW_DEFAULT_HTML = R"(<!DOCTYPE html>
<html>
<head>
<style>
  body { font-family: sans-serif; margin: 16px; background: #f9f9f9; color: #222; }
  h1 { color: #336699; border-bottom: 2px solid #336699; padding-bottom: 4px; }
  h2 { color: #558833; }
  p  { line-height: 1.6; }
  a  { color: #0066cc; }
  ul { padding-left: 24px; }
  li { margin-bottom: 4px; }
  code { background: #eee; padding: 1px 4px; font-family: monospace; }
</style>
</head>
<body>
  <h1>svision3 HTML Viewer</h1>
  <p>Edit this HTML and switch to <strong>Markdown</strong> mode to try the Markdown renderer.</p>
  <ul>
    <li>Text decoration: <u>underline</u>, <s>strikethrough</s></li>
    <li><strong>Bold</strong> and <em>italic</em> text</li>
    <li><code>Monospace</code> code snippets</li>
    <li><a href="https://github.com/litehtml/litehtml">litehtml on GitHub</a></li>
  </ul>
</body>
</html>)";

static constexpr auto PREVIEW_DEFAULT_MD = R"(# Hello from svision3

This is a **live preview** tab. Edit the text on the left and the
rendered output updates automatically.

## Markdown features

- *Italic* and **bold** text
- ~~Strikethrough~~
- `inline code`
- [Links](https://github.com/litehtml/litehtml)

## Code block

```cpp
int main() {
    return 0;
}
```

> Blockquotes work too.
)";

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
    style_names.push_back("Other");
    auto window = app.create_window("Declarative Kitchen sink", {600, 400});
    auto platformText =
        fmt::format("Platform: {} | Painter: {}", app.platform_name(), window->painter_name());
    auto group = ui::radio_group().on_change([&app, window](int index) {
        current_scheme = (index == 0) ? toolkit::ColorScheme::Light : toolkit::ColorScheme::Dark;
        apply_theme(app, window);
    });
    auto rb_light = ui::radio_button("Light", group).selected(true);
    auto rb_dark = ui::radio_button("Dark", group);
    auto *rb_light_ptr = rb_light.get();
    auto *rb_dark_ptr = rb_dark.get();

    auto toolbar_theme_combo = ui::combobox(style_names).on_change([&app, window](int index) {
        if (index < toolkit::theme_style_count) {
            current_style = static_cast<toolkit::ThemeStyle>(index);
            apply_theme(app, window);
        }
    });
    auto toolbar_theme_toggle = ui::checkbox("Light Theme").on_toggle([&app, window](bool checked) {
        current_scheme = checked ? toolkit::ColorScheme::Light : toolkit::ColorScheme::Dark;
        apply_theme(app, window);
    });

    auto main_theme_combo = ui::combobox(style_names).on_change([&app, window](int index) {
        if (index < toolkit::theme_style_count) {
            current_style = static_cast<toolkit::ThemeStyle>(index);
            apply_theme(app, window);
        }
    });

    auto status_bar_elem = ui::status_bar();
    auto *status_bar_ptr = status_bar_elem.get();
    status_bar_ptr->add_section("status", "Ready");
    status_bar_ptr->add_section("progress", "Connecting...").appear(350);
    status_bar_ptr->add_section("work", "Processing").spinner(350);
    status_bar_ptr->add_section("pulse", "Scanning").pulse(350);

    auto *t_combo_ptr = toolbar_theme_combo.get();
    auto *t_toggle_ptr = toolbar_theme_toggle.get();
    auto *m_combo_ptr = main_theme_combo.get();

    // Initial state and observer
    auto sync_ui = [t_combo_ptr, t_toggle_ptr, m_combo_ptr, group, rb_light_ptr,
                    rb_dark_ptr](const toolkit::Theme &theme) {
        int selected = -1;
        for (int i = 0; i < toolkit::theme_style_count; i++) {
            if (theme.name == toolkit::Theme::style_name(static_cast<toolkit::ThemeStyle>(i))) {
                selected = i;
                break;
            }
        }
        if (selected == -1) {
            selected = toolkit::theme_style_count; // "Other"
        }

        t_combo_ptr->set_selected(selected);
        m_combo_ptr->set_selected(selected);

        bool light = theme.palette.window.luma() > 0.5f;
        t_toggle_ptr->set_checked(light);
        group.group->select(light ? rb_light_ptr : rb_dark_ptr);
    };

    sync_ui(toolkit::Theme::current());
    toolkit::Theme::add_theme_observer(sync_ui);

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
    auto use_native_cb = ui::checkbox("Use native dialogs").checked(true);
    auto use_native_cb_ptr = use_native_cb.get();

    auto table_model = std::make_shared<toolkit::StringTableModel>(
        std::vector<std::string>{"Song", "Album", "Year", "Duration"}, beatlesSongsLength);

    auto songs_adapter = std::make_shared<toolkit::StringListModel>(beatlesSongs);
    auto filter_adapter = std::make_shared<toolkit::FilterAdapter>(songs_adapter);
    auto email_stats_label = ui::label("Empty");
    auto filter_progress = ui::progress_bar();
    filter_adapter->set_simulated_delay_ms(10);
    filter_adapter->on_progress = [filter_progress_ptr = filter_progress.get(),
                                   &window](float progress) {
        if (progress < 0.0f) {
            filter_progress_ptr->set_visible(false);
        } else {
            filter_progress_ptr->set_visible(true);
            filter_progress_ptr->set_value(progress);
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
                          {.text = "vacation.jpg"}, {.text = "family.png"},
                          {.text = "cats 1.png"},   {.text = "cats 2.png"},
                          {.text = "cats 3.png"},   {.text = "cats 4.png"},
                          {.text = "cats 5.png"},   {.text = "cats 6.png"},
                          {.text = "cats 7.png"},   {.text = "cats 8.png"},
                          {.text = "cats 9.png"},   {.text = "cats 10.png"},
                          {.text = "cats 11.png"},  {.text = "cats 12.png"},
                          {.text = "cats 13.png"},  {.text = "cats 14.png"},
                          {.text = "cats 15.png"},  {.text = "cats 16.png"},
                          {.text = "cats 17.png"},  {.text = "cats 18.png"},
                          {.text = "cats 19.png"},
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

    auto grid_model =
        std::make_shared<toolkit::StandardIconModel>(std::vector<toolkit::StandardIconItem>{
            {.text = "Folder",
             .icon_name = XDG::IconMimeTypes::inodeDirectory,
             .icon_category = XDG::IconContexts::places},
            {.text = "Document",
             .icon_name = XDG::IconMimeTypes::textXGeneric,
             .icon_category = XDG::IconContexts::mimeTypes},
            {.text = "Image",
             .icon_name = XDG::IconMimeTypes::imageXGeneric,
             .icon_category = XDG::IconContexts::mimeTypes},
            {.text = "Audio",
             .icon_name = XDG::IconMimeTypes::audioXGeneric,
             .icon_category = XDG::IconContexts::mimeTypes},
            {.text = "Video",
             .icon_name = XDG::IconMimeTypes::videoXGeneric,
             .icon_category = XDG::IconContexts::mimeTypes},
            {.text = "Archive",
             .icon_name = XDG::IconMimeTypes::applicationXArchive,
             .icon_category = XDG::IconContexts::mimeTypes},
            {.text = "Executable",
             .icon_name = XDG::IconMimeTypes::applicationXExecutable,
             .icon_category = XDG::IconContexts::mimeTypes},
            {.text = "Computer",
             .icon_name = XDG::IconDevices::computer,
             .icon_category = XDG::IconContexts::devices},
            {.text = "Harddisk",
             .icon_name = XDG::IconDevices::driveHarddisk,
             .icon_category = XDG::IconContexts::devices},
            {.text = "Removable",
             .icon_name = XDG::IconDevices::driveRemovableMedia,
             .icon_category = XDG::IconContexts::devices},
            {.text = "Keyboard",
             .icon_name = XDG::IconDevices::inputKeyboard,
             .icon_category = XDG::IconContexts::devices},
            {.text = "Mouse",
             .icon_name = XDG::IconDevices::inputMouse,
             .icon_category = XDG::IconContexts::devices},
            {.text = "Game",
             .icon_name = XDG::IconDevices::inputGaming,
             .icon_category = XDG::IconContexts::devices},
            {.text = "Calculator",
             .icon_name = XDG::IconApplications::accessoriesCalculator,
             .icon_category = XDG::IconContexts::applications},
            {.text = "Editor",
             .icon_name = XDG::IconApplications::accessoriesTextEditor,
             .icon_category = XDG::IconContexts::applications},
            {.text = "Terminal",
             .icon_name = XDG::IconApplications::utilitiesTerminal,
             .icon_category = XDG::IconContexts::applications},
            {.text = "Browser",
             .icon_name = XDG::IconApplications::internetWebBrowser,
             .icon_category = XDG::IconContexts::applications},
            {.text = "System",
             .icon_name = XDG::IconApplications::preferencesSystem,
             .icon_category = XDG::IconContexts::categories},
            {.text = "Error",
             .icon_name = XDG::IconStatus::dialogError,
             .icon_category = XDG::IconContexts::status},
            {.text = "Warning",
             .icon_name = XDG::IconStatus::dialogWarning,
             .icon_category = XDG::IconContexts::status},
            {.text = "Info",
             .icon_name = XDG::IconStatus::dialogInformation,
             .icon_category = XDG::IconContexts::status},
            {.text = "Question",
             .icon_name = XDG::IconStatus::dialogQuestion,
             .icon_category = XDG::IconContexts::status},
        });

    auto repeat_label = ui::label("Count: 0");
    auto repeat_action = [l = repeat_label.get()]() {
        static int count = 0;
        count++;
        l->set_text(fmt::format("Count: {}", count));
    };

    auto open_icon = app.load_icon(XDG::IconActions::documentOpen, 16, XDG::IconContexts::actions);

    auto open_action = [editor = editor.get(), window]() {
        toolkit::FileDialog(window).title("Open File").open().then([editor](auto path) {
            if (path) {
                std::ifstream f(*path);
                if (f) {
                    std::string contents((std::istreambuf_iterator<char>(f)),
                                         std::istreambuf_iterator<char>());
                    editor->set_text(contents);
                }
            }
        });
    };

    auto new_cmd = ui::command("New", [] { spdlog::info("Menu: New"); }).shortcut("Std+N");
    auto open_cmd = ui::command("Open...", open_action).shortcut("F3");
    auto save_cmd = ui::command("Save", [] { spdlog::info("Menu: Save"); }).shortcut("Std+S");
    auto exit_cmd =
        ui::command("Exit", [&window] { window->close(); }).icon(XDG::IconActions::applicationExit);
    auto export_cmd = ui::command("Export JSON", [window, use_native_cb_ptr] {
        toolkit::FileDialog(window)
            .title("Export Window to JSON")
            .use_native(use_native_cb_ptr->checked())
            .save()
            .then([window](auto path) {
                if (path) {
                    std::ofstream f(*path);
                    if (f) {
                        f << window->to_json().dump(4);
                    }
                }
            });
    });
    auto undo_cmd = ui::command("Undo", [] { spdlog::info("Menu: Undo"); }).shortcut("Std+Z");
    auto redo_cmd = ui::command("Redo", [] { spdlog::info("Menu: Redo"); }).shortcut("Std+Y");

    window->add_command(new_cmd);
    window->add_command(open_cmd);
    window->add_command(save_cmd);
    window->add_command(export_cmd);
    window->add_command(exit_cmd);
    window->add_command(undo_cmd);
    window->add_command(redo_cmd);

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

    // ── Preview tab setup ─────────────────────────────────────────────
    static constexpr float PREVIEW_DELAY_SEC = 5.0f;

    auto preview_html_elem = ui::html_view();
    auto *preview_html_ptr = preview_html_elem.get();
    preview_html_ptr->set_css(GITHUB_MARKDOWN_CSS_LIGHT, GITHUB_MARKDOWN_CSS_DARK);
    preview_html_ptr->set_markdown(PREVIEW_DEFAULT_MD);
    preview_html_ptr->on_link_click = [](std::string const &url) {
        spdlog::info("Preview link: {}", url);
    };

    auto preview_editor_elem = ui::text_edit(PREVIEW_DEFAULT_MD);
    auto *preview_editor_ptr = preview_editor_elem.get();

    auto preview_progress_elem = ui::progress_bar();
    auto *preview_progress_ptr = preview_progress_elem.get();
    preview_progress_ptr->set_visible(false);

    auto preview_is_markdown = std::make_shared<bool>(true);
    auto preview_pending = std::make_shared<bool>(false);
    auto preview_last_edit =
        std::make_shared<std::chrono::steady_clock::time_point>(std::chrono::steady_clock::now());

    auto apply_preview = [preview_html_ptr, preview_editor_ptr, preview_is_markdown]() {
        if (*preview_is_markdown) {
            preview_html_ptr->set_markdown(preview_editor_ptr->text());
        } else {
            preview_html_ptr->set_html(preview_editor_ptr->text());
        }
    };

    preview_editor_ptr->on_change = [preview_pending, preview_last_edit, preview_progress_ptr,
                                     window]() {
        *preview_last_edit = std::chrono::steady_clock::now();
        if (!*preview_pending) {
            *preview_pending = true;
            preview_progress_ptr->set_visible(true);
            preview_progress_ptr->set_value(0.0f);
            window->request_redraw("preview pending");
        }
    };

    window->start_timer(
        0.25f, [preview_pending, preview_last_edit, preview_progress_ptr, apply_preview, window]() {
            if (!*preview_pending) {
                return;
            }
            auto elapsed =
                std::chrono::duration<float>(std::chrono::steady_clock::now() - *preview_last_edit)
                    .count();
            preview_progress_ptr->set_value(std::min(elapsed / PREVIEW_DELAY_SEC, 1.0f));
            if (elapsed >= PREVIEW_DELAY_SEC) {
                *preview_pending = false;
                preview_progress_ptr->set_visible(false);
                apply_preview();
                window->request_redraw("preview done");
            }
        });

    auto preview_mode_group =
        ui::radio_group().on_change([preview_is_markdown, preview_pending, preview_progress_ptr,
                                     preview_editor_ptr, apply_preview](int index) {
            *preview_is_markdown = (index == 0);
            *preview_pending = false;
            preview_progress_ptr->set_visible(false);
            preview_editor_ptr->set_text(*preview_is_markdown ? PREVIEW_DEFAULT_MD
                                                              : PREVIEW_DEFAULT_HTML);
            apply_preview();
        });

    auto preview_md_btn = ui::radio_button("Markdown", preview_mode_group);
    auto *preview_md_btn_ptr = preview_md_btn.get();
    auto preview_html_mode_btn = ui::radio_button("HTML", preview_mode_group);
    preview_mode_group.group->select(preview_md_btn_ptr);

    auto github_css_cb =
        ui::checkbox("Use GitHub CSS")
            .checked(true)
            .on_toggle([preview_html_ptr, apply_preview](bool checked) {
                if (checked) {
                    preview_html_ptr->set_css(GITHUB_MARKDOWN_CSS_LIGHT, GITHUB_MARKDOWN_CSS_DARK);
                } else {
                    preview_html_ptr->set_css("", "");
                }
                apply_preview();
            });

    auto preview_tab = ui::vbox()
                           .margins(ui::no_margins())
                           .spacing(ui::no_spacing)
                           .add(ui::hsplit(std::move(preview_editor_elem),
                                           ui::scroll_area(std::move(preview_html_elem))),
                                ui::expand)
                           .add(preview_progress_elem)
                           .add(ui::hbox()
                                    .margins({4, 8, 4, 8})
                                    .spacing(16)
                                    .add(preview_md_btn)
                                    .add(preview_html_mode_btn)
                                    .add(github_css_cb));

    auto debug_stats_widget = [&window]() {
        return ui::vbox()
            .add(ui::checkbox("Show debug frames").on_toggle([&window](auto checked) {
                toolkit::Widget::debug_show_frames = checked;
                window->request_redraw();
            }))
            .add(ui::checkbox("Show performance stats").on_toggle([&window](auto checked) {
                if (checked) {
                    window->reset_statistics();
                    spdlog::set_level(spdlog::level::trace);
                } else {
                    spdlog::set_level(spdlog::level::info);
                }
                window->set_statistics_logging_enabled(checked);
            }));
    };

    auto rootWidget =
        ui::tab_widget()
            .orientation(toolkit::TabOrientation::WestVertical)
            .trailing_widget(debug_stats_widget())
            .min_tab_width(100)
            .tabs_closable(false)
            .tabs_movable(false)
            .add_tab(
                "Main",
                ui::vbox()
                    .add(ui::label(LOREM_IPSUM).tooltip(LOREM_IPSUM).shrinkable(true))
                    .add(ui::rich_label_md(LOREM_IPSUM_MD).markdownToolTip(LOREM_IPSUM_MD))
                    .add(ui::checkbox("Enable notifications")
                             .tooltip("Toggle desktop notifications")
                             .checked(true))
                    .add(ui::checkbox("Tri-state option")
                             .tri_state(true)
                             .tooltip("This checkbox cycles trough 3 state")
                             .checked(true))
                    .add(main_theme_combo)
                    .add(rb_light)
                    .add(rb_dark)
                    .add(progressBar)
                    .add([&]() {
                        return ui::hbox()
                            .margins(ui::no_margins())
                            .add(
                                ui::button("Auto repeat").auto_repeat(true).on_click(repeat_action))
                            .add(ui::button("Open")
                                     .icon(open_icon)
                                     .on_click(open_action)
                                     .menu(ui::menu()
                                               .action("Open File", open_action)
                                               .action("Open Document",
                                                       [] { spdlog::info("Open Document"); })
                                               .action("Open Image",
                                                       [] { spdlog::info("Open Image"); })))
                            .add(
                                ui::button("Toggle me").checkable(true).on_toggle([](bool checked) {
                                    spdlog::info("Button toggled (declarative): {}", checked);
                                }))
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
                    .add([&]() {
                        auto val = ui::label("0");
                        return ui::hbox()
                            .add(ui::slider().on_change([l = val.get()](float v) {
                                l->set_text(fmt::format("{:.0f}", v));
                            }),
                                 ui::expand)
                            .add(val);
                    }()))
            .add_tab(
                "Songs",
                ui::vbox()
                    .margins(ui::default_margins_no_bottom())
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
            .add_tab("Table",
                     ui::vbox()
                         .margins(ui::default_margins_no_bottom())
                         .add(ui::table_view(table_model).alternate_row_colors(true), ui::expand))
            .add_tab("Image",
                     [&]() {
                         auto iw = ui::image_widget();
                         auto *iw_ptr = iw.get();
                         return ui::vbox()
                             .margins(ui::default_margins_no_bottom())
                             .add(ui::button("Open Image").on_click([window, iw_ptr]() {
                                 toolkit::FileDialog(window)
                                     .title("Open Image")
                                     .open()
                                     .then([iw_ptr, window](auto path) {
                                         if (path) {
                                             iw_ptr->load(*path);
                                             window->start_timer(
                                                 0.01f,
                                                 [iw_ptr, window] {
                                                     iw_ptr->fit_to_widget();
                                                     window->request_redraw("fit");
                                                 },
                                                 false);
                                         }
                                     });
                             }))
                             .add(std::move(iw), ui::expand);
                     }())
            .add_tab("Grid",
                     [&]() {
                         auto iconGrid = ui::icon_grid(grid_model);
                         auto sizeLabel = ui::label("48px");
                         return ui::vbox()
                             .margins(ui::default_margins_no_bottom())
                             .add(ui::hbox()
                                      .margins(ui::no_margins())
                                      .add(ui::label("Icon Size:"))
                                      .add(ui::slider(48, 16, 256)
                                               .on_change([g = iconGrid.get(),
                                                           l = sizeLabel.get()](auto v) {
                                                   int size = static_cast<int>(v);
                                                   g->set_icon_size(size);
                                                   l->set_text(fmt::format("{}px", size));
                                               }),
                                           ui::expand)
                                      .add(sizeLabel)
                                      .add(ui::checkbox("Scale Icons")
                                               .on_toggle([g = iconGrid.get()](auto checked) {
                                                   g->set_scale_icons(checked);
                                               })))
                             .add(iconGrid, ui::expand);
                     }())
            .add_tab(
                "Editor",
                ui::vbox()
                    .margins(ui::default_margins_no_bottom())
                    .add(ui::hbox()
                             .spacing(8)
                             .add(ui::button("Open...").on_click(open_action))
                             .add(ui::button("Save As...")
                                      .on_click([&window, editor_ptr = editor.get(),
                                                 use_native_cb_ptr]() {
                                          toolkit::FileDialog(window)
                                              .title("Save File")
                                              .use_native(use_native_cb_ptr->checked())
                                              .save()
                                              .then([editor_ptr](auto path) {
                                                  if (path) {
                                                      std::ofstream f(*path);
                                                      if (f) {
                                                          f << editor_ptr->text();
                                                      }
                                                  }
                                              });
                                      }))
                             .add(ui::button("Choose Directory...")
                                      .on_click([&window, use_native_cb_ptr]() {
                                          toolkit::DirectoryDialog(window)
                                              .title("Choose Directory")
                                              .use_native(use_native_cb_ptr->checked())
                                              .choose()
                                              .then([](auto path) {
                                                  if (path) {
                                                      spdlog::info("Directory chosen: {}", *path);
                                                  } else {
                                                      spdlog::info("Directory dialog cancelled");
                                                  }
                                              });
                                      }))
                             .add(use_native_cb)
                             .add(ui::spacer(), ui::expand))
                    .add(editor, ui::expand))
            .add_tab("Tree",
                     ui::vbox()
                         .margins(ui::default_margins_no_bottom())
                         .add(ui::tree_view(tree_model).alternate_row_colors(true), ui::expand))
            .add_tab("Preview", preview_tab)
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
                                                            .alignment(toolkit::Alignment::Center))
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
                                                            .alignment(toolkit::Alignment::Center))
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
                             .add_tab(
                                 "South 1",
                                 ui::label("South 1 Content").alignment(toolkit::Alignment::Center))
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
            .spacing(ui::no_spacing)
            .add(ui::menubar()
                     .add_menu(
                         ui::menu("&File")
                             .action(new_cmd)
                             .action(open_cmd)
                             .action(save_cmd)
                             .action(export_cmd)
                             .separator()
                             .submenu(
                                 "Recent &Files",
                                 ui::menu()
                                     .action("&File1.txt", [] { spdlog::info("Opening File1"); })
                                     .action("&File2.txt", [] { spdlog::info("Opening File2"); }))
                             .separator()
                             .action(exit_cmd))
                     .add_menu(ui::menu("&Edit")
                                   .action(undo_cmd)
                                   .action(redo_cmd)
                                   .separator()
                                   .action("Cut", [] { spdlog::info("Menu: Cut"); })
                                   .action("Copy", [] { spdlog::info("Menu: Copy"); })
                                   .action("Paste", [] { spdlog::info("Menu: Paste"); }))
                     .add_menu(ui::menu("&Help")
                                   .action("About",
                                           [&window] {
                                               toolkit::MessageBox(window)
                                                   .title("About SVision3")
                                                   .markdown()
                                                   .message("## SVision3\n\n"
                                                            "Declarative API demo application.\n\n"
                                                            "Demo of a C++ **cross-platform** UI "
                                                            "toolkit.")
                                                   .show();
                                           })))
            .add(ui::toolbar()
                     .command("OK", [] { spdlog::info("Toolbar: OK"); })
                     .command(exit_cmd)
                     .separator()
                     .command("Disabled", "This command is disabled")
                     .disable()
                     .command("Increase count", "Increase the counter", {}, repeat_action)
                     .add(ui::spacer(), ui::expand)
                     .add(toolbar_theme_toggle)
                     .add(toolbar_theme_combo))
            .add(rootWidget, ui::expand)
            .add(ui::hbox()
                     .add(ui::button("About").disable())
                     .add(ui::spacer(), ui::expand)
                     .add(ui::button("Toast").on_click([window] {
                         static const std::optional<toolkit::Color> colors[] = {
                             toolkit::Color::rgb(1.0f, 0.85f, 0.85f),
                             toolkit::Color::rgb(0.85f, 1.0f, 0.85f),
                             toolkit::Color::rgb(0.85f, 0.85f, 1.0f),
                             toolkit::Color::rgb(1.0f, 1.0f, 0.85f),
                             toolkit::Color::rgb(1.0f, 0.85f, 1.0f),
                             std::nullopt,
                         };
                         static int count = 0;
                         auto title = fmt::format("Toast #{}", count++);
                         auto builder = ui::toast().title(title).timeout(7);
                         auto bg = colors[count % 6];

                         if (count % 2 == 0) {
                             builder.text(LOREM_IPSUM);
                         } else {
                             builder.rich_text(LOREM_IPSUM_MD);
                         }
                         if (bg) {
                             builder.background(*bg);
                         }
                         window->show_toast(builder);
                     }))
                     .add(ui::button("Export to JSON").command(export_cmd))
                     .add(ui::button("&Quit").on_click([&window] { window->close(); })))
            .add(std::move(status_bar_elem)));

    window->resize_to_fit();
    window->show();
    return app.run();
}
