// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/scroll_area.hpp"
#include "svision3/theme.hpp"
#include "svision3/window.hpp"
#include <cmath>
#include <nlohmann/json.hpp>

namespace svision3 {

// ── helpers ───────────────────────────────────────────────────────────────────

static float lerp(float a, float b, float t) { return a + (b - a) * t; }

// ── ScrollArea ────────────────────────────────────────────────────────────────

ScrollArea::ScrollArea() {
    state.non_focus_input = true;
    state.focusable = true;
}

std::weak_ptr<Widget> ScrollArea::set_content(std::shared_ptr<Widget> widget) {
    content_ = std::move(widget);
    if (content_) {
        content_->set_parent(this);
        content_->set_window(window_);
    }
    scroll_x_ = scroll_y_ = 0;
    update_content_rect();
    return content_;
}

void ScrollArea::on_scroll(float /*x*/, float /*y*/) {
    if (window_) {
        window_->request_redraw("ScrollArea scroll");
    }
}

void ScrollArea::update_content_rect() {
    if (!content_) {
        update_scrollbars({0, 0});
        return;
    }
    auto vr = viewport_rect();
    // Pass 1: give content the full viewport width so width-sensitive content
    // (e.g. HTML) can compute its natural height.
    float pass1_w = vr.width;
    float pass1_h = std::max(vr.height, content_->size_hint().height);
    content_->set_rect({0, 0, pass1_w, pass1_h});

    // Pass 2: re-query height after the first layout and set final rect.
    float final_h = std::max(vr.height, content_->size_hint().height);
    if (std::abs(final_h - pass1_h) > 0.5f) {
        content_->set_rect({0, 0, pass1_w, final_h});
    }

    update_scrollbars({content_->rect().width, content_->rect().height});
}

// ── Widget overrides ──────────────────────────────────────────────────────────

void ScrollArea::set_rect(Rect const &r) {
    Widget::set_rect(r);
    update_content_rect();
}

void ScrollArea::on_theme_changed() {
    ScrollableWidget::on_theme_changed();
    if (content_) {
        content_->on_theme_changed();
    }
}

void ScrollArea::paint(Painter &painter) {
    if (!content_) {
        draw_scrollbars(painter);
        return;
    }

    auto vr = viewport_rect();
    // ── viewport clip + scroll translation ──────────────────────────────────
    painter.push_clip(vr);
    painter.push_translation({-scroll_x_ + vr.x, -scroll_y_ + vr.y});
    content_->draw(painter);
    painter.pop_translation();
    painter.pop_clip();

    draw_scrollbars(painter);
}

bool ScrollArea::handle_mouse(MouseEvent const &event) {
    if (!content_) {
        return false;
    }

    if (handle_scrollbar_mouse(event)) {
        return true;
    }

    // ── forward to child (with scroll translation) ───────────────────────────
    auto vr = viewport_rect();
    if (!vr.contains(event.position)) {
        return false;
    }
    auto translated = event;
    translated.position.x += scroll_x_ - vr.x;
    translated.position.y += scroll_y_ - vr.y;
    return Widget::dispatch_mouse_event(content_.get(), translated);
}

bool ScrollArea::handle_key(KeyEvent const &event) {
    if (content_) {
        return content_->handle_key(event);
    }
    return false;
}

void ScrollArea::collect_focusables(std::vector<Widget *> &out) {
    if (content_) {
        content_->collect_focusables(out);
    }
}

void ScrollArea::for_each_child(std::function<void(Widget *)> const &cb) {
    if (content_) {
        cb(content_.get());
    }
}

nlohmann::json ScrollArea::to_json() const {
    auto j = Widget::to_json();
    j["scroll_x"] = scroll_x_;
    j["scroll_y"] = scroll_y_;
    if (content_) {
        j["content"] = content_->to_json();
    }
    return j;
}

} // namespace svision3
