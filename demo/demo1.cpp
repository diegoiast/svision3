// SPDX-License-Identifier: MIT
// SPDX-&FileCopyrightText: l2026 Diego Iastrubni <diegoiast@gmail.com>

#include "declarative.hpp"
#include "github_markdown_css.hpp"
#include "toolkit/application.hpp"
#include "toolkit/button.hpp"
#include "toolkit/checkbox.hpp"
#include "toolkit/combobox.hpp"
#include "toolkit/directory_dialog.hpp"
#include "toolkit/file_dialog.hpp"
#include "toolkit/html_view.hpp"
#include "toolkit/icon_grid.hpp"
#include "toolkit/image_widget.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/line_input.hpp"
#include "toolkit/list_view.hpp"
#include "toolkit/menu.hpp"
#include "toolkit/menubar.hpp"
#include "toolkit/message_box.hpp"
#include "toolkit/progress_bar.hpp"
#include "toolkit/radio_button.hpp"
#include "toolkit/rich_label.hpp"
#include "toolkit/scroll_area.hpp"
#include "toolkit/scrollbar.hpp"
#include "toolkit/slider.hpp"
#include "toolkit/spin_box.hpp"
#include "toolkit/splitter.hpp"
#include "toolkit/status_bar.hpp"
#include "toolkit/tab_widget.hpp"
#include "toolkit/table_view.hpp"
#include "toolkit/text_edit.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
#include "toolkit/toolbar.hpp"
#include "toolkit/tree_view.hpp"
#include "toolkit/window.hpp"
#include "toolkit/xdg_image_loader.hpp"

#include <chrono>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <regex>
#include <set>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

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

static toolkit::RadioGroup scheme_group;
static toolkit::ThemeStyle current_style = toolkit::ThemeStyle::MacOS; // Placeholder, set in main
static toolkit::ColorScheme current_scheme = toolkit::ColorScheme::Light;

#include "BeatesSongs.hpp"

static void apply_theme(toolkit::Application &app, toolkit::Window *window) {
    toolkit::Theme::set_current(toolkit::ThemeFactory::create(current_style, current_scheme));
    window->request_redraw("theme apply");
}

