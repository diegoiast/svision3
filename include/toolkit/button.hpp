// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <functional>
#include <optional>
#include <string>

namespace toolkit {

class Button : public Widget {
  public:
    explicit Button(std::string text);

    void set_text(std::string text);
    std::string const &text() const { return display_text_; }

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    CursorShape cursor() const override {
        return enabled_ ? CursorShape::Arrow : CursorShape::NotAllowed;
    }
    bool trigger_mnemonic(char key) override;
    void collect_mnemonics(std::vector<Widget *> &out) override {
        if (mnemonic_key_) {
            out.push_back(this);
        }
    }

    void set_padding(Margins const &padding) { padding_override_ = padding; }
    void set_flat(bool f) { flat_ = f; }
    bool is_flat() const { return flat_; }

    void set_visible(bool v) override;
    bool is_hovered() const { return hovered_; }
    bool is_pressed() const { return pressed_; }

    std::function<void()> on_click;

  private:
    std::string display_text_;
    int mnemonic_index_ = -1;
    char mnemonic_key_ = 0;
    bool hovered_ = false;
    bool pressed_ = false;
    bool flat_ = false;
    std::optional<Margins> padding_override_;
};

} // namespace toolkit
