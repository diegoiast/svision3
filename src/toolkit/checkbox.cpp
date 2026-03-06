// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/checkbox.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

namespace toolkit {

Checkbox::Checkbox(std::string text) : text_(std::move(text)) {
    focusable_ = true;
}

void Checkbox::set_checked(bool c) {
    set_check_state(c ? CheckState::Checked : CheckState::Unchecked);
}

void Checkbox::set_check_state(CheckState newState) {
    if (state_ == newState) {
        return;
    }
    state_ = newState;
    if (window_) {
        window_->request_redraw();
    }
}

void Checkbox::toggle() {
    if (tri_state_) {
        if (state_ == CheckState::Unchecked) {
            state_ = CheckState::Checked;
        } else if (state_ == CheckState::Checked) {
            state_ = CheckState::Partial;
        } else {
            state_ = CheckState::Unchecked;
        }
    } else {
        state_ = (state_ == CheckState::Checked) ? CheckState::Unchecked : CheckState::Checked;
    }

    if (on_toggle) {
        on_toggle(state_ == CheckState::Checked);
    }
    if (on_state_change) {
        on_state_change(state_);
    }
    if (window_) {
        window_->request_redraw();
    }
}

void Checkbox::paint(Painter &painter) {
    auto const &style = Theme::current().checkbox;
    auto fm = painter.font_metrics(style.font_size);
    auto box = style.box_size;
    auto box_y = (rect_.height - box) / 2.0f;
    auto box_rect = Rect{0, box_y, box, box};

    painter.draw_frame(box_rect, style.background, style.border, style, true);
    // FIXME drawing of checkbox should be done by theme
    if (state_ == CheckState::Checked) {
        auto cx = box_rect.x + box * 0.22f;
        auto cy = box_rect.y + box * 0.5f;
        auto lw = std::max(1.5f, box * 0.14f);
        painter.draw_line({cx, cy}, {cx + box * 0.18f, cy + box * 0.2f}, style.indicator, lw);
        painter.draw_line({cx + box * 0.18f, cy + box * 0.2f}, {cx + box * 0.55f, cy - box * 0.25f},
                          style.indicator, lw);
    } else if (state_ == CheckState::Partial) {
        auto gap = box * 0.25f;
        auto inner = box_rect.inset(gap);
        painter.fill_rect(inner, style.indicator);
    }

    auto text_x = box + style.spacing;
    auto baseline_y = (rect_.height - fm.height) / 2.0f + fm.ascent;
    painter.draw_text(text_, {text_x, baseline_y}, style.text, style.font_size);

    // FIXME: ring should not be drawed by widgets
    if (focused_) {
        painter.draw_focus_ring({0, 0, rect_.width, rect_.height}, style.corner_radius);
    }
}

bool Checkbox::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Press) {
        if (Rect{0, 0, rect_.width, rect_.height}.contains(event.position)) {
            toggle();
            return true;
        }
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

    // FIXME: what is this +4?
    return {style.box_size + style.spacing + tw, h + 4.0f};
}

} // namespace toolkit
