// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/context_menu.hpp"
#include "svision3/text/bidi.hpp"
#include "svision3/text/text_layout.hpp"
#include "svision3/undo_stack.hpp"
#include "svision3/widget.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace svision3 {

class LineInput;

// One icon slot on a LineInput's edge. Within each side, icons lay out in
// vector order starting from the text: `trailing_icons_[0]` is the leftmost
// icon on the right edge (nearest the text), `leading_icons_[0]` the
// rightmost on the left edge (nearest the text). `visible` gates both painting and the space
// reserved from the text; a null `click` makes the icon a passive indicator
// (hover cursor falls back to IBeam). The built-ins (caps lock, clear, peek)
// all live on the trailing side; the leading side is currently empty and
// exposed via add_leading_icon() for applications that want it.
struct InputIcon {
    std::function<bool(LineInput const &)> visible;
    std::function<void(Painter &, Rect const &, LineInput const &, bool hovered, bool pressed)>
        paint;
    std::function<bool(LineInput &)> click;
};

class LineInput : public Widget, public Fluent<LineInput> {
    DECLARE_WIDGET(LineInput)
  public:
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
        if (hovered_icon_) {
            auto const &icons = hovered_icon_->leading ? leading_icons_ : trailing_icons_;
            auto const &icon = icons[hovered_icon_->index];
            if (icon.click) {
                return CursorShape::Hand;
            }
        }
        return CursorShape::IBeam;
    }
    LineInput &set_focused(bool focused) override;

    // Appends to the trailing (right) side; the icon lands rightmost of the
    // group, beyond any icons added before it.
    LineInput &add_trailing_icon(InputIcon icon);
    // Appends to the leading (left) side; the icon lands leftmost of that
    // group, beyond any leading icons added before it.
    LineInput &add_leading_icon(InputIcon icon);

    // Which drawing the caps lock icon uses; switch to compare variants.
    enum class CapsLockIcon { Letter, FilledCircle, ChevronBar, OutlineCircle };
    LineInput &set_caps_lock_icon(CapsLockIcon style) {
        caps_lock_icon_style_ = style;
        return *this;
    }
    CapsLockIcon caps_lock_icon() const { return caps_lock_icon_style_; }

    // Per-widget opt-out: hide the caps lock icon on this input only.
    LineInput &set_show_caps_lock_icon(bool show) {
        show_caps_lock_icon_ = show;
        return *this;
    }
    bool is_show_caps_lock_icon() const { return show_caps_lock_icon_; }

    LineInput &set_text(std::string const &text);
    std::string const &text() const { return text_; }

    LineInput &set_password_mode(bool enable);
    bool is_password_mode() const { return password_mode_; }

    LineInput &set_read_only(bool enable);
    bool is_read_only() const { return read_only_; }

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

    enum class TextDirection { LTR, RTL, Auto };
    LineInput &set_text_direction(TextDirection mode);
    TextDirection text_direction() const { return direction_mode_; }
    // What direction_mode_ actually resolves to right now -- Auto runs
    // detect_base_direction() against the current text, LTR/RTL are fixed.
    bidi::BaseDirection resolved_direction() const;

    Command::Ptr select_all_cmd;
    Command::Ptr cut_cmd;
    Command::Ptr copy_cmd;
    Command::Ptr paste_cmd;
    Command::Ptr undo_cmd;
    Command::Ptr redo_cmd;

    std::function<void(std::string const &, LineInput &widget)> on_change;
    std::function<void(std::string const &, LineInput &widget)> on_submit;

  private:
    void reset_cursor_blink();
    void ensure_cursor_visible(Painter &painter, text::TextLayout const *layout);
    std::optional<text::TextLayout> build_layout() const;
    size_t pos_from_x(float x) const;
    bool has_selection() const { return sel_anchor_ != cursor_pos_; }
    size_t sel_start() const { return std::min(sel_anchor_, cursor_pos_); }
    size_t sel_end() const { return std::max(sel_anchor_, cursor_pos_); }
    void delete_selection();
    void move_cursor(size_t pos, bool extend_selection);
    void move_arrow(int key_dir, bool extend_selection);
    size_t arrow_collapse_target(int key_dir) const;
    void move_word_left(bool extend_selection);
    void move_word_right(bool extend_selection);
    void select_word_at(size_t pos);
    void select_all();

    struct IconHit {
        bool leading;
        size_t index;
        bool operator==(IconHit const &) const = default;
    };
    std::optional<IconHit> hit_icon(Point pos) const;
    Rect icon_slot_rect(size_t from_right) const;
    Rect leading_icon_slot_rect(size_t from_left) const;
    float icon_size() const;
    float icon_spacing() const;
    float leading_inset() const;
    float content_right_inset() const;
    float content_left_inset() const;
    float content_available_width() const;
    void show_context_menu(Point pos);
    void paint_clear_icon(Painter &painter, Rect const &slot, bool hovered, bool pressed) const;
    void paint_peek_icon(Painter &painter, Rect const &slot, bool hovered, bool pressed) const;
    void paint_caps_lock_icon(Painter &painter, Rect const &slot, bool hovered, bool pressed) const;
    void paint_caps_lock_letter(Painter &painter, Rect const &slot, bool hovered) const;
    void paint_caps_lock_filled_circle(Painter &painter, Rect const &slot, bool hovered) const;
    void paint_caps_lock_chevron_bar(Painter &painter, Rect const &slot, bool hovered) const;
    void paint_caps_lock_outline_circle(Painter &painter, Rect const &slot, bool hovered) const;
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
    std::optional<IconHit> hovered_icon_;
    std::optional<IconHit> pressed_icon_;
    CapsLockIcon caps_lock_icon_style_ = CapsLockIcon::Letter;
    bool show_caps_lock_icon_ = true;
    bool is_password_field_ = false;
    ValidationMode validation_mode_ = ValidationMode::None;
    std::function<bool(std::string const &, LineInput const &)> validator_;
    std::chrono::steady_clock::time_point cursor_blink_time_;
    int blink_timer_id_ = 0;
    std::unique_ptr<ContextMenu> context_menu_;
    UndoStack undo_stack_;
    TextDirection direction_mode_ = TextDirection::Auto;
    std::vector<InputIcon> trailing_icons_;
    std::vector<InputIcon> leading_icons_;
};

} // namespace svision3
