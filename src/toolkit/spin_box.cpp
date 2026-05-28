// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/spin_box.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <nlohmann/json.hpp>

namespace toolkit {

SpinBox::SpinBox(int value, int min_val, int max_val, int step)
    : value_(value), min_val_(min_val), max_val_(max_val), step_(step),
      cursor_blink_time_(std::chrono::steady_clock::now()) {
    state.focusable = true;
    sync_text();

    up_button_ = std::make_unique<Button>("+");
    up_button_->set_parent(this);
    up_button_->set_flat(true);
    up_button_->set_padding({0, 0, 0, 0});
    up_button_->set_focusable(false);
    up_button_->set_auto_repeat(true, 0.5f, 0.1f);
    up_button_->on_click = [this] { step_up(); };

    down_button_ = std::make_unique<Button>("-");
    down_button_->set_parent(this);
    down_button_->set_flat(true);
    down_button_->set_padding({0, 0, 0, 0});
    down_button_->set_focusable(false);
    down_button_->set_auto_repeat(true, 0.5f, 0.1f);
    down_button_->on_click = [this] { step_down(); };
}

SpinBox &SpinBox::set_value(int v) {
    v = std::clamp(v, min_val_, max_val_);
    if (v == value_) {
        return *this;
    }
    value_ = v;
    sync_text();
    return *this;
}

SpinBox &SpinBox::set_range(int min_val, int max_val) {
    min_val_ = min_val;
    max_val_ = max_val;
    set_value(value_);
    return *this;
}

SpinBox &SpinBox::set_focused(bool focused) {
    Widget::set_focused(focused);
    if (focused) {
        editing_ = true;
        cursor_pos_ = text_.size();
        sel_anchor_ = 0;
        cursor_blink_time_ = std::chrono::steady_clock::now();
    } else {
        commit_text();
        editing_ = false;
    }
    return *this;
}

void SpinBox::set_rect(Rect const &rect) {
    Widget::set_rect(rect);
    relayout_buttons();
}

void SpinBox::set_window(Window *w) {
    Widget::set_window(w);
    up_button_->set_window(w);
    down_button_->set_window(w);
}

void SpinBox::relayout_buttons() {
    auto bw = rect_.height;
    auto bh = rect_.height / 2.0f;
    up_button_->set_rect({rect_.width - bw, 0, bw, bh});
    down_button_->set_rect({rect_.width - bw, bh, bw, rect_.height - bh});
}

Widget *SpinBox::widget_at(Point p) {
    if (!state.visible || !hit_test(p)) {
        return nullptr;
    }
    auto up_p = p;
    up_p.x -= up_button_->rect().x;
    up_p.y -= up_button_->rect().y;
    if (auto *w = up_button_->widget_at(up_p)) {
        return w;
    }
    auto down_p = p;
    down_p.x -= down_button_->rect().x;
    down_p.y -= down_button_->rect().y;
    if (auto *w = down_button_->widget_at(down_p)) {
        return w;
    }
    return this;
}

void SpinBox::for_each_child(std::function<void(Widget *)> const &callback) {
    callback(up_button_.get());
    callback(down_button_.get());
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
                on_change(value_, *this);
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
        on_change(value_, *this);
    }
}

void SpinBox::step_down() {
    auto old = value_;
    value_ = std::clamp(value_ - step_, min_val_, max_val_);
    sync_text();
    if (value_ != old && on_change) {
        on_change(value_, *this);
    }
}

CursorShape SpinBox::cursor() const { return CursorShape::IBeam; }

Size SpinBox::size_hint() const {
    auto const &style = Theme::current().line_input;
    auto const &palette = Theme::current().palette;

    auto h = palette.fonts.size + style.padding.top + style.padding.bottom + 8.0f;
    auto max_text = std::to_string(max_val_);
    auto sz = measure_text(max_text, palette.fonts.size);
    auto w = sz.width + style.padding.left + style.padding.right + h + 16.0f;
    return {w, h};
}

void SpinBox::paint(Painter &painter) {
    auto const &theme = Theme::current();
    auto sel_start_pos =
        (is_focused() && editing_) ? static_cast<int>(std::min(sel_anchor_, cursor_pos_)) : -1;
    auto sel_end_pos =
        (is_focused() && editing_) ? static_cast<int>(std::max(sel_anchor_, cursor_pos_)) : -1;
    auto cursor_pos = (is_focused() && editing_) ? static_cast<int>(cursor_pos_) : -1;
    auto rect = Rect{0.0f, 0.0f, rect_.width, rect_.height};

    auto cursor_visible = true;
    if (is_focused() && editing_) {
        auto elapsed = std::chrono::steady_clock::now() - cursor_blink_time_;
        cursor_visible =
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() % 1000 < 500;
    }

    auto wstate = WidgetState{
        .interaction = ButtonState::Normal,
        .focused = is_focused(),
        .enabled = is_enabled(),
        .window_active = window_ ? window_->is_active() : true,
    };

    theme.draw_spinbox(painter, rect, text_, cursor_pos, sel_start_pos, sel_end_pos, wstate, false,
                       false, false, false, cursor_visible);

    up_button_->draw(painter);
    down_button_->draw(painter);
}

bool SpinBox::handle_mouse(MouseEvent const &event) {
    if (Widget::dispatch_mouse_event(up_button_.get(), event)) {
        return true;
    }
    if (Widget::dispatch_mouse_event(down_button_.get(), event)) {
        return true;
    }

    if (event.type == MouseEvent::Type::Press) {
        auto inside = Rect{0, 0, rect_.width, rect_.height}.contains(event.position);
        if (inside) {
            set_focused(true);
            return true;
        }
        return false;
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
        if (this->window_) {
            window_->request_redraw("blink");
        }
        return true;
    case Key::Down:
        step_down();
        if (this->window_) {
            window_->request_redraw("blink");
        }
        return true;
    case Key::Enter:
        if (this->window_) {
            window_->request_redraw("blink");
        }
        commit_text();
        return true;
    case Key::Home:
        cursor_pos_ = 0;
        if (!event.shift) {
            sel_anchor_ = cursor_pos_;
        }
        if (this->window_) {
            window_->request_redraw("blink");
        }
        return true;
    case Key::End:
        cursor_pos_ = text_.size();
        if (!event.shift) {
            sel_anchor_ = cursor_pos_;
        }
        if (this->window_) {
            window_->request_redraw("blink");
        }
        return true;
    case Key::Left:
        if (cursor_pos_ > 0) {
            cursor_pos_--;
        }
        if (!event.shift) {
            sel_anchor_ = cursor_pos_;
        }
        if (this->window_) {
            window_->request_redraw("blink");
        }
        return true;
    case Key::Right:
        if (cursor_pos_ < text_.size()) {
            cursor_pos_++;
        }
        if (!event.shift) {
            sel_anchor_ = cursor_pos_;
        }
        if (this->window_) {
            window_->request_redraw("blink");
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
        if (this->window_) {
            window_->request_redraw("blink");
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
        if (this->window_) {
            window_->request_redraw("blink");
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
            if (this->window_) {
                window_->request_redraw("blink");
            }
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
            if (this->window_) {
                window_->request_redraw("blink");
            }
            return true;
        }
    }
    return false;
}

nlohmann::json SpinBox::to_json() const {
    auto j = Widget::to_json();
    j["value"] = value_;
    j["min"] = min_val_;
    j["max"] = max_val_;
    j["step"] = step_;
    return j;
}
} // namespace toolkit
