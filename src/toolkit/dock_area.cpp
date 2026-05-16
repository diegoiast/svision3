// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/dock_area.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/theme.hpp"
#include <algorithm>

namespace toolkit {

// ---------------------------------------------------------------------------
// DockPanel
// ---------------------------------------------------------------------------

DockPanel::DockPanel(std::string title, std::unique_ptr<Widget> content)
    : title_(std::move(title)), content_(std::move(content)) {
    if (content_) {
        content_->set_parent(this);
    }
}

void DockPanel::set_content(std::unique_ptr<Widget> content) {
    content_ = std::move(content);
    if (content_) {
        content_->set_parent(this);
        if (window_) {
            content_->set_window(window_);
        }
        // Apply rect immediately if we already have one.
        if (rect_.width > 0 || rect_.height > 0) {
            content_->set_rect(
                {0, title_bar_height, rect_.width, rect_.height - title_bar_height});
        }
    }
    state.layout_dirty = true;
}

void DockPanel::paint(Painter &painter) {
    auto const &palette = Theme::current().palette;

    // Title bar background
    Rect title_rect{0, 0, rect_.width, title_bar_height};
    painter.fill_rect(title_rect, palette.base);

    // Bottom border of title bar
    painter.draw_rect(
        {0, title_bar_height - palette.border_width, rect_.width, palette.border_width},
        palette.border, palette.border_width);

    // Title text — centered vertically, left-aligned with 8px padding
    auto const font_size = palette.fonts.size;
    auto const metrics = painter.font_metrics(font_size);
    auto text_y = (title_bar_height - metrics.height) / 2.0f + metrics.ascent;
    painter.draw_text(title_, {8, text_y}, palette.text, font_size);

    // Content
    if (content_ && content_->is_visible()) {
        content_->draw(painter);
    }
}

bool DockPanel::handle_mouse(MouseEvent const &event) {
    if (content_ && content_->is_visible()) {
        if (Widget::dispatch_mouse_event(content_.get(), event)) {
            return true;
        }
    }
    return false;
}

Size DockPanel::size_hint() const {
    if (content_) {
        auto hint = content_->size_hint();
        auto mins = content_->min_size();
        auto w = (mins.width > 0 && hint.width < mins.width) ? mins.width : hint.width;
        auto h = (mins.height > 0 && hint.height < mins.height) ? mins.height : hint.height;
        return {w, h + title_bar_height};
    }
    return {0, title_bar_height};
}

void DockPanel::set_rect(Rect const &rect) {
    Widget::set_rect(rect);
    if (content_) {
        auto content_h = rect_.height - title_bar_height;
        if (content_h < 0) {
            content_h = 0;
        }
        content_->set_rect({0, title_bar_height, rect_.width, content_h});
    }
    state.layout_dirty = false;
}

void DockPanel::set_window(Window *w) {
    Widget::set_window(w);
    if (content_) {
        content_->set_window(w);
    }
}

void DockPanel::for_each_child(std::function<void(Widget *)> const &cb) {
    if (content_) {
        cb(content_.get());
    }
}

void DockPanel::collect_focusables(std::vector<Widget *> &out) {
    if (content_ && content_->is_visible()) {
        content_->collect_focusables(out);
    }
}

// ---------------------------------------------------------------------------
// DockArea
// ---------------------------------------------------------------------------

DockArea::DockArea() {}

void DockArea::set_center(std::unique_ptr<Widget> w) {
    center_ = std::move(w);
    if (center_) {
        center_->set_parent(this);
        if (window_) {
            center_->set_window(window_);
        }
    }
    state.layout_dirty = true;
    if (rect_.width > 0 || rect_.height > 0) {
        apply_layout();
    }
}

void DockArea::set_top(std::unique_ptr<Widget> w) {
    top_ = std::move(w);
    if (top_) {
        top_->set_parent(this);
        if (window_) {
            top_->set_window(window_);
        }
    }
    state.layout_dirty = true;
    if (rect_.width > 0 || rect_.height > 0) {
        apply_layout();
    }
}

void DockArea::set_bottom(std::unique_ptr<Widget> w) {
    bottom_ = std::move(w);
    if (bottom_) {
        bottom_->set_parent(this);
        if (window_) {
            bottom_->set_window(window_);
        }
    }
    state.layout_dirty = true;
    if (rect_.width > 0 || rect_.height > 0) {
        apply_layout();
    }
}

void DockArea::set_left(std::unique_ptr<Widget> w) {
    left_ = std::move(w);
    if (left_) {
        left_->set_parent(this);
        if (window_) {
            left_->set_window(window_);
        }
    }
    state.layout_dirty = true;
    if (rect_.width > 0 || rect_.height > 0) {
        apply_layout();
    }
}

void DockArea::set_right(std::unique_ptr<Widget> w) {
    right_ = std::move(w);
    if (right_) {
        right_->set_parent(this);
        if (window_) {
            right_->set_window(window_);
        }
    }
    state.layout_dirty = true;
    if (rect_.width > 0 || rect_.height > 0) {
        apply_layout();
    }
}

void DockArea::apply_layout() {
    auto x = 0.0f;
    auto y = 0.0f;
    auto w = rect_.width;
    auto h = rect_.height;

    // Top — spans full width
    auto top_h = 0.0f;
    if (top_ && top_->is_visible()) {
        auto hint = top_->size_hint();
        auto mins = top_->min_size();
        top_h = (mins.height > 0 && hint.height < mins.height) ? mins.height : hint.height;
        top_->set_rect({x, y, w, top_h});
        y += top_h;
        h -= top_h;
    }

    // Bottom — spans full width, measured from the bottom
    auto bottom_h = 0.0f;
    if (bottom_ && bottom_->is_visible()) {
        auto hint = bottom_->size_hint();
        auto mins = bottom_->min_size();
        bottom_h = (mins.height > 0 && hint.height < mins.height) ? mins.height : hint.height;
        h -= bottom_h;
        bottom_->set_rect({x, y + h, w, bottom_h});
    }

    // Left / right / center share the remaining row
    auto row_x = x;
    auto row_y = y;
    auto row_w = w;
    auto row_h = h;

    auto left_w = 0.0f;
    if (left_ && left_->is_visible()) {
        auto hint = left_->size_hint();
        auto mins = left_->min_size();
        left_w = (mins.width > 0 && hint.width < mins.width) ? mins.width : hint.width;
        left_->set_rect({row_x, row_y, left_w, row_h});
        row_x += left_w;
        row_w -= left_w;
    }

    auto right_w = 0.0f;
    if (right_ && right_->is_visible()) {
        auto hint = right_->size_hint();
        auto mins = right_->min_size();
        right_w = (mins.width > 0 && hint.width < mins.width) ? mins.width : hint.width;
        row_w -= right_w;
        right_->set_rect({row_x + row_w, row_y, right_w, row_h});
    }

    if (center_ && center_->is_visible()) {
        center_->set_rect({row_x, row_y, row_w, row_h});
    }
}

void DockArea::paint(Painter &painter) {
    if (state.layout_dirty) {
        apply_layout();
        state.layout_dirty = false;
    }

    auto const &palette = Theme::current().palette;
    auto const bw = palette.border_width;

    // Fill background
    painter.fill_rect({0, 0, rect_.width, rect_.height}, palette.window);

    // Paint children
    if (top_ && top_->is_visible()) {
        top_->draw(painter);
    }
    if (bottom_ && bottom_->is_visible()) {
        bottom_->draw(painter);
    }
    if (left_ && left_->is_visible()) {
        left_->draw(painter);
    }
    if (right_ && right_->is_visible()) {
        right_->draw(painter);
    }
    if (center_ && center_->is_visible()) {
        center_->draw(painter);
    }

    // Separator lines between dock panels and center
    if (top_ && top_->is_visible()) {
        auto const &r = top_->rect();
        painter.draw_rect({r.x, r.y + r.height - bw, r.width, bw}, palette.border, bw);
    }
    if (bottom_ && bottom_->is_visible()) {
        auto const &r = bottom_->rect();
        painter.draw_rect({r.x, r.y, r.width, bw}, palette.border, bw);
    }
    if (left_ && left_->is_visible()) {
        auto const &r = left_->rect();
        painter.draw_rect({r.x + r.width - bw, r.y, bw, r.height}, palette.border, bw);
    }
    if (right_ && right_->is_visible()) {
        auto const &r = right_->rect();
        painter.draw_rect({r.x, r.y, bw, r.height}, palette.border, bw);
    }
}

bool DockArea::handle_mouse(MouseEvent const &event) {
    if (state.layout_dirty) {
        apply_layout();
        state.layout_dirty = false;
    }

    // Dispatch to each child; for Move/Drag we keep going so all children
    // can receive Enter/Leave transitions, but we stop on first handler for
    // Press/Release.
    auto handled = false;
    Widget *children[] = {top_.get(), bottom_.get(), left_.get(), right_.get(), center_.get()};
    for (auto *child : children) {
        if (!child || !child->is_visible()) {
            continue;
        }
        if (Widget::dispatch_mouse_event(child, event)) {
            handled = true;
            if (event.type != MouseEvent::Type::Move && event.type != MouseEvent::Type::Drag) {
                break;
            }
        }
    }
    return handled;
}

bool DockArea::handle_key(KeyEvent const &event) {
    Widget *children[] = {top_.get(), bottom_.get(), left_.get(), right_.get(), center_.get()};
    for (auto *child : children) {
        if (!child || !child->is_visible()) {
            continue;
        }
        if (child->handle_key(event)) {
            return true;
        }
    }
    return false;
}

Size DockArea::size_hint() const {
    // Minimum size is the sum of top+bottom heights, left+right widths,
    // plus whatever the center wants.
    auto center_hint = center_ ? center_->size_hint() : Size{0, 0};

    auto top_h = 0.0f;
    if (top_ && top_->is_visible()) {
        auto h = top_->size_hint().height;
        auto m = top_->min_size().height;
        top_h = (m > 0 && h < m) ? m : h;
    }

    auto bottom_h = 0.0f;
    if (bottom_ && bottom_->is_visible()) {
        auto h = bottom_->size_hint().height;
        auto m = bottom_->min_size().height;
        bottom_h = (m > 0 && h < m) ? m : h;
    }

    auto left_w = 0.0f;
    if (left_ && left_->is_visible()) {
        auto w = left_->size_hint().width;
        auto m = left_->min_size().width;
        left_w = (m > 0 && w < m) ? m : w;
    }

    auto right_w = 0.0f;
    if (right_ && right_->is_visible()) {
        auto w = right_->size_hint().width;
        auto m = right_->min_size().width;
        right_w = (m > 0 && w < m) ? m : w;
    }

    auto total_w = left_w + center_hint.width + right_w;
    auto total_h = top_h + center_hint.height + bottom_h;
    return {total_w, total_h};
}

void DockArea::set_rect(Rect const &rect) {
    Widget::set_rect(rect);
    apply_layout();
    state.layout_dirty = false;
}

void DockArea::set_window(Window *w) {
    Widget::set_window(w);
    Widget *children[] = {top_.get(), bottom_.get(), left_.get(), right_.get(), center_.get()};
    for (auto *child : children) {
        if (child) {
            child->set_window(w);
        }
    }
}

void DockArea::for_each_child(std::function<void(Widget *)> const &cb) {
    Widget *children[] = {top_.get(), bottom_.get(), left_.get(), right_.get(), center_.get()};
    for (auto *child : children) {
        if (child) {
            cb(child);
        }
    }
}

void DockArea::collect_focusables(std::vector<Widget *> &out) {
    Widget *children[] = {top_.get(), bottom_.get(), left_.get(), right_.get(), center_.get()};
    for (auto *child : children) {
        if (child && child->is_visible()) {
            child->collect_focusables(out);
        }
    }
}

} // namespace toolkit
