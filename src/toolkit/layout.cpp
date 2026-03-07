// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/layout.hpp"
#include <algorithm>

namespace toolkit {

static auto clamp_dim(float val, float lo, float hi) -> float {
    if (lo > 0 && val < lo) {
        val = lo;
    }
    if (hi > 0 && val > hi) {
        val = hi;
    }
    return val;
}

VBoxLayout::VBoxLayout() {}

void VBoxLayout::add_widget(std::unique_ptr<Widget> widget, int stretch, Alignment h_align) {
    widget->set_parent(this);
    if (window_) {
        widget->set_window(window_);
    }
    items_.push_back({std::move(widget), stretch, h_align});
    if (rect_.width > 0 || rect_.height > 0) {
        apply_layout();
    }
}

void VBoxLayout::set_rect(Rect const &rect) { Widget::set_rect(rect); }

void VBoxLayout::set_window(Window *w) {
    Widget::set_window(w);
    for (auto &item : items_) {
        item.widget->set_window(w);
    }
}

void VBoxLayout::apply_layout() {
    auto content_x = margins_.left;
    auto content_y = margins_.top;
    auto content_w = rect_.width - margins_.left - margins_.right;
    auto content_h = rect_.height - margins_.top - margins_.bottom;
    auto visible_count = 0;
    auto total_spacing = 0.0f;
    auto available_height = 0.0f;
    auto fixed_height = 0.0f;
    auto total_stretch = 0;
    auto remaining_height = 0.0f;
    auto stretch_unit = 0.0f;
    auto current_y = 0.0f;

    if (items_.empty()) {
        return;
    }

    for (auto const &item : items_) {
        if (item.widget->is_visible()) {
            visible_count++;
        }
    }
    if (visible_count == 0) {
        return;
    }

    total_spacing = spacing_ * (visible_count - 1);
    available_height = content_h - total_spacing;

    for (auto const &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        if (item.stretch == 0) {
            fixed_height += item.widget->size_hint().height;
        } else {
            total_stretch += item.stretch;
        }
    }

    remaining_height = std::max(0.0f, available_height - fixed_height);
    stretch_unit = total_stretch > 0 ? remaining_height / total_stretch : 0.0f;
    current_y = content_y;

    for (auto &item : items_) {
        auto item_w = 0.0f;
        auto item_h = 0.0f;
        auto item_x = 0.0f;
        auto mins = Size{};
        auto maxs = Size{};

        if (!item.widget->is_visible()) {
            continue;
        }

        item_w = content_w;
        item_x = content_x;
        mins = item.widget->min_size();
        maxs = item.widget->max_size();
        item_h = item.stretch == 0 ? item.widget->size_hint().height : stretch_unit * item.stretch;
        item_h = clamp_dim(item_h, mins.height, maxs.height);

        if (item.h_align != Alignment::Fill) {
            auto hint_w = item.widget->size_hint().width;
            if (hint_w > 0 && hint_w < content_w) {
                item_w = hint_w;
            }
            switch (item.h_align) {
            case Alignment::Center:
                item_x = content_x + (content_w - item_w) / 2.0f;
                break;
            case Alignment::End:
                item_x = content_x + content_w - item_w;
                break;
            case Alignment::Start:
            default:
                break;
            }
        }
        item_w = clamp_dim(item_w, mins.width, maxs.width);
        item.widget->set_rect({item_x, current_y, item_w, item_h});
        current_y += item_h + spacing_;
    }
}

void VBoxLayout::paint(Painter &painter) {
    if (layout_dirty) {
        apply_layout();
        layout_dirty = false;
    }

    for (auto &item : items_) {
        item.widget->draw(painter);
    }
}

auto VBoxLayout::handle_mouse(MouseEvent const &event) -> bool {
    auto handled = false;

    if (layout_dirty) {
        apply_layout();
        layout_dirty = false;
    }

    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        if (Widget::dispatch_mouse_event(item.widget.get(), event)) {
            handled = true;
            if (event.type != MouseEvent::Type::Move && event.type != MouseEvent::Type::Drag) {
                break;
            }
        }
    }
    return handled;
}

