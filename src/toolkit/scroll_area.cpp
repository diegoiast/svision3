// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/scroll_area.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>
#include <cmath>

namespace toolkit {

// ── helpers ───────────────────────────────────────────────────────────────────

static float lerp(float a, float b, float t) { return a + (b - a) * t; }

// ── ScrollArea ────────────────────────────────────────────────────────────────

ScrollArea::ScrollArea() {
    state.non_focus_input = true;
    state.focusable = true;
}

void ScrollArea::set_content(std::unique_ptr<Widget> widget) {
    content_ = std::move(widget);
    if (content_) {
        content_->set_parent(this);
        content_->set_window(window_);
    }
    scroll_x_ = scroll_y_ = 0;
    update_content_rect();
    if (window_) {
        window_->request_redraw("ScrollArea::set_content");
    }
}

void ScrollArea::scroll_to(float x, float y) {
    scroll_x_ = x;
    scroll_y_ = y;
    clamp_scroll();
    if (window_) {
        window_->request_redraw("ScrollArea::scroll_to");
    }
}

// ── geometry helpers ──────────────────────────────────────────────────────────

float ScrollArea::content_w() const {
    return content_ ? content_->size_hint().width : 0;
}

float ScrollArea::content_h() const {
    return content_ ? content_->size_hint().height : 0;
}

bool ScrollArea::needs_vscroll() const { return content_h() > rect_.height; }
bool ScrollArea::needs_hscroll() const { return content_w() > rect_.width; }

Rect ScrollArea::vthumb_rect() const {
    if (!needs_vscroll()) {
        return {};
    }
    float ch = content_h();
    float vh = rect_.height;
    float ratio = vh / ch;
    float thumb_h = std::max(kThumbMinLen, vh * ratio);
    float track_h = vh - thumb_h;
    float thumb_y = (ch > vh) ? (scroll_y_ / (ch - vh)) * track_h : 0;
    float tx = rect_.width - kThumbWidth - 2.0f;
    return {tx, thumb_y, kThumbWidth, thumb_h};
}

Rect ScrollArea::hthumb_rect() const {
    if (!needs_hscroll()) {
        return {};
    }
    float cw = content_w();
    float vw = rect_.width;
    float ratio = vw / cw;
    float thumb_w = std::max(kThumbMinLen, vw * ratio);
    float track_w = vw - thumb_w;
    float thumb_x = (cw > vw) ? (scroll_x_ / (cw - vw)) * track_w : 0;
    float ty = rect_.height - kThumbWidth - 2.0f;
    return {thumb_x, ty, thumb_w, kThumbWidth};
}

void ScrollArea::clamp_scroll() {
    float max_x = std::max(0.0f, content_w() - rect_.width);
    float max_y = std::max(0.0f, content_h() - rect_.height);
    scroll_x_ = std::clamp(scroll_x_, 0.0f, max_x);
    scroll_y_ = std::clamp(scroll_y_, 0.0f, max_y);
}

void ScrollArea::update_content_rect() {
    if (!content_) {
        return;
    }
    // Pass 1: give content the full viewport width so width-sensitive content
    // (e.g. HTML) can compute its natural height.
    float pass1_w = rect_.width;
    float pass1_h = std::max(rect_.height, content_->size_hint().height);
    content_->set_rect({0, 0, pass1_w, pass1_h});

    // Pass 2: re-query height after the first layout and set final rect.
    float final_h = std::max(rect_.height, content_->size_hint().height);
    if (std::abs(final_h - pass1_h) > 0.5f) {
        content_->set_rect({0, 0, pass1_w, final_h});
    }

    clamp_scroll();
}

// ── Widget overrides ──────────────────────────────────────────────────────────

void ScrollArea::set_rect(Rect const &r) {
    Widget::set_rect(r);
    update_content_rect();
}

void ScrollArea::set_window(Window *w) {
    Widget::set_window(w);
    if (content_) {
        content_->set_window(w);
    }
}

void ScrollArea::on_theme_changed() {
    Widget::on_theme_changed();
    if (content_) {
        content_->on_theme_changed();
    }
}

void ScrollArea::paint(Painter &painter) {
    if (!content_) {
        return;
    }

    // ── viewport clip + scroll translation ──────────────────────────────────
    painter.push_clip({0, 0, rect_.width, rect_.height});
    painter.push_translation({-scroll_x_, -scroll_y_});
    content_->draw(painter);
    painter.pop_translation();
    painter.pop_clip();

    // ── scrollbar thumbs (overlay, no space taken) ───────────────────────────
    auto const &palette = Theme::current().palette;
    auto thumb_color = palette.text.with_alpha(0.35f);

    if (needs_vscroll()) {
        painter.fill_rounded_rect(vthumb_rect(), thumb_color, kThumbWidth / 2.0f);
    }
    if (needs_hscroll()) {
        painter.fill_rounded_rect(hthumb_rect(), thumb_color, kThumbWidth / 2.0f);
    }
}

bool ScrollArea::handle_mouse(MouseEvent const &event) {
    if (!content_) {
        return false;
    }

    // ── mouse wheel ──────────────────────────────────────────────────────────
    if (event.type == MouseEvent::Type::Scroll) {
        if (event.scroll_dy != 0 && needs_vscroll()) {
            scroll_y_ -= event.scroll_dy * kScrollStep;
            clamp_scroll();
            if (window_) {
                window_->request_redraw("ScrollArea scroll");
            }
            return true;
        }
        if (event.scroll_dx != 0 && needs_hscroll()) {
            scroll_x_ -= event.scroll_dx * kScrollStep;
            clamp_scroll();
            if (window_) {
                window_->request_redraw("ScrollArea scroll");
            }
            return true;
        }
        return false;
    }

    // ── vertical scrollbar drag ──────────────────────────────────────────────
    auto vt = vthumb_rect();
    auto ht = hthumb_rect();

    if (event.type == MouseEvent::Type::Press && event.button == 1) {
        if (needs_vscroll() && vt.contains(event.position)) {
            dragging_v_ = true;
            drag_start_mouse_ = event.position.y;
            drag_start_scroll_ = scroll_y_;
            return true;
        }
        if (needs_hscroll() && ht.contains(event.position)) {
            dragging_h_ = true;
            drag_start_mouse_ = event.position.x;
            drag_start_scroll_ = scroll_x_;
            return true;
        }
    }

    if (event.type == MouseEvent::Type::Release) {
        dragging_v_ = dragging_h_ = false;
    }

    if (event.type == MouseEvent::Type::Drag || event.type == MouseEvent::Type::Move) {
        if (dragging_v_) {
            float ch = content_h();
            float vh = rect_.height;
            float thumb_h = std::max(kThumbMinLen, vh * vh / ch);
            float track_h = vh - thumb_h;
            float delta_mouse = event.position.y - drag_start_mouse_;
            float delta_scroll = (track_h > 0) ? delta_mouse * (ch - vh) / track_h : 0;
            scroll_y_ = drag_start_scroll_ + delta_scroll;
            clamp_scroll();
            if (window_) {
                window_->request_redraw("ScrollArea drag");
            }
            return true;
        }
        if (dragging_h_) {
            float cw = content_w();
            float vw = rect_.width;
            float thumb_w = std::max(kThumbMinLen, vw * vw / cw);
            float track_w = vw - thumb_w;
            float delta_mouse = event.position.x - drag_start_mouse_;
            float delta_scroll = (track_w > 0) ? delta_mouse * (cw - vw) / track_w : 0;
            scroll_x_ = drag_start_scroll_ + delta_scroll;
            clamp_scroll();
            if (window_) {
                window_->request_redraw("ScrollArea drag");
            }
            return true;
        }
    }

    // ── forward to child (with scroll translation) ───────────────────────────
    auto translated = event;
    translated.position.x += scroll_x_;
    translated.position.y += scroll_y_;
    return Widget::dispatch_mouse_event(content_.get(), translated);
}

bool ScrollArea::handle_key(KeyEvent const &event) {
    if (content_) {
        return content_->handle_key(event);
    }
    return false;
}

Widget *ScrollArea::find_focusable_at(Point p) {
    if (!is_enabled() || !is_visible() || !hit_test(p)) {
        return nullptr;
    }
    return this;
}

Widget *ScrollArea::widget_at(Point p) {
    if (!is_visible() || !hit_test(p)) {
        return nullptr;
    }
    return this;
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

} // namespace toolkit
