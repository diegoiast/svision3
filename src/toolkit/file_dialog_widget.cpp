// SPDX-License-Identifier: MITP
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/file_dialog_widget.hpp"
#include "toolkit/application.hpp"
#include <chrono>
#include <ctime>
#include <filesystem>

#if defined(_WIN32)
#include <algorithm>
#include <windows.h>
#else
#include <algorithm>
#include <cstdlib>
#include <dirent.h>
#include <sys/types.h>
#include <vector>
#endif

#include <string>

namespace toolkit {

namespace {

struct Spacer : Widget {
    void paint(Painter &) override {}
    bool handle_mouse(MouseEvent const &) override { return false; }
};

struct DirItem {
    std::string text;
    std::string icon_name;
    std::string icon_category;
    bool is_dir;
    std::filesystem::file_time_type mtime;
    std::uintmax_t size = 0;
    mutable Icon cached_icon;
    mutable int cached_icon_size = -1;
};

static std::string format_size(std::uintmax_t bytes) {
    if (bytes < 1024) {
        return std::to_string(bytes) + " B";
    }
    if (bytes < 1024 * 1024) {
        return std::to_string(bytes / 1024) + " KB";
    }
    if (bytes < 1024 * 1024 * 1024) {
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    }
    return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
}

static std::string format_mtime(std::filesystem::file_time_type t) {
    auto sys = std::chrono::file_clock::to_sys(t);
    auto tt = std::chrono::system_clock::to_time_t(sys);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&tt));
    return buf;
}

class DirDetailsModel : public ItemModel {
  public:
    size_t row_count() const override { return items_.size(); }
    size_t column_count() const override { return 3; }

    std::string cell_text(size_t row, size_t col) const override {
        if (row >= items_.size()) {
            return {};
        }
        switch (col) {
        case 0:
            return items_[row].text;
        case 1:
            return items_[row].is_dir ? "--" : format_size(items_[row].size);
        case 2:
            return format_mtime(items_[row].mtime);
        default:
            return {};
        }
    }

