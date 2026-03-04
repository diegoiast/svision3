#include "toolkit/button.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>
#include <cctype>

namespace toolkit {

static Color mid(Color a, Color b) {
    return Color::rgb((a.r + b.r) / 2, (a.g + b.g) / 2, (a.b + b.b) / 2);
}

Button::Button(std::string text) {
    focusable_ = true;
    auto pos = text.find('&');
    if (pos != std::string::npos && pos + 1 < text.size()) {
        mnemonic_index_ = static_cast<int>(pos);
        mnemonic_key_ = static_cast<char>(std::tolower(static_cast<unsigned char>(text[pos + 1])));
        display_text_ = text.substr(0, pos) + text.substr(pos + 1);
    } else {
        display_text_ = std::move(text);
    }
}

void Button::set_text(std::string text) {
    auto pos = text.find('&');
    if (pos != std::string::npos && pos + 1 < text.size()) {
        mnemonic_index_ = static_cast<int>(pos);
        mnemonic_key_ = static_cast<char>(std::tolower(static_cast<unsigned char>(text[pos + 1])));
        display_text_ = text.substr(0, pos) + text.substr(pos + 1);
    } else {
        mnemonic_index_ = -1;
        mnemonic_key_ = 0;
        display_text_ = std::move(text);
    }
    if (window_) window_->request_redraw();
}

void Button::set_visible(bool v) {
    if (visible_ == v) return;
    Widget::set_visible(v);
    if (!visible_) {
        bool changed = hovered_ || pressed_;
        hovered_ = false;
        pressed_ = false;
        if (changed && window_) window_->request_redraw();
    }
}

void Button::paint(Painter &painter) {
    auto const &style = Theme::current().button;
    auto bg = background_color_.value_or(style.background);
    auto border_c = enabled_ ? style.border : mid(style.border, style.background);
    auto text_c = enabled_ ? style.text : mid(style.text, style.background);
    auto text_offset = (style.beveled && pressed_ && enabled_) ? 1.0f : 0.0f;
    auto fm = painter.font_metrics(style.font_size);
    auto text_w = painter.text_size(display_text_, style.font_size).width;
    auto baseline_y = rect_.y + (rect_.height - fm.height) / 2.0f + fm.ascent;
    auto text_x = rect_.x + (rect_.width - text_w) / 2.0f + text_offset;
    auto text_pos = Point{text_x, baseline_y + text_offset};

    if (enabled_) {
        if (background_color_) {
            if (pressed_) {
                bg = background_color_->darken(0.1f);
            } else if (hovered_) {
                bg = background_color_->lighten(0.1f);
            }
        } else {
            if (pressed_ && style.background_pressed) {
                bg = *style.background_pressed;
            } else if (hovered_ && style.background_hovered) {
                bg = *style.background_hovered;
            }
        }
    }

    bool show_full_frame = !flat_ || hovered_ || pressed_;
    if (show_full_frame) {
        painter.draw_frame(rect_, bg, border_c, style, pressed_ && enabled_);
    } else if (background_color_) {
        // Flat button, not hovered/pressed, but has a custom background:
        // just fill the background without border.
        if (style.corner_radius > 0.0f) {
            painter.fill_rounded_rect(rect_, bg, style.corner_radius);
        } else {
            painter.fill_rect(rect_, bg);
        }
    }
    
    painter.draw_text(display_text_, text_pos, text_c, style.font_size);

    if (mnemonic_index_ >= 0 && enabled_) {
        auto before = display_text_.substr(0, mnemonic_index_);
        std::string ch(1, display_text_[mnemonic_index_]);
        auto before_w = before.empty() ? 0.0f : painter.text_size(before, style.font_size).width;
        auto ch_w = painter.text_size(ch, style.font_size).width;
        auto ul_y = baseline_y + text_offset + fm.descent * 0.4f;

        painter.draw_line({text_x + before_w, ul_y}, {text_x + before_w + ch_w, ul_y}, text_c,
                          1.0f);
    }

    if (focused_ && enabled_) {
        painter.draw_focus_ring(rect_, style.corner_radius);
    }
}

bool Button::trigger_mnemonic(char key) {
    if (!enabled_) {
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
    if (!enabled_) {
        return false;
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
    if (!enabled_ || !visible_) {
        if (hovered_ || pressed_) {
            hovered_ = false;
            pressed_ = false;
            if (window()) window()->request_redraw();
        }
        return false;
    }
    auto inside = hit_test(event.position);

    switch (event.type) {
    case MouseEvent::Type::Move:
        if (hovered_ != inside) {
            hovered_ = inside;
            if (window()) window()->request_redraw();
        }
        return inside;
    case MouseEvent::Type::Press:
        if (inside) {
            pressed_ = true;
            if (window()) window()->request_redraw();
            return true;
        }
        return false;
    case MouseEvent::Type::Release:
        if (pressed_) {
            if (inside && on_click) {
                on_click();
            }
            pressed_ = false;
            if (window()) window()->request_redraw();
        }
        return inside;
    case MouseEvent::Type::Leave:
        if (hovered_ || pressed_) {
            hovered_ = false;
            pressed_ = false;
            if (window()) window()->request_redraw();
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
