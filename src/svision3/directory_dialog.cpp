// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/directory_dialog.hpp"
#include "svision3/application.hpp"
#include "svision3/button.hpp"
#include "svision3/checkbox.hpp"
#include "svision3/combobox.hpp"
#include "svision3/icon_grid.hpp"
#include "svision3/label.hpp"
#include "svision3/layout.hpp"
#include "svision3/line_input.hpp"
#include "svision3/platform.hpp"
#include "svision3/table_view.hpp"
#include "svision3/tree_view.hpp"
#include "svision3/window.hpp"
#include <cstdlib>
#include <filesystem>
#include <nfd.h>
#include <spdlog/spdlog.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#include <windows.h>
#endif

namespace svision3 {

namespace {

static bool is_hidden(std::string const &name, std::string const &path) {
#if defined(_WIN32)
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_HIDDEN);
#else
    return name.starts_with('.');
#endif
}

struct DirItem {
    std::string text;
    std::string icon_name;
    std::string icon_category;
    std::filesystem::file_time_type mtime;
    mutable Icon cached_icon;
    mutable int cached_icon_size = -1;
};

class DirListModel : public ItemModel {
  public:
    size_t row_count() const override { return items_.size(); }
    size_t column_count() const override { return 2; }

    std::string cell_text(size_t row, size_t col) const override {
        if (row >= items_.size()) {
            return {};
        }
        switch (col) {
        case 0:
            return items_[row].text;
        case 1: {
            auto sys = std::chrono::clock_cast<std::chrono::system_clock>(items_[row].mtime);
            return std::format("{:%Y-%m-%d %H:%M}",
                               std::chrono::zoned_time{std::chrono::current_zone(), sys});
        }
        default:
            return {};
        }
    }

    std::string header_text(size_t col) const override {
        switch (col) {
        case 0:
            return "Name";
        case 1:
            return "Modified";
        default:
            return {};
        }
    }

    Icon icon_at(size_t row, size_t col, int size) const override {
        if (col != 0 || row >= items_.size()) {
            return nullptr;
        }
        auto const &item = items_[row];
        if (item.cached_icon && item.cached_icon_size == size) {
            return item.cached_icon;
        }
        item.cached_icon =
            Application::instance().load_icon(item.icon_name, size, item.icon_category);
        item.cached_icon_size = size;
        return item.cached_icon;
    }

    void set_items(std::vector<DirItem> items) {
        items_ = std::move(items);
        if (on_data_changed) {
            on_data_changed();
        }
    }

    std::vector<DirItem> const &items() const { return items_; }

  private:
    std::vector<DirItem> items_;
};

enum class ViewMode { Icons, List, Details, Tree };

static void collapse_all_nodes(std::vector<TreeNode> &nodes) {
    for (auto &node : nodes) {
        node.expanded = false;
        if (!node.children.empty()) {
            collapse_all_nodes(node.children);
        }
    }
}

static bool expand_path_in_tree(std::vector<TreeNode> &nodes, std::string const &target_path,
                                std::string current_prefix = "") {
    for (auto &node : nodes) {
#if defined(_WIN32)
        auto node_path =
            current_prefix.empty() ? node.text + "\\" : current_prefix + node.text + "\\";
        auto test_path = current_prefix.empty() ? node.text : current_prefix + node.text;
#else
        auto node_path =
            current_prefix.empty() ? "/" + node.text : current_prefix + "/" + node.text;
        auto test_path = node_path;
#endif

        // Check if target path starts with this node's path or exactly matches it
        if (target_path == test_path || target_path.starts_with(node_path)) {
            node.expanded = true;
            if (!node.children.empty() && test_path != target_path) {
                // Recurse with the node path as the new prefix
                expand_path_in_tree(node.children, target_path, node_path);
            }
            return true;
        }
    }
    return false;
}

static std::vector<TreeNode> build_tree_nodes(std::string const &path, int depth = 0,
                                              bool show_hidden = false) {
    auto nodes = std::vector<TreeNode>{};
    if (depth > 3) { // Limit recursion depth
        return nodes;
    }

    try {
        for (auto const &entry : std::filesystem::directory_iterator(path)) {
            if (!entry.is_directory()) {
                continue;
            }
            auto name = entry.path().filename().string();
            if (name == "." || name == "..") {
                continue;
            }
            if (!show_hidden && is_hidden(name, entry.path().string())) {
                continue;
            }

            auto children = build_tree_nodes(entry.path().string(), depth + 1, show_hidden);
            nodes.push_back({.text = name, .children = std::move(children), .expanded = depth < 2});
        }
    } catch (...) {
        // Silently ignore directories we can't read
    }

    std::sort(nodes.begin(), nodes.end(),
              [](auto const &a, auto const &b) { return a.text < b.text; });
    return nodes;
}

class DirChooserWidget : public VBoxLayout {
  public:
    explicit DirChooserWidget(std::string start_path) : start_path_(std::move(start_path)) {
        auto home = std::getenv("HOME");
        if (!home) {
            home = std::getenv("USERPROFILE");
        }

        if (start_path_.empty()) {
            if (home) {
                start_path_ = home;
            } else {
#if defined(_WIN32)
                start_path_ = "C:\\";
#else
                start_path_ = "/";
#endif
            }
        }
        current_path_ = start_path_;
        model_ = std::make_shared<DirListModel>();
        setup_ui();
        load_directory();
    }

