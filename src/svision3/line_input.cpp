// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/line_input.hpp"
#include "svision3/clipboard.hpp"
#include "svision3/platform.hpp"
#include "svision3/text/bidi.hpp"
#include "svision3/theme.hpp"
#include "svision3/utf8.hpp"
#include "svision3/window.hpp"
#include <cctype>
#include <nlohmann/json.hpp>

namespace svision3 {

class TextCommand : public UndoCommand {
  public:
    TextCommand(std::string *text, std::string old_val, std::string new_val, size_t pos)
        : text_(text), old_val_(std::move(old_val)), new_val_(std::move(new_val)), pos_(pos) {}

    void undo() override { text_->replace(pos_, new_val_.size(), old_val_); }

    void redo() override { text_->replace(pos_, old_val_.size(), new_val_); }

    int id() const override { return 1; }

    std::string text() const override { return old_val_.empty() ? "Typing" : "Deletion"; }

    bool merge_with(const UndoCommand *other) override {
        auto const *o = static_cast<const TextCommand *>(other);
        if (o->pos_ == pos_ + new_val_.size()) {
            new_val_ += o->new_val_;
            return true;
        }
        return false;
    }

  private:
    std::string *text_;
    std::string old_val_;
    std::string new_val_;
    size_t pos_;
};

static std::string get_masked_text(std::string_view text, size_t byte_limit = std::string::npos) {
    std::string result;
    size_t i = 0;
    while (i < text.size() && i < byte_limit) {
        i = Utf8Iterator::next(text, i);
        // We use a character that has a representative width for a "pro" dot
        result += "8";
    }
    return result;
}

LineInput::LineInput(std::string placeholder)
    : placeholder_(std::move(placeholder)), cursor_blink_time_(std::chrono::steady_clock::now()) {
    state.focusable = true;
    state.focused = false;
    read_only_ = false;

    select_all_cmd = Command::create("Select All", [this] { select_all(); });
    select_all_cmd->set_shortcut("Std+A");
    add_command(select_all_cmd);

    cut_cmd = Command::create("Cut", [this] { cut(); });
    cut_cmd->set_shortcut("Std+X");
    add_command(cut_cmd);

    copy_cmd = Command::create("Copy", [this] { copy(); });
    copy_cmd->set_shortcut("Std+C");
    add_command(copy_cmd);

    paste_cmd = Command::create("Paste", [this] { paste(); });
    paste_cmd->set_shortcut("Std+V");
    add_command(paste_cmd);

    undo_cmd = Command::create("Undo", [this] { undo(); });
    undo_cmd->set_shortcut("Std+Z");
    add_command(undo_cmd);

    redo_cmd = Command::create("Redo", [this] { redo(); });
    redo_cmd->set_shortcut("Std+Y");
    add_command(redo_cmd);

    // macOS shows the caps lock glyph in a field while it is being edited, so
    // visibility is focus-gated; the predicate is evaluated live at paint time.
    // First in the vector: the trailing group lays out left-to-right in vector
    // order, so this is the leftmost icon on the right edge, nearest the text.
    trailing_icons_.push_back({
        .visible =
            [this](LineInput const &) {
                auto *plat = detail::current_platform();
                return show_caps_lock_icon_ && is_focused() && plat && plat->caps_lock_on();
            },
        .paint = [this](Painter &painter, Rect const &slot, LineInput const &, bool hovered,
                        bool pressed) { paint_caps_lock_icon(painter, slot, hovered, pressed); },
    });

    trailing_icons_.push_back({
        .visible = [this](LineInput const &) { return !text_.empty() && !read_only_; },
        .paint = [this](Painter &painter, Rect const &slot, LineInput const &, bool hovered,
                        bool pressed) { paint_clear_icon(painter, slot, hovered, pressed); },
        .click =
            [this](LineInput &) {
                text_.clear();
                cursor_pos_ = 0;
                sel_anchor_ = 0;
                scroll_offset_ = 0;
                sync_commands();
                if (on_change) {
                    on_change(text_, *this);
                }
                return true;
            },
    });

    trailing_icons_.push_back({
        .visible = [this](LineInput const &) { return is_password_field_; },
        .paint = [this](Painter &painter, Rect const &slot, LineInput const &, bool hovered,
                        bool pressed) { paint_peek_icon(painter, slot, hovered, pressed); },
        .click =
            [this](LineInput &) {
                password_mode_ = !password_mode_;
                sync_commands();
                return true;
            },
    });

    sync_commands();
}

LineInput &LineInput::add_trailing_icon(InputIcon icon) {
    trailing_icons_.push_back(std::move(icon));
    return *this;
}

LineInput &LineInput::add_leading_icon(InputIcon icon) {
    leading_icons_.push_back(std::move(icon));
    return *this;
}

nlohmann::json LineInput::to_json() const {
    auto j = Widget::to_json();
    j["text"] = text_;
    j["placeholder"] = placeholder_;
    j["read_only"] = read_only_;
    j["is_password"] = password_mode_;
    j["cursor"] = cursor_pos_;
    j["direction"] = direction_mode_ == TextDirection::LTR   ? "LTR"
                     : direction_mode_ == TextDirection::RTL ? "RTL"
                                                             : "Auto";
    return j;
}

void LineInput::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("text")) {
        text_ = j["text"];
    }
    if (j.contains("placeholder")) {
        placeholder_ = j["placeholder"];
    }
    if (j.contains("read_only")) {
        read_only_ = j["read_only"];
    }
    if (j.contains("is_password")) {
        password_mode_ = j["is_password"];
    }
    if (j.contains("cursor")) {
        cursor_pos_ = j["cursor"];
    }
    if (j.contains("direction")) {
        auto d = j["direction"];
        if (d == "LTR") {
            direction_mode_ = TextDirection::LTR;
        } else if (d == "RTL") {
            direction_mode_ = TextDirection::RTL;
        } else {
            direction_mode_ = TextDirection::Auto;
        }
    }
}

