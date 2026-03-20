// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/decorator_widget.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

namespace toolkit {

DecoratorWidget::DecoratorWidget(DecoratorStyle style, std::string title)
    : style_(style), title_(std::move(title)) {
    set_rect({0, 0, 0, style_.title_height});
}

void DecoratorWidget::set_title(std::string title) {
    title_ = std::move(title);
    if (window_) {
        window_->request_redraw("title changed");
    }
}

void DecoratorWidget::set_callbacks(DecoratorCallbacks callbacks) {
    callbacks_ = std::move(callbacks);
}

Rect DecoratorWidget::client_area() const {
    return {border_width_, style_.title_height, rect_.width - border_width_ * 2,
            rect_.height - style_.title_height - border_width_};
}

Point DecoratorWidget::content_offset() const { return {border_width_, style_.title_height}; }

Rect DecoratorWidget::close_button() const {
    float size = style_.button_size;
    float padding = style_.padding;
    return {rect_.width - padding - size, (style_.title_height - size) / 2.0f, size, size};
}

Rect DecoratorWidget::minimize_button() const {
    float size = style_.button_size;
    float padding = style_.padding;
    float spacing = style_.button_spacing;
    return {rect_.width - padding - size * 3 - spacing * 2, (style_.title_height - size) / 2.0f,
            size, size};
}

Rect DecoratorWidget::maximize_button() const {
    float size = style_.button_size;
    float padding = style_.padding;
    float spacing = style_.button_spacing;
    return {rect_.width - padding - size * 2 - spacing, (style_.title_height - size) / 2.0f, size,
            size};
}

void DecoratorWidget::paint_background(Painter &painter) {
    auto &style = Theme::current();
    auto focused = window_ && window_->focused_widget() != nullptr;
    auto title_bg = focused ? style_.title_bg : style_.title_bg_inactive;

    painter.fill_rect({0, 0, rect_.width, style_.title_height}, title_bg);
}

void DecoratorWidget::paint_borders(Painter &painter) {
    auto &style = Theme::current();
    auto focused = window_ && window_->focused_widget() != nullptr;
    auto border = focused ? style_.border : style_.border_inactive;

    painter.draw_line({0, 0}, {rect_.width, 0}, border, border_width_);
    painter.draw_line({0, rect_.height - border_width_},
                      {rect_.width, rect_.height - border_width_}, border, border_width_);
    painter.draw_line({0, 0}, {0, rect_.height}, border, border_width_);
    painter.draw_line({rect_.width - border_width_, 0}, {rect_.width - border_width_, rect_.height},
                      border, border_width_);

    float title_x = style_.padding;
    painter.draw_text(title_, {title_x, style_.title_height / 2.0f}, style_.title_text, 14.0f);

    auto draw_button = [&](Rect r, Color color) {
        if (r.width <= 0 || r.height <= 0) {
            return;
        }
        bool is_hovered = (r == close_button() && hovered_button_ == Button::Close) ||
                          (r == minimize_button() && hovered_button_ == Button::Minimize) ||
                          (r == maximize_button() && hovered_button_ == Button::Maximize);
        bool is_pressed = (r == close_button() && pressed_button_ == Button::Close) ||
                          (r == minimize_button() && pressed_button_ == Button::Minimize) ||
                          (r == maximize_button() && pressed_button_ == Button::Maximize);

        Color bg = is_pressed ? style_.button_pressed
                              : (is_hovered ? style_.button_hover : Color::rgba(0, 0, 0, 0));
        painter.fill_rect(r, bg);
        painter.draw_rect(r, Color::rgba(0, 0, 0, 0.2f), 1.0f);

        float cx = r.x + r.width / 2.0f;
        float cy = r.y + r.height / 2.0f;
        float s = r.width * 0.3f;
        painter.draw_line({cx - s, cy - s}, {cx + s, cy + s}, color, 2.0f);
        painter.draw_line({cx - s, cy + s}, {cx + s, cy - s}, color, 2.0f);
    };

    draw_button(close_button(), style_.button_close);
    draw_button(minimize_button(), style_.button_minimize);
    draw_button(maximize_button(), style_.button_maximize);
}

void DecoratorWidget::paint(Painter &painter) {
    paint_background(painter);
    paint_borders(painter);
}

bool DecoratorWidget::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Move) {
        auto old_hovered = hovered_button_;
        hovered_button_ = Button::None;

        if (close_button().contains(event.position)) {
            hovered_button_ = Button::Close;
        } else if (minimize_button().contains(event.position)) {
            hovered_button_ = Button::Minimize;
        } else if (maximize_button().contains(event.position)) {
            hovered_button_ = Button::Maximize;
        }

        if (old_hovered != hovered_button_) {
            if (window_) {
                window_->request_redraw("decorator hover");
            }
        }
        return old_hovered != hovered_button_;
    }

    if (event.type == MouseEvent::Type::Press) {
        pressed_button_ = Button::None;
        if (close_button().contains(event.position)) {
            pressed_button_ = Button::Close;
        } else if (minimize_button().contains(event.position)) {
            pressed_button_ = Button::Minimize;
        } else if (maximize_button().contains(event.position)) {
            pressed_button_ = Button::Maximize;
        }
        if (pressed_button_ != Button::None && window_) {
            window_->request_redraw("decorator press");
        }
        return pressed_button_ != Button::None;
    }

    if (event.type == MouseEvent::Type::Release) {
        auto was_button = pressed_button_;
        if (was_button != Button::None) {
            if (was_button == Button::Close && callbacks_.on_close) {
                callbacks_.on_close();
            } else if (was_button == Button::Minimize && callbacks_.on_minimize) {
                callbacks_.on_minimize();
            } else if (was_button == Button::Maximize && callbacks_.on_maximize) {
                callbacks_.on_maximize();
            }
        }
        pressed_button_ = Button::None;
        if (was_button != Button::None && window_) {
            window_->request_redraw("decorator release");
        }
        return was_button != Button::None;
    }

    return false;
}

} // namespace toolkit