auto VBoxLayout::size_hint() const -> Size {
    auto w = 0.0f;
    auto h = 0.0f;
    auto visible_count = 0;

    for (auto const &item : items_) {
        auto hint = Size{};

        if (!item.widget->is_visible()) {
            continue;
        }
        hint = item.widget->size_hint();
        w = std::max(w, hint.width);
        h += hint.height;
        visible_count++;
    }
    if (visible_count > 1) {
        h += spacing_ * (visible_count - 1);
    }
    h += margins_.top + margins_.bottom;
    w += margins_.left + margins_.right;
    return {w, h};
}

void VBoxLayout::collect_focusables(std::vector<Widget *> &out) {
    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        item.widget->collect_focusables(out);
    }
}

void VBoxLayout::collect_mnemonics(std::vector<Widget *> &out) {
    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        item.widget->collect_mnemonics(out);
    }
}

auto VBoxLayout::find_focusable_at(Point p) -> Widget * {
    if (layout_dirty) {
        apply_layout();
        layout_dirty = false;
    }
    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        auto local_p = p;
        local_p.x -= item.widget->rect().x;
        local_p.y -= item.widget->rect().y;
        if (auto *w = item.widget->find_focusable_at(local_p)) {
            return w;
        }
    }
    return nullptr;
}

auto VBoxLayout::widget_at(Point p) -> Widget * {
    if (layout_dirty) {
        apply_layout();
        layout_dirty = false;
    }
    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        auto local_p = p;
        local_p.x -= item.widget->rect().x;
        local_p.y -= item.widget->rect().y;
        if (auto *w = item.widget->widget_at(local_p)) {
            return w;
        }
    }
    return nullptr;
}

void VBoxLayout::for_each_child(std::function<void(Widget *)> const &callback) {
    for (auto &item : items_) {
        callback(item.widget.get());
    }
}

// --- HBoxLayout ---

HBoxLayout::HBoxLayout() {}

void HBoxLayout::add_widget(std::unique_ptr<Widget> widget, int stretch, Alignment v_align) {
    widget->set_parent(this);
    if (window_) {
        widget->set_window(window_);
    }
    items_.push_back({std::move(widget), stretch, v_align});
    if (rect_.width > 0 || rect_.height > 0) {
        apply_layout();
    }
}

void HBoxLayout::set_rect(Rect const &rect) { Widget::set_rect(rect); }

void HBoxLayout::set_window(Window *w) {
    Widget::set_window(w);
    for (auto &item : items_) {
        item.widget->set_window(w);
    }
}