LineInput &LineInput::set_focused(bool focused) {
    Widget::set_focused(focused);
    if (!focused) {
        sel_anchor_ = cursor_pos_;
    }
    sync_commands();
    return *this;
}

LineInput &LineInput::set_text(std::string const &text) {
    if (text_ == text) {
        return *this;
    }
    text_ = text;
    cursor_pos_ = text_.size();
    sel_anchor_ = cursor_pos_;
    sync_commands();
    if (window_) {
        window_->request_redraw("input state");
    }
    return *this;
}

LineInput &LineInput::set_password_mode(bool enable) {
    password_mode_ = enable;
    if (enable) {
        is_password_field_ = true;
    }
    sync_commands();
    if (window_) {
        window_->request_redraw("input state");
    }
    return *this;
}

LineInput &LineInput::set_read_only(bool enable) {
    read_only_ = enable;
    return *this;
}

LineInput &LineInput::set_text_direction(TextDirection mode) {
    if (direction_mode_ == mode) {
        return *this;
    }
    direction_mode_ = mode;
    if (window_) {
        window_->request_redraw("input state");
    }
    return *this;
}

bidi::BaseDirection LineInput::resolved_direction() const {
    switch (direction_mode_) {
    case TextDirection::LTR:
        return bidi::BaseDirection::LTR;
    case TextDirection::RTL:
        return bidi::BaseDirection::RTL;
    case TextDirection::Auto:
    default:
        return bidi::detect_base_direction(text_);
    }
}

void LineInput::reset_cursor_blink() { cursor_blink_time_ = std::chrono::steady_clock::now(); }

void LineInput::delete_selection() {
    if (!has_selection()) {
        return;
    }
    auto s = sel_start();
    auto e = sel_end();
    std::string old_text = text_.substr(s, e - s);

    text_.erase(s, e - s);
    undo_stack_.push(std::make_unique<TextCommand>(&text_, old_text, "", s));
    cursor_pos_ = s;
    sel_anchor_ = s;
    sync_commands();
    if (on_change) {
        on_change(text_, *this);
    }
}

void LineInput::move_cursor(size_t pos, bool extend_selection) {
    cursor_pos_ = pos;
    if (!extend_selection) {
        sel_anchor_ = cursor_pos_;
    }
    sync_commands();
    reset_cursor_blink();
}

// Movement stays logical (Utf8Iterator::prev/next through the byte string),
// but which arrow key means "forward" flips with the field's base direction
// -- the OS convention for RTL text: Right moves toward the start of the
// typed string, Left toward the end, mirroring the LTR mapping. `key_dir` is
// the key that was pressed (-1 = Left, +1 = Right), not a screen direction.
void LineInput::move_arrow(int key_dir, bool extend_selection) {
    auto forward =
        (resolved_direction() == bidi::BaseDirection::RTL) ? (key_dir < 0) : (key_dir > 0);
    if (forward && cursor_pos_ < text_.size()) {
        move_cursor(Utf8Iterator::next(text_, cursor_pos_), extend_selection);
    } else if (!forward && cursor_pos_ > 0) {
        move_cursor(Utf8Iterator::prev(text_, cursor_pos_), extend_selection);
    }
}

// Which end of the current selection Left/Right should collapse to without
// Shift: the same forward/backward flip as move_arrow(), landing on
// sel_end() when this key means "forward" and sel_start() otherwise.
size_t LineInput::arrow_collapse_target(int key_dir) const {
    auto forward =
        (resolved_direction() == bidi::BaseDirection::RTL) ? (key_dir < 0) : (key_dir > 0);
    return forward ? sel_end() : sel_start();
}

