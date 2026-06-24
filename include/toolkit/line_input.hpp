// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/context_menu.hpp"
#include "toolkit/undo_stack.hpp"
#include "toolkit/widget.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace toolkit {

class LineInput : public Widget, public Fluent<LineInput> {
    DECLARE_WIDGET(LineInput)

  public:
    using TextDirection = Painter::TextDirection;

    explicit LineInput(std::string placeholder = "");
    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    void paint(Painter &painter) override;
    void paint_buttons(Painter &painter);
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    void on_focus() override;
    void on_blur() override;

    Size size_hint() const override;
    CursorShape cursor() const override {
        return (clear_hovered_ || peek_hovered_) ? CursorShape::Hand : CursorShape::IBeam;
    }
    LineInput &set_focused(bool focused) override;

    LineInput &set_text(std::string const &text);
    std::string const &text() const { return text_; }

    LineInput &set_password_mode(bool enable);
    bool is_password_mode() const { return password_mode_; }

    LineInput &set_read_only(bool enable);
    bool is_read_only() const { return read_only_; }

    LineInput &set_text_direction(TextDirection dir) {
        text_direction_ = dir;
        return *this;
    }
    TextDirection text_direction() const { return text_direction_; }

    LineInput &set_font_family(FontFamily f) {
        font_family_ = f;
        return *this;
    }
    FontFamily font_family() const { return font_family_; }

    size_t cursor_position() const { return cursor_pos_; }
    size_t cursor_codepoint() const { return static_cast<size_t>(to_masked_offset(text_, cursor_pos_)); }
    float cursor_physical_x(Painter &painter) const;

    enum class ValidationMode { None, Block, Notify };
    LineInput &
    set_validator(std::function<bool(std::string const &, LineInput const &widget)> validator) {
        validator_ = std::move(validator);
        return *this;
    }
    LineInput &set_validation_mode(ValidationMode mode) {
        validation_mode_ = mode;
        return *this;
    }
    ValidationMode validation_mode() const { return validation_mode_; }
    bool is_valid() const;

    Command::Ptr select_all_cmd;
    Command::Ptr cut_cmd;
    Command::Ptr copy_cmd;
    Command::Ptr paste_cmd;
    Command::Ptr undo_cmd;
    Command::Ptr redo_cmd;
    Command::Ptr dir_auto_cmd;
    Command::Ptr dir_ltr_cmd;
    Command::Ptr dir_rtl_cmd;

    std::function<void(std::string const &, LineInput &widget)> on_change;
    std::function<void(std::string const &, LineInput &widget)> on_submit;

  private:
    void reset_cursor_blink();
    void ensure_cursor_visible(Painter &painter);
    size_t pos_from_x(float x) const;
    bool resolve_rtl_dir(std::vector<double> const &positions) const;
    bool has_selection() const { return sel_anchor_ != cursor_pos_; }
    size_t sel_start() const { return std::min(sel_anchor_, cursor_pos_); }
    size_t sel_end() const { return std::max(sel_anchor_, cursor_pos_); }
    void delete_selection();
    void move_cursor(size_t pos, bool extend_selection);
    void move_word_left(bool extend_selection);
    void move_word_right(bool extend_selection);
    void select_word_at(size_t pos);
    void select_all();

    bool hit_clear_btn(Point pos) const;
    float clear_btn_size() const;
    bool hit_peek_btn(Point pos) const;
    float peek_btn_size() const;
    float content_right_inset() const;
    float content_available_width() const;
    void show_context_menu(Point pos);
    static auto to_masked_offset(std::string_view text, size_t byte_pos) -> int;
    void cut();
    void copy();
    void paste();
    void undo();
    void redo();
    void sync_commands();

    std::string text_;
    std::string placeholder_;
    bool read_only_ = false;
    bool password_mode_ = false;
    size_t cursor_pos_ = 0;

    size_t sel_anchor_ = 0;
    float scroll_offset_ = 0.0f;
    bool dragging_ = false;
    bool clear_hovered_ = false;
    bool clear_pressed_ = false;
    bool peek_hovered_ = false;
    bool peek_pressed_ = false;
    bool is_password_field_ = false;
    ValidationMode validation_mode_ = ValidationMode::None;
    std::function<bool(std::string const &, LineInput const &)> validator_;
    std::chrono::steady_clock::time_point cursor_blink_time_;
    int blink_timer_id_ = 0;
    std::unique_ptr<ContextMenu> context_menu_;
    UndoStack undo_stack_;
    TextDirection text_direction_ = TextDirection::Auto;
    FontFamily font_family_ = FontFamily::System;

    // Cached password-mode offsets, recomputed in sync_commands()
    int cached_pw_cursor_pos_ = 0;
    int cached_pw_sel_start_ = -1;
    int cached_pw_sel_end_ = -1;
};

} // namespace toolkit