    std::function<void(std::optional<std::string>)> on_confirm;

  private:
    void setup_ui() {
        set_spacing(10);
        set_margins({10, 10, 10, 10});

        auto toolbar = std::make_unique<HBoxLayout>();
        toolbar->set_margins({});
        toolbar->set_spacing(5);

        toolbar->add_widget(std::make_unique<Label>("Look in:"), 0);

        std::vector<std::string> drives;
#if defined(_WIN32)
        if (GetLogicalDrives() > 0) {
            for (char letter = 'A'; letter <= 'Z'; letter++) {
                auto drive_root = std::string{} + letter + ":\\";
                if (GetDriveTypeA(drive_root.c_str()) != DRIVE_NO_ROOT_DIR) {
                    drives.push_back(std::string{} + letter + ":");
                }
            }
        }
#else
        drives.push_back("/");
#endif

        auto combo = std::make_unique<Combobox>(drives);
        combo->set_selected(0);
        drive_combo_ = combo.get();
        drive_combo_->on_change = [this](int) {
            auto text = drive_combo_->selected_text();
#if defined(_WIN32)
            current_path_ = text + "\\";
#else
            current_path_ = text;
#endif
            load_directory();
        };
        toolbar->add_widget(std::move(combo));
        toolbar->add_widget(std::make_unique<Label>(), 1);

        auto home_btn = std::make_unique<Button>("");
        home_btn->set_tooltip("Go to Home");
        home_btn->set_icon(Application::instance().load_icon("go-home", 16, "actions"));
        home_btn->on_click = [this]() {
            auto home = std::getenv("HOME");
            if (!home) {
                home = std::getenv("USERPROFILE");
            }
            if (home) {
                current_path_ = home;
                load_directory();
                expand_tree_to_path();
            }
        };
        toolbar->add_widget(std::move(home_btn));

        auto up_btn = std::make_unique<Button>("");
        up_btn->set_tooltip("Up One Level");
        up_btn->set_icon(Application::instance().load_icon("go-up", 16, "actions"));
        up_button_ = up_btn.get();
        up_button_->on_click = [this]() { navigate_up(); };
        toolbar->add_widget(std::move(up_btn));

        auto list_btn = std::make_unique<Button>("");
        list_btn->set_tooltip("List View");
        list_btn->set_icon(
            Application::instance().load_icon("view-list-icons-symbolic", 16, "actions"));
        list_btn->on_click = [this] { set_view_mode(ViewMode::List); };
        toolbar->add_widget(std::move(list_btn));

        auto icon_btn = std::make_unique<Button>("");
        icon_btn->set_tooltip("Icon View");
        icon_btn->set_icon(
            Application::instance().load_icon("view-list-compact-symbolic", 16, "actions"));
        icon_btn->on_click = [this] { set_view_mode(ViewMode::Icons); };
        toolbar->add_widget(std::move(icon_btn));

        auto details_btn = std::make_unique<Button>("");
        details_btn->set_tooltip("Details View");
        details_btn->set_icon(
            Application::instance().load_icon("view-list-details-symbolic", 16, "actions"));
        details_btn->on_click = [this] { set_view_mode(ViewMode::Details); };
        toolbar->add_widget(std::move(details_btn));

        auto tree_btn = std::make_unique<Button>("");
        tree_btn->set_tooltip("Tree View");
        tree_btn->set_icon(Application::instance().load_icon("view-list-tree", 16, "actions"));
        tree_btn->on_click = [this] { set_view_mode(ViewMode::Tree); };
        toolbar->add_widget(std::move(tree_btn));

        auto show_hidden_btn = std::make_unique<Checkbox>("Show Hidden");
        show_hidden_btn->on_toggle = [this](bool checked) {
            show_hidden_ = checked;
            load_directory();
        };
        toolbar->add_widget(std::move(show_hidden_btn));

        add_widget(std::move(toolbar));

        auto path_row = std::make_unique<HBoxLayout>();
        path_row->set_spacing(5);
        path_row->add_widget(std::make_unique<Label>("Path:"), 0);

        auto path_input = std::make_unique<LineInput>();
        path_input->set_text(current_path_);
        path_input_ = path_input.get();
        path_input_->on_submit = [this](std::string const &text, LineInput &) {
            if (std::filesystem::is_directory(text)) {
                current_path_ = text;
                load_directory();
            }
        };
        path_row->add_widget(std::move(path_input), 1);

        add_widget(std::move(path_row));

        auto content = std::make_unique<HBoxLayout>();
        content->set_margins({});

        auto grid = std::make_unique<IconGrid>(model_);
        icon_grid_ = grid.get();
        icon_grid_->set_focusable(true);
        icon_grid_->on_item_activated = [this](size_t index) { activate_directory(index); };
        icon_grid_->on_back_requested = [this]() { navigate_up(); };

        auto table = std::make_unique<TableView>(model_);
        table_view_ = table.get();
        table_view_->set_visible(false);
        table_view_->set_show_header(false);
        table_view_->set_column_width(0, 1000.0f);
        table_view_->on_item_activated = [this](size_t index) { activate_directory(index); };
        table_view_->on_back_requested = [this]() { navigate_up(); };

        auto details = std::make_unique<TableView>(model_);
        table_view_details_ = details.get();
        table_view_details_->set_visible(false);
        table_view_details_->set_show_header(true);
        table_view_details_->set_alternating_row_colors(true);
        table_view_details_->set_column_width(0, 700.0f);
        table_view_details_->set_column_width(1, 300.0f);
        table_view_details_->on_item_activated = [this](size_t index) {
            activate_directory(index);
        };
        table_view_details_->on_back_requested = [this]() { navigate_up(); };

        auto tree =
            std::make_unique<TreeView>(std::make_shared<SimpleTreeModel>(std::vector<TreeNode>{}));
        tree_view_ = tree.get();
        tree_view_->set_visible(false);

        content->add_widget(std::move(grid), 1);
        content->add_widget(std::move(table), 1);
        content->add_widget(std::move(details), 1);
        content->add_widget(std::move(tree), 1);

        add_widget(std::move(content), 1);

        auto button_row = std::make_unique<HBoxLayout>();
        button_row->set_spacing(8);
        button_row->add_widget(std::make_unique<Label>(), 1);

        auto choose_btn = std::make_unique<Button>("Choose");
        choose_btn->on_click = [this] {
            if (on_confirm) {
                on_confirm(current_path_);
            }
        };
        button_row->add_widget(std::move(choose_btn), 0);

        auto cancel_btn = std::make_unique<Button>("Cancel");
        cancel_btn->on_click = [this] {
            if (on_confirm) {
                on_confirm(std::nullopt);
            }
        };
        button_row->add_widget(std::move(cancel_btn), 0);

        add_widget(std::move(button_row));
    }