void LineInput::move_word_left(bool extend_selection) {
    auto p = cursor_pos_;
    while (p > 0 && std::isspace(static_cast<unsigned char>(text_[p - 1]))) {
        p--;
    }
    while (p > 0 && !std::isspace(static_cast<unsigned char>(text_[p - 1]))) {
        p--;
    }
    move_cursor(p, extend_selection);
}

void LineInput::move_word_right(bool extend_selection) {
    auto p = cursor_pos_;

    while (p < text_.size() && !std::isspace(static_cast<unsigned char>(text_[p]))) {
        p++;
    }
    while (p < text_.size() && std::isspace(static_cast<unsigned char>(text_[p]))) {
        p++;
    }
    move_cursor(p, extend_selection);
}

void LineInput::select_word_at(size_t pos) {
    auto start = pos;
    auto end = pos;

    if (pos < text_.size() && !std::isspace(static_cast<unsigned char>(text_[pos]))) {
        while (start > 0 && !std::isspace(static_cast<unsigned char>(text_[start - 1]))) {
            start--;
        }
        while (end < text_.size() && !std::isspace(static_cast<unsigned char>(text_[end]))) {
            end++;
        }
    }
    sel_anchor_ = start;
    cursor_pos_ = end;
    sync_commands();
    reset_cursor_blink();
}

void LineInput::select_all() {
    sel_anchor_ = 0;
    cursor_pos_ = text_.size();
    sync_commands();
    if (window_) {
        window_->request_redraw("input state");
    }
}

void LineInput::sync_commands() {
    bool has_sel = has_selection();
    select_all_cmd->set_enabled(!text_.empty());
    cut_cmd->set_enabled(has_sel && !password_mode_ && !read_only_);
    copy_cmd->set_enabled(has_sel && !password_mode_);
    paste_cmd->set_enabled(!read_only_ && !Clipboard::get_text().empty());

    undo_cmd->set_enabled(undo_stack_.can_undo());
    if (undo_stack_.can_undo()) {
        undo_cmd->set_tooltip("Undo " + undo_stack_.undo_text());
    } else {
        undo_cmd->set_tooltip("Undo");
    }

    redo_cmd->set_enabled(undo_stack_.can_redo());
    if (undo_stack_.can_redo()) {
        redo_cmd->set_tooltip("Redo " + undo_stack_.redo_text());
    } else {
        redo_cmd->set_tooltip("Redo");
    }
}

void LineInput::undo() {
    if (undo_stack_.undo()) {
        sync_commands();
        if (window_) {
            window_->request_redraw("input state");
        }
    }
}

void LineInput::redo() {
    if (undo_stack_.redo()) {
        sync_commands();
        if (window_) {
            window_->request_redraw("input state");
        }
    }
}

float LineInput::icon_size() const {
    auto const &palette = Theme::current().palette;
    return palette.fonts.size + 2.0f;
}
float LineInput::icon_spacing() const {
    return 4.0f;
}

float LineInput::leading_inset() const {
    float inset = 0.0f;

    for (auto const &icon : leading_icons_) {
        if (!icon.visible || icon.visible(*this)) {
            inset += icon_size() + icon_spacing();
        }
    }
    return inset;
}

float LineInput::content_right_inset() const {
    auto const &style = Theme::current().style.lineInput;
    auto inset = style.padding.right;

    for (auto const &icon : trailing_icons_) {
        if (!icon.visible || icon.visible(*this)) {
            inset += icon_size() + icon_spacing();
        }
    }
    return inset;
}

float LineInput::content_left_inset() const {
    auto const &style = Theme::current().style.lineInput;
    return style.padding.left + leading_inset();
}

float LineInput::content_available_width() const {
    return rect_.width - content_left_inset() - content_right_inset();
}

Rect LineInput::icon_slot_rect(size_t from_right) const {
    auto const &style = Theme::current().style.lineInput;
    auto sz = icon_size();
    auto x = rect_.width - style.padding.right -
             static_cast<float>(from_right) * (sz + icon_spacing()) - sz;
    auto y = (rect_.height - sz) / 2.0f;

    return Rect{x, y, sz, sz};
}

Rect LineInput::leading_icon_slot_rect(size_t from_left) const {
    auto const &style = Theme::current().style.lineInput;
    auto sz = icon_size();
    auto x = style.padding.left + static_cast<float>(from_left) * (sz + icon_spacing());
    auto y = (rect_.height - sz) / 2.0f;

    return Rect{x, y, sz, sz};
}

