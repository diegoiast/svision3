// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/context_menu.hpp"
#include "toolkit/widget.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace toolkit {

class LineInput : public Widget {
  public:
    explicit LineInput(std::string placeholder = "");

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    bool focusable() const override { return true; }
    CursorShape cursor() const override {
        return clear_hovered_ ? CursorShape::Arrow : CursorShape::IBeam;
    }
    void set_focused(bool focused) override;

    std::string const &text() const { return text_; }
    void set_text(std::string const &text);

    std::function<void(std::string const &)> on_change;
    std::function<void(std::string const &)> on_submit;

  private:
    void reset_cursor_blink();
    void ensure_cursor_visible(Painter &painter);
    size_t pos_from_x(float x) const;
    bool has_selection() const { return sel_anchor_ != cursor_pos_; }
    size_t sel_start() const { return std::min(sel_anchor_, cursor_pos_); }
    size_t sel_end() const { return std::max(sel_anchor_, cursor_pos_); }
    void delete_selection();
    void move_cursor(size_t pos, bool extend_selection);
    void move_word_left(bool extend_selection);
    void move_word_right(bool extend_selection);
    void select_word_at(size_t pos);

    bool hit_clear_btn(Point pos) const;
    float clear_btn_size() const;
    float content_right_inset() const;
    void show_context_menu(Point pos);
    void cut();
    void copy();
    void paste();

    std::string text_;
    std::string placeholder_;
    size_t cursor_pos_ = 0;
    size_t sel_anchor_ = 0;
    float scroll_offset_ = 0.0f;
    bool dragging_ = false;
    bool clear_hovered_ = false;
    bool clear_pressed_ = false;
    std::chrono::steady_clock::time_point cursor_blink_time_;
    std::unique_ptr<ContextMenu> context_menu_;
};

} // namespace toolkit