    void set_view_mode(ViewMode mode) {
        view_mode_ = mode;
        if (icon_grid_) {
            icon_grid_->set_visible(mode == ViewMode::Icons);
        }
        if (table_view_) {
            table_view_->set_visible(mode == ViewMode::List);
        }
        if (table_view_details_) {
            table_view_details_->set_visible(mode == ViewMode::Details);
        }
        if (tree_view_) {
            tree_view_->set_visible(mode == ViewMode::Tree);
        }
        invalidate_layout();
    }

    void navigate_up() {
        if (current_path_ != "/" && current_path_ != "") {
            auto pos = current_path_.rfind('/');
            if (pos != std::string::npos) {
                current_path_ = current_path_.substr(0, pos);
                if (current_path_.empty()) {
                    current_path_ = "/";
                }
                load_directory();
            }
        }
    }

    void activate_directory(size_t index) {
        if (index >= model_->items().size()) {
            return;
        }
        current_path_ = (current_path_ == "/") ? "/" + model_->items()[index].text
                                               : current_path_ + "/" + model_->items()[index].text;
        load_directory();
    }

    void expand_tree_to_path() {
        if (!tree_view_) {
            return;
        }

#if defined(_WIN32)
        auto root = "C:\\";
#else
        auto root = "/";
#endif

        auto tree_nodes = build_tree_nodes(root, 0, show_hidden_);
        collapse_all_nodes(tree_nodes);
        expand_path_in_tree(tree_nodes, current_path_);
        auto tree_model = std::make_shared<SimpleTreeModel>(tree_nodes);
        tree_view_->set_model(tree_model);
    }