// Trailing icons are painted from the right edge inward, so that vector is
// walked in reverse -- the last visible icon occupies slot 0; leading icons
// mirror it from the left edge forward.
std::optional<LineInput::IconHit> LineInput::hit_icon(Point pos) const {
    auto visible_index = 0;
    for (size_t i = trailing_icons_.size(); i-- > 0;) {
        auto const &icon = trailing_icons_[i];
        if (icon.visible && !icon.visible(*this)) {
            continue;
        }
        if (icon_slot_rect(visible_index).contains(pos)) {
            return IconHit{false, i};
        }
        visible_index++;
    }

    visible_index = 0;
    for (size_t i = leading_icons_.size(); i-- > 0;) {
        auto const &icon = leading_icons_[i];
        if (icon.visible && !icon.visible(*this)) {
            continue;
        }
        if (leading_icon_slot_rect(visible_index).contains(pos)) {
            return IconHit{true, i};
        }
        visible_index++;
    }
    return std::nullopt;
}
std::optional<text::TextLayout> LineInput::build_layout() const {
    if (password_mode_) {
        return std::nullopt;
    }
    auto *plat = detail::current_platform();
    auto *shaper = plat ? plat->shaper() : nullptr;
    if (!shaper) {
        return std::nullopt;
    }
    return text::TextLayout(text_, resolved_direction(), *shaper,
                            Theme::current().palette.fonts.size);
}

size_t LineInput::pos_from_x(float x) const {
    auto const &style = Theme::current().style.lineInput;

    if (auto layout = build_layout()) {
        auto rtl_base = layout->bidi_line().base() == bidi::BaseDirection::RTL;
        // content_right_inset() already folds in style.padding.right plus any
        // clear/peek button reservation -- mirrors the inset BaseTheme::draw_line_input
        // is handed, so hit-testing always agrees with where the glyphs were painted.
        auto tx = rtl_base
                      ? rect_.width - content_right_inset() - layout->total_width() + scroll_offset_
                      : style.padding.left - scroll_offset_;
        return layout->index_from_x(x - tx);
    }

    auto const &palette = Theme::current().palette;
    auto click_x = x - content_left_inset() + scroll_offset_;
    size_t current_pos = 0;

    if (click_x <= 0) {
        return 0;
    }

    while (current_pos < text_.size()) {
        size_t next_pos = Utf8Iterator::next(text_, current_pos);
        std::string before =
            password_mode_ ? get_masked_text(text_, next_pos) : text_.substr(0, next_pos);
        auto sz = measure_text(before, palette.fonts.size);
        if (sz.width > click_x) {
            // Check if we are closer to the previous or next character
            std::string before_prev =
                password_mode_ ? get_masked_text(text_, current_pos) : text_.substr(0, current_pos);
            auto prev_sz = measure_text(before_prev, palette.fonts.size);
            if (click_x - prev_sz.width < sz.width - click_x) {
                return current_pos;
            } else {
                return next_pos;
            }
        }
        current_pos = next_pos;
    }
    return current_pos;
}

bool LineInput::is_valid() const {
    if (validator_) {
        return validator_(text_, *this);
    }
    return true;
}

void LineInput::paint(Painter &painter) {
    auto const &theme = Theme::current();
    auto const &style = theme.style.lineInput;
    auto rect = Rect{0, 0, rect_.width, rect_.height};
    auto sel_start_pos = has_selection() ? static_cast<int>(sel_start()) : -1;
    auto sel_end_pos = has_selection() ? static_cast<int>(sel_end()) : -1;

    std::optional<Color> bg;
    if (validation_mode_ == ValidationMode::Notify && !is_valid()) {
        bg = theme.palette.error;
    }

    auto layout = build_layout();
    ensure_cursor_visible(painter, layout ? &*layout : nullptr);

    auto d_text = password_mode_ ? get_masked_text(text_) : text_;
    auto cursor_visible = true;
    if (is_focused()) {
        auto elapsed = std::chrono::steady_clock::now() - cursor_blink_time_;
        cursor_visible =
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() % 1000 < 500;
    }

    auto wstate = WidgetState{
        .interaction = ButtonState::Normal,
        .focused = is_focused(),
        .enabled = !read_only_,
        .window_active = window_ ? window_->is_active() : true,
    };
    // content_right_inset()/leading_inset() already account for the icons on
    // top of style.padding on each side; the theme keeps text/caret/selection
    // clear of them via these insets.
    auto button_inset_right = content_right_inset() - style.padding.right;
    auto button_inset_left = leading_inset();
    theme.draw_line_input(painter, rect, d_text, placeholder_, static_cast<int>(cursor_pos_),
                          sel_start_pos, sel_end_pos, wstate, password_mode_, scroll_offset_, bg,
                          cursor_visible, layout ? &*layout : nullptr, button_inset_left,
                          button_inset_right);

    paint_buttons(painter);
}

