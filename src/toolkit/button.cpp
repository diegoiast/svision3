// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/button.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>
#include <cctype>

namespace toolkit {

Button::Button(std::string text) {
    auto pos = text.find('&');
    auto const &style = Theme::current().button;

    state.focusable = true;
    if (pos != std::string::npos && pos + 1 < text.size()) {
        mnemonic_index_ = static_cast<int>(pos);
        mnemonic_key_ = static_cast<char>(std::tolower(static_cast<unsigned char>(text[pos + 1])));
        display_text_ = text.substr(0, pos) + text.substr(pos + 1);
    } else {
        display_text_ = std::move(text);
    }

    auto_repeat_delay_ = style.auto_repeat_delay;
    auto_repeat_interval_ = style.auto_repeat_interval;
}

void Button::set_text(std::string text) {
    auto pos = text.find('&');
    std::string new_display_text;
    int new_mnemonic_index = -1;
    char new_mnemonic_key = 0;

    if (pos != std::string::npos && pos + 1 < text.size()) {
        new_mnemonic_index = static_cast<int>(pos);
        new_mnemonic_key =
            static_cast<char>(std::tolower(static_cast<unsigned char>(text[pos + 1])));
        new_display_text = text.substr(0, pos) + text.substr(pos + 1);
    } else {
        new_display_text = std::move(text);
    }

    if (display_text_ == new_display_text && mnemonic_index_ == new_mnemonic_index) {
        return;
    }

    display_text_ = std::move(new_display_text);
    mnemonic_index_ = new_mnemonic_index;
    mnemonic_key_ = new_mnemonic_key;

    if (window_) {
        window_->request_redraw("button state");
    }
}

void Button::stop_auto_repeat() {
    if (auto_repeat_timer_id_ && window_) {
        window_->stop_timer(auto_repeat_timer_id_);
        auto_repeat_timer_id_ = 0;
    }
}

void Button::start_auto_repeat_delay() {
    stop_auto_repeat();
    if (window_ && auto_repeat_) {
        auto_repeat_timer_id_ = window_->start_timer(
            auto_repeat_delay_,
            [this] {
                if (pressed_ && hovered_ && on_click) {
                    on_click();
                    start_auto_repeat_interval();
                } else {
                    auto_repeat_timer_id_ = 0;
                }
            },
            false);
    }
}

void Button::start_auto_repeat_interval() {
    stop_auto_repeat();
    if (window_ && auto_repeat_) {
        auto_repeat_timer_id_ = window_->start_timer(
            auto_repeat_interval_,
            [this] {
                if (pressed_ && hovered_ && on_click) {
                    on_click();
                } else {
                    stop_auto_repeat();
                }
            },
            true);
    }
}

void Button::set_visible(bool v) {
    auto changed = hovered_ || pressed_;

    if (is_visible() == v) {
        return;
    }
    Widget::set_visible(v);
    if (!is_visible()) {
        hovered_ = false;
        pressed_ = false;
        stop_auto_repeat();
        if (changed && window_) {
            window_->request_redraw("button state");
        }
    }
}

void Button::paint(Painter &painter) {
    auto const &style = Theme::current().button;
    auto bg = background_color_.value_or(style.background);
    auto border_c = is_focused() ? style.border_focused : style.border;
    auto text_c = is_enabled() ? style.text : style.text_disabled;

    auto text_offset = (style.beveled && pressed_ && is_enabled()) ? 1.0f : 0.0f;
    auto fm = painter.font_metrics(style.font_size);
    auto text_w = painter.text_size(display_text_, style.font_size).width;
    auto baseline_y = (rect_.height - fm.height) / 2.0f + fm.ascent;
    auto text_x = (rect_.width - text_w) / 2.0f + text_offset;
    auto text_pos = Point{text_x, baseline_y + text_offset};
    auto local_rect = Rect{0, 0, rect_.width, rect_.height};

    if (is_enabled()) {
        if (background_color_) {
            if (pressed_) {
                bg = background_color_->darken(0.1f);
            } else if (hovered_) {
                bg = background_color_->lighten(0.1f);
            }
        } else {
            if (pressed_ && style.background_pressed) {
                bg = *style.background_pressed;
            } else if (is_focused()) {
                bg = style.background_selected;
            } else if (!flat_ && hovered_ && style.background_hovered) {
                bg = *style.background_hovered;
            }
        }
    }

    bool show_full_frame = !flat_ || hovered_ || pressed_;
    if (show_full_frame) {
        painter.draw_frame(local_rect, bg, border_c, style, pressed_ && is_enabled());
    } else if (background_color_) {
        // Flat button, not hovered/pressed, but has a custom background:
        // just fill the background without border.
        if (style.corner_radius > 0.0f) {
            painter.fill_rounded_rect(local_rect, bg, style.corner_radius);
        } else {
            painter.fill_rect(local_rect, bg);
        }
    }

    painter.draw_text(display_text_, text_pos, text_c, style.font_size);

    if (mnemonic_index_ >= 0 && is_enabled()) {
        // FIXME: mnemonic drawing should be more generalized, and dependent on style
        auto before = display_text_.substr(0, mnemonic_index_);
        auto ch = std::string(1, display_text_[mnemonic_index_]);
        auto before_w = before.empty() ? 0.0f : painter.text_size(before, style.font_size).width;
        auto ch_w = painter.text_size(ch, style.font_size).width;
        auto ul_y = baseline_y + text_offset + fm.descent * 0.4f;

        painter.draw_line({text_x + before_w, ul_y}, {text_x + before_w + ch_w, ul_y}, text_c,
                          2.0f);
    }

    if (is_focused() && is_enabled()) {
        painter.draw_focus_ring(local_rect, style.corner_radius);
    }
}

bool Button::trigger_mnemonic(char key) {
    if (!is_enabled()) {
        return false;
    }
    if (mnemonic_key_ && mnemonic_key_ == key) {
        if (on_click) {
            on_click();
        }
        return true;
    }
    return false;
}

bool Button::handle_key(KeyEvent const &event) {
    if (!is_enabled()) {
        return false;
    }
    if (Widget::handle_key(event)) {
        return true;
    }
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }
    if (event.key == Key::Enter || (!event.text.empty() && event.text[0] == ' ')) {
        if (on_click) {
            on_click();
        }
        return true;
    }
    return false;
}