    void load_directory() {
        auto items = std::vector<DirItem>{};

        try {
            for (auto const &entry : std::filesystem::directory_iterator(current_path_)) {
                auto path = entry.path();
                if (std::filesystem::is_symlink(path)) {
                    try {
                        path = std::filesystem::read_symlink(path);
                    } catch (...) {
                        continue;
                    }
                }
                if (!std::filesystem::is_directory(path)) {
                    continue;
                }
                auto name = entry.path().filename().string();
                if (name == "." || name == "..") {
                    continue;
                }
                if (!show_hidden_ && is_hidden(name, entry.path().string())) {
                    continue;
                }
                try {
                    auto mtime = std::filesystem::last_write_time(entry.path());
                    items.push_back({name, XDG::IconMimeTypes::inodeDirectory,
                                     XDG::IconContexts::places, mtime});
                } catch (...) {
                    items.push_back({name, XDG::IconMimeTypes::inodeDirectory,
                                     XDG::IconContexts::places, std::filesystem::file_time_type()});
                }
            }
        } catch (...) {
            spdlog::warn("Failed to read directory: {}", current_path_);
        }

        std::sort(items.begin(), items.end(),
                  [](auto const &a, auto const &b) { return a.text < b.text; });

        auto grid_items = std::vector<StandardIconItem>{};
        grid_items.reserve(items.size());
        for (auto const &item : items) {
            grid_items.push_back({item.text, item.text, item.icon_name, item.icon_category});
        }
        model_->set_items(std::move(items));
        path_input_->set_text(current_path_);

        // Update tree view with directory hierarchy
        if (tree_view_) {
            auto tree_nodes = build_tree_nodes(current_path_, 0, show_hidden_);
            auto tree_model = std::make_shared<SimpleTreeModel>(tree_nodes);
            tree_view_->set_model(tree_model);
        }

        invalidate_layout();
    }

    std::shared_ptr<DirListModel> model_;
    Combobox *drive_combo_ = nullptr;
    Button *up_button_ = nullptr;
    IconGrid *icon_grid_ = nullptr;
    TableView *table_view_ = nullptr;
    TableView *table_view_details_ = nullptr;
    TreeView *tree_view_ = nullptr;
    LineInput *path_input_ = nullptr;
    ViewMode view_mode_ = ViewMode::Icons;
    bool show_hidden_ = false;
    std::string start_path_;
    std::string current_path_;
};

} // namespace

DirectoryDialog::DirectoryDialog(Window *parent)
    : parent_(parent), title_("Choose Directory"), start_path_("/"), use_native_(true) {}

DirectoryDialog &DirectoryDialog::title(std::string_view t) {
    title_ = std::string(t);
    return *this;
}

DirectoryDialog &DirectoryDialog::start_path(std::string_view path) {
    start_path_ = std::string(path);
    return *this;
}

DirectoryDialog &DirectoryDialog::use_native(bool v) {
    use_native_ = v;
    return *this;
}

DirectoryDialog::Future DirectoryDialog::choose() { return show(use_native_); }

DirectoryDialog::Future DirectoryDialog::show(bool use_native) {
    if (use_native) {
        return show_native();
    } else {
        return show_toolkit();
    }
}

DirectoryDialog::Future DirectoryDialog::show_native() {
    auto callback = std::make_shared<Callback>();
    auto start = start_path_;

    Application::post_to_main_thread([callback, start] {
        auto const *default_path = start.empty() ? nullptr : start.c_str();

        NFD_Init();
        nfdu8char_t *out_path = nullptr;
        nfdresult_t result = NFD_PickFolderU8(&out_path, default_path);

        Result path;
        if (result == NFD_OKAY && out_path) {
            path = std::string{out_path};
            NFD_FreePathU8(out_path);
        } else if (result != NFD_CANCEL) {
            spdlog::error("NFD error: {}", NFD_GetError());
        }
        NFD_Quit();

        if (*callback) {
            (*callback)(std::move(path));
        }
    });

    return Future{callback};
}

DirectoryDialog::Future DirectoryDialog::show_toolkit() {
    auto callback = std::make_shared<Callback>();
    auto settled = std::make_shared<bool>(false);
    auto title = title_;
    auto start = start_path_;
    auto parent = parent_;

    auto widget = std::make_unique<DirChooserWidget>(start);
    auto widget_ptr = widget.get();
    auto win = Application::instance().create_window(title, {400, 150});
    // on_confirm lives on the widget the window owns, so it takes a weak
    // reference back -- a shared one would be a cycle.
    auto weak_win = std::weak_ptr<Window>(win);

    widget_ptr->on_confirm = [callback, settled, weak_win](std::optional<std::string> result) {
        if (*settled) {
            return;
        }
        *settled = true;
        if (auto win = weak_win.lock()) {
            win->close();
        }
        if (*callback) {
            (*callback)(result);
        }
    };

    win->set_root(std::move(widget));
    win->on_key = [widget_ptr](KeyEvent const &event) {
        if (event.type == KeyEvent::Type::Press && event.key == Key::Escape) {
            widget_ptr->on_confirm(std::nullopt);
            return true;
        }
        return false;
    };
    if (parent && parent->platform_window()) {
        win->platform_window()->set_modal_for(parent->platform_window());
    }
    win->show();
    win->resize_to_fit();
    win->relayout();

    return Future{callback};
}

} // namespace svision3