// FIXME: move this to use the theme paint buttons primitives
void LineInput::paint_buttons(Painter &painter) {
    auto visible_index = 0;
    for (size_t i = trailing_icons_.size(); i-- > 0;) {
        auto const &icon = trailing_icons_[i];
        if (icon.visible && !icon.visible(*this)) {
            continue;
        }
        auto slot = icon_slot_rect(visible_index);
        auto hovered = hovered_icon_ == IconHit{false, i};
        auto pressed = pressed_icon_ == IconHit{false, i};
        icon.paint(painter, slot, *this, hovered, pressed);
        visible_index++;
    }

    visible_index = 0;
    for (size_t i = leading_icons_.size(); i-- > 0;) {
        auto const &icon = leading_icons_[i];
        if (icon.visible && !icon.visible(*this)) {
            continue;
        }
        auto slot = leading_icon_slot_rect(visible_index);
        auto hovered = hovered_icon_ == IconHit{true, i};
        auto pressed = pressed_icon_ == IconHit{true, i};
        icon.paint(painter, slot, *this, hovered, pressed);
        visible_index++;
    }
}

void LineInput::paint_clear_icon(Painter &painter, Rect const &slot, bool hovered,
                                 bool pressed) const {
    auto const &palette = Theme::current().palette;
    auto sz = slot.width;
    auto cx = slot.x + sz / 2.0f;
    auto cy = slot.y + sz / 2.0f;
    auto r = sz / 2.0f;

    if (hovered || pressed) {
        Color circle_bg = palette.text;
        circle_bg.a = pressed ? 0.22f : 0.12f;
        painter.fill_rounded_rect(slot, circle_bg, r);
    }

    auto x_col = palette.text;
    auto half = sz * 0.22f;
    x_col.a = hovered ? 0.8f : 0.45f;
    painter.draw_line({cx - half, cy - half}, {cx + half, cy + half}, x_col, 1.5f);
    painter.draw_line({cx - half, cy + half}, {cx + half, cy - half}, x_col, 1.5f);
}

void LineInput::paint_peek_icon(Painter &painter, Rect const &slot, bool hovered,
                                bool pressed) const {
    auto const &palette = Theme::current().palette;
    auto sz = slot.width;
    auto cx = slot.x + sz / 2.0f;
    auto cy = slot.y + sz / 2.0f;
    auto r = sz / 2.0f;

    if (hovered || pressed) {
        Color circle_bg = palette.text;
        circle_bg.a = pressed ? 0.22f : 0.12f;
        painter.fill_rounded_rect(slot, circle_bg, r);
    }

    auto eye_col = palette.text;
    eye_col.a = hovered ? 0.8f : 0.45f;

    auto eye_radius = sz * 0.35f;
    auto pupil_radius = sz * 0.15f;

    painter.draw_circle({cx, cy}, eye_radius, eye_col, 1.2f);

    if (password_mode_) {
        painter.fill_circle({cx, cy}, pupil_radius * 0.6f, eye_col);
    } else {
        painter.fill_circle({cx, cy}, pupil_radius, eye_col);
    }
}

void LineInput::paint_caps_lock_icon(Painter &painter, Rect const &slot, bool hovered,
                                      bool pressed) const {
    switch (caps_lock_icon_style_) {
    case CapsLockIcon::FilledCircle:
        paint_caps_lock_filled_circle(painter, slot, hovered);
        break;
    case CapsLockIcon::ChevronBar:
        paint_caps_lock_chevron_bar(painter, slot, hovered);
        break;
    case CapsLockIcon::OutlineCircle:
        paint_caps_lock_outline_circle(painter, slot, hovered);
        break;
    case CapsLockIcon::Letter:
    default:
        paint_caps_lock_letter(painter, slot, hovered);
        break;
    }
}

// The letter caps produces, knocked out of a filled circle.
void LineInput::paint_caps_lock_letter(Painter &painter, Rect const &slot, bool hovered) const {
    auto const &palette = Theme::current().palette;
    auto sz = slot.width;
    auto cx = slot.x + sz / 2.0f;
    auto cy = slot.y + sz / 2.0f;

    auto circle_col = palette.text;
    circle_col.a = hovered ? 0.85f : 0.55f;
    painter.fill_circle({cx, cy}, sz * 0.42f, circle_col);

    // draw_text takes the baseline, so center via the font's own metrics.
    auto font_sz = palette.fonts.size * 0.6f;
    auto fm = painter.font_metrics(font_sz);
    auto m = painter.measure_text("A", font_sz);
    painter.draw_text("A", {cx - m.width / 2.0f, cy + (fm.ascent - fm.descent) / 2.0f},
                      palette.base, font_sz);
}

// A filled circle with the up chevron knocked out -- the macOS key glyph.
void LineInput::paint_caps_lock_filled_circle(Painter &painter, Rect const &slot,
                                              bool hovered) const {
    auto const &palette = Theme::current().palette;
    auto sz = slot.width;
    auto cx = slot.x + sz / 2.0f;
    auto cy = slot.y + sz / 2.0f;

    auto circle_col = palette.text;
    circle_col.a = hovered ? 0.85f : 0.55f;
    painter.fill_circle({cx, cy}, sz * 0.42f, circle_col);

    auto chevron_col = palette.base;
    auto w = sz * 0.20f;
    auto h = sz * 0.16f;
    painter.draw_line({cx - w, cy + h * 0.5f}, {cx, cy - h}, chevron_col, 1.4f);
    painter.draw_line({cx, cy - h}, {cx + w, cy + h * 0.5f}, chevron_col, 1.4f);
}

