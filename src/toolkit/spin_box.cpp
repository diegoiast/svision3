#include "toolkit/spin_box.hpp"
#include "toolkit/theme.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>

namespace toolkit {

SpinBox::SpinBox(int value, int min_val, int max_val, int step)
    : value_(value), min_val_(min_val), max_val_(max_val), step_(step),
      cursor_blink_time_(std::chrono::steady_clock::now()) {
    sync_text();
}

void SpinBox::set_value(int v) {
    v = std::clamp(v, min_val_, max_val_);
    if (v == value_) return;
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
    int v = value_;
    auto [ptr, ec] = std::from_chars(text_.data(), text_.data() + text_.size(), v);
    if (ec == std::errc{}) {
        v = std::clamp(v, min_val_, max_val_);
        if (v != value_) {
            value_ = v;
            if (on_change) on_change(value_);
        }
    }
    sync_text();
}

void SpinBox::step_up() {
    int old = value_;
    value_ = std::clamp(value_ + step_, min_val_, max_val_);
    sync_text();
    if (value_ != old && on_change) on_change(value_);
}

void SpinBox::step_down() {
    int old = value_;
    value_ = std::clamp(value_ - step_, min_val_, max_val_);
    sync_text();
    if (value_ != old && on_change) on_change(value_);
}

float SpinBox::btn_width() const {
    return 20.0f;
}

Rect SpinBox::up_btn_rect() const {
    float bw = btn_width();
    return {rect_.x + rect_.width - bw, rect_.y, bw, rect_.height / 2.0f};
}

Rect SpinBox::down_btn_rect() const {
    float bw = btn_width();
    float half = rect_.height / 2.0f;
    return {rect_.x + rect_.width - bw, rect_.y + half, bw, rect_.height - half};
}

SpinBox::HitZone SpinBox::hit_zone(Point pos) const {
    if (!hit_test(pos)) return HitZone::None;
    if (up_btn_rect().contains(pos)) return HitZone::Up;
    if (down_btn_rect().contains(pos)) return HitZone::Down;
    return HitZone::Field;
}

CursorShape SpinBox::cursor() const {
    switch (hovered_zone_) {
    case HitZone::Up:
    case HitZone::Down:  return CursorShape::Arrow;
    case HitZone::Field: return CursorShape::IBeam;
    default:             return CursorShape::Arrow;
    }
}

Size SpinBox::size_hint() const {
    auto const &style = Theme::current().line_input;
    float h = style.font_size + style.padding.top + style.padding.bottom + 8.0f;
    auto max_text = std::to_string(max_val_);
    auto sz = Painter::measure_text(max_text, style.font_size);
    float w = sz.width + style.padding.left + style.padding.right + btn_width() + 16.0f;
    return {w, h};
}

void SpinBox::paint(Painter &painter) {
    auto const &style = Theme::current().line_input;

    Color bg = focused_ ? style.background_focused : style.background;
    Color border = focused_ ? style.border_focused : style.border;
    float bw = btn_width();

    Rect field_rect = {rect_.x, rect_.y, rect_.width - bw, rect_.height};
    painter.draw_frame(field_rect, bg, border, style, true);

    // Up button
    Rect up = up_btn_rect();
    {
        bool hov = hovered_zone_ == HitZone::Up;
        bool press = pressed_zone_ == HitZone::Up;
        auto const &btn_style = Theme::current().button;
        Color btn_bg = btn_style.background;
        if (press && btn_style.background_pressed)
            btn_bg = *btn_style.background_pressed;
        else if (hov && btn_style.background_hovered)
            btn_bg = *btn_style.background_hovered;

        painter.draw_frame(up, btn_bg, border, btn_style, false);

        float cx = up.x + up.width / 2.0f;
        float cy = up.y + up.height / 2.0f;
        float arrow_sz = 3.5f;
        painter.draw_line({cx - arrow_sz, cy + arrow_sz * 0.4f},
                          {cx, cy - arrow_sz * 0.4f}, style.text, 1.5f);
        painter.draw_line({cx, cy - arrow_sz * 0.4f},
                          {cx + arrow_sz, cy + arrow_sz * 0.4f}, style.text, 1.5f);
    }

    // Down button
    Rect down = down_btn_rect();
    {
        bool hov = hovered_zone_ == HitZone::Down;
        bool press = pressed_zone_ == HitZone::Down;
        auto const &btn_style = Theme::current().button;
        Color btn_bg = btn_style.background;
        if (press && btn_style.background_pressed)
            btn_bg = *btn_style.background_pressed;
        else if (hov && btn_style.background_hovered)
            btn_bg = *btn_style.background_hovered;

        painter.draw_frame(down, btn_bg, border, btn_style, false);

        float cx = down.x + down.width / 2.0f;
        float cy = down.y + down.height / 2.0f;
        float arrow_sz = 3.5f;
        painter.draw_line({cx - arrow_sz, cy - arrow_sz * 0.4f},
                          {cx, cy + arrow_sz * 0.4f}, style.text, 1.5f);
        painter.draw_line({cx, cy + arrow_sz * 0.4f},
                          {cx + arrow_sz, cy - arrow_sz * 0.4f}, style.text, 1.5f);
    }

    // Text
    auto fm = painter.font_metrics(style.font_size);
    float baseline_y = rect_.y + (rect_.height - fm.height) / 2.0f + fm.ascent;
    float content_x = rect_.x + style.padding.left;
    float content_w = rect_.width - bw - style.padding.left - style.padding.right;

    painter.push_clip({content_x, rect_.y, content_w, rect_.height});

    if (focused_ && editing_) {
        size_t s = std::min(sel_anchor_, cursor_pos_);
        size_t e = std::max(sel_anchor_, cursor_pos_);
        if (s != e) {
            float sx = content_x + (s > 0 ? painter.text_size(text_.substr(0, s), style.font_size).width : 0.0f);
            float ex = content_x + painter.text_size(text_.substr(0, e), style.font_size).width;
            float hy = rect_.y + (rect_.height - fm.height) / 2.0f - 1.0f;
            Color sel_bg = Color::rgba(0.26f, 0.52f, 0.96f, 0.35f);
            painter.fill_rect({sx, hy, ex - sx, fm.height + 2.0f}, sel_bg);
        }
    }

    painter.draw_text(text_, {content_x, baseline_y}, style.text, style.font_size);

    if (focused_ && editing_) {
        auto elapsed = std::chrono::steady_clock::now() - cursor_blink_time_;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        if ((ms / 500) % 2 == 0) {
            std::string before = text_.substr(0, cursor_pos_);
            float cx = content_x;
            if (!before.empty())
                cx += painter.text_size(before, style.font_size).width;
            float cy_top = rect_.y + (rect_.height - fm.height) / 2.0f - 1.0f;
            float cy_bot = cy_top + fm.height + 2.0f;
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
        if (!hit_test(event.position)) return false;
        auto zone = hit_zone(event.position);
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
        if (!hit_test(event.position)) return false;
        if (event.scroll_dy > 0)
            step_up();
        else if (event.scroll_dy < 0)
            step_down();
        return true;
    }

    return false;
}

bool SpinBox::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) return false;
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
        if (!event.shift) sel_anchor_ = cursor_pos_;
        return true;
    case Key::End:
        cursor_pos_ = text_.size();
        if (!event.shift) sel_anchor_ = cursor_pos_;
        return true;
    case Key::Left:
        if (cursor_pos_ > 0) cursor_pos_--;
        if (!event.shift) sel_anchor_ = cursor_pos_;
        return true;
    case Key::Right:
        if (cursor_pos_ < text_.size()) cursor_pos_++;
        if (!event.shift) sel_anchor_ = cursor_pos_;
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
        char ch = event.text[0];
        if (ch >= '0' && ch <= '9') {
            size_t s = std::min(sel_anchor_, cursor_pos_);
            size_t e = std::max(sel_anchor_, cursor_pos_);
            if (s != e) text_.erase(s, e - s);
            cursor_pos_ = s;
            text_.insert(cursor_pos_, 1, ch);
            cursor_pos_++;
            sel_anchor_ = cursor_pos_;
            return true;
        }
        if (ch == '-' && cursor_pos_ == 0 && min_val_ < 0) {
            size_t s = std::min(sel_anchor_, cursor_pos_);
            size_t e = std::max(sel_anchor_, cursor_pos_);
            if (s != e) text_.erase(s, e - s);
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
