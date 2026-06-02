// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/scrollable_widget.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>

namespace toolkit {

ScrollableWidget::ScrollableWidget() {
    vscroll_ = std::make_unique<Scrollbar>(Orientation::Vertical);
    vscroll_->set_focusable(false);
    vscroll_->set_parent(this);
    vscroll_->on_change = [this](float v) {
        scroll_y_ = v;
        on_scroll(scroll_x_, scroll_y_);
        if (window()) {
            window()->request_redraw("vscroll change");
        }
    };

    hscroll_ = std::make_unique<Scrollbar>(Orientation::Horizontal);
    hscroll_->set_focusable(false);
    hscroll_->set_parent(this);
    hscroll_->on_change = [this](float v) {
        scroll_x_ = v;
        on_scroll(scroll_x_, scroll_y_);
        if (window()) {
            window()->request_redraw("hscroll change");
        }
    };
}

void ScrollableWidget::set_window(Window *w) {
    Widget::set_window(w);
    if (vscroll_)
        vscroll_->set_window(w);
    if (hscroll_)
        hscroll_->set_window(w);
}

void ScrollableWidget::on_theme_changed() {
    Widget::on_theme_changed();
    if (vscroll_)
        vscroll_->on_theme_changed();
    if (hscroll_)
        hscroll_->on_theme_changed();
    layout_scrollbars();
    clamp_scroll();
}

Widget *ScrollableWidget::find_focusable_at(Point p) {
    if (!is_enabled() || !is_visible() || !hit_test(p)) {
        return nullptr;
    }
    if (vscroll_ && vscroll_->is_visible() && vscroll_->rect().contains(p)) {
        return vscroll_->find_focusable_at(p - Point{vscroll_->rect().x, vscroll_->rect().y});
    }
    if (hscroll_ && hscroll_->is_visible() && hscroll_->rect().contains(p)) {
        return hscroll_->find_focusable_at(p - Point{hscroll_->rect().x, hscroll_->rect().y});
    }
    return this;
}

Widget *ScrollableWidget::widget_at(Point p) {
    if (!is_visible() || !hit_test(p)) {
        return nullptr;
    }
    if (vscroll_ && vscroll_->is_visible() && vscroll_->rect().contains(p)) {
        return vscroll_->widget_at(p - Point{vscroll_->rect().x, vscroll_->rect().y});
    }
    if (hscroll_ && hscroll_->is_visible() && hscroll_->rect().contains(p)) {
        return hscroll_->widget_at(p - Point{hscroll_->rect().x, hscroll_->rect().y});
    }
    return this;
}

void ScrollableWidget::for_each_child(std::function<void(Widget *)> const &callback) {
    if (vscroll_)
        callback(vscroll_.get());
    if (hscroll_)
        callback(hscroll_.get());
}

void ScrollableWidget::scroll_to(float x, float y) {
    scroll_x_ = x;
    scroll_y_ = y;
    clamp_scroll();
    on_scroll(scroll_x_, scroll_y_);
    if (window()) {
        window()->request_redraw("scroll_to");
    }
}

void ScrollableWidget::update_scrollbars(Size content_size) {
    content_size_ = content_size;
    layout_scrollbars();
    clamp_scroll();
}

void ScrollableWidget::layout_scrollbars() {
    auto const &palette = Theme::current().palette;
    auto bw = palette.border_width;
    float sw = 16.0f;

    if (palette.inline_scrollbars) {
        vscroll_->set_visible(false);
        hscroll_->set_visible(false);
        return;
    }

    bool needs_v = needs_vscroll();
    bool needs_h = needs_hscroll();

    // Check if adding one scrollbar triggers the other
    if (needs_v && !needs_h) {
        if (content_size_.width > rect_.width - sw - bw * 2) {
            needs_h = true;
        }
    } else if (needs_h && !needs_v) {
        if (content_size_.height > rect_.height - sw - bw * 2) {
            needs_v = true;
        }
    }

    vscroll_->set_visible(needs_v);
    hscroll_->set_visible(needs_h);

    if (needs_v) {
        float vh = rect_.height - bw * 2 - (needs_h ? sw : 0);
        vscroll_->set_rect({rect_.width - bw - sw, bw, sw, vh});
    }
    if (needs_h) {
        float hw = rect_.width - bw * 2 - (needs_v ? sw : 0);
        hscroll_->set_rect({bw, rect_.height - bw - sw, hw, sw});
    }
}

void ScrollableWidget::clamp_scroll() {
    auto vr = viewport_rect();
    float max_x = std::max(0.0f, content_size_.width - vr.width);
    float max_y = std::max(0.0f, content_size_.height - vr.height);
    scroll_x_ = std::clamp(scroll_x_, 0.0f, max_x);
    scroll_y_ = std::clamp(scroll_y_, 0.0f, max_y);

    if (vscroll_ && vscroll_->is_visible()) {
        vscroll_->set_range(0, max_y);
        vscroll_->set_value(scroll_y_);
        vscroll_->set_step_page(vr.height);
    }
    if (hscroll_ && hscroll_->is_visible()) {
        hscroll_->set_range(0, max_x);
        hscroll_->set_value(scroll_x_);
        hscroll_->set_step_page(vr.width);
    }
}

Rect ScrollableWidget::viewport_rect() const {
    auto const &palette = Theme::current().palette;
    auto bw = palette.border_width;
    auto r = Rect{bw, bw, rect_.width - bw * 2, rect_.height - bw * 2};
    if (vscroll_ && vscroll_->is_visible()) {
        r.width -= vscroll_->rect().width;
    }
    if (hscroll_ && hscroll_->is_visible()) {
        r.height -= hscroll_->rect().height;
    }
    return r;
}

void ScrollableWidget::draw_scrollbars(Painter &painter) {
    auto const &palette = Theme::current().palette;
    if (!palette.inline_scrollbars) {
        if (vscroll_ && vscroll_->is_visible()) {
            vscroll_->draw(painter);
        }
        if (hscroll_ && hscroll_->is_visible()) {
            hscroll_->draw(painter);
        }
        return;
    }

    // Inline overlay scrollbars
    auto thumb_color = palette.text.with_alpha(0.35f);
    if (needs_vscroll()) {
        painter.fill_rounded_rect(vthumb_rect(), thumb_color, kThumbWidth / 2.0f);
    }
    if (needs_hscroll()) {
        painter.fill_rounded_rect(hthumb_rect(), thumb_color, kThumbWidth / 2.0f);
    }
}

bool ScrollableWidget::handle_scrollbar_mouse(MouseEvent const &event) {
    if (vscroll_ && vscroll_->is_visible() && vscroll_->rect().contains(event.position)) {
        return Widget::dispatch_mouse_event(vscroll_.get(), event);
    }
    if (hscroll_ && hscroll_->is_visible() && hscroll_->rect().contains(event.position)) {
        return Widget::dispatch_mouse_event(hscroll_.get(), event);
    }

    if (event.type == MouseEvent::Type::Scroll) {
        if (!hit_test(event.position)) {
            return false;
        }
        bool moved = false;
        if (event.scroll_dy != 0 && (needs_vscroll() || (vscroll_ && vscroll_->is_visible()))) {
            scroll_y_ -= event.scroll_dy * kScrollStep;
            moved = true;
        }
        if (event.scroll_dx != 0 && (needs_hscroll() || (hscroll_ && hscroll_->is_visible()))) {
            scroll_x_ += event.scroll_dx * kScrollStep;
            moved = true;
        }
        if (moved) {
            clamp_scroll();
            on_scroll(scroll_x_, scroll_y_);
            if (window()) {
                window()->request_redraw("mouse wheel scroll");
            }
            return true;
        }
    }

    return false;
}

bool ScrollableWidget::needs_vscroll() const { return content_size_.height > rect_.height - Theme::current().palette.border_width * 2; }
bool ScrollableWidget::needs_hscroll() const { return content_size_.width > rect_.width - Theme::current().palette.border_width * 2; }

Rect ScrollableWidget::vthumb_rect() const {
    if (!needs_vscroll()) return {};
    auto vr = viewport_rect();
    float ch = content_size_.height;
    float vh = vr.height;
    float ratio = vh / ch;
    float thumb_h = std::max(kThumbMinLen, vh * ratio);
    float track_h = vh - thumb_h;
    float thumb_y = (ch > vh) ? (scroll_y_ / (ch - vh)) * track_h : 0;
    return {rect_.width - kThumbWidth - 2.0f, vr.y + thumb_y, kThumbWidth, thumb_h};
}

Rect ScrollableWidget::hthumb_rect() const {
    if (!needs_hscroll()) return {};
    auto vr = viewport_rect();
    float cw = content_size_.width;
    float vw = vr.width;
    float ratio = vw / cw;
    float thumb_w = std::max(kThumbMinLen, vw * ratio);
    float track_w = vw - thumb_w;
    float thumb_x = (cw > vw) ? (scroll_x_ / (cw - vw)) * track_w : 0;
    return {vr.x + thumb_x, rect_.height - kThumbWidth - 2.0f, thumb_w, kThumbWidth};
}

} // namespace toolkit