// The bare key glyph: an up chevron over a horizontal bar, no circle.
void LineInput::paint_caps_lock_chevron_bar(Painter &painter, Rect const &slot,
                                             bool hovered) const {
    auto const &palette = Theme::current().palette;
    auto sz = slot.width;
    auto cx = slot.x + sz / 2.0f;
    auto cy = slot.y + sz / 2.0f;

    auto col = palette.text;
    col.a = hovered ? 0.8f : 0.45f;

    auto w = sz * 0.26f;
    auto h = sz * 0.20f;
    painter.draw_line({cx - w, cy}, {cx, cy - h}, col, 1.4f);
    painter.draw_line({cx, cy - h}, {cx + w, cy}, col, 1.4f);
    painter.draw_line({cx - w * 0.8f, cy + h * 0.7f}, {cx + w * 0.8f, cy + h * 0.7f}, col, 1.4f);
}

// An outlined circle with an up chevron inside.
void LineInput::paint_caps_lock_outline_circle(Painter &painter, Rect const &slot,
                                               bool hovered) const {
    auto const &palette = Theme::current().palette;
    auto sz = slot.width;
    auto cx = slot.x + sz / 2.0f;
    auto cy = slot.y + sz / 2.0f;

    auto col = palette.text;
    col.a = hovered ? 0.8f : 0.45f;

    painter.draw_circle({cx, cy}, sz * 0.38f, col, 1.2f);
    auto w = sz * 0.20f;
    auto h = sz * 0.16f;
    painter.draw_line({cx - w, cy + h * 0.5f}, {cx, cy - h}, col, 1.2f);
    painter.draw_line({cx, cy - h}, {cx + w, cy + h * 0.5f}, col, 1.2f);
}

