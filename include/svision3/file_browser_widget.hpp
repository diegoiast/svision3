// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/button.hpp"
#include "svision3/combobox.hpp"
#include "svision3/icon_grid.hpp"
#include "svision3/label.hpp"
#include "svision3/layout.hpp"
#include "svision3/line_input.hpp"
#include "svision3/table_view.hpp"
#include "svision3/widget.hpp"
#include "svision3/xdg_icons.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace svision3 {

class FileBrowserWidget : public VBoxLayout, public Fluent<FileBrowserWidget> {
    DECLARE_WIDGET(FileBrowserWidget)
  public:
    enum class SortOrder { Name, Time };

    FileBrowserWidget();

    // Dialog callbacks
    std::function<void()> on_ok;
    std::function<void()> on_cancel;
    std::function<void()> on_up;
    std::function<void()> on_new;

    // Browser callbacks (also fire in dialog mode)
    std::function<void(std::string const &path)> on_file_activated;
    std::function<void(std::string const &path)> on_path_changed;

    void set_current_path(std::string path);
    void navigate_to(std::string path); // alias for set_current_path
    void navigate_home();
    std::string const &current_path() const { return current_path_; }

    void set_filename(std::string_view name);
    std::string selected_path() const;
    void set_ok_label(std::string_view label);

    // Hide the filename/filter/OK/Cancel bar — turns the widget into an embedded browser
    FileBrowserWidget &set_browser_mode(bool browser);
    bool browser_mode() const { return browser_mode_; }

    bool handle_key(KeyEvent const &event) override;

    FileBrowserWidget &set_sort_order(SortOrder order);
    SortOrder sort_order() const { return sort_order_; }

    FileBrowserWidget &set_dirs_first(bool dirs_first);
    bool dirs_first() const { return dirs_first_; }

    FileBrowserWidget &set_show_hidden(bool show_hidden);
    bool show_hidden() const { return show_hidden_; }

    FileBrowserWidget &set_file_must_exist(bool must_exist);
    bool file_must_exist() const { return file_must_exist_; }

    enum class ViewMode { List, Icons, Details };
    FileBrowserWidget &set_view_mode(ViewMode mode);
    ViewMode view_mode() const { return view_mode_; }

    IconGrid *icon_grid() const { return icon_grid_; }

  private:
    void setup_ui();
    void load_directory();
    void navigate_up();
    void navigate_back();
    void navigate_forward();
    void push_history(std::string path);
    void update_buttons();
    void on_activated(size_t index);
    ViewMode view_mode_ = ViewMode::Icons;

    std::string current_path_;
    SortOrder sort_order_ = SortOrder::Name;
    bool dirs_first_ = true;
    bool show_hidden_ = false;
    bool file_must_exist_ = false;
    bool browser_mode_ = true;
    std::shared_ptr<ItemModel> model_;
    std::vector<std::string> back_stack_;
    std::vector<std::string> forward_stack_;

    Button *back_btn_ = nullptr;
    Button *forward_btn_ = nullptr;
    LineInput *path_input_ = nullptr;
    Combobox *extension_combo_ = nullptr;
    Button *up_button_ = nullptr;
    Button *new_button_ = nullptr;
    Button *config_btn_ = nullptr;

    StackedLayout *stacked_ = nullptr;
    StackedLayout *toolbar_extras_ = nullptr; // page 0 = empty (browser), page 1 = look-in + drive
    StackedLayout *bottom_stack_ = nullptr;   // page 0 = empty (browser), page 1 = dialog chrome
    IconGrid *icon_grid_ = nullptr;
    TableView *list_view_ = nullptr;
    TableView *details_view_ = nullptr;
    Button *cancel_button_ = nullptr;
    Button *ok_button_ = nullptr;
};

} // namespace svision3