bool Button::handle_mouse(MouseEvent const &event) {
    if (!is_enabled() || !is_visible()) {
        if (hovered_ || pressed_) {
            hovered_ = false;
            pressed_ = false;
            stop_auto_repeat();
            if (window_) {
                window_->request_redraw("button state");
            }
        }
        return false;
    }
    auto inside = Rect{0, 0, rect_.width, rect_.height}.contains(event.position);

    switch (event.type) {
    case MouseEvent::Type::Move:
        if (hovered_ != inside) {
            hovered_ = inside;
            if (auto_repeat_ && pressed_) {
                if (hovered_) {
                    start_auto_repeat_delay();
                } else {
                    stop_auto_repeat();
                }
            }
            if (window()) {
                window()->request_redraw("button state");
            }
        }
        return inside;
    case MouseEvent::Type::Press:
        if (inside) {
            pressed_ = true;
            if (auto_repeat_ && on_click && window_) {
                on_click();
                start_auto_repeat_delay();
            }
            if (window()) {
                window()->request_redraw("button state");
            }
            return true;
        }
        return false;
    case MouseEvent::Type::Release:
        if (pressed_) {
            pressed_ = false;
            stop_auto_repeat();
            if (inside && on_click && !auto_repeat_) {
                on_click();
            }
            if (window()) {
                window()->request_redraw("button state");
            }
        }
        return inside;
    case MouseEvent::Type::Leave:
        if (hovered_ || pressed_) {
            hovered_ = false;
            pressed_ = false;
            stop_auto_repeat();
            if (window()) {
                window()->request_redraw("button state");
            }
        }
        return true;
    case MouseEvent::Type::Drag:
    case MouseEvent::Type::Scroll:
        return false;
    }
    return false;
}

Size Button::size_hint() const {
    auto const &style = Theme::current().button;
    auto padding = padding_override_.value_or(style.padding);
    auto text_w = Painter::measure_text(display_text_, style.font_size).width;
    auto fm = Painter::measure_font_metrics(style.font_size);
    return {text_w + padding.left + padding.right, fm.height + padding.top + padding.bottom};
}

} // namespace toolkit