static constexpr auto LOREM_IPSUM = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
                   "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::debug);

    toolkit::Application app;
    app.set_icon_provider(std::make_unique<toolkit::XdgImageLoader>("Faenza"));

    std::string screenshot_path;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.starts_with("--screenshot=")) {
            screenshot_path = arg.substr(13);
        }
    }
    current_style = toolkit::Theme::detect_system_style();
    toolkit::Theme::set_current(toolkit::ThemeFactory::create(current_style, current_scheme));

    auto *window = app.create_window("Demo", {600, 400});

    auto root = std::make_unique<toolkit::VBoxLayout>();
    root->set_margins({0, 0, 0, 0});
    root->set_spacing(0);

    // This needs to be acessible by the menu and the editor tab
    auto editor = new toolkit::TextEdit();
    editor->set_text("#include <iostream>\n"
                     "\n"
                     "int main() {\n"
                     "    std::cout << \"Hello, world!\" << std::endl;\n"
                     "    return 0;\n"
                     "}\n");

    auto *use_native_cb = new toolkit::Checkbox("Use native dialogs");
    use_native_cb->set_checked(true);

    auto open_action = [editor, use_native_cb, window] {
        toolkit::FileDialog(window)
            .title("Open File")
            .use_native(use_native_cb->checked())
            .open()
            .then([editor](auto path) {
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

    // ── MenuBar ─────────────────────────────────────────────────────────
    auto menubar = std::make_unique<toolkit::MenuBar>();
    auto *menubar_ptr = menubar.get();

    auto file_menu = menubar->add_menu("&File");
    auto new_cmd = toolkit::Command::create("New", [] { spdlog::info("Menu: New"); });
    new_cmd->set_shortcut("Std+N");
    file_menu->add_action(new_cmd);

    auto open_menu_cmd = toolkit::Command::create("Open...", open_action);
    open_menu_cmd->set_shortcut("F3");
    open_menu_cmd->set_icon(XDG::IconActions::documentOpen);
    file_menu->add_action(open_menu_cmd);

    auto save_action = [editor, use_native_cb, window] {
        toolkit::FileDialog(window)
            .title("Save File")
            .use_native(use_native_cb->checked())
            .save()
            .then([editor](auto path) {
                if (path) {
                    std::ofstream f(*path);
                    if (f) {
                        f << editor->text();
                    }
                }
            });
    };
    auto save_cmd = toolkit::Command::create("Save As...", save_action);
    save_cmd->set_shortcut("Std+S");
    save_cmd->set_icon(XDG::IconActions::documentSaveAs);
    file_menu->add_action(save_cmd);

    file_menu->add_separator();

    // Nested menu example
    auto recent_files_menu = std::make_shared<toolkit::Menu>("Recent &Files");
    recent_files_menu->add_action("&File1.txt", [] { spdlog::info("Opening &File1.txt"); });
    recent_files_menu->add_action("&File2.txt", [] { spdlog::info("Opening &File2.txt"); });
    file_menu->add_submenu("Recent &Files", recent_files_menu);

    file_menu->add_separator();
    auto exit_cmd = toolkit::Command::create("Exit", [window] { window->close(); });
    exit_cmd->set_icon(XDG::IconActions::applicationExit);
    file_menu->add_action(exit_cmd);

    auto edit_menu = menubar->add_menu("&Edit");
    auto undo_cmd = toolkit::Command::create("Undo", [] { spdlog::info("Menu: Undo"); });
    undo_cmd->set_shortcut("Std+Z");
    edit_menu->add_action(undo_cmd);

    auto redo_cmd = toolkit::Command::create("Redo", [] { spdlog::info("Menu: Redo"); });
    redo_cmd->set_shortcut("Std+Y");
    edit_menu->add_action(redo_cmd);

    edit_menu->add_separator();
    edit_menu->add_action("Cut", [] { spdlog::info("Menu: Cut"); });
    edit_menu->add_action("Copy", [] { spdlog::info("Menu: Copy"); });
    edit_menu->add_action("Paste", [] { spdlog::info("Menu: Paste"); });

    menubar->add_menu("&Help")->add_action("About", [window] {
        toolkit::MessageBox(window)
            .title("About Demo")
            .markdown()
            .message("## SVision3\n\n"
                     "Imperative API demo application.\n\n"
                     "Demo of a C++ **cross-platform** UI toolkit.")
            .icon(toolkit::MessageBoxIcon::Information)
            .show();
    });

    // Add shortcuts to the window globally so they are always active
    window->add_command(new_cmd);
    window->add_command(open_menu_cmd);
    window->add_command(save_cmd);
    window->add_command(undo_cmd);
    window->add_command(redo_cmd);

    root->add_widget(std::move(menubar));

    // ── Toolbar ──────────────────────────────────────────────────────────
    auto toolbar = std::make_unique<toolkit::Toolbar>();

    auto ok_action = [] { spdlog::info("Toolbar: OK triggered"); };
    auto ok_cmd = toolkit::Command::create("OK", ok_action);
    ok_cmd->set_tooltip("Trigger the OK action");
    ok_cmd->set_icon(XDG::IconActions::dialogApply);
    toolbar->add_command(ok_cmd);

    exit_cmd->set_tooltip("Close the application");
    toolbar->add_command(exit_cmd);

    toolbar->add_separator();

    auto disabled_cmd = toolkit::Command::create("Disabled", [] {});
    disabled_cmd->set_enabled(false);
    disabled_cmd->set_tooltip("This command is disabled");
    toolbar->add_command(disabled_cmd);

    auto autoclick_cmd = toolkit::Command::create("Increase count", [] {});
    autoclick_cmd->set_tooltip("Increase the counter");
    toolbar->add_command(autoclick_cmd);

    std::vector<std::string> style_names;
    for (int i = 0; i < toolkit::theme_style_count; i++) {
        style_names.push_back(toolkit::Theme::style_name(static_cast<toolkit::ThemeStyle>(i)));
    }
    style_names.push_back("Other");

    auto theme_combo = std::make_unique<toolkit::Combobox>(style_names);
    {
        int initial_selected = -1;
        auto const &theme = toolkit::Theme::current();
        for (int i = 0; i < toolkit::theme_style_count; i++) {
            if (theme.name == toolkit::Theme::style_name(static_cast<toolkit::ThemeStyle>(i))) {
                initial_selected = i;
                break;
            }
        }
        if (initial_selected == -1) {
            initial_selected = toolkit::theme_style_count;
        }
        theme_combo->set_selected(initial_selected);
    }

    auto theme_toggle = std::make_unique<toolkit::Checkbox>("Light Theme");
    theme_toggle->set_checked(toolkit::Theme::current().palette.window.luma() > 0.5f);

    // UI sync function
    auto sync_theme_ui = [theme_combo = theme_combo.get(),
                          theme_toggle = theme_toggle.get()](const toolkit::Theme &theme) {
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

        theme_combo->set_selected(selected);
        theme_toggle->set_checked(theme.palette.window.luma() > 0.5f);
    };
    toolkit::Theme::add_theme_observer(sync_theme_ui);

    theme_combo->on_change = [window](int index) {
        auto scheme = toolkit::Theme::current().palette.window.luma() < 0.5f
                          ? toolkit::ColorScheme::Dark
                          : toolkit::ColorScheme::Light;
        auto style = (index < toolkit::theme_style_count) ? static_cast<toolkit::ThemeStyle>(index)
                                                          : toolkit::ThemeStyle::Material;
        auto new_theme = toolkit::ThemeFactory::create(style, scheme);
        toolkit::Theme::set_current(std::move(new_theme));
        window->request_redraw("theme changed");
    };

    theme_toggle->on_toggle = [window, theme_combo = theme_combo.get()](bool checked) {
        auto scheme = checked ? toolkit::ColorScheme::Light : toolkit::ColorScheme::Dark;
        int index = theme_combo->selected();
        auto style =
            (index == 0) ? toolkit::ThemeStyle::System : static_cast<toolkit::ThemeStyle>(index);
        auto new_theme = toolkit::ThemeFactory::create(style, scheme);
        toolkit::Theme::set_current(std::move(new_theme));
        window->request_redraw("scheme changed");
    };

    auto spacer = std::make_unique<toolkit::Label>("");
    toolbar->add_widget(std::move(spacer), 1.0f);
    toolbar->add_widget(std::move(theme_toggle));
    toolbar->add_widget(std::move(theme_combo));

    root->add_widget(std::move(toolbar));

    auto debug_stats_widget = [&window]() {
        return ui::vbox()
            .add(ui::checkbox("Show Debug Frames").on_toggle([&window](bool checked) {
                toolkit::Widget::debug_show_frames = checked;
                window->request_redraw();
            }))
            .add(ui::checkbox("Show Performance Stats").on_toggle([&window](bool checked) {
                if (checked) {
                    window->reset_statistics();
                    spdlog::set_level(spdlog::level::trace);
                } else {
                    spdlog::set_level(spdlog::level::info);
                }
                window->set_statistics_logging_enabled(checked);
            }))
            .add(ui::checkbox("Show Widget Inspector").on_toggle([&window](bool checked) {
                toolkit::Widget::debug_show_inspector = checked;
                window->request_redraw();
            }));
    };

    auto tabs = std::make_unique<toolkit::TabWidget>();
    tabs->set_tabs_closable(false);
    tabs->set_tabs_movable(false);
    tabs->set_orientation(toolkit::TabOrientation::WestVertical);
    tabs->set_trailing_widget(debug_stats_widget());
    tabs->set_min_tab_width(200.0f);
    auto *tabs_ptr = tabs.get();

    // ── Tab: Main ──────────────────────────────────────────────────────
    auto tab_main = std::make_unique<toolkit::VBoxLayout>();
    tab_main->set_margins({20, 20, 20, 20});
    tab_main->set_spacing(12);

    auto plain_label = std::make_unique<toolkit::Label>(
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
        "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.");
    plain_label->set_elide(true);
    plain_label->set_shrinkable(true);
    tab_main->add_widget(std::move(plain_label));

    auto rich_label = std::make_unique<toolkit::RichLabel>(
        "**Lorem** _ipsum_ dolor sit amet, [consectetur](https://example.com) adipiscing elit, "
        "sed do eiusmodmarkdown `tempor incididunt ut labore` et dolore magna ***aliqua***.");
    rich_label->set_markdown_tooltip(rich_label->text());
    tab_main->add_widget(std::move(rich_label));

    auto cb1 = std::make_unique<toolkit::Checkbox>("Enable notifications");
    cb1->set_checked(true);
    cb1->set_tooltip("Toggle desktop notifications");
    tab_main->add_widget(std::move(cb1));

    auto cb2 = std::make_unique<toolkit::Checkbox>("Tri-state option");
    cb2->set_tri_state(true);
    cb2->set_check_state(toolkit::CheckState::Partial);
    cb2->set_tooltip("This checkbox cycles through three states");
    tab_main->add_widget(std::move(cb2));

    std::vector<std::string> main_page_style_names;
    for (int i = 0; i < toolkit::theme_style_count; i++) {
        main_page_style_names.push_back(
            toolkit::Theme::style_name(static_cast<toolkit::ThemeStyle>(i)));
    }

    auto combo = std::make_unique<toolkit::Combobox>(main_page_style_names);
    combo->set_selected(static_cast<int>(current_style));
    combo->set_tooltip("Select a theme style");
    combo->on_change = [&app, window](int index) {
        current_style = static_cast<toolkit::ThemeStyle>(index);
        apply_theme(app, window);
    };
    toolkit::Theme::add_theme_observer([combo = combo.get()](const toolkit::Theme &theme) {
        int selected = -1;
        for (int i = 0; i < toolkit::theme_style_count; i++) {
            if (theme.name == toolkit::Theme::style_name(static_cast<toolkit::ThemeStyle>(i))) {
                selected = i;
                break;
            }
        }
        if (selected != -1) {
            combo->set_selected(selected);
        } else {
            combo->set_selected(toolkit::theme_style_count); // "Other"
        }
    });
    tab_main->add_widget(std::move(combo));

    auto rb_light = std::make_unique<toolkit::RadioButton>("Light", scheme_group);
    rb_light->set_tooltip("Light color scheme");
    auto rb_dark = std::make_unique<toolkit::RadioButton>("Dark", scheme_group);
    rb_dark->set_tooltip("Dark color scheme");
    scheme_group.select(rb_light.get());
    scheme_group.on_change = [&app, window](int index) {
        constexpr toolkit::ColorScheme schemes[] = {toolkit::ColorScheme::Light,
                                                    toolkit::ColorScheme::Dark};
        current_scheme = schemes[index];
        apply_theme(app, window);
    };
    tab_main->add_widget(std::move(rb_light));
    tab_main->add_widget(std::move(rb_dark));

    auto progress = std::make_unique<toolkit::ProgressBar>();
    auto *progress_ptr = progress.get();
    progress->set_value(0.0f);
    progress->set_tooltip("0%");
    tab_main->add_widget(std::move(progress));

    auto repeat_row = std::make_unique<toolkit::HBoxLayout>();
    repeat_row->set_spacing(10);
    auto repeat_btn = std::make_unique<toolkit::Button>("Auto &repeat");
    repeat_btn->set_auto_repeat(true, 1.0f, 0.2f);
    repeat_btn->set_markdown_tooltip("Auto repeat button, every _second_");
    auto repeat_label = std::make_unique<toolkit::Label>("Count: 0");
    auto *repeat_label_ptr = repeat_label.get();
    static int repeat_count = 0;
    auto repeat_action = [repeat_label_ptr] {
        repeat_count++;
        repeat_label_ptr->set_text(fmt::format("Count: {}", repeat_count));
    };
    repeat_btn->set_command(autoclick_cmd);

    auto open_icon_btn = std::make_unique<toolkit::Button>("&Open");
    auto icon = app.load_icon(XDG::IconActions::documentOpen, 16);
    if (icon) {
        open_icon_btn->set_icon(icon);
    }
    open_icon_btn->set_tooltip("Open file");

    auto open_menu = std::make_shared<toolkit::Menu>();
    open_menu->add_action("Open File", open_action);
    open_menu->add_action("Open Document", [] { spdlog::info("Menu: Open Document"); });
    open_menu->add_action("Open Image", [] { spdlog::info("Menu: Open Image"); });
    open_icon_btn->set_menu(open_menu);

    auto toggle_btn = std::make_unique<toolkit::Button>("Toggle me");
    toggle_btn->set_checkable(true);
    toggle_btn->on_toggle = [](bool checked) { spdlog::info("Button toggled: {}", checked); };

    autoclick_cmd->set_execute_func(repeat_action);
    autoclick_cmd->set_shortcut("F4");
    autoclick_cmd->set_tooltip("Press F4");
    repeat_btn->add_command(autoclick_cmd);

    repeat_row->add_widget(std::move(repeat_btn));
    repeat_row->add_widget(std::move(open_icon_btn));
    repeat_row->add_widget(std::move(toggle_btn));
    repeat_row->add_widget(std::move(repeat_label), 1);
    tab_main->add_widget(std::move(repeat_row));

    auto scrollbar = std::make_unique<toolkit::Scrollbar>();
    scrollbar->set_range(0, 100);
    scrollbar->set_step_small(5);
    scrollbar->set_step_page(20);
    scrollbar->set_value(50);
    auto *scrollbar_ptr = scrollbar.get();
    auto scrollbar_label = std::make_unique<toolkit::Label>("Scroll: 50");
    auto *scrollbar_label_ptr = scrollbar_label.get();
    scrollbar->on_change = [scrollbar_label_ptr](float v) {
        scrollbar_label_ptr->set_text(fmt::format("Scroll: {:.0f}", v));
    };
    tab_main->add_widget(std::move(scrollbar));
    tab_main->add_widget(std::move(scrollbar_label));

    tab_main->add_widget(std::make_unique<toolkit::Label>(
        fmt::format("Platform: {} | Painter: {}", app.platform_name(), window->painter_name())));

    tabs->add_tab("Main", std::move(tab_main));

    // ── Tab: Inputs ────────────────────────────────────────────────────
    auto tab_inputs = std::make_unique<toolkit::VBoxLayout>();
    tab_inputs->set_margins({20, 20, 20, 20});
    tab_inputs->set_spacing(12);

    auto input1 = std::make_unique<toolkit::LineInput>("Regular input");
    tab_inputs->add_widget(std::move(input1));

    auto input2 = std::make_unique<toolkit::LineInput>("Password input");
    input2->set_password_mode(true);
    tab_inputs->add_widget(std::move(input2));

    auto input3 = std::make_unique<toolkit::LineInput>("Read-only input");
    input3->set_text("This text cannot be edited");
    input3->set_read_only(true);
    tab_inputs->add_widget(std::move(input3));

    auto input4 = std::make_unique<toolkit::LineInput>("Numbers only (Blocking)");
    input4->set_validator([](std::string const &text, auto &) {
        return std::regex_match(text, std::regex("[0-9]*"));
    });
    input4->set_validation_mode(toolkit::LineInput::ValidationMode::Block);
    tab_inputs->add_widget(std::move(input4));

    auto email_row = std::make_unique<toolkit::HBoxLayout>();
    email_row->set_spacing(10);

    auto input5 = std::make_unique<toolkit::LineInput>("Email address (Visual)");
    auto *input5_ptr = input5.get();
    input5->set_validator([](std::string const &text, auto &) {
        if (text.empty()) {
            return true;
        }
        static const std::regex email_regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
        return std::regex_match(text, email_regex);
    });
    input5->set_validation_mode(toolkit::LineInput::ValidationMode::Notify);

    auto status_label = std::make_unique<toolkit::Label>("Empty");
    auto *status_ptr = status_label.get();
    status_ptr->set_background_color(toolkit::Theme::current().palette.warning);

    input5->on_change = [input5_ptr, status_ptr](std::string const &s, auto &input) {
        auto const &palette = toolkit::Theme::current().palette;
        if (s.empty()) {
            status_ptr->set_text("Empty");
            status_ptr->set_background_color(palette.warning);
        } else if (input.is_valid()) {
            status_ptr->set_text("Valid");
            status_ptr->set_background_color(palette.success);
        } else {
            status_ptr->set_text("Invalid");
            status_ptr->set_background_color(palette.error);
        }
    };

    email_row->add_widget(std::move(input5), 1);
    email_row->add_widget(std::move(status_label));
    tab_inputs->add_widget(std::move(email_row));

    auto slider_row = std::make_unique<toolkit::HBoxLayout>();
    slider_row->set_spacing(10);

    static int auto_progress_timer = 0;

    auto &slider = slider_row->add<toolkit::Slider>(1).set_range(0, 100);
    auto &slider_val =
        slider_row->add<toolkit::Label>().set_text(fmt::format("{:.0f}%", slider.value()));

    slider.set_on_change([progress_ptr, &slider_val, window](toolkit::Slider &slider, float v) {
        if (auto_progress_timer != 0) {
            window->stop_timer(auto_progress_timer);
            auto_progress_timer = 0;
        }
        progress_ptr->set_value(v / 100.0f);
        progress_ptr->set_tooltip(fmt::format("{:.0f}%", v));
        slider_val.set_text(fmt::format("{:.0f}%", v));
    });

    // We need to capture the timer ID to stop it if the user interacts with the slider

    tab_inputs->add_widget(std::move(slider_row));

    auto inputs_spacer = std::make_unique<toolkit::Label>("");
    tab_inputs->add_widget(std::move(inputs_spacer), 1);

    tabs->add_tab("Inputs", std::move(tab_inputs));

    // ── Tab: Icon Grid ──────────────────────────────────────────────────
    auto tab_grid = std::make_unique<toolkit::VBoxLayout>();
    tab_grid->set_margins({20, 20, 20, 20});
    tab_grid->set_spacing(12);

    auto grid_model =
        std::make_shared<toolkit::StandardIconModel>(std::vector<toolkit::StandardIconItem>{
        {.text = "Folder", .icon_name = XDG::IconMimeTypes::inodeDirectory},
        {.text = "Document", .icon_name = XDG::IconMimeTypes::textXGeneric},
        {.text = "Image", .icon_name = XDG::IconMimeTypes::imageXGeneric},
        {.text = "Audio", .icon_name = XDG::IconMimeTypes::audioXGeneric},
        {.text = "Video", .icon_name = XDG::IconMimeTypes::videoXGeneric},
        {.text = "Archive", .icon_name = XDG::IconMimeTypes::applicationXArchive},
        {.text = "Executable", .icon_name = XDG::IconMimeTypes::applicationXExecutable},
        {.text = "Computer", .icon_name = XDG::IconDevices::computer},
        {.text = "Harddisk", .icon_name = XDG::IconDevices::driveHarddisk},
        {.text = "Removable", .icon_name = XDG::IconDevices::driveRemovableMedia},
        {.text = "Keyboard", .icon_name = XDG::IconDevices::inputKeyboard},
        {.text = "Mouse", .icon_name = XDG::IconDevices::inputMouse},
        {.text = "Game", .icon_name = XDG::IconDevices::inputGaming},
        {.text = "Calculator", .icon_name = XDG::IconApplications::accessoriesCalculator},
        {.text = "Editor", .icon_name = XDG::IconApplications::accessoriesTextEditor},
        {.text = "Terminal", .icon_name = XDG::IconApplications::utilitiesTerminal},
        {.text = "Browser", .icon_name = XDG::IconApplications::internetWebBrowser},
        {.text = "System", .icon_name = XDG::IconApplications::preferencesSystem},
        {.text = "Error", .icon_name = XDG::IconStatus::dialogError},
        {.text = "Warning", .icon_name = XDG::IconStatus::dialogWarning},
        {.text = "Info", .icon_name = XDG::IconStatus::dialogInformation},
        {.text = "Question", .icon_name = XDG::IconStatus::dialogQuestion},
        });

    auto grid = std::make_unique<toolkit::IconGrid>(grid_model);
    auto *grid_ptr = grid.get();
    grid->on_selection_changed = [](std::set<size_t> const &indices) {
        if (indices.empty()) {
            spdlog::info("Icon Grid selection cleared");
        } else {
            spdlog::info("Icon Grid selected: {} item(s)", indices.size());
            for (auto i : indices) {
                spdlog::info("  - index {}", i);
            }
        }
    };

    auto grid_controls = std::make_unique<toolkit::HBoxLayout>();
    grid_controls->set_spacing(10);
    grid_controls->add<toolkit::Label>().set_text("Icon Size:");

    auto &size_slider = grid_controls->add<toolkit::Slider>(1).set_range(16, 256).set_value(48);
    auto &size_label = grid_controls->add<toolkit::Label>().set_text("48px");

    size_slider.set_on_change([grid_ptr, &size_label](toolkit::Slider &, float v) {
        auto size = static_cast<int>(v);
        grid_ptr->set_icon_size(size);
        size_label.set_text(fmt::format("{}px", size));
    });

    auto scaling_cb = std::make_unique<toolkit::Checkbox>("Scale Icons");
    scaling_cb->on_toggle = [grid_ptr](bool checked) { grid_ptr->set_scale_icons(checked); };
    grid_controls->add_widget(std::move(scaling_cb));

    tab_grid->add_widget(std::move(grid_controls));
    tab_grid->add_widget(std::move(grid), 1);

    tabs->add_tab("Grid", std::move(tab_grid));

    // ── Tab: Songs ─────────────────────────────────────────────────────
    auto tab3 = std::make_unique<toolkit::VBoxLayout>();
    tab3->set_margins({20, 20, 20, 20});
    tab3->set_spacing(12);

    auto songs_adapter = std::make_shared<toolkit::StringListModel>(beatlesSongs);

    auto filter_adapter = std::make_shared<toolkit::FilterAdapter>(songs_adapter);
    filter_adapter->set_simulated_delay_ms(10);

    auto filter_row = std::make_unique<toolkit::HBoxLayout>();
    filter_row->set_spacing(8);

    auto filter_input = std::make_unique<toolkit::LineInput>("Filter songs...");
    filter_input->set_tooltip("Type to filter the song list");
    filter_input->on_change = [filter_adapter](std::string const &text, auto &) {
        filter_adapter->set_filter(text);
    };
    filter_row->add_widget(std::move(filter_input), 1);

    auto delay_label = std::make_unique<toolkit::Label>("Delay (ms):");
    filter_row->add_widget(std::move(delay_label));

    auto delay_spin = std::make_unique<toolkit::SpinBox>(10, 0, 200, 5);
    delay_spin->on_change = [filter_adapter](int val, toolkit::SpinBox &) {
        filter_adapter->set_simulated_delay_ms(val);
    };
    delay_spin->set_tooltip("Simulated delay per item (ms)");
    filter_row->add_widget(std::move(delay_spin));

    tab3->add_widget(std::move(filter_row));

    auto filter_progress = std::make_unique<toolkit::ProgressBar>();
    auto *filter_progress_ptr = filter_progress.get();
    filter_progress->set_visible(false);
    tab3->add_widget(std::move(filter_progress));

    filter_adapter->on_progress = [filter_progress_ptr, window](float progress) {
        if (progress < 0.0f) {
            filter_progress_ptr->set_visible(false);
        } else {
            filter_progress_ptr->set_visible(true);
            filter_progress_ptr->set_value(progress);
        }
        window->request_redraw();
    };

    auto songs_list = std::make_unique<toolkit::ListView>(filter_adapter);
    songs_list->set_alternating_row_colors(true);
    songs_list->set_multi_select(true);
    songs_list->on_selection_changed = [filter_adapter](std::optional<size_t> idx) {
        if (idx) {
            auto src = filter_adapter->source_index(*idx);
            spdlog::info("Selected: [{}] {}", src ? *src : SIZE_MAX,
                         filter_adapter->cell_text(*idx, 0));
        }
    };
    tab3->add_widget(std::move(songs_list), 1);

    tabs->add_tab("Songs", std::move(tab3));

    // ── Tab: Table ──────────────────────────────────────────────────────
    auto tab4 = std::make_unique<toolkit::VBoxLayout>();
    tab4->set_margins({20, 20, 20, 20});
    tab4->set_spacing(12);

    auto table_model = std::make_shared<toolkit::StringTableModel>(
        std::vector<std::string>{"Song", "Album", "Year", "Duration"}, beatlesSongsLength);

    auto table = std::make_unique<toolkit::TableView>(table_model);
    auto *table_ptr = table.get();
    table->set_alternating_row_colors(true);
    table->set_multi_select(true);
    table->set_column_width(0, 220.0f);
    table->set_column_width(1, 140.0f);
    table->set_column_width(2, 60.0f);
    table->set_column_width(3, 80.0f);
    table->on_selection_changed = [table_model](std::optional<size_t> row) {
        if (row) {
            spdlog::info("Table: {} - {}", table_model->cell_text(*row, 0),
                         table_model->cell_text(*row, 1));
        }
    };

    auto table_toggle_row = std::make_unique<toolkit::HBoxLayout>();
    auto header_toggle = std::make_unique<toolkit::Checkbox>("Show Header");
    header_toggle->set_checked(true);
    header_toggle->on_toggle = [table_ptr](bool checked) { table_ptr->set_show_header(checked); };
    table_toggle_row->add_widget(std::move(header_toggle));
    tab4->add_widget(std::move(table_toggle_row));

    tab4->add_widget(std::move(table), 1);

    tabs->add_tab("Table", std::move(tab4));

    // ── Tab: Editor ─────────────────────────────────────────────────────
    auto tab5 = std::make_unique<toolkit::VBoxLayout>();
    tab5->set_margins({20, 20, 20, 20});
    tab5->set_spacing(12);

    auto editor_toolbar = std::make_unique<toolkit::HBoxLayout>();
    editor_toolbar->set_spacing(8);

    auto open_btn = std::make_unique<toolkit::Button>("Open...");
    open_btn->set_tooltip("Open a text file (F3)");
    open_btn->set_command(open_menu_cmd);
    editor_toolbar->add_widget(std::move(open_btn));

    auto save_btn = std::make_unique<toolkit::Button>("Save As...");
    save_btn->set_tooltip("Save the file (Ctrl+S)");
    save_btn->set_command(save_cmd);
    editor_toolbar->add_widget(std::move(save_btn));

    auto choose_dir_btn = std::make_unique<toolkit::Button>("Choose Directory...");
    choose_dir_btn->set_tooltip("Choose a directory");
    choose_dir_btn->on_click = [use_native_cb, window] {
        toolkit::DirectoryDialog(window)
            .title("Choose Directory")
            .use_native(use_native_cb->checked())
            .choose()
            .then([](auto path) {
                if (path) {
                    spdlog::info("Directory chosen: {}", *path);
                } else {
                    spdlog::info("Directory dialog cancelled");
                }
            });
    };
    editor_toolbar->add_widget(std::move(choose_dir_btn));

    editor_toolbar->add_widget(std::unique_ptr<toolkit::Checkbox>(use_native_cb));

    auto toolbar_spacer = std::make_unique<toolkit::Label>("");
    editor_toolbar->add_widget(std::move(toolbar_spacer), 1);

    tab5->add_widget(std::move(editor_toolbar));
    tab5->add_widget(std::unique_ptr<toolkit::TextEdit>(editor), 1);

    tabs->add_tab("Editor", std::move(tab5));

    // ── Tab 6: Tree ───────────────────────────────────────────────────────
    auto tab_tree = std::make_unique<toolkit::VBoxLayout>();
    tab_tree->set_margins({20, 20, 20, 20});
    tab_tree->set_spacing(12);

    auto tree_data = std::vector<toolkit::TreeNode>{
        toolkit::TreeNode{
            .text = "Documents",
            .children =
                {
                    toolkit::TreeNode{.text = "resume.pdf"},
                    toolkit::TreeNode{.text = "cover_letter.pdf"},
                    toolkit::TreeNode{
                        .text = "Projects",
                        .children =
                            {
                                toolkit::TreeNode{.text = "svision"},
                                toolkit::TreeNode{.text = "toolkit"},
                                toolkit::TreeNode{.text = "web"},
                            },
                        .expanded = true,
                    },
                },
            .expanded = true,
        },
        toolkit::TreeNode{
            .text = "Music",
            .children =
                {
                    toolkit::TreeNode{
                        .text = "Beatles",
                        .children =
                            {
                                toolkit::TreeNode{.text = "Abbey Road"},
                                toolkit::TreeNode{.text = "Revolver"},
                                toolkit::TreeNode{.text = "Sgt. Pepper's"},
                            },
                    },
                    toolkit::TreeNode{.text = "Pink Floyd"},
                    toolkit::TreeNode{.text = "Queen"},
                },
        },
        toolkit::TreeNode{
            .text = "Pictures",
            .children =
                {
                    toolkit::TreeNode{
                        .text = "2024",
                        .children =
                            {
                                toolkit::TreeNode{.text = "vacation.jpg"},
                                toolkit::TreeNode{.text = "family.png"},
                            },
                    },
                    toolkit::TreeNode{.text = "2025"},
                },
        },
        toolkit::TreeNode{
            .text = "Downloads",
            .children =
                {
                    toolkit::TreeNode{.text = "archive.tar.gz"},
                    toolkit::TreeNode{.text = "installer.dmg"},
                },
        },
    };

    auto tree_model = std::make_shared<toolkit::SimpleTreeModel>(tree_data);
    auto tree = std::make_unique<toolkit::TreeView>(tree_model);
    tree->set_alternating_row_colors(true);
    tree->set_multi_select(true);
    tree->on_selection_changed = [tree_model](int idx) {
        if (idx >= 0 && idx < tree_model->root_count()) {
            spdlog::info("Tree selected: {}", tree_model->root_at(idx).text);
        }
    };
    tab_tree->add_widget(std::move(tree), 1);

    tabs->add_tab("Tree", std::move(tab_tree));

    // ── Tab: Preview (Markdown / HTML live editor) ─────────────────────
    static constexpr float PREVIEW_DELAY_SEC = 5.0f;

    auto preview_html = new toolkit::HtmlView();
    preview_html->set_css(GITHUB_MARKDOWN_CSS_LIGHT, GITHUB_MARKDOWN_CSS_DARK);
    preview_html->set_markdown(PREVIEW_DEFAULT_MD);
    preview_html->on_link_click = [](std::string const &url) {
        spdlog::info("Preview link clicked: {}", url);
    };

    auto preview_scroll = std::make_unique<toolkit::ScrollArea>();
    preview_scroll->set_content(std::unique_ptr<toolkit::HtmlView>(preview_html));

    auto preview_editor = new toolkit::TextEdit();
    preview_editor->set_text(PREVIEW_DEFAULT_MD);

    auto preview_columns =
        std::make_unique<toolkit::Splitter>(toolkit::Orientation::Horizontal);
    preview_columns->set_first(std::unique_ptr<toolkit::TextEdit>(preview_editor));
    preview_columns->set_second(std::move(preview_scroll));

    auto preview_progress = new toolkit::ProgressBar();
    preview_progress->set_visible(false);

    auto preview_mode_group = std::make_shared<toolkit::RadioGroup>();
    auto preview_is_markdown = std::make_shared<bool>(true);
    auto preview_pending = std::make_shared<bool>(false);
    auto preview_last_edit =
        std::make_shared<std::chrono::steady_clock::time_point>(std::chrono::steady_clock::now());

    auto apply_preview = [preview_html, preview_editor, preview_is_markdown]() {
        if (*preview_is_markdown) {
            preview_html->set_markdown(preview_editor->text());
        } else {
            preview_html->set_html(preview_editor->text());
        }
    };

    preview_editor->on_change = [preview_pending, preview_last_edit, preview_progress, window]() {
        *preview_last_edit = std::chrono::steady_clock::now();
        if (!*preview_pending) {
            *preview_pending = true;
            preview_progress->set_visible(true);
            preview_progress->set_value(0.0f);
            window->request_redraw("preview pending");
        }
    };

    window->start_timer(
        0.25f, [preview_pending, preview_last_edit, preview_progress, apply_preview, window]() {
            if (!*preview_pending) {
                return;
            }
            auto elapsed =
                std::chrono::duration<float>(std::chrono::steady_clock::now() - *preview_last_edit)
                    .count();
            preview_progress->set_value(std::min(elapsed / PREVIEW_DELAY_SEC, 1.0f));
            if (elapsed >= PREVIEW_DELAY_SEC) {
                *preview_pending = false;
                preview_progress->set_visible(false);
                apply_preview();
                window->request_redraw("preview done");
            }
        });

    preview_mode_group->on_change = [preview_is_markdown, preview_pending, preview_progress,
                                     preview_editor, apply_preview](int index) {
        *preview_is_markdown = (index == 0);
        *preview_pending = false;
        preview_progress->set_visible(false);
        preview_editor->set_text(*preview_is_markdown ? PREVIEW_DEFAULT_MD : PREVIEW_DEFAULT_HTML);
        apply_preview();
    };

    auto rb_md = std::make_unique<toolkit::RadioButton>("Markdown", *preview_mode_group);
    auto rb_html_mode = std::make_unique<toolkit::RadioButton>("HTML", *preview_mode_group);
    preview_mode_group->select(rb_md.get());

    auto github_css_cb = std::make_unique<toolkit::Checkbox>("Use GitHub CSS");
    github_css_cb->set_checked(true);
    github_css_cb->on_toggle = [preview_html, apply_preview](bool checked) {
        if (checked) {
            preview_html->set_css(GITHUB_MARKDOWN_CSS_LIGHT, GITHUB_MARKDOWN_CSS_DARK);
        } else {
            preview_html->set_css("", "");
        }
        apply_preview();
    };

    auto preview_bottom = std::make_unique<toolkit::HBoxLayout>();
    preview_bottom->set_margins({4, 8, 4, 8});
    preview_bottom->set_spacing(16);
    preview_bottom->add_widget(std::move(rb_md));
    preview_bottom->add_widget(std::move(rb_html_mode));
    preview_bottom->add_widget(std::move(github_css_cb));

    auto tab_preview = std::make_unique<toolkit::VBoxLayout>();
    tab_preview->set_margins({0, 0, 0, 0});
    tab_preview->set_spacing(0);
    tab_preview->add_widget(std::move(preview_columns), 1);
    tab_preview->add_widget(std::unique_ptr<toolkit::ProgressBar>(preview_progress));
    tab_preview->add_widget(std::move(preview_bottom));
    tabs->add_tab("Preview", std::move(tab_preview));

    // ── Tab: Tabs (Orientations) ────────────────────────────────────────
    auto tab6 = std::make_unique<toolkit::VBoxLayout>();
    tab6->set_margins({0, 0, 0, 0});
    tab6->set_spacing(0);
    auto make_plus = []() {
        auto b = std::make_unique<toolkit::Button>("+");
        b->set_flat(true);
        b->set_focusable(false);
        b->set_padding({2, 8, 2, 8});
        return b;
    };
    auto make_close = []() {
        auto b = std::make_unique<toolkit::Button>("x");
        b->set_flat(true);
        b->set_focusable(false);
        b->set_padding({2, 8, 2, 8});
        b->set_background_color(toolkit::Color::rgb(1.0f, 0.8f, 0.8f));
        return b;
    };

    auto south_tabs = std::make_unique<toolkit::TabWidget>();
    south_tabs->set_orientation(toolkit::TabOrientation::South);
    south_tabs->set_leading_widget(make_plus());
    south_tabs->set_trailing_widget(make_close());
    for (int i = 1; i <= 5; ++i) {
        auto label = std::make_unique<toolkit::Label>(fmt::format("South Tab {} Content", i));
        label->set_alignment(toolkit::Alignment::Center);
        label->set_background_color(toolkit::Color::rgb(1.0f, 1.0f - i * 0.1f, 1.0f - i * 0.1f));
        south_tabs->add_tab(fmt::format("South {}", i), std::move(label));
    }

    auto side_row = std::make_unique<toolkit::HBoxLayout>();
    side_row->set_margins({0, 0, 0, 0});
    side_row->set_spacing(0);

    auto west_tabs = std::make_unique<toolkit::TabWidget>();
    west_tabs->set_orientation(toolkit::TabOrientation::West);
    west_tabs->set_leading_widget(make_plus());
    west_tabs->set_trailing_widget(make_close());
    for (int i = 1; i <= 5; ++i) {
        auto label = std::make_unique<toolkit::Label>(fmt::format("West Tab {} Content", i));
        label->set_alignment(toolkit::Alignment::Center);
        label->set_background_color(toolkit::Color::rgb(1.0f - i * 0.1f, 1.0f, 1.0f - i * 0.1f));
        west_tabs->add_tab(fmt::format("West {}", i), std::move(label));
    }

    auto east_tabs = std::make_unique<toolkit::TabWidget>();
    east_tabs->set_orientation(toolkit::TabOrientation::East);
    east_tabs->set_leading_widget(make_plus());
    east_tabs->set_trailing_widget(make_close());
    for (int i = 1; i <= 5; ++i) {
        auto label = std::make_unique<toolkit::Label>(fmt::format("East Tab {} Content", i));
        label->set_alignment(toolkit::Alignment::Center);
        label->set_background_color(toolkit::Color::rgb(1.0f - i * 0.1f, 1.0f - i * 0.1f, 1.0f));
        east_tabs->add_tab(fmt::format("East {}", i), std::move(label));
    }

    side_row->add_widget(std::move(west_tabs), 1, toolkit::Alignment::Fill);
    side_row->add_widget(std::move(east_tabs), 1, toolkit::Alignment::Fill);

    tab6->add_widget(std::move(side_row), 1);
    tab6->add_widget(std::move(south_tabs), 0);

    tabs->add_tab("Tabs", std::move(tab6));

    // ── Tab: Image ───────────────────────────────────────────────────────
    auto tab_image = std::make_unique<toolkit::VBoxLayout>();
    tab_image->set_margins({20, 20, 20, 20});

    auto img_widget = std::make_unique<toolkit::ImageWidget>();
    img_widget->set_show_checkerboard(true);
    img_widget->load("vampire-riding-a-dinozaur.png");
    auto *img_widget_ptr = img_widget.get();

    auto load_btn = std::make_unique<toolkit::Button>("Load Image...");
    load_btn->on_click = [img_widget_ptr, window, use_native_cb] {
        toolkit::FileDialog(window)
            .title("Load Image")
            .use_native(use_native_cb->checked())
            .open()
            .then([img_widget_ptr](auto path) {
                if (path) {
                    img_widget_ptr->load(*path);
                }
            });
    };
    tab_image->add_widget(std::move(load_btn));

    /*auto scroll = std::make_unique<toolkit::ScrollArea>();
    scroll->set_content(std::move(img_widget));
    tab_image->add_widget(std::move(scroll), 1); // Expand to fill tab*/
    tab_image->add_widget(std::move(img_widget), 1);
    tabs->add_tab("Image", std::move(tab_image));

    root->add_widget(std::move(tabs), 1);

    // ── Button bar ───────────────────────────────────────────────────────
    auto button_bar = std::make_unique<toolkit::HBoxLayout>();
    button_bar->set_margins({20, 10, 20, 10});
    button_bar->set_spacing(10);

    auto ok_btn = std::make_unique<toolkit::Button>("&ok");
    ok_btn->on_click = [] { spdlog::info("Button clicked!"); };
    ok_btn->set_tooltip("Log a message to the console");
    button_bar->add_widget(std::move(ok_btn));

    auto toast_btn = std::make_unique<toolkit::Button>("Toast");
    toast_btn->on_click = [window] {
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
        auto builder = toolkit::ToastBuilder().title(title).timeout(7).text(LOREM_IPSUM);
        auto bg = colors[count % 6];
        if (bg) {
            builder.background(*bg);
        }
        window->show_toast(builder);
    };
    button_bar->add_widget(std::move(toast_btn));

    auto disabled_btn = std::make_unique<toolkit::Button>("Disabled");
    disabled_btn->set_enabled(false);
    disabled_btn->set_tooltip("This button is disabled");
    button_bar->add_widget(std::move(disabled_btn));

    auto btn_spacer = std::make_unique<toolkit::Label>("");
    button_bar->add_widget(std::move(btn_spacer), 1);

    auto export_btn = std::make_unique<toolkit::Button>("Export to JSON");
    export_btn->on_click = [window, use_native_cb] {
        toolkit::FileDialog(window)
            .title("Export Window to JSON")
            .use_native(use_native_cb->checked())
            .save()
            .then([window](auto path) {
                if (path) {
                    std::ofstream f(*path);
                    if (f) {
                        f << window->to_json().dump(4);
                    }
                }
            });
    };
    button_bar->add_widget(std::move(export_btn));

    auto quit_btn = std::make_unique<toolkit::Button>("&Quit");
    quit_btn->on_click = [window] { window->close(); };
    quit_btn->set_tooltip("Close the application (Cmd+Q)");
    button_bar->add_widget(std::move(quit_btn));

    root->add_widget(std::move(button_bar));

    auto status_bar = std::make_unique<toolkit::StatusBar>();
    status_bar->add_section("status", "Ready");
    status_bar->add_section("progress", "Connecting...").appear(250);
    status_bar->add_section("work", "Processing").spinner(100);
    status_bar->add_section("pulse", "Scanning").pulse(100);
    root->add_widget(std::move(status_bar));

    window->set_root(std::move(root));
    window->resize_to_fit();
    window->relayout();

    if (!screenshot_path.empty()) {
        bool ok = window->save_to_png(screenshot_path);
        spdlog::info("Screenshot saved to '{}': {}", screenshot_path, ok ? "success" : "failed");
        return ok ? 0 : 1;
    }

    window->show();

    auto_progress_timer = window->start_timer(0.25f, [progress_ptr] {
        float v = progress_ptr->value() + 0.01f;
        if (v > 1.0f) {
            v = 0.0f;
        }
        progress_ptr->set_value(v);
        progress_ptr->set_tooltip(std::to_string(static_cast<int>(v * 100)) + "%");
    });

    return app.run();
}
