// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/button.hpp"
#include "svision3/layout.hpp"
#include <functional>
#include <memory>
#include <vector>

namespace svision3 {

class ButtonGroup : public HBoxLayout {
  public:
    ButtonGroup() = default;

    Button &add_button(std::string text);
    Button &add_button(std::unique_ptr<Button> button);

    void set_checked(int index);
    int checked_index() const { return checked_index_; }
    Button *checked_button() const;

    std::function<void(int)> on_change;

  private:
    void register_button(int index, Button &button);

    std::vector<Button *> buttons_;
    int checked_index_ = -1;
    bool updating_ = false;
};

} // namespace svision3
