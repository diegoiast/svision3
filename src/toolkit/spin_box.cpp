// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/spin_box.hpp"
#include "toolkit/theme.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>

namespace toolkit {

SpinBox::SpinBox(int value, int min_val, int max_val, int step)
    : value_(value), min_val_(min_val), max_val_(max_val), step_(step),
      cursor_blink_time_(std::chrono::steady_clock::now()) {
    focusable_ = true;
    relative_coords_ = true;
    sync_text();
}

void SpinBox::set_value(int v) {
    v = std::clamp(v, min_val_, max_val_);
    if (v == value_) {
        return;
    }
    value_ = v;
    sync_text();
}

void SpinBox::set_range(int min_val, int max_val) {
    min_val_ = min_val;
    max_val_ = max_val;
    set_value(value_);
}

void SpinBox::set_focused(bool focused) {
    focused_ = focused;
    if (focused) {
        editing_ = true;
        cursor_pos_ = text_.size();
        sel_anchor_ = 0;
        cursor_blink_time_ = std::chrono::steady_clock::now();
    } else {
        commit_text();
        editing_ = false;
    }
}

void SpinBox::sync_text() {
    text_ = std::to_string(value_);
    cursor_pos_ = text_.size();
    sel_anchor_ = cursor_pos_;
}

void SpinBox::commit_text() {
    auto parsed_value = value_;
    auto [end_pointer, error_code] =
        std::from_chars(text_.data(), text_.data() + text_.size(), parsed_value);

    if (error_code == std::errc{}) {
        parsed_value = std::clamp(parsed_value, min_val_, max_val_);
        if (parsed_value != value_) {
            value_ = parsed_value;
            if (on_change) {
                on_change(value_);
            }
        }
    }
    sync_text();
}

void SpinBox::step_up() {
    auto old = value_;
    value_ = std::clamp(value_ + step_, min_val_, max_val_);
    sync_text();
    if (value_ != old && on_change) {
        on_change(value_);
    }
}

void SpinBox::step_down() {
    auto old = value_;
    value_ = std::clamp(value_ - step_, min_val_, max_val_);
    sync_text();
    if (value_ != old && on_change) {
        on_change(value_);
    }
}

float SpinBox::btn_width() const { return 20.0f; }

Rect SpinBox::up_btn_rect() const {
    auto bw = btn_width();
    return {rect_.width - bw, 0, bw, rect_.height / 2.0f};
}

Rect SpinBox::down_btn_rect() const {
    auto bw = btn_width();
    auto half = rect_.height / 2.0f;
    return {rect_.width - bw, half, bw, rect_.height - half};
}

SpinBox::HitZone SpinBox::hit_zone(Point pos) const {
    if (pos.x < 0 || pos.x > rect_.width || pos.y < 0 || pos.y > rect_.height) {
        return HitZone::None;
    }
    if (up_btn_rect().contains(pos)) {
        return HitZone::Up;
    }
    if (down_btn_rect().contains(pos)) {
        return HitZone::Down;
    }
    return HitZone::Field;
}

CursorShape SpinBox::cursor() const {
    switch (hovered_zone_) {
    case HitZone::Up:
    case HitZone::Down:
        return CursorShape::Arrow;
    case HitZone::Field:
        return CursorShape::IBeam;
    default:
        return CursorShape::Arrow;
    }
}

Size SpinBox::size_hint() const {
    auto const &style = Theme::current().line_input;
    auto h = style.font_size + style.padding.top + style.padding.bottom + 8.0f;
    auto max_text = std::to_string(max_val_);
    auto sz = Painter::measure_text(max_text, style.font_size);
    auto w = sz.width + style.padding.left + style.padding.right + btn_width() + 16.0f;
    return {w, h};
}

void SpinBox::paint(Painter &painter) {
    auto const &style = Theme::current().line_input;
    auto bg = focused_ ? style.background_focused : style.background;
    auto border = focused_ ? style.border_focused : style.border;
    auto bw = btn_width();
    auto field_rect = Rect{0.0f, 0.0f, rect_.width - bw, rect_.height};

    painter.draw_frame(field_rect, bg, border, style, true);

    // Up button
    auto up = up_btn_rect();
    {
        auto const &btn_style = Theme::current().button;
        auto hov = hovered_zone_ == HitZone::Up;
        auto press = pressed_zone_ == HitZone::Up;
        auto btn_bg = btn_style.background;
        auto cx = up.x + up.width / 2.0f;
        auto cy = up.y + up.height / 2.0f;
        auto arrow_sz = 3.5f;

        if (press && btn_style.background_pressed) {
            btn_bg = *btn_style.background_pressed;
        } else if (hov && btn_style.background_hovered) {
            btn_bg = *btn_style.background_hovered;
        }
        painter.draw_frame(up, btn_bg, border, btn_style, false);
        painter.draw_line({cx - arrow_sz, cy + arrow_sz * 0.4f}, {cx, cy - arrow_sz * 0.4f},
                          style.text, 1.5f);
        painter.draw_line({cx, cy - arrow_sz * 0.4f}, {cx + arrow_sz, cy + arrow_sz * 0.4f},
                          style.text, 1.5f);
    }

    // Down button
    auto down = down_btn_rect();
    {
        auto hov = hovered_zone_ == HitZone::Down;
        auto press = pressed_zone_ == HitZone::Down;
        auto const &btn_style = Theme::current().button;
        auto btn_bg = btn_style.background;
        auto cx = down.x + down.width / 2.0f;
        auto cy = down.y + down.height / 2.0f;
        auto arrow_sz = 3.5f;

        if (press && btn_style.background_pressed) {
            btn_bg = *btn_style.background_pressed;
        } else if (hov && btn_style.background_hovered) {
            btn_bg = *btn_style.background_hovered;
        }
        painter.draw_frame(down, btn_bg, border, btn_style, false);
        painter.draw_line({cx - arrow_sz, cy - arrow_sz * 0.4f}, {cx, cy + arrow_sz * 0.4f},
                          style.text, 1.5f);
        painter.draw_line({cx, cy + arrow_sz * 0.4f}, {cx + arrow_sz, cy - arrow_sz * 0.4f},
                          style.text, 1.5f);
    }

    // Text
    auto fm = painter.font_metrics(style.font_size);
    auto baseline_y = (rect_.height - fm.height) / 2.0f + fm.ascent;
    auto content_x = style.padding.left;
    auto content_w = rect_.width - bw - style.padding.left - style.padding.right;

    painter.push_clip({content_x, 0, content_w, rect_.height});
    if (focused_ && editing_) {
        auto s = std::min(sel_anchor_, cursor_pos_);
        auto e = std::max(sel_anchor_, cursor_pos_);
        if (s != e) {
            auto ex = content_x + painter.text_size(text_.substr(0, e), style.font_size).width;
            auto hy = (rect_.height - fm.height) / 2.0f - 1.0f;
            auto sel_bg = Color::rgba(0.26f, 0.52f, 0.96f, 0.35f);
            auto sx = content_x +
                      (s > 0 ? painter.text_size(text_.substr(0, s), style.font_size).width : 0.0f);
            painter.fill_rect({sx, hy, ex - sx, fm.height + 2.0f}, sel_bg);
        }
    }
    painter.draw_text(text_, {content_x, baseline_y}, style.text, style.font_size);
    if (focused_ && editing_) {
        auto elapsed = std::chrono::steady_clock::now() - cursor_blink_time_;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        if ((ms / 500) % 2 == 0) {
            auto before = text_.substr(0, cursor_pos_);
            auto cx = content_x;
            auto cy_top = (rect_.height - fm.height) / 2.0f - 1.0f;
            auto cy_bot = cy_top + fm.height + 2.0f;

            if (!before.empty()) {
                cx += painter.text_size(before, style.font_size).width;
            }
            painter.draw_line({cx, cy_top}, {cx, cy_bot}, style.cursor, 1.5f);
        }
    }

    painter.pop_clip();
}

bool SpinBox::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Move) {
        hovered_zone_ = hit_zone(event.position);
        return false;
    }

    if (event.type == MouseEvent::Type::Press) {
        auto zone = hit_zone(event.position);
        if (zone == HitZone::None) {
            return false;
        }
        pressed_zone_ = zone;

        if (zone == HitZone::Up) {
            step_up();
            return true;
        }
        if (zone == HitZone::Down) {
            step_down();
            return true;
        }
        if (zone == HitZone::Field) {
            editing_ = true;
            cursor_pos_ = text_.size();
            sel_anchor_ = 0;
            cursor_blink_time_ = std::chrono::steady_clock::now();
            return true;
        }
    }

    if (event.type == MouseEvent::Type::Release) {
        pressed_zone_ = HitZone::None;
        return false;
    }

    if (event.type == MouseEvent::Type::Scroll) {
        auto zone = hit_zone(event.position);
        if (zone == HitZone::None) {
            return false;
        }
        if (event.scroll_dy > 0) {
            step_up();
        } else if (event.scroll_dy < 0) {
            step_down();
        }
        return true;
    }

    return false;
}

