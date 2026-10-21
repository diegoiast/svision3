// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "declarative.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>

#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <svision3/application.hpp>
#include <svision3/command_source.hpp>
#include <svision3/dock_area.hpp>
#include <svision3/message_box.hpp>
#include <svision3/plugin.hpp>
#include <svision3/tab_command_anchor.hpp>
#include <svision3/theme.hpp>
#include <svision3/theme_factory.hpp>
#include <svision3/window.hpp>
#include <svision3/xdg_icons.hpp>
#include <svision3/xdg_image_loader.hpp>

using namespace svision3;

// A TextEdit that remembers where it came from, and so can offer to save itself.
//
// The Save command is tagged "file" while the Cut/Copy/Paste/Undo/Redo commands
// TextEdit registers for itself are tagged "edit". That tag is the whole
// contract: the File menu asks for "file" commands and the Edit menu asks for
// "edit" ones, and neither has ever heard of this class. A widget arriving from
// a plugin would join both menus on exactly the same terms.
//
// Nothing here is tagged "help", so a text tab leaves the Help menu as just
// About -- ImageView is the only widget that fills it now.
class FileTextEdit : public TextEdit {
    DECLARE_WIDGET(FileTextEdit)
  public:
    explicit FileTextEdit(std::string path) : path_(std::move(path)) {
        set_highlight_current_line(true);

        auto f = std::ifstream(path_);
        if (f) {
            set_text(std::string(std::istreambuf_iterator<char>(f), {}));
        }

        save_cmd_ = Command::create("Save", [this] { save(); });
        save_cmd_->set_shortcut("Std+S");
        save_cmd_->set_icon(XDG::IconActions::documentSave);
        save_cmd_->set_group("file");
        add_command(save_cmd_);
    }

    std::string const &path() const { return path_; }

  private:
    void save() {
        auto f = std::ofstream(path_);
        if (!f) {
            spdlog::error("Could not open '{}' for writing", path_);
            return;
        }
        f << text();
        spdlog::info("Saved '{}'", path_);
    }

    std::string path_;
    Command::Ptr save_cmd_;
};

// An ImageWidget that can describe itself.
//
// The report lives here rather than in ImageWidget because it is presentation:
// what to mention, how to word it, and that it appears in a modal box at all are
// application decisions. The widget supplies the facts -- image(), source_path(),
// zoom() -- and stays out of it.
class ImageView : public ImageWidget {
    DECLARE_WIDGET(ImageView)
  public:
    ImageView() {
        info_cmd_ = Command::create("Image Information", [this] { show_info(); });
        info_cmd_->set_icon(XDG::IconActions::documentProperties);
        info_cmd_->set_group("help");
        add_command(info_cmd_);
    }

  private:
    void show_info() {
        if (!window()) {
            return;
        }
        MessageBox(window())
            .title("Image Information")
            .icon(MessageBoxIcon::Information)
            .markdown()
            .message(describe())
            .show();
    }

    std::string describe() const {
        auto const &data = image();
        if (!data) {
            return "No image is loaded.";
        }

        auto name = source_path().empty()
                        ? std::string("Untitled image")
                        : std::filesystem::path(source_path()).filename().string();
        auto bytes = static_cast<double>(data->pixels.size());
        auto const *unit = "bytes";
        if (bytes >= 1024.0 * 1024.0) {
            bytes /= 1024.0 * 1024.0;
            unit = "MiB";
        } else if (bytes >= 1024.0) {
            bytes /= 1024.0;
            unit = "KiB";
        }

        auto text = "## " + name + "\n\n";
        if (!source_path().empty()) {
            text += "- **Location:** " + source_path() + "\n";
        }
        text += "- **Dimensions:** " + std::to_string(data->width) + " x " +
                std::to_string(data->height) + " pixels\n";
        text += "- **Pixel format:** " +
                std::string(data->format == PixelFormat::RGBA ? "RGBA" : "BGRA") + ", " +
                std::to_string(data->channels) + " channels\n";
        text += "- **In memory:** " + fmt::format("{:.1f}", bytes) + " " + unit + "\n";
        text +=
            "- **Zoom:** " + std::to_string(static_cast<int>(std::lround(zoom() * 100.0f))) + "%\n";
        return text;
    }

