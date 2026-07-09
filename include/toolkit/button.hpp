// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/button_state.hpp"
#include "toolkit/widget.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace toolkit {

class Menu;

class Button : public Widget, public Fluent<Button> {
    DECLARE_WIDGET(Button)
  public:
    explicit Button(std::string text);
    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    void paint(Painter &painter) override;
    void paint_background(Painter &) override {}
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    CursorShape cursor() const override {
        return is_enabled() ? CursorShape::Arrow : CursorShape::NotAllowed;
    }
    bool trigger_mnemonic(std::string_view key) override;
    void collect_mnemonics(std::vector<Widget *> &out) override {
        if (!mnemonic_key_.empty()) {
            out.push_back(this);
        }
    }

    Button &set_icon(Icon icon);
    Button &clear_icon();
    Button &set_text(std::string text);
    std::string const &text() const { return text_; }
    Button &set_padding(Margins const &padding) {
        padding_override_ = padding;
        return *this;
    }
    const std::optional<Margins> &get_padding() const { return padding_override_; }
    Button &set_flat(bool f);
    bool is_flat() const { return flat_; }
    Button &set_checkable(bool c);
    bool is_checkable() const { return checkable_; }
    Button &set_checked(bool c);
    bool is_checked() const { return checked_; }

    void set_auto_repeat(bool ar, float delay = 0.5f, float interval = 0.4f) {
        auto_repeat_ = ar;
        auto_repeat_delay_ = delay;
        auto_repeat_interval_ = interval;
    }

    void stop_auto_repeat();
    Widget &set_visible(bool v) override;
    bool is_hovered() const {
        return state_handler_.button_state == ButtonState::Hovered ||
               state_handler_.button_state == ButtonState::ClickedInside;
    }
    bool is_pressed() const { return state_handler_.button_state == ButtonState::ClickedInside; }

    Button &set_command(Command::Ptr cmd);
    Command::Ptr command() const { return command_; }

    Button &set_menu(std::shared_ptr<Menu> menu);
    std::shared_ptr<Menu> menu() const { return menu_; }
    bool has_menu() const { return menu_ != nullptr; }

    std::function<void()> on_click;
    std::function<void(bool)> on_toggle;

  private:
    void fire_click();
    void sync_from_command();

    Command::Ptr command_;
    std::shared_ptr<Menu> menu_;
    bool menu_open_ = false;
    void show_menu();
    void start_auto_repeat_delay();
    void start_auto_repeat_interval();
    void on_state_changed();
    bool should_fire_click() const;

    std::string text_;
    Icon icon_;
    std::string mnemonic_key_;
    bool flat_ = false;
    bool checkable_ = false;
    bool checked_ = false;
    bool auto_repeat_ = false;
    float auto_repeat_delay_ = 0.5f;
    float auto_repeat_interval_ = 0.4f;
    int auto_repeat_timer_id_ = 0;
    ButtonStateHandler state_handler_;
    std::optional<Margins> padding_override_;
};

} // namespace toolkit
