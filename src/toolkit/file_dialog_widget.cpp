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

std::vector<DirItem> scan_directory(std::string const &path_str, FileDialogWidget::SortOrder order, bool dirs_first) {
    auto items = std::vector<DirItem>{};
    std::filesystem::path path(path_str);

    try {
        for (auto const &entry : std::filesystem::directory_iterator(path)) {
            auto name = entry.path().filename().string();
            if (name == "." || name == "..") {
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

void FileDialogWidget::set_current_path(std::string path) {
    current_path_ = std::move(path);
    load_directory();
}

void FileDialogWidget::load_directory() {
    auto items = scan_directory(current_path_, sort_order_, dirs_first_);

    if (items.empty()) {
        return;
    }

    path_input_->set_text(current_path_);

    auto grid_items = std::vector<StandardIconItem>{};
    auto &app = Application::instance();
    grid_items.reserve(items.size());

    for (auto const &item : items) {
        auto icon = app.load_icon(item.icon_name, 48, item.icon_category);
        grid_items.push_back({item.text, item.text, item.icon_name, item.icon_category});
    }

    model_->set_items(std::move(grid_items));

    if (window_) {
        window_->request_redraw("file_dialog_load");
    }
}

void FileDialogWidget::setup_ui() {
    toolbar_ = std::make_unique<HBoxLayout>();
    toolbar_->set_margins({});

    std::vector<std::string> drives = {"/"};
#if defined(_WIN32)
    char drives_buf[128];
    DWORD size = sizeof(drives_buf);
    if (GetLogicalDrives() > 0) {
        for (char letter = 'A'; letter <= 'Z'; letter++) {
            auto drive_letter = std::string{} + letter + ":";
            auto drive_type = GetDriveTypeA(drive_letter.c_str());
            if (drive_type != DRIVE_NO根_DIR) {
                drives.push_back(drive_letter);
            }
        }
    }
#else
    // FIXME: this would be great to find "mounts" instead of drives here
#endif

    auto combo = std::make_unique<Combobox>(drives);
    combo->set_selected(0);
    drive_combo_ = combo.get();
#if !defined(_WIN32)
    drive_combo_->set_enabled(false);
#endif
    toolbar_->add_widget(std::move(combo));

    auto path = std::make_unique<LineInput>();
    path_input_ = path.get();
    path_input_->set_text(current_path_);
    path_input_->on_submit = [this](std::string const &, LineInput &) {
        set_current_path(path_input_->text());
    };
    toolbar_->add_widget(std::move(path), 1);

    auto up_btn = std::make_unique<Button>("Up");
    up_btn->set_tooltip("Go to parent directory");
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

    auto new_btn = std::make_unique<Button>("New");
    new_btn->set_tooltip("Create new folder");
    new_button_ = new_btn.get();
    new_button_->on_click = [this]() {
        if (on_new) {
            on_new();
        }
    };
    toolbar_->add_widget(std::move(new_btn));

    auto dirs_first_btn = std::make_unique<Button>("D");
    dirs_first_btn->set_tooltip("Toggle Dirs First");
    dirs_first_btn->on_click = [this] {
        set_dirs_first(!dirs_first());
    };
    toolbar_->add_widget(std::move(dirs_first_btn));

    auto sort_time_btn = std::make_unique<Button>("T");
    sort_time_btn->set_tooltip("Toggle Sort by Time/Name");
    sort_time_btn->on_click = [this] {
        set_sort_order(sort_order() == SortOrder::Name ? SortOrder::Time : SortOrder::Name);
    };
    toolbar_->add_widget(std::move(sort_time_btn));

    add_widget(std::move(toolbar_));

    content_ = std::make_unique<VBoxLayout>();
    content_->set_margins({});

    auto grid = std::make_unique<IconGrid>(model_);
    icon_grid_ = grid.get();
    icon_grid_->set_focusable(true);
    icon_grid_->on_item_activated = [this](size_t index) {
        auto items = scan_directory(current_path_, sort_order_, dirs_first_);
        if (index >= items.size()) {
            return;
        }
        auto &item = items[index];
        if (item.is_dir) {
            if (current_path_ == "/") {
                current_path_ = "/" + item.text;
            } else {
                current_path_ = current_path_ + "/" + item.text;
            }
            load_directory();
        }
    };
    icon_grid_->on_back_requested = [this]() {
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
    content_->add_widget(std::move(grid), 1);

    add_widget(std::move(content_), 1);

    button_bar_ = std::make_unique<HBoxLayout>();
    button_bar_->set_margins({});

    button_bar_->add_widget(std::make_unique<Label>(""), 1);

    auto cancel_btn = std::make_unique<Button>("Cancel");
    cancel_button_ = cancel_btn.get();
    cancel_button_->on_click = [this]() {
        if (on_cancel) {
            on_cancel();
        }
    };
    button_bar_->add_widget(std::move(cancel_btn));

    auto ok_btn = std::make_unique<Button>("OK");
    ok_button_ = ok_btn.get();
    ok_button_->on_click = [this]() {
        if (on_ok) {
            on_ok();
        }
    };
    button_bar_->add_widget(std::move(ok_btn));

    add_widget(std::move(button_bar_));
}

} // namespace toolkit