    std::string header_text(size_t col) const override {
        switch (col) {
        case 0:
            return "Name";
        case 1:
            return "Size";
        case 2:
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

std::vector<DirItem> scan_directory(std::string const &path_str, FileDialogWidget::SortOrder order,
                                    bool dirs_first, bool show_hidden) {
    auto items = std::vector<DirItem>{};
    std::filesystem::path path(path_str);

    try {
        for (auto const &entry : std::filesystem::directory_iterator(path)) {
            auto name = entry.path().filename().string();
            if (name == "." || name == "..") {
                continue;
            }

            if (!show_hidden && name.starts_with('.')) {
                continue;
            }

            auto is_dir = entry.is_directory();
            auto icon_name =
                is_dir ? XDG::IconMimeTypes::inodeDirectory : XDG::IconMimeTypes::textXGeneric;
            auto icon_cat = is_dir ? XDG::IconContexts::places : XDG::IconContexts::mimeTypes;

            std::uintmax_t sz = is_dir ? 0 : entry.file_size();
            items.push_back(
                DirItem{name, icon_name, icon_cat, is_dir, entry.last_write_time(), sz});
        }
    } catch (...) {
        return items;
    }

    std::sort(items.begin(), items.end(), [order, dirs_first](auto const &a, auto const &b) {
        if (dirs_first && a.is_dir != b.is_dir) {
            return a.is_dir;
        }

        if (order == FileDialogWidget::SortOrder::Time) {
            if (a.mtime != b.mtime) {
                return a.mtime > b.mtime;
            }
        }

        return a.text < b.text;
    });

    return items;
}

} // namespace

FileDialogWidget::FileDialogWidget() {
    model_ = std::make_shared<StandardIconModel>();
    details_model_ = std::make_shared<DirDetailsModel>();
    state.non_focus_input = true;
#if defined(_WIN32)
    current_path_ = "C:\\";
#else
    current_path_ = "/";
#endif
    setup_ui();
}

FileDialogWidget &FileDialogWidget::set_sort_order(SortOrder order) {
    if (sort_order_ != order) {
        sort_order_ = order;
        load_directory();
    }
    return *this;
}

FileDialogWidget &FileDialogWidget::set_dirs_first(bool dirs_first) {
    if (dirs_first_ != dirs_first) {
        dirs_first_ = dirs_first;
        load_directory();
    }
    return *this;
}

FileDialogWidget &FileDialogWidget::set_show_hidden(bool show_hidden) {
    if (show_hidden_ != show_hidden) {
        show_hidden_ = show_hidden;
        load_directory();
    }
    return *this;
}

void FileDialogWidget::set_current_path(std::string path) {
    current_path_ = std::move(path);
    load_directory();
}

std::string FileDialogWidget::selected_path() const {
    return (std::filesystem::path(current_path_) / path_input_->text()).string();
}

void FileDialogWidget::set_ok_label(std::string_view label) {
    ok_button_->set_text(std::string{label});
}

bool FileDialogWidget::handle_key(KeyEvent const &event) {
    if (event.type == KeyEvent::Type::Press && event.key == Key::Escape) {
        if (on_cancel) {
            Application::post_to_main_thread(on_cancel);
        }
        return true;
    }
    return VBoxLayout::handle_key(event);
}

void FileDialogWidget::load_directory() {
    auto items = scan_directory(current_path_, sort_order_, dirs_first_, show_hidden_);

    if (items.empty()) {
        return;
    }

    path_input_->set_text(current_path_);

    auto grid_items = std::vector<StandardIconItem>{};
    grid_items.reserve(items.size());
    for (auto const &item : items) {
        grid_items.push_back({item.text, item.text, item.icon_name, item.icon_category});
    }
    model_->set_items(std::move(grid_items));
    static_cast<DirDetailsModel *>(details_model_.get())->set_items(std::move(items));
    invalidate_layout();
}

void FileDialogWidget::setup_ui() {
    auto &app = Application::instance();
    set_spacing(10);
    set_margins({10, 10, 10, 10});

    toolbar_ = std::make_unique<HBoxLayout>();
    toolbar_->set_margins({});
    toolbar_->set_spacing(5);

    toolbar_->add_widget(std::make_unique<Label>("Look in:"));

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
        set_current_path(text + "\\");
#else
        set_current_path(text);
#endif
    };
    toolbar_->add_widget(std::move(combo));
    toolbar_->add_widget(std::make_unique<Spacer>(), 1);

    auto up_btn = std::make_unique<Button>("");
    up_btn->set_tooltip("Up One Level");
    up_btn->set_icon(app.load_icon("go-up", 16, "actions"));
    up_button_ = up_btn.get();
    up_button_->on_click = [this]() {
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
    };
    toolbar_->add_widget(std::move(up_btn));

    auto new_btn = std::make_unique<Button>("");
    new_btn->set_tooltip("Create New Folder");
    new_btn->set_icon(app.load_icon(XDG::IconActions::folderNew, 16, XDG::IconContexts::actions));
    new_button_ = new_btn.get();
    new_button_->on_click = [this]() {
        if (on_new) {
            on_new();
        }
    };
    toolbar_->add_widget(std::move(new_btn));

    auto list_btn = std::make_unique<Button>("");
    list_btn->set_tooltip("List View");
    list_btn->set_icon(app.load_icon("view-list-icons-symbolic", 16, XDG::IconContexts::actions));
    list_view_btn_ = list_btn.get();
    list_view_btn_->on_click = [this] {
        view_mode_ = ViewMode::List;
        if (icon_grid_) {
            icon_grid_->set_visible(false);
        }
        if (table_view_) {
            table_view_->set_visible(true);
            table_view_->set_model(model_);
            table_view_->auto_fit_columns();
        }
        if (table_view_details_) {
            table_view_details_->set_visible(false);
        }
        this->invalidate_layout();
    };
    toolbar_->add_widget(std::move(list_btn));

    auto icon_btn = std::make_unique<Button>("");
    icon_btn->set_tooltip("Icon View");
    icon_btn->set_icon(app.load_icon("view-list-compact-symbolic", 16, XDG::IconContexts::actions));
    icon_view_btn_ = icon_btn.get();
    icon_view_btn_->on_click = [this] {
        view_mode_ = ViewMode::Icons;
        if (icon_grid_) {
            icon_grid_->set_visible(true);
            icon_grid_->set_model(model_);
        }
        if (table_view_) {
            table_view_->set_visible(false);
        }
        if (table_view_details_) {
            table_view_details_->set_visible(false);
        }
        this->invalidate_layout();
    };

    toolbar_->add_widget(std::move(icon_btn));
    auto details_btn = std::make_unique<Button>("");
    details_btn->set_tooltip("Details View");
    details_btn->set_icon(
        app.load_icon("view-list-details-symbolic", 16, XDG::IconContexts::actions));
    details_view_btn_ = details_btn.get();
    details_view_btn_->on_click = [this] {
        view_mode_ = ViewMode::Details;
        if (icon_grid_) {
            icon_grid_->set_visible(false);
        }
        if (table_view_) {
            table_view_->set_visible(false);
        }
        if (table_view_details_) {
            table_view_details_->set_visible(true);
            table_view_details_->auto_fit_columns();
        }
        this->invalidate_layout();
    };
    toolbar_->add_widget(std::move(details_btn));

    auto show_hidden_btn = std::make_unique<Button>("H");
    show_hidden_btn->set_tooltip("Toggle Show Hidden Files");
    show_hidden_btn->on_click = [this] { set_show_hidden(!show_hidden()); };
    toolbar_->add_widget(std::move(show_hidden_btn));

    add_widget(std::move(toolbar_));

    // ── Center: File Area ─────────────────────────────────────────────
    content_ = std::make_unique<VBoxLayout>();
    content_->set_margins({});

    auto grid = std::make_unique<IconGrid>(model_);
    icon_grid_ = grid.get();
    icon_grid_->set_focusable(true);

    auto table = std::make_unique<TableView>(model_);
    table_view_ = table.get();
    table_view_->set_visible(false);
    table_view_->set_show_header(false);
    table_view_->auto_fit_columns();
    table_view_->on_item_activated = [this](size_t index) {
        auto items = scan_directory(current_path_, sort_order_, dirs_first_, show_hidden_);
        if (index >= items.size()) {
            return;
        }
        auto &item = items[index];
        if (item.is_dir) {
            current_path_ =
                (current_path_ == "/") ? "/" + item.text : current_path_ + "/" + item.text;
            load_directory();
        } else {
            path_input_->set_text(item.text);
            if (on_ok) {
                on_ok();
            }
        }
    };
    table_view_->on_selection_changed = [this](std::optional<size_t> idx) {
        if (idx) {
            auto items = scan_directory(current_path_, sort_order_, dirs_first_, show_hidden_);
            if (*idx < items.size() && !items[*idx].is_dir) {
                path_input_->set_text(items[*idx].text);
            }
        }
    };

    icon_grid_->on_item_activated = [this](size_t index) {
        auto items = scan_directory(current_path_, sort_order_, dirs_first_, show_hidden_);
        if (index >= items.size()) {
            return;
        }
        auto &item = items[index];
        if (item.is_dir) {
            current_path_ =
                (current_path_ == "/") ? "/" + item.text : current_path_ + "/" + item.text;
            load_directory();
        } else {
            path_input_->set_text(item.text);
            if (on_ok) {
                on_ok();
            }
        }
    };
    icon_grid_->on_selection_changed = [this](std::set<size_t> const &indices) {
        if (!indices.empty()) {
            auto items = scan_directory(current_path_, sort_order_, dirs_first_, show_hidden_);
            auto idx = *indices.begin();
            if (idx < items.size() && !items[idx].is_dir) {
                path_input_->set_text(items[idx].text);
            }
        }
    };
    icon_grid_->on_back_requested = [this]() { up_button_->on_click(); };
    table_view_->on_back_requested = [this]() { up_button_->on_click(); };

    auto details = std::make_unique<TableView>(details_model_);
    table_view_details_ = details.get();
    table_view_details_->set_visible(false);
    table_view_details_->set_show_header(true);
    table_view_details_->set_alternating_row_colors(true);
    table_view_details_->auto_fit_columns();
    table_view_details_->on_item_activated = [this](size_t index) {
        auto *m = static_cast<DirDetailsModel *>(details_model_.get());
        auto const &items = m->items();
        if (index >= items.size()) {
            return;
        }
        auto const &item = items[index];
        if (item.is_dir) {
            current_path_ =
                (current_path_ == "/") ? "/" + item.text : current_path_ + "/" + item.text;
            load_directory();
        } else {
            path_input_->set_text(item.text);
            if (on_ok) {
                on_ok();
            }
        }
    };
    table_view_details_->on_selection_changed = [this](std::optional<size_t> idx) {
        if (!idx) {
            return;
        }
        auto *m = static_cast<DirDetailsModel *>(details_model_.get());
        auto const &items = m->items();
        if (*idx < items.size() && !items[*idx].is_dir) {
            path_input_->set_text(items[*idx].text);
        }
    };
    table_view_details_->on_back_requested = [this]() { up_button_->on_click(); };

    content_->add_widget(std::move(grid), 1);
    content_->add_widget(std::move(table), 1);
    content_->add_widget(std::move(details), 1);

    add_widget(std::move(content_), 1);

    // ── Bottom: Filename and Filter ───────────────────────────────────
    bottom_controls_ = std::make_unique<VBoxLayout>();
    bottom_controls_->set_margins({});
    bottom_controls_->set_spacing(4);

    auto const label_min = Size{100, 0};
    auto const button_min = Size{80, 0};

    // Row 1: File name label | input | Open button
    auto name_row = std::make_unique<HBoxLayout>();
    name_row->set_spacing(8);
    auto name_label = std::make_unique<Label>("File name:");
    name_label->set_min_size(label_min);
    name_row->add_widget(std::move(name_label), 0, Alignment::Center);

    auto path_in = std::make_unique<LineInput>();
    path_input_ = path_in.get();
    path_input_->on_submit = [this](std::string const &, LineInput &) {
        if (on_ok) {
            on_ok();
        }
    };
    name_row->add_widget(std::move(path_in), 1);

    auto open_btn = std::make_unique<Button>("Open");
    ok_button_ = open_btn.get();
    ok_button_->set_min_size(button_min);
    ok_button_->on_click = [this] {
        if (on_ok) {
            on_ok();
        }
    };
    name_row->add_widget(std::move(open_btn), 0);
    bottom_controls_->add_widget(std::move(name_row));

    // Row 2: Files of type label | combo | Cancel button
    auto type_row = std::make_unique<HBoxLayout>();
    type_row->set_spacing(8);
    auto type_label = std::make_unique<Label>("Files of type:");
    type_label->set_min_size(label_min);
    type_row->add_widget(std::move(type_label), 0, Alignment::Center);

    auto ext_combo = std::make_unique<Combobox>(std::vector<std::string>{"All Files (*.*)"});
    extension_combo_ = ext_combo.get();
    type_row->add_widget(std::move(ext_combo), 1);

    auto cancel_btn = std::make_unique<Button>("Cancel");
    cancel_button_ = cancel_btn.get();
    cancel_button_->set_min_size(button_min);
    cancel_button_->on_click = [this] {
        if (on_cancel) {
            on_cancel();
        }
    };
    type_row->add_widget(std::move(cancel_btn), 0);
    bottom_controls_->add_widget(std::move(type_row));

    add_widget(std::move(bottom_controls_));
}

} // namespace toolkit
