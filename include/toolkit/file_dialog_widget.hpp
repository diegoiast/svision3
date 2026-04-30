// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/button.hpp"
#include "toolkit/combobox.hpp"
#include "toolkit/icon_grid.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/line_input.hpp"
#include "toolkit/table_view.hpp"
#include "toolkit/widget.hpp"
#include "toolkit/xdg_icons.hpp"
#include <memory>
#include <string>
#include <vector>

namespace toolkit {

class FileDialogWidget : public VBoxLayout, public Fluent<FileDialogWidget> {
  public:
    enum class SortOrder { Name, Time };

    FileDialogWidget();

    std::function<void()> on_ok;
    std::function<void()> on_cancel;
    std::function<void()> on_up;
    std::function<void()> on_new;

    void set_current_path(std::string path);
    std::string selected_path() const;
    void set_ok_label(std::string_view label);

    bool handle_key(KeyEvent const &event) override;

    FileDialogWidget &set_sort_order(SortOrder order);
    SortOrder sort_order() const { return sort_order_; }

    FileDialogWidget &set_dirs_first(bool dirs_first);
    bool dirs_first() const { return dirs_first_; }

    FileDialogWidget &set_show_hidden(bool show_hidden);
    bool show_hidden() const { return show_hidden_; }

    IconGrid *icon_grid() const { return icon_grid_; }

  private:
    void setup_ui();
    void load_directory();

    enum class ViewMode { List, Icons, Details };
    ViewMode view_mode_ = ViewMode::Icons;

    std::string current_path_;
    SortOrder sort_order_ = SortOrder::Name;
    bool dirs_first_ = true;
    bool show_hidden_ = false;
    std::shared_ptr<StandardIconModel> model_;
    std::shared_ptr<ItemModel> details_model_;

    std::unique_ptr<HBoxLayout> toolbar_;
    std::unique_ptr<VBoxLayout> content_;
    std::unique_ptr<VBoxLayout> bottom_controls_;

    Combobox *drive_combo_ = nullptr;
    LineInput *path_input_ = nullptr;
    Combobox *extension_combo_ = nullptr;
    Button *up_button_ = nullptr;
    Button *new_button_ = nullptr;
    Button *list_view_btn_ = nullptr;
    Button *icon_view_btn_ = nullptr;
    Button *details_view_btn_ = nullptr;

    IconGrid *icon_grid_ = nullptr;
    TableView *table_view_ = nullptr;
    TableView *table_view_details_ = nullptr;
    Button *cancel_button_ = nullptr;
    Button *ok_button_ = nullptr;
};

} // namespace toolkit