    Command::Ptr info_cmd_;
};

// What the platform image loaders can open. Anything else is treated as text.
static bool is_image_file(std::filesystem::path const &path) {
    static constexpr std::array extensions = {".png", ".jpg", ".jpeg", ".bmp", ".gif",
                                              ".tga", ".psd", ".hdr",  ".pic", ".pnm",
                                              ".ppm", ".pgm", ".svg",  ".ico", ".webp"};
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
}

// Copies a menu's structure into fresh Menu objects.
//
// The Command pointers are shared rather than duplicated, so the copy drives the
// same actions and shows the same enabled and checked state -- only the Menu
// objects are new. That is the whole point: a Menu carries its open/hover state
// and has its parent_menu_ and close callback written by whoever opens it, so one
// cannot hang off two triggers at once. on_about_to_show comes along because a
// menu that refreshes itself on open (the plugin list) must still do so here.
static std::shared_ptr<Menu> clone_menu(Menu const &source) {
    auto copy = std::make_shared<Menu>(source.title());
    for (auto const &item : source.items()) {
        if (item.is_separator()) {
            copy->add_separator();
        } else if (item.is_submenu() && item.submenu) {
            copy->add_submenu(item.command->name(), clone_menu(*item.submenu));
        } else if (item.command) {
            copy->add_action(item.command);
        }
    }
    copy->on_about_to_show = source.on_about_to_show;
    return copy;
}

// A command with an icon, for the places that want one inline.
static Command::Ptr make_icon_action(std::string name, char const *icon,
                                     std::function<void()> action) {
    auto cmd = Command::create(std::move(name), std::move(action));
    cmd->set_icon(icon);
    return cmd;
}

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

// A plugin that turns the bare editor into a project IDE: it brings the Outline,
// Inspector, Console and Problems panels, and a Project menu to drive a build.
//
// Everything it adds, it takes back out again in uninstall(). That is not
// tidiness -- a plugin's code and vtables live in a shared library that may be
// unloaded the moment it is removed, so anything of the plugin's still reachable
// from the application afterwards is a crash waiting for the next event.
class ProjectPlugin : public Plugin {
  public:
    std::string_view name() const override { return "project"; }
    std::string_view display_name() const override { return "&Project"; }

    void install(PluginHost &host) override {
        if (auto *docks = host.docks()) {
            // Keep what add_dock() hands back: uninstall() then takes out exactly
            // these panels, rather than whatever currently answers to their names.
            docks_ = {
                docks->add_dock(DockPosition::East, "Outline", make_outline().w),
                docks->add_dock(DockPosition::West, "Inspector", make_inspector()),
                docks->add_dock(DockPosition::South, "Console", make_console().w),
                docks->add_dock(DockPosition::South, "Problems", make_problems().w),
            };
        }

        auto *bar = host.menubar();
        if (!bar) {
            return;
        }

        menu_ = std::make_shared<Menu>("&Project");
        // Build and Clean have no icon: the freedesktop set has nothing that means
        // either, and a wrong icon reads worse than none.
        auto run = Command::create("&Run", [] { spdlog::info("Project: run"); });
        run->set_icon(XDG::IconActions::systemRun);
        menu_->add_action("&Build", [] { spdlog::info("Project: build"); })
            .add_action("&Clean", [] { spdlog::info("Project: clean"); })
            .add_separator()
            .add_action(run);

        // Help is last by convention, so merge in ahead of it rather than
        // appending. A menubar with no Help menu just takes it at the end.
        auto help = bar->find_menu("Help");
        bar->insert_menu(help < 0 ? bar->menu_count() : help, menu_);

        // The plugin's own menu takes part in the context system on the same terms
        // as the application's: any widget contributing a "project"-tagged command
        // shows up here while it has focus. Nothing does yet, so this slice stays
        // empty -- which is exactly why it brings its own separator.
        if (auto *commands = host.commands()) {
            anchor_ = std::make_unique<CommandAnchor>(*commands, *menu_, 0, "project",
                                                      CommandAnchor::Separator::After);
        }
    }