bool LineInput::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Move) {
        auto over = hit_icon(event.position);
        bool changed = (over != hovered_icon_);
        hovered_icon_ = over;
        if (changed && window()) {
            window()->request_redraw("input state");
        }
        return changed;
    }

    if (event.type == MouseEvent::Type::Leave) {
        if (hovered_icon_) {
            hovered_icon_ = std::nullopt;
            if (window()) {
                window()->request_redraw("input state");
            }
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Press) {
        auto const local_rect = Rect{0, 0, rect_.width, rect_.height};

        if (!local_rect.contains(event.position)) {
            return false;
        }

        if (event.button == 1) {
            show_context_menu(event.position);
            return true;
        }

        if (auto hit = hit_icon(event.position)) {
            auto const &icons = hit->leading ? leading_icons_ : trailing_icons_;
            auto const &icon = icons[hit->index];
            if (icon.click) {
                pressed_icon_ = hit;
                if (window()) {
                    window()->request_redraw("input state");
                }
            }
            return true;
        }

        auto pos = pos_from_x(event.position.x);

        if (event.click_count == 2) {
            select_word_at(pos);
        } else if (event.click_count >= 3) {
            sel_anchor_ = 0;
            cursor_pos_ = text_.size();
            sync_commands();
            reset_cursor_blink();
        } else {
            move_cursor(pos, event.shift);
            dragging_ = true;
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Release) {
        if (pressed_icon_) {
            auto hit = *pressed_icon_;
            pressed_icon_ = std::nullopt;
            if (hit_icon(event.position) == hit) {
                auto const &icons = hit.leading ? leading_icons_ : trailing_icons_;
                auto const &icon = icons[hit.index];
                if (icon.click) {
                    icon.click(*this);
                }
            }
            if (window()) {
                window()->request_redraw("input state");
            }
            return true;
        }
        dragging_ = false;
        return false;
    }

    if (event.type == MouseEvent::Type::Drag && dragging_) {
        cursor_pos_ = pos_from_x(event.position.x);
        sync_commands();
        reset_cursor_blink();
        return true;
    }

    return false;
}

void LineInput::ensure_cursor_visible(Painter &painter, text::TextLayout const *layout) {
    auto content_w = content_available_width();
    auto cursor_x = [&] {
        if (layout) {
            return layout->caret_x(cursor_pos_);
        }
        auto const &palette = Theme::current().palette;
        auto before_str =
            password_mode_ ? get_masked_text(text_, cursor_pos_) : text_.substr(0, cursor_pos_);
        return before_str.empty() ? 0.0f
                                  : painter.measure_text(before_str, palette.fonts.size).width;
    }();

    if (cursor_x - scroll_offset_ > content_w) {
        scroll_offset_ = cursor_x - content_w;
    } else if (cursor_x - scroll_offset_ < 0) {
        scroll_offset_ = cursor_x;
    }

    if (scroll_offset_ < 0) {
        scroll_offset_ = 0;
    }
}

bool LineInput::handle_key(KeyEvent const &event) {
    if (Widget::handle_key(event)) {
        return true;
    }
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }
    reset_cursor_blink();

    // A caps lock toggle changes the leading icon set; the visibility
    // predicate is evaluated live at paint time, so a repaint is all it needs.
    if (event.key == Key::CapsLock && window()) {
        window()->request_redraw("caps lock");
    }

    switch (event.key) {
    case Key::Backspace:
        if (read_only_) {
            return true;
        }
        if (has_selection()) {
            if (validation_mode_ == ValidationMode::Block && validator_) {
                std::string next_text = text_;
                next_text.erase(sel_start(), sel_end() - sel_start());
                if (!validator_(next_text, *this)) {
                    return true;
                }
            }
            delete_selection();
        } else if (cursor_pos_ > 0) {
            size_t prev = Utf8Iterator::prev(text_, cursor_pos_);
            if (validation_mode_ == ValidationMode::Block && validator_) {
                std::string next_text = text_;
                next_text.erase(prev, cursor_pos_ - prev);
                if (!validator_(next_text, *this)) {
                    return true;
                }
            }
            std::string old_text = text_.substr(prev, cursor_pos_ - prev);
            undo_stack_.push(std::make_unique<TextCommand>(&text_, old_text, "", prev));
            text_.erase(prev, cursor_pos_ - prev);
            cursor_pos_ = prev;
            sel_anchor_ = cursor_pos_;
            sync_commands();
            if (on_change) {
                on_change(text_, *this);
            }
        }
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    case Key::Delete:
        if (read_only_) {
            return true;
        }
        if (has_selection()) {
            if (validation_mode_ == ValidationMode::Block && validator_) {
                std::string next_text = text_;
                next_text.erase(sel_start(), sel_end() - sel_start());
                if (!validator_(next_text, *this)) {
                    return true;
                }
            }
            delete_selection();
        } else if (cursor_pos_ < text_.size()) {
            auto next = Utf8Iterator::next(text_, cursor_pos_);

            if (validation_mode_ == ValidationMode::Block && validator_) {
                std::string next_text = text_;
                next_text.erase(cursor_pos_, next - cursor_pos_);
                if (!validator_(next_text, *this)) {
                    return true;
                }
            }
            text_.erase(cursor_pos_, next - cursor_pos_);
            sync_commands();
            if (on_change) {
                on_change(text_, *this);
            }
        }
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    case Key::Left:
        if (event.alt) {
            move_word_left(event.shift);
        } else if (!event.shift && has_selection()) {
            move_cursor(arrow_collapse_target(-1), false);
        } else {
            move_arrow(-1, event.shift);
        }
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    case Key::Right:
        if (event.alt) {
            move_word_right(event.shift);
        } else if (!event.shift && has_selection()) {
            move_cursor(arrow_collapse_target(1), false);
        } else {
            move_arrow(1, event.shift);
        }
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    case Key::Home:
        move_cursor(0, event.shift);
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    case Key::End:
        move_cursor(text_.size(), event.shift);
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    case Key::Enter:
        if (read_only_) {
            return true;
        }
        if (on_submit) {
            on_submit(text_, *this);
        }
        return true;
    // Windows convention for toggling paragraph direction: the chord can be
    // pressed in either order, so each side just checks whether the *other*
    // modifier is already down when it sees its own key go down.
    case Key::LeftShift:
        if (event.ctrl) {
            set_text_direction(TextDirection::LTR);
            return true;
        }
        return false;
    case Key::RightShift:
        if (event.ctrl) {
            set_text_direction(TextDirection::RTL);
            return true;
        }
        return false;
    case Key::LeftControl:
        if (event.shift) {
            set_text_direction(TextDirection::LTR);
            return true;
        }
        return false;
    case Key::RightControl:
        if (event.shift) {
            set_text_direction(TextDirection::RTL);
            return true;
        }
        return false;
    default:
        break;
    }

    if (!event.text.empty() && !event.ctrl && !event.alt && !event.super) {
        if (read_only_) {
            return false;
        }

        std::string next_text = text_;
        size_t start_pos = cursor_pos_;
        std::string old_sel;
        if (has_selection()) {
            start_pos = sel_start();
            old_sel = text_.substr(start_pos, sel_end() - start_pos);
            next_text.erase(start_pos, sel_end() - start_pos);
        }
        next_text.insert(start_pos, event.text);

        if (validation_mode_ == ValidationMode::Block && validator_ &&
            !validator_(next_text, *this)) {
            return true;
        }

        if (has_selection()) {
            delete_selection();
        }

        text_.insert(start_pos, event.text);
        undo_stack_.push(std::make_unique<TextCommand>(&text_, old_sel, event.text, start_pos));
        cursor_pos_ = start_pos + event.text.size();
        sel_anchor_ = cursor_pos_;
        sync_commands();
        if (on_change) {
            on_change(text_, *this);
        }
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    }

    return false;
}

