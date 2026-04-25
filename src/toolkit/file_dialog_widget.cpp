// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/file_dialog_widget.hpp"
#include "toolkit/application.hpp"
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

struct DirItem {
    std::string text;
    std::string icon_name;
    std::string icon_category;
    bool is_dir;
    std::filesystem::file_time_type mtime;
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

            items.push_back(DirItem{name, icon_name, icon_cat, is_dir, entry.last_write_time()});
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

    std::vector<std::string> drives = {"/"};
#if defined(_WIN32)
    if (GetLogicalDrives() > 0) {
        for (char letter = 'A'; letter <= 'Z'; letter++) {
            auto drive_letter = std::string{} + letter + ":\\";
            auto drive_type = GetDriveTypeA(drive_letter.c_str());
            if (drive_type != DRIVE_NO_ROOT_DIR) {
                drives.push_back(drive_letter);
            }
        }
    }
#endif

    auto combo = std::make_unique<Combobox>(drives);
    combo->set_selected(0);
    drive_combo_ = combo.get();
    drive_combo_->set_enabled(false);
    toolbar_->add_widget(std::move(combo), 1);

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
    new_btn->set_icon(app.load_icon("folder-new", 16, "actions"));
    new_button_ = new_btn.get();
    new_button_->on_click = [this]() {
        if (on_new) {
            on_new();
        }
    };
    toolbar_->add_widget(std::move(new_btn));

    auto list_btn = std::make_unique<Button>("List");
    list_btn->set_tooltip("List View");
    list_btn->set_icon(app.load_icon("view-list-compact", 16, "actions"));
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
        this->invalidate_layout();
    };
    toolbar_->add_widget(std::move(list_btn));

    auto icon_btn = std::make_unique<Button>("");
    icon_btn->set_tooltip("Icon View");
    icon_btn->set_icon(app.load_icon("view-grid", 16, "actions"));
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
        this->invalidate_layout();
    };
    toolbar_->add_widget(std::move(icon_btn));

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

    content_->add_widget(std::move(grid), 1);
    content_->add_widget(std::move(table), 1);

    add_widget(std::move(content_), 1);

    // ── Bottom: Filename and Filter ───────────────────────────────────
    bottom_controls_ = std::make_unique<VBoxLayout>();
    bottom_controls_->set_margins({});
    bottom_controls_->set_spacing(5);

    auto name_row = std::make_unique<HBoxLayout>();
    name_row->set_spacing(10);
    name_row->add_widget(std::make_unique<Label>("File name:"), 0, Alignment::Start);
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
    ok_button_->on_click = [this] {
        if (on_ok) {
            on_ok();
        }
    };
    name_row->add_widget(std::move(open_btn), 0);
    bottom_controls_->add_widget(std::move(name_row));

    auto type_row = std::make_unique<HBoxLayout>();
    type_row->set_spacing(10);
    type_row->add_widget(std::make_unique<Label>("Files of type:"), 0, Alignment::Start);
    auto ext_combo = std::make_unique<Combobox>(std::vector<std::string>{"All Files (*.*)"});
    extension_combo_ = ext_combo.get();
    type_row->add_widget(std::move(ext_combo), 1);

    auto cancel_btn = std::make_unique<Button>("Cancel");
    cancel_button_ = cancel_btn.get();
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