void HBoxLayout::apply_layout() {
    auto content_x = margins_.left;
    auto content_y = margins_.top;
    auto content_w = rect_.width - margins_.left - margins_.right;
    auto content_h = rect_.height - margins_.top - margins_.bottom;
    auto visible_count = 0;
    auto total_spacing = 0.0f;
    auto available_width = 0.0f;
    auto fixed_width = 0.0f;
    auto total_stretch = 0;
    auto remaining_width = 0.0f;
    auto stretch_unit = 0.0f;
    auto current_x = 0.0f;

    if (items_.empty()) {
        return;
    }

    for (auto const &item : items_) {
        if (item.widget->is_visible()) {
            visible_count++;
        }
    }
    if (visible_count == 0) {
        return;
    }

    total_spacing = spacing_ * (visible_count - 1);
    available_width = content_w - total_spacing;

    for (auto const &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        if (item.stretch == 0) {
            fixed_width += item.widget->size_hint().width;
        } else {
            total_stretch += item.stretch;
        }
    }

    remaining_width = std::max(0.0f, available_width - fixed_width);
    stretch_unit = total_stretch > 0 ? remaining_width / total_stretch : 0.0f;
    current_x = content_x;

    for (auto &item : items_) {
        auto item_h = 0.0f;
        auto item_y = 0.0f;
        auto mins = Size{};
        auto maxs = Size{};
        auto item_w = 0.0f;

        if (!item.widget->is_visible()) {
            continue;
        }

        item_h = content_h;
        item_y = content_y;
        mins = item.widget->min_size();
        maxs = item.widget->max_size();
        item_w = item.stretch == 0 ? item.widget->size_hint().width : stretch_unit * item.stretch;
        item_w = clamp_dim(item_w, mins.width, maxs.width);

        if (item.v_align != Alignment::Fill) {
            auto hint_h = item.widget->size_hint().height;
            if (hint_h > 0 && hint_h < content_h) {
                item_h = hint_h;
            }
            switch (item.v_align) {
            case Alignment::Center:
                item_y = content_y + (content_h - item_h) / 2.0f;
                break;
            case Alignment::End:
                item_y = content_y + content_h - item_h;
                break;
            case Alignment::Start:
            default:
                break;
            }
        }
        item_h = clamp_dim(item_h, mins.height, maxs.height);

        item.widget->set_rect({current_x, item_y, item_w, item_h});
        current_x += item_w + spacing_;
    }
}

void HBoxLayout::paint(Painter &painter) {
    if (layout_dirty) {
        apply_layout();
        layout_dirty = false;
    }

    for (auto &item : items_) {
        item.widget->draw(painter);
    }
}

auto HBoxLayout::handle_mouse(MouseEvent const &event) -> bool {
    auto handled = false;

    if (layout_dirty) {
        apply_layout();
        layout_dirty = false;
    }

    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        if (Widget::dispatch_mouse_event(item.widget.get(), event)) {
            handled = true;
            if (event.type != MouseEvent::Type::Move && event.type != MouseEvent::Type::Drag) {
                break;
            }
        }
    }
    return handled;
}

auto HBoxLayout::size_hint() const -> Size {
    auto w = 0.0f;
    auto h = 0.0f;
    auto visible_count = 0;

    for (auto const &item : items_) {
        auto hint = Size{};

        if (!item.widget->is_visible()) {
            continue;
        }
        hint = item.widget->size_hint();
        h = std::max(h, hint.height);
        w += hint.width;
        visible_count++;
    }
    if (visible_count > 1) {
        w += spacing_ * (visible_count - 1);
    }
    w += margins_.left + margins_.right;
    h += margins_.top + margins_.bottom;
    return {w, h};
}

void HBoxLayout::collect_focusables(std::vector<Widget *> &out) {
    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        item.widget->collect_focusables(out);
    }
}

void HBoxLayout::collect_mnemonics(std::vector<Widget *> &out) {
    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        item.widget->collect_mnemonics(out);
    }
}

auto HBoxLayout::find_focusable_at(Point p) -> Widget * {
    if (layout_dirty) {
        apply_layout();
        layout_dirty = false;
    }
    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        auto local_p = p;
        local_p.x -= item.widget->rect().x;
        local_p.y -= item.widget->rect().y;
        if (auto *w = item.widget->find_focusable_at(local_p)) {
            return w;
        }
    }
    return nullptr;
}

auto HBoxLayout::widget_at(Point p) -> Widget * {
    if (layout_dirty) {
        apply_layout();
        layout_dirty = false;
    }
    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        auto local_p = p;
        local_p.x -= item.widget->rect().x;
        local_p.y -= item.widget->rect().y;
        if (auto *w = item.widget->widget_at(local_p)) {
            return w;
        }
    }
    return nullptr;
}

void HBoxLayout::for_each_child(std::function<void(Widget *)> const &callback) {
    for (auto &item : items_) {
        callback(item.widget.get());
    }
}

} // namespace toolkit
