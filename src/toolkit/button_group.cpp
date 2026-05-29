// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/button_group.hpp"

namespace toolkit {

Button &ButtonGroup::add_button(std::string text) {
    auto btn = std::make_unique<Button>(std::move(text));
    btn->set_checkable(true);
    int index = static_cast<int>(buttons_.size());
    Button &ref = *btn;
    register_button(index, ref);
    HBoxLayout::add_widget(std::move(btn));
    return ref;
}

Button &ButtonGroup::add_button(std::unique_ptr<Button> button) {
    button->set_checkable(true);
    int index = static_cast<int>(buttons_.size());
    Button &ref = *button;
    register_button(index, ref);
    HBoxLayout::add_widget(std::move(button));
    return ref;
}

void ButtonGroup::set_checked(int index) {
    if (index < 0 || index >= static_cast<int>(buttons_.size()) || index == checked_index_) {
        return;
    }

    updating_ = true;

    for (size_t i = 0; i < buttons_.size(); i++) {
        buttons_[i]->set_checked(i == static_cast<size_t>(index));
    }

    checked_index_ = index;
    updating_ = false;

    if (on_change) {
        on_change(index);
    }
}

Button *ButtonGroup::checked_button() const {
    if (checked_index_ >= 0 && checked_index_ < static_cast<int>(buttons_.size())) {
        return buttons_[checked_index_];
    }
    return nullptr;
}

void ButtonGroup::register_button(int index, Button &button) {
    buttons_.push_back(&button);

    auto existing_toggle = button.on_toggle;
    button.on_toggle = [this, index, existing_toggle](bool checked) {
        if (updating_) {
            if (existing_toggle) {
                existing_toggle(checked);
            }
            return;
        }

        if (checked) {
            for (size_t i = 0; i < buttons_.size(); i++) {
                if (i != static_cast<size_t>(index) && buttons_[i]->is_checked()) {
                    buttons_[i]->set_checked(false);
                }
            }
            checked_index_ = index;
            if (on_change) {
                on_change(index);
            }
        }

        if (existing_toggle) {
            existing_toggle(checked);
        }
    };

    if (buttons_.size() == 1) {
        updating_ = true;
        button.set_checked(true);
        checked_index_ = 0;
        updating_ = false;
    }
}

} // namespace toolkit