bool SpinBox::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }
    cursor_blink_time_ = std::chrono::steady_clock::now();

    switch (event.key) {
    case Key::Up:
        step_up();
        return true;
    case Key::Down:
        step_down();
        return true;
    case Key::Enter:
        commit_text();
        return true;
    case Key::Home:
        cursor_pos_ = 0;
        if (!event.shift) {
            sel_anchor_ = cursor_pos_;
        }
        return true;
    case Key::End:
        cursor_pos_ = text_.size();
        if (!event.shift) {
            sel_anchor_ = cursor_pos_;
        }
        return true;
    case Key::Left:
        if (cursor_pos_ > 0) {
            cursor_pos_--;
        }
        if (!event.shift) {
            sel_anchor_ = cursor_pos_;
        }
        return true;
    case Key::Right:
        if (cursor_pos_ < text_.size()) {
            cursor_pos_++;
        }
        if (!event.shift) {
            sel_anchor_ = cursor_pos_;
        }
        return true;
    case Key::Backspace: {
        size_t s = std::min(sel_anchor_, cursor_pos_);
        size_t e = std::max(sel_anchor_, cursor_pos_);
        if (s != e) {
            text_.erase(s, e - s);
            cursor_pos_ = s;
            sel_anchor_ = s;
        } else if (cursor_pos_ > 0) {
            text_.erase(cursor_pos_ - 1, 1);
            cursor_pos_--;
            sel_anchor_ = cursor_pos_;
        }
        return true;
    }
    case Key::Delete: {
        size_t s = std::min(sel_anchor_, cursor_pos_);
        size_t e = std::max(sel_anchor_, cursor_pos_);
        if (s != e) {
            text_.erase(s, e - s);
            cursor_pos_ = s;
            sel_anchor_ = s;
        } else if (cursor_pos_ < text_.size()) {
            text_.erase(cursor_pos_, 1);
        }
        return true;
    }
    default:
        break;
    }

    if (event.super && !event.text.empty() && event.text[0] == 'a') {
        sel_anchor_ = 0;
        cursor_pos_ = text_.size();
        return true;
    }

    if (!event.text.empty()) {
        auto ch = event.text[0];
        if (ch >= '0' && ch <= '9') {
            auto s = std::min(sel_anchor_, cursor_pos_);
            auto e = std::max(sel_anchor_, cursor_pos_);

            if (s != e) {
                text_.erase(s, e - s);
            }
            cursor_pos_ = s;
            text_.insert(cursor_pos_, 1, ch);
            cursor_pos_++;
            sel_anchor_ = cursor_pos_;
            return true;
        }
        if (ch == '-' && cursor_pos_ == 0 && min_val_ < 0) {
            auto s = std::min(sel_anchor_, cursor_pos_);
            auto e = std::max(sel_anchor_, cursor_pos_);

            if (s != e) {
                text_.erase(s, e - s);
            }
            cursor_pos_ = s;
            text_.insert(cursor_pos_, 1, ch);
            cursor_pos_++;
            sel_anchor_ = cursor_pos_;
            return true;
        }
    }
    return false;
}

} // namespace toolkit
