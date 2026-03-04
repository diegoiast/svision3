// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/checkbox.hpp"
#include "toolkit/theme.hpp"

namespace toolkit {

Checkbox::Checkbox(std::string text) : text_(std::move(text)) {
    focusable_ = true;
}

void Checkbox::set_checked(bool c) { checked_ = c; }

void Checkbox::toggle() {
    checked_ = !checked_;
    if (on_toggle) {
        on_toggle(checked_);
    }
}

void Checkbox::paint(Painter &painter) {
    auto const &style = Theme::current().checkbox;
    auto fm = painter.font_metrics(style.font_size);
    auto box = style.box_size;
    auto box_y = rect_.y + (rect_.height - box) / 2.0f;
    auto box_rect = Rect{rect_.x, box_y, box, box};

    painter.draw_frame(box_rect, style.background, style.border, style, true);
    if (checked_) {
        auto cx = box_rect.x + box * 0.22f;
        auto cy = box_rect.y + box * 0.5f;
        auto lw = std::max(1.5f, box * 0.14f);
        painter.draw_line({cx, cy}, {cx + box * 0.18f, cy + box * 0.2f}, style.indicator, lw);
        painter.draw_line({cx + box * 0.18f, cy + box * 0.2f}, {cx + box * 0.55f, cy - box * 0.25f},
                          style.indicator, lw);
    }

    auto text_x = rect_.x + box + style.spacing;
    auto baseline_y = rect_.y + (rect_.height - fm.height) / 2.0f + fm.ascent;
    painter.draw_text(text_, {text_x, baseline_y}, style.text, style.font_size);

    // FIXME: how about we let the theme decide this? Or move it to the window?
    if (focused_) {
        painter.draw_focus_ring(rect_, style.corner_radius);
    }
}

bool Checkbox::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Press && hit_test(event.position)) {
        toggle();
        return true;
    }
    return false;
}

bool Checkbox::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }
    if (event.key == Key::Enter || (!event.text.empty() && event.text[0] == ' ')) {
        toggle();
        return true;
    }
    return false;
}

Size Checkbox::size_hint() const {
    auto const &style = Theme::current().checkbox;
    auto fm = Painter::measure_font_metrics(style.font_size);
    auto tw = Painter::measure_text(text_, style.font_size).width;
    auto h = std::max(style.box_size, fm.height);
    return {style.box_size + style.spacing + tw, h + 4.0f};
}

} // namespace toolkit