    void uninstall(PluginHost &host) override {
        // Anchor first: it holds references to both the menu it writes into and
        // the source it subscribes to, and its destructor touches both.
        anchor_.reset();

        if (auto *bar = host.menubar()) {
            bar->remove_menu(menu_);
        }
        menu_.reset();

        if (auto *docks = host.docks()) {
            for (auto const &panel : docks_) {
                docks->remove_dock(panel);
            }
        }
        docks_.clear();
    }

  private:
    std::vector<std::weak_ptr<Widget>> docks_;
    std::shared_ptr<Menu> menu_;
    std::unique_ptr<CommandAnchor> anchor_;
};

// The application's side of the bargain: what it is prepared to let a plugin
// reach. A plugin can do nothing this does not hand it.
//
// Held as shared_ptr rather than raw: the host outlives nothing here, but it is
// the only thing standing between a plugin and a dangling widget, so it owns a
// reference to everything it hands out. The .get() in each accessor is forced by
// PluginHost's signature, not a choice made here.
class DemoHost : public PluginHost {
  public:
    DemoHost(std::shared_ptr<Window> win, std::shared_ptr<DockArea> docks,
             std::shared_ptr<MenuBar> menubar, std::shared_ptr<CommandSource> commands)
        : win_(std::move(win)), docks_(std::move(docks)), menubar_(std::move(menubar)),
          commands_(std::move(commands)) {}

    Window *window() const override { return win_.get(); }
    DockArea *docks() const override { return docks_.get(); }
    MenuBar *menubar() const override { return menubar_.get(); }
    CommandSource *commands() const override { return commands_.get(); }