void LineInput::on_focus() {
    reset_cursor_blink();
    if (window_ && blink_timer_id_ == 0) {
        // FIXME: cursor blink time should be defined by platform
        blink_timer_id_ = window_->start_timer(
            0.5f,
            [this] {
                if (is_effectively_visible()) {
                    window_->request_redraw("blink");
                }
            },
            true);
    }
}

void LineInput::on_blur() {
    if (window_ && blink_timer_id_ != 0) {
        window_->stop_timer(blink_timer_id_);
        blink_timer_id_ = 0;
    }
}

void LineInput::cut() {
    if (!has_selection() || password_mode_ || read_only_) {
        return;
    }
    Clipboard::set_text(text_.substr(sel_start(), sel_end() - sel_start()));
    delete_selection();
}

void LineInput::copy() {
    if (!has_selection() || password_mode_) {
        return;
    }
    Clipboard::set_text(text_.substr(sel_start(), sel_end() - sel_start()));
}

void LineInput::paste() {
    if (read_only_) {
        return;
    }
    auto clip = Clipboard::get_text();

    if (clip.empty()) {
        return;
    }
    if (validation_mode_ == ValidationMode::Block && validator_) {
        auto next_text = text_;
        if (has_selection()) {
            next_text.erase(sel_start(), sel_end() - sel_start());
        }
        auto insert_pos = has_selection() ? sel_start() : cursor_pos_;

        next_text.insert(insert_pos, clip);
        if (!validator_(next_text, *this)) {
            return;
        }
    }
    if (has_selection()) {
        delete_selection();
    }
    text_.insert(cursor_pos_, clip);
    cursor_pos_ += clip.size();
    sel_anchor_ = cursor_pos_;
    sync_commands();
    if (on_change) {
        on_change(text_, *this);
    }
}

void LineInput::show_context_menu(Point pos) {
    if (!window()) {
        return;
    }

    auto has_sel = has_selection();
    auto not_empty = !text_.empty();
    auto can_paste = !Clipboard::get_text().empty();

    // FIXME: use i18n for menu text
    std::vector<MenuItem> items;
    items.push_back(MenuItem::action(undo_cmd));
    items.push_back(MenuItem::action(redo_cmd));
    items.push_back(MenuItem::sep());
    items.push_back(MenuItem::action(cut_cmd));
    items.push_back(MenuItem::action(copy_cmd));
    items.push_back(MenuItem::action(paste_cmd));
    items.push_back(MenuItem::sep());
    items.push_back(MenuItem::action(select_all_cmd));

    auto delete_cmd =
        Command::create("Delete", [this] { delete_selection(); }, !read_only_ && has_sel);
    delete_cmd->set_shortcut("Delete");
    items.push_back(MenuItem::action(delete_cmd));

    items.push_back(MenuItem::sep());
    auto direction_menu = std::make_shared<Menu>("Text Direction");
    // A radio group: each carries a mark, so each is declared checkable before its
    // state is set -- set_checked() on a command that is not is ignored.
    auto auto_cmd = Command::create("Auto", [this] { set_text_direction(TextDirection::Auto); });
    auto_cmd->set_checkable(true);
    auto_cmd->set_checked(direction_mode_ == TextDirection::Auto);
    auto ltr_cmd =
        Command::create("Left to Right", [this] { set_text_direction(TextDirection::LTR); });
    ltr_cmd->set_checkable(true);
    ltr_cmd->set_checked(direction_mode_ == TextDirection::LTR);
    auto rtl_cmd =
        Command::create("Right to Left", [this] { set_text_direction(TextDirection::RTL); });
    rtl_cmd->set_checkable(true);
    rtl_cmd->set_checked(direction_mode_ == TextDirection::RTL);
    direction_menu->add_action(auto_cmd);
    direction_menu->add_action(ltr_cmd);
    direction_menu->add_action(rtl_cmd);
    items.push_back(MenuItem::submenu_item("Text Direction", direction_menu));

    context_menu_ = std::make_unique<ContextMenu>(std::move(items));
    context_menu_->show(window(), map_to_window(pos));
}

Size LineInput::size_hint() const {
    auto const &style = Theme::current().style.lineInput;
    auto const &palette = Theme::current().palette;
    auto fm = font_metrics(palette.fonts.size);
    auto h = fm.height + style.padding.top + style.padding.bottom;
    return {150.0f, h};
}

} // namespace svision3
