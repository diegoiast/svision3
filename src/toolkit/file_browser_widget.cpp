// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/file_browser_widget.hpp"
#include "toolkit/application.hpp"
#include "toolkit/menu.hpp"
#include "toolkit/types.hpp"
#include "toolkit/xdg_icons.hpp"
#include <algorithm>
#include <filesystem>
#include <set>

#if defined(_WIN32)
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace toolkit {

namespace {

struct DirEntry {
    std::string name;
    bool is_dir = false;
    std::uintmax_t size = 0;
    std::filesystem::file_time_type mtime;
    mutable Icon cached_icon;
    mutable int cached_icon_size = -1;
};

class DirModel : public ItemModel {
  public:
    void set_detail_mode(bool d) { detail_mode_ = d; }

    size_t row_count() const override { return entries_.size(); }
    size_t column_count() const override { return detail_mode_ ? 3 : 1; }

    std::string cell_text(size_t row, size_t col) const override {
        if (row >= entries_.size()) {
            return {};
        }
        auto const &e = entries_[row];
        switch (col) {
        case 0:
            return e.name;
        case 1:
            return e.is_dir ? "" : format_size(e.size);
        case 2:
            return format_mtime(e.mtime);
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
        if (col != 0 || row >= entries_.size()) {
            return nullptr;
        }
        auto const &e = entries_[row];
        if (e.cached_icon && e.cached_icon_size == size) {
            return e.cached_icon;
        }
        auto icon_name =
            e.is_dir ? XDG::IconMimeTypes::inodeDirectory : XDG::IconMimeTypes::textXGeneric;
        e.cached_icon =
            Application::instance().load_icon(icon_name, size, XDG::IconContexts::mimeTypes);
        e.cached_icon_size = size;
        return e.cached_icon;
    }

    void set_entries(std::vector<DirEntry> entries) {
        entries_ = std::move(entries);
        if (on_data_changed) {
            on_data_changed();
        }
    }

    std::vector<DirEntry> const &entries() const { return entries_; }

  private:
    std::vector<DirEntry> entries_;
    bool detail_mode_ = false;
};

static std::string home_path() {
#if defined(_WIN32)
    const char *h = std::getenv("USERPROFILE");
    return h ? h : "C:\\";
#else
    const char *h = std::getenv("HOME");
    return h ? h : "/";
#endif
}

static std::vector<DirEntry> scan_dir(std::string const &path_str,
                                      FileBrowserWidget::SortOrder sort_order, bool dirs_first,
                                      bool show_hidden) {
    auto entries = std::vector<DirEntry>{};
    try {
        for (auto const &e : std::filesystem::directory_iterator(path_str)) {
            auto name = e.path().filename().string();
            if (name == "." || name == "..") {
                continue;
            }
            if (!show_hidden && name.starts_with('.')) {
                continue;
            }
            auto is_dir = e.is_directory();
            std::uintmax_t sz = is_dir ? 0 : e.file_size();
            entries.push_back({name, is_dir, sz, e.last_write_time(), {}, -1});
        }
    } catch (...) {
    }

    std::sort(entries.begin(), entries.end(),
              [sort_order, dirs_first](DirEntry const &a, DirEntry const &b) {
                  if (dirs_first && a.is_dir != b.is_dir) {
                      return a.is_dir > b.is_dir;
                  }
                  if (sort_order == FileBrowserWidget::SortOrder::Time) {
                      if (a.mtime != b.mtime) {
                          return a.mtime > b.mtime;
                      }
                  }
                  return a.name < b.name;
              });
    return entries;
}

} // namespace

FileBrowserWidget::FileBrowserWidget() {
    model_ = std::make_shared<DirModel>();
    state.non_focus_input = true;
#if defined(_WIN32)
    current_path_ = "C:\\";
#else
    current_path_ = "/";
#endif
    setup_ui();
}

FileBrowserWidget &FileBrowserWidget::set_sort_order(SortOrder order) {
    if (sort_order_ != order) {
        sort_order_ = order;
        load_directory();
    }
    return *this;
}

FileBrowserWidget &FileBrowserWidget::set_dirs_first(bool dirs_first) {
    if (dirs_first_ != dirs_first) {
        dirs_first_ = dirs_first;
        load_directory();
    }
    return *this;
}

FileBrowserWidget &FileBrowserWidget::set_show_hidden(bool show_hidden) {
    if (show_hidden_ != show_hidden) {
        show_hidden_ = show_hidden;
        load_directory();
    }
    return *this;
}

FileBrowserWidget &FileBrowserWidget::set_file_must_exist(bool must_exist) {
    file_must_exist_ = must_exist;
    return *this;
}

void FileBrowserWidget::set_current_path(std::string path) {
    if (path == current_path_) {
        return;
    }
    push_history(current_path_);
    forward_stack_.clear();
    current_path_ = std::move(path);
    load_directory();
}

void FileBrowserWidget::navigate_to(std::string path) { set_current_path(std::move(path)); }

void FileBrowserWidget::navigate_home() { set_current_path(home_path()); }

FileBrowserWidget &FileBrowserWidget::set_view_mode(ViewMode mode) {
    view_mode_ = mode;
    if (stacked_) {
        auto *m = static_cast<DirModel *>(model_.get());
        m->set_detail_mode(mode == ViewMode::Details);
        stacked_->set_current(static_cast<int>(mode));
        if (m->on_data_changed) {
            m->on_data_changed();
        }
    }
    return *this;
}

FileBrowserWidget &FileBrowserWidget::set_browser_mode(bool browser) {
    browser_mode_ = browser;
    auto page = browser ? 0 : 1;
    if (toolbar_extras_) {
        toolbar_extras_->set_current(page);
    }
    if (bottom_stack_) {
        bottom_stack_->set_current(page);
    }
    return *this;
}

void FileBrowserWidget::set_filename(std::string_view name) {
    if (path_input_) {
        path_input_->set_text(std::string{name});
    }
}

std::string FileBrowserWidget::selected_path() const {
    if (path_input_) {
        return (std::filesystem::path(current_path_) / path_input_->text()).string();
    }
    return current_path_;
}

void FileBrowserWidget::set_ok_label(std::string_view label) {
    if (ok_button_) {
        ok_button_->set_text(std::string{label});
    }
}

bool FileBrowserWidget::handle_key(KeyEvent const &event) {
    if (event.type == KeyEvent::Type::Press) {
        if (event.key == Key::Escape) {
            if (on_cancel) {
                Application::post_to_main_thread(on_cancel);
            }
            return true;
        }
        if ((event.key == Key::Up && event.alt) ||
            (event.key == Key::Backspace && path_input_ && !path_input_->is_focused())) {
            navigate_up();
            return true;
        }
    }
    return VBoxLayout::handle_key(event);
}

void FileBrowserWidget::load_directory() {
    auto *m = static_cast<DirModel *>(model_.get());
    m->set_detail_mode(view_mode_ == ViewMode::Details);
    m->set_entries(scan_dir(current_path_, sort_order_, dirs_first_, show_hidden_));
    if (path_input_) {
        path_input_->set_text(current_path_);
    }
    update_buttons();
    if (on_path_changed) {
        on_path_changed(current_path_);
    }
}

void FileBrowserWidget::navigate_up() {
    std::filesystem::path p(current_path_);
    if (p.has_parent_path() && p.parent_path() != p) {
        push_history(current_path_);
        forward_stack_.clear();
        current_path_ = p.parent_path().string();
        load_directory();
        if (on_up) {
            on_up();
        }
    }
}

void FileBrowserWidget::navigate_back() {
    if (back_stack_.empty()) {
        return;
    }
    forward_stack_.push_back(current_path_);
    current_path_ = back_stack_.back();
    back_stack_.pop_back();
    load_directory();
}

void FileBrowserWidget::navigate_forward() {
    if (forward_stack_.empty()) {
        return;
    }
    back_stack_.push_back(current_path_);
    current_path_ = forward_stack_.back();
    forward_stack_.pop_back();
    load_directory();
}

void FileBrowserWidget::push_history(std::string path) {
    back_stack_.push_back(std::move(path));
    if (back_stack_.size() > 50) {
        back_stack_.erase(back_stack_.begin());
    }
}

void FileBrowserWidget::update_buttons() {
    if (back_btn_) {
        back_btn_->set_enabled(!back_stack_.empty());
    }
    if (forward_btn_) {
        forward_btn_->set_enabled(!forward_stack_.empty());
    }
    if (up_button_) {
        std::filesystem::path p(current_path_);
        up_button_->set_enabled(p.has_parent_path() && p.parent_path() != p);
    }
}

void FileBrowserWidget::on_activated(size_t index) {
    auto *m = static_cast<DirModel const *>(model_.get());
    auto const &entries = m->entries();
    if (index >= entries.size()) {
        return;
    }
    auto const &e = entries[index];
    auto full = (std::filesystem::path(current_path_) / e.name).string();
    if (e.is_dir) {
        push_history(current_path_);
        forward_stack_.clear();
        current_path_ = full;
        load_directory();
    } else {
        if (on_file_activated) {
            on_file_activated(full);
        }
        if (!browser_mode_) {
            if (path_input_) {
                path_input_->set_text(e.name);
            }
            if (on_ok) {
                on_ok();
            }
        }
    }
}

void FileBrowserWidget::setup_ui() {
    auto &app = Application::instance();
    set_spacing(10);
    set_margins({10, 10, 10, 10});

    // ── Toolbar ──────────────────────────────────────────────────────────────
    auto toolbar = std::make_unique<HBoxLayout>();
    toolbar->set_margins({});
    toolbar->set_spacing(5);

    auto make_icon_btn = [&](std::string_view icon_name, std::string_view fallback) {
        auto btn = std::make_unique<Button>(std::string(fallback));
        auto icon = app.load_icon(icon_name, 16);
        if (icon) {
            btn->set_icon(icon);
            btn->set_text("");
        }
        return btn;
    };

    // Navigation block
    auto back_btn = make_icon_btn("go-previous", "←");
    back_btn_ = back_btn.get();
    back_btn_->set_tooltip("Go Back");
    back_btn_->set_enabled(false);
    back_btn_->on_click = [this] { navigate_back(); };
    toolbar->add_widget(std::move(back_btn));

    auto fwd_btn = make_icon_btn("go-next", "→");
    forward_btn_ = fwd_btn.get();
    forward_btn_->set_tooltip("Go Forward");
    forward_btn_->set_enabled(false);
    forward_btn_->on_click = [this] { navigate_forward(); };
    toolbar->add_widget(std::move(fwd_btn));

    auto up_btn = make_icon_btn(XDG::IconActions::goUp, "↑");
    up_button_ = up_btn.get();
    up_button_->set_tooltip("Up One Level");
    up_button_->on_click = [this] { navigate_up(); };
    toolbar->add_widget(std::move(up_btn));

    auto home_btn = make_icon_btn(XDG::IconActions::goHome, "⌂");
    home_btn->set_tooltip("Go to Home");
    home_btn->on_click = [this] { navigate_home(); };
    toolbar->add_widget(std::move(home_btn));

    // Dialog-only extras: look-in label + drive selector (page 0 = hidden in browser mode)
    auto toolbar_extras = std::make_unique<StackedLayout>();
    toolbar_extras_ = toolbar_extras.get();
    toolbar_extras->add_widget(std::make_unique<HBoxLayout>()); // page 0: empty (browser mode)

    auto extras_row = std::make_unique<HBoxLayout>();
    extras_row->set_margins({});
    extras_row->set_spacing(4);
    extras_row->add_widget(std::make_unique<Label>("Look in:"));

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
    combo->on_change = [this, raw = combo.get()](int) {
#if defined(_WIN32)
        set_current_path(raw->selected_text() + "\\");
#else
        set_current_path(raw->selected_text());
#endif
    };
    extras_row->add_widget(std::move(combo), 1);
    toolbar_extras->add_widget(std::move(extras_row)); // page 1: dialog mode
    toolbar->add_widget(std::move(toolbar_extras), 1); // takes the stretch slot

    // New folder button
    auto new_btn = make_icon_btn(XDG::IconActions::folderNew, "New");
    new_button_ = new_btn.get();
    new_button_->set_tooltip("Create New Folder");
    new_button_->on_click = [this] {
        if (on_new) {
            on_new();
        }
    };
    toolbar->add_widget(std::move(new_btn));

    // Config popup menu: view mode + show hidden
    auto switch_view = [this](ViewMode mode) {
        auto *m = static_cast<DirModel *>(model_.get());
        view_mode_ = mode;
        m->set_detail_mode(mode == ViewMode::Details);
        stacked_->set_current(static_cast<int>(mode));
        if (m->on_data_changed) {
            m->on_data_changed();
        }
    };
    auto config_menu = std::make_shared<Menu>("");
    config_menu->add_action("Icons", [switch_view] { switch_view(ViewMode::Icons); });
    config_menu->add_action("List", [switch_view] { switch_view(ViewMode::List); });
    config_menu->add_action("Details", [switch_view] { switch_view(ViewMode::Details); });
    config_menu->add_separator();
    config_menu->add_action("Show Hidden Files", [this] { set_show_hidden(!show_hidden()); });

    auto config_btn = make_icon_btn("configure", "☰");
    config_btn_ = config_btn.get();
    config_btn_->set_tooltip("View Options");
    config_btn_->set_menu(config_menu);
    toolbar->add_widget(std::move(config_btn));

    add_widget(std::move(toolbar));

    // ── Stacked views ─────────────────────────────────────────────────────────
    auto stacked = std::make_unique<StackedLayout>();
    stacked_ = stacked.get();

    // 0: List (compact, no header)
    auto list_tbl = std::make_unique<TableView>(model_);
    list_view_ = list_tbl.get();
    list_view_->set_show_header(false);
    list_view_->auto_fit_columns();
    list_view_->on_item_activated = [this](size_t i) { on_activated(i); };
    list_view_->on_back_requested = [this] { navigate_back(); };
    list_view_->on_selection_changed = [this](std::optional<size_t> idx) {
        if (!idx || !path_input_) {
            return;
        }
        auto *m = static_cast<DirModel const *>(model_.get());
        auto const &entries = m->entries();
        if (*idx < entries.size() && !entries[*idx].is_dir) {
            path_input_->set_text(entries[*idx].name);
        }
    };
    stacked->add_widget(std::move(list_tbl));

    // 1: Icons
    auto grid = std::make_unique<IconGrid>(model_);
    icon_grid_ = grid.get();
    icon_grid_->set_focusable(true);
    icon_grid_->on_item_activated = [this](size_t i) { on_activated(i); };
    icon_grid_->on_back_requested = [this] { navigate_back(); };
    icon_grid_->on_selection_changed = [this](std::set<size_t> const &indices) {
        if (indices.empty() || !path_input_) {
            return;
        }
        auto *m = static_cast<DirModel const *>(model_.get());
        auto const &entries = m->entries();
        auto idx = *indices.begin();
        if (idx < entries.size() && !entries[idx].is_dir) {
            path_input_->set_text(entries[idx].name);
        }
    };
    stacked->add_widget(std::move(grid));

    // 2: Details (with header)
    auto det_tbl = std::make_unique<TableView>(model_);
    details_view_ = det_tbl.get();
    details_view_->set_show_header(true);
    details_view_->set_alternating_row_colors(true);
    details_view_->auto_fit_columns();
    details_view_->on_item_activated = [this](size_t i) { on_activated(i); };
    details_view_->on_back_requested = [this] { navigate_back(); };
    details_view_->on_selection_changed = [this](std::optional<size_t> idx) {
        if (!idx || !path_input_) {
            return;
        }
        auto *m = static_cast<DirModel const *>(model_.get());
        auto const &entries = m->entries();
        if (*idx < entries.size() && !entries[*idx].is_dir) {
            path_input_->set_text(entries[*idx].name);
        }
    };
    stacked->add_widget(std::move(det_tbl));

    stacked_->set_current(1); // default: Icons
    add_widget(std::move(stacked), 1);

    // ── Bottom: Filename and Filter ───────────────────────────────────────────
    auto bottom = std::make_unique<FormLayout>();
    bottom->set_margins({});
    bottom->set_spacing(4);
    bottom->set_label_spacing(8);

    auto const button_min = Size{80, 0};

    auto name_field = std::make_unique<HBoxLayout>();
    name_field->set_margins({});
    name_field->set_spacing(8);

    auto path_in = std::make_unique<LineInput>();
    path_input_ = path_in.get();
    path_input_->on_submit = [this](std::string const &, LineInput &) {
        if (file_must_exist_ && !std::filesystem::exists(selected_path())) {
            return;
        }
        if (on_ok) {
            on_ok();
        }
    };
    name_field->add_widget(std::move(path_in), 1);

    auto open_btn = std::make_unique<Button>("Open");
    ok_button_ = open_btn.get();
    ok_button_->set_min_size(button_min);
    ok_button_->on_click = [this] {
        if (file_must_exist_ && !std::filesystem::exists(selected_path())) {
            return;
        }
        if (on_ok) {
            on_ok();
        }
    };
    name_field->add_widget(std::move(open_btn));

    bottom->add_row(std::make_unique<Label>("File name:"), std::move(name_field));

    auto type_field = std::make_unique<HBoxLayout>();
    type_field->set_margins({});
    type_field->set_spacing(8);

    auto ext_combo = std::make_unique<Combobox>(std::vector<std::string>{"All Files (*.*)"});
    extension_combo_ = ext_combo.get();
    type_field->add_widget(std::move(ext_combo), 1);

    auto cancel_btn = std::make_unique<Button>("Cancel");
    cancel_button_ = cancel_btn.get();
    cancel_button_->set_min_size(button_min);
    cancel_button_->on_click = [this] {
        if (on_cancel) {
            on_cancel();
        }
    };
    type_field->add_widget(std::move(cancel_btn));

    bottom->add_row(std::make_unique<Label>("Files of type:"), std::move(type_field));

    auto bottom_stack = std::make_unique<StackedLayout>();
    bottom_stack_ = bottom_stack.get();
    bottom_stack->add_widget(std::make_unique<HBoxLayout>()); // page 0: empty (browser mode)
    bottom_stack->add_widget(std::move(bottom));              // page 1: dialog chrome
    add_widget(std::move(bottom_stack));

    // Apply initial mode
    auto initial_page = browser_mode_ ? 0 : 1;
    toolbar_extras_->set_current(initial_page);
    bottom_stack_->set_current(initial_page);

    load_directory();
}

} // namespace toolkit