  private:
    std::shared_ptr<Window> win_;
    std::shared_ptr<DockArea> docks_;
    std::shared_ptr<MenuBar> menubar_;
    std::shared_ptr<CommandSource> commands_;
};

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
               "    dock->add_dock<svision3::TextEdit>(svision3::DockPosition::Bottom, "
               "\"Console\");\n"
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

    // Center: TabWidget with an editor and a picture, so the demo opens with both
    // kinds of tab already there -- switching between them is what makes the menus
    // change shape, and that is easier to notice than to go and set up.
    auto center = ui::tab_widget().tabs_closable(true).add_tab("Welcome", make_center_editor());
    auto center_ref = center.ref();
    center->on_tab_close = [center_ref](int index, std::string const &) {
        auto center_ptr = center_ref.lock();
        if (center_ptr && center_ptr->get_tab_count() > 1) {
            center_ptr->remove_tab(index);
        }
    };

    auto vampire = std::make_shared<ImageView>();
    vampire->load("vampire-riding-a-dinozaur.png");
    center->add_tab("vampire.png", vampire);
    center->set_tab_tooltip(
        1, std::filesystem::weakly_canonical("vampire-riding-a-dinozaur.png").string());
    // The editor leads: it is the tab whose commands the menus and toolbar show
    // when the window comes up. add_tab() does not move the current index, so this
    // only says so out loud -- and keeps saying it if that ever changes.
    center->set_current(0);

    auto dock =
        ui::dock_area()
            .center(std::move(center))
            .dock_size(DockPosition::East, 200.0f)
            .dock_size(DockPosition::West, 200.0f)
            .dock_size(DockPosition::South, 200.0f)
            .add_dock(DockPosition::East, "Files",
                      ui::file_browser()
                          .margins({0, 0, 0, 0})
                          .browser_mode(true)
                          .on_file_activated([center_ref](std::string const &path) {
                              auto center_ptr = center_ref.lock();
                              if (!center_ptr) {
                                  return;
                              }
                              auto name = std::filesystem::path(path).filename().string();
                              for (auto i = 0; i < static_cast<int>(center_ptr->get_tab_count());
                                   ++i) {
                                  if (center_ptr->get_tab_title(i) == name) {
                                      center_ptr->set_current(i);
                                      return;
                                  }
                              }

                              // The two kinds of tab carry different command sets, so
                              // opening a picture here is what makes the menus above
                              // change shape: the Edit menu goes away and a View menu
                              // of zoom commands takes its place.
                              auto content = std::shared_ptr<Widget>{};
                              if (is_image_file(path)) {
                                  auto image = std::make_shared<ImageView>();
                                  image->load(path);
                                  content = std::move(image);
                              } else {
                                  content = std::make_shared<FileTextEdit>(path);
                              }

                              auto canonical = std::filesystem::weakly_canonical(path).string();
                              auto tab_index = static_cast<int>(center_ptr->get_tab_count());
                              center_ptr->add_tab(name, std::move(content));
                              center_ptr->set_tab_tooltip(tab_index, canonical);
                              center_ptr->set_current(tab_index);
                          }));
    // Outline, Inspector, Console and Problems are not built here: they arrive
    // with the "project" plugin, further down, and leave again when it does.

    auto dock_ref = dock.ref();
    // The side docks start vertical -- DockArea gives them East/West tab strips.
    auto vertical = std::make_shared<bool>(true);

    // The themes are a radio group: picking one checks it and clears the rest, so
    // the menu always says which is in use. They share one list of commands, so a
    // pick made through the tab bar's mirrored copy shows up here too.
    struct ThemeChoice {
        Command::Ptr command;
        ThemeStyle style;
        ColorScheme scheme;
    };
    auto theme_cmds = std::make_shared<std::vector<ThemeChoice>>();
    auto make_theme = [theme_cmds](std::string name, ThemeStyle style, ColorScheme scheme) {
        auto cmd = Command::create(std::move(name), nullptr);
        cmd->set_checkable(true);
        cmd->set_execute_func([theme_cmds, chosen = cmd.get(), style, scheme] {
            Theme::set_current(ThemeFactory::create(style, scheme));
            for (auto const &other : *theme_cmds) {
                other.command->set_checked(other.command.get() == chosen);
            }
        });
        theme_cmds->push_back({cmd, style, scheme});
        return cmd;
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
    cmd_toggle_docks->set_enabled(*vertical);
    win->add_command(cmd_toggle_docks);

    auto cmd_vertical_tabs = Command::create("Vertical Tabs", nullptr);
    cmd_vertical_tabs->set_checkable(true);
    cmd_vertical_tabs->set_checked(*vertical);
    auto toggle_vertical = [dock_ref, vertical, cmd_toggle_docks, cmd = cmd_vertical_tabs.get()]() {
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

        // Collapsing a dock leaves just its tab strip behind. That is worth doing
        // while the strip is a narrow rail down the side; with horizontal tabs the
        // collapsed dock is a full-width row of tab headers, which saves nothing.
        cmd_toggle_docks->set_enabled(*vertical);
        cmd->set_checked(*vertical);

        if (!*vertical) {
            // Leaving them collapsed on the way out would strand them: the one
            // command that could bring them back is the one just disabled.
            for (auto pos : {DockPosition::East, DockPosition::West, DockPosition::South}) {
                if (auto tab = dock_ptr->dock_tab_widget(pos).lock()) {
                    tab->set_collapsed(false);
                }
            }
        }
    };

    cmd_vertical_tabs->set_execute_func(toggle_vertical);

    auto cmd_inspector = Command::create("Inspector", nullptr);
    cmd_inspector->set_checkable(true);
    cmd_inspector->set_checked(Widget::debug_show_inspector);
    // Weak: this Command is stored on the window, so a shared capture is a cycle.
    cmd_inspector->set_execute_func([weak_win = std::weak_ptr(win), cmd = cmd_inspector.get()]() {
        Widget::debug_show_inspector = !Widget::debug_show_inspector;
        cmd->set_checked(Widget::debug_show_inspector);
        if (auto win = weak_win.lock()) {
            win->request_redraw("inspector toggle");
        }
    });
    cmd_inspector->set_icon(XDG::IconActions::viewFullscreen);
    cmd_inspector->set_shortcut("super+f12");
    win->add_command(cmd_inspector);

    auto theme_menu = std::make_shared<Menu>("Theme");
    theme_menu->add_action(make_theme("Material Light", ThemeStyle::Material, ColorScheme::Light))
        .add_action(make_theme("Material Dark", ThemeStyle::Material, ColorScheme::Dark))
        .add_separator()
        .add_action(make_theme("GNOME Light", ThemeStyle::GNOME, ColorScheme::Light))
        .add_action(make_theme("GNOME Dark", ThemeStyle::GNOME, ColorScheme::Dark))
        .add_separator()
        .add_action(make_theme("Plasma Light", ThemeStyle::Plasma6, ColorScheme::Light))
        .add_action(make_theme("Plasma Dark", ThemeStyle::Plasma6, ColorScheme::Dark))
        .add_separator()
        .add_action(make_theme("Windows 11 Light", ThemeStyle::Win11, ColorScheme::Light))
        .add_action(make_theme("Windows 11 Dark", ThemeStyle::Win11, ColorScheme::Dark))
        .add_separator()
        .add_action(make_theme("macOS Light", ThemeStyle::MacOS, ColorScheme::Light))
        .add_action(make_theme("macOS Dark", ThemeStyle::MacOS, ColorScheme::Dark))
        .add_separator()
        .add_action(make_theme("Windows 95", ThemeStyle::Win95, ColorScheme::Light));

    // The window opened with ThemeFactory::create(detect_system_style()), whose
    // scheme argument defaults to Light -- so that is the item that starts checked.
    for (auto const &choice : *theme_cmds) {
        choice.command->set_checked(choice.style == style && choice.scheme == ColorScheme::Light);
    }

    // ------------------------------------------------------------------------
    // Menus and toolbar driven by the active tab.
    //
    // Each CommandAnchor owns one contiguous run of items at a fixed position in
    // its menu or toolbar, refills that run from the active tab's widget on every
    // tab switch, and touches nothing outside it -- the static items around them
    // (New, Quit, Close Others) keep their place no matter how the dynamic side
    // grows or shrinks. Which commands land where is decided purely by the group
    // tag each command carries, so nothing here knows that FileTextEdit or
    // ImageWidget exist.
    // ------------------------------------------------------------------------
    // The command context follows keyboard focus, not the tab bar: clicking into
    // the Console pane docked at the bottom hands the menus to that editor, the
    // same as switching centre tabs does. Only widgets that declare themselves a
    // context count -- TextEdit and ImageWidget do, the toolbar's controls and the
    // dock lists do not, so reaching for those leaves the menus alone.
    auto command_source = std::make_shared<FocusCommandSource>(*win);

    auto cmd_quit = Command::create("&Quit", [] { Application::instance().quit(); });
    cmd_quit->set_icon(XDG::IconActions::applicationExit);

    auto file_menu = std::make_shared<Menu>("&File");
    file_menu
        ->add_action(make_icon_action(
            "&New", XDG::IconActions::documentNew,
            [center_ref] {
                auto center_ptr = center_ref.lock();
                if (!center_ptr) {
                    return;
                }
                auto index = static_cast<int>(center_ptr->get_tab_count());
                center_ptr->add_tab("untitled", ui::text_edit().highlight_current_line().w);
                center_ptr->set_current(index);
            }))
        .add_separator()
        .add_action(cmd_quit);

    // Between New and the Quit block. The slice brings its own leading separator,
    // so when the active tab has no "file" command to offer -- an image, or the
    // plain TextEdit on the Welcome tab -- nothing is left dangling behind it.
    auto file_anchor =
        CommandAnchor{*command_source, *file_menu, 1, "file", CommandAnchor::Separator::Before};

    // Edit and View have no static items at all: each is nothing but its slice,
    // so it disappears from the menubar entirely when the active tab has nothing
    // of that kind to contribute. Text tabs show Edit, image tabs show View.
    auto edit_menu = std::make_shared<Menu>("&Edit");
    auto edit_anchor = CommandAnchor{*command_source, *edit_menu, 0, "edit"};

    auto view_menu = std::make_shared<Menu>("&View");
    auto view_anchor = CommandAnchor{*command_source, *view_menu, 0, "view"};

    // Everything that used to live on the tab bar's own drop-down button, in one
    // place. Two near-identical menus was how the Plugins entry ended up on one
    // and not the other.
    auto settings_menu = std::make_shared<Menu>("&Settings");
    settings_menu->add_action(cmd_vertical_tabs)
        .add_action(cmd_toggle_docks)
        .add_action(cmd_inspector)
        .add_separator()
        .add_submenu("Theme", theme_menu);

    // Help: one always-present About, preceded by whatever the active tab has to
    // say about itself -- "Image Information" for a picture, "Document
    // Information" for a text file, nothing at all for the Welcome tab.
    auto help_menu = std::make_shared<Menu>("&Help");
    help_menu->add_action(
        make_icon_action("&About", XDG::IconActions::helpAbout, [weak_win = std::weak_ptr(win)] {
            auto win_ptr = weak_win.lock();
            if (!win_ptr) {
                return;
            }
            MessageBox(win_ptr.get())
                .title("About Dock Demo")
                .icon(MessageBoxIcon::Information)
                .markdown()
                .message("## Dock Demo\n\n"
                         "A demo of **svision3** docking, tabs, and menus that follow "
                         "the active tab.\n\n"
                         "The Edit, View and Help menus are filled from whichever "
                         "widget is in the front tab, through its own command list.")
                .show();
        }));
    auto help_anchor =
        CommandAnchor{*command_source, *help_menu, 0, "help", CommandAnchor::Separator::After};

    auto menubar = ui::menubar();
    menubar->add_menu(file_menu);
    menubar->add_menu(edit_menu);
    menubar->add_menu(view_menu);
    menubar->add_menu(settings_menu);
    menubar->add_menu(help_menu);

    // Toolbar: the whole command set of the active tab (group "" filters nothing),
    // then a separator, then one static button that no refresh ever disturbs.
    auto main_toolbar = std::make_shared<Toolbar>();
    main_toolbar->add_command(
        make_icon_action("Close Others", XDG::IconActions::windowClose, [center_ref] {
            auto center_ptr = center_ref.lock();
            if (!center_ptr) {
                return;
            }
            auto keep = center_ptr->get_current_index();
            for (auto i = static_cast<int>(center_ptr->get_tab_count()) - 1; i >= 0; --i) {
                if (i != keep) {
                    center_ptr->remove_tab(i);
                }
            }
        }));
    auto toolbar_anchor =
        CommandAnchor{*command_source, *main_toolbar, 0, "", CommandAnchor::Separator::After};

    // ------------------------------------------------------------------------
    // The menubar, mirrored onto a button in the tab bar, for when the menubar
    // itself is hidden. Only one of the two is ever on screen.
    //
    // Rebuilt each time it opens rather than copied once: Edit, View and Help are
    // filled from whichever widget has focus, and Project exists only while the
    // project plugin is installed, so any snapshot taken here would be wrong by
    // the time it was shown.
    // ------------------------------------------------------------------------
    auto mirror_menu = std::make_shared<Menu>();
    mirror_menu->on_about_to_show = [weak_bar = std::weak_ptr(menubar.w),
                                     weak_mirror = std::weak_ptr(mirror_menu)] {
        auto bar = weak_bar.lock();
        auto mirror = weak_mirror.lock();
        if (!bar || !mirror) {
            return;
        }
        mirror->clear();
        for (auto i = 0; i < bar->menu_count(); ++i) {
            auto source = bar->menu_at(i);
            // Skip the empty ones, exactly as the menubar does -- Edit and View
            // empty out depending on what has focus.
            if (!source || source->item_count() == 0) {
                continue;
            }
            mirror->add_submenu(source->display_title(), clone_menu(*source));
        }
    };

    // "&Menu": the mnemonic is what makes Alt+M drop it down.
    auto menu_button = ui::button("&Menu")
                           .focusable(false)
                           .background_color(Color::from_rgb(0x41cd52))
                           .menu(mirror_menu)
                           .w;

    // Ctrl+Alt+M clears the chrome away: menubar and toolbar both go, and the tab
    // bar grows the Menu button so everything is still reachable (Alt+M opens it).
    // Registered on the window rather than left to the menubar's own shortcut scan,
    // because a hidden menubar is skipped by the widget-tree dispatch -- the one
    // command that can bring it back must not live where hiding it takes it away.
    auto cmd_show_menubar = Command::create("Show Menu and &Toolbar", nullptr);
    cmd_show_menubar->set_shortcut("ctrl+alt+m");
    cmd_show_menubar->set_checkable(true);
    cmd_show_menubar->set_checked(true);
    cmd_show_menubar->set_execute_func([cmd = cmd_show_menubar.get(),
                                        weak_bar = std::weak_ptr(menubar.w), main_toolbar,
                                        center_ref, menu_button, weak_win = std::weak_ptr(win)] {
        auto bar = weak_bar.lock();
        auto center_ptr = center_ref.lock();
        auto win_ptr = weak_win.lock();
        if (!bar || !center_ptr || !win_ptr) {
            return;
        }

        auto show = !bar->is_visible();
        bar->set_visible(show);
        main_toolbar->set_visible(show);
        // TabWidget lays out a leading widget whenever one is set, visible or not,
        // so the button is swapped in and out rather than hidden.
        center_ptr->set_leading_widget(show ? nullptr : menu_button);
        cmd->set_checked(show);

        win_ptr->relayout();
        win_ptr->request_redraw("chrome visibility");
    });
    win->add_command(cmd_show_menubar);

    // ------------------------------------------------------------------------
    // Plugins.
    //
    // The host decides what a plugin can reach; the manager owns the plugins and
    // guarantees uninstall() runs before one is destroyed. "project" brings the
    // four side panels and the Project menu, and takes them away again -- the
    // Window > Plugins toggle makes that visible.
    // ------------------------------------------------------------------------
    // dock.w and menubar.w are moved into the window's root below; taking a
    // reference to each here is what keeps the host's handles valid afterwards.
    auto host = DemoHost{win, dock.w, menubar.w, command_source};
    auto plugins = PluginManager{host};

    plugins.install(std::make_shared<ProjectPlugin>());

    // Built from the registry, not by any plugin: a plugin's own install() cannot
    // own the item that switches it back on, because uninstall() takes that item
    // away with everything else it added. Switching "project" off leaves it
    // registered, so the same instance goes back in when it is switched on again.
    settings_menu->add_separator()
        .add_action(cmd_show_menubar)
        .add_submenu("Plugins", plugins.make_menu());

    win->set_root(ui::vbox()
                      .margins(ui::no_margins())
                      .spacing(ui::no_spacing)
                      .add(std::move(menubar))
                      .add(main_toolbar)
                      .add(std::move(dock.w), ui::expand)
                      .w);

    // A window comes up with nothing focused, and a focus-driven context has
    // nothing to show until something is. Hand the centre tab its focus now, which
    // settles the menus on the Welcome document before the first paint.
    center_ref.lock()->focus_content();

    // Weak: this callback lives on the window, so capturing the shared_ptr
    // would be a cycle that never frees.
    win->on_key = [weak_win = std::weak_ptr(win), dock_ref](KeyEvent const &e) {
        if (e.key == Key::Escape) {
            auto win = weak_win.lock();
            auto dock_ptr = dock_ref.lock();
            if (!win || !dock_ptr) {
                return false;
            }
            // on_key is first-chance: Window::handle_key consults it before both
            // the popup stack and whoever holds the keyboard. Claiming Escape
            // unconditionally would leave an open menu with no way to dismiss it,
            // and strand a menubar being driven by the keyboard (F10, Alt) with
            // no way out. Both of those own Escape ahead of this fallback.
            if (win->has_popup() || win->key_grab()) {
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
