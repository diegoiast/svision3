// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/button.hpp"
#include "toolkit/combobox.hpp"
#include "toolkit/icon_grid.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/line_input.hpp"
#include "toolkit/widget.hpp"
#include "toolkit/xdg_icons.hpp"
#include <memory>
#include <string>
#include <vector>

namespace toolkit {

class StandardIconModel;

class FileDialogWidget : public VBoxLayout, public Fluent<FileDialogWidget> {
  public:
    enum class SortOrder { Name, Time };

    FileDialogWidget();

    std::function<void()> on_ok;
    std::function<void()> on_cancel;
    std::function<void()> on_up;
    std::function<void()> on_new;

    void set_current_path(std::string path);
    
    FileDialogWidget &set_sort_order(SortOrder order);
    SortOrder sort_order() const { return sort_order_; }
    
    FileDialogWidget &set_dirs_first(bool dirs_first);
    bool dirs_first() const { return dirs_first_; }

    IconGrid *icon_grid() const { return icon_grid_; }

  private:
    void setup_ui();
    void load_directory();

    std::string current_path_;
    SortOrder sort_order_ = SortOrder::Name;
    bool dirs_first_ = true;
    std::shared_ptr<StandardIconModel> model_;

    std::unique_ptr<HBoxLayout> toolbar_;
    std::unique_ptr<VBoxLayout> content_;
    std::unique_ptr<HBoxLayout> button_bar_;

    Combobox *drive_combo_ = nullptr;
    LineInput *path_input_ = nullptr;
    Button *up_button_ = nullptr;
    Button *new_button_ = nullptr;
    IconGrid *icon_grid_ = nullptr;
    Button *cancel_button_ = nullptr;
    Button *ok_button_ = nullptr;
};

} // namespace toolkit