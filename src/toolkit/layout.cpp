// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/layout.hpp"
#include <algorithm>

namespace toolkit {

static float clamp_dim(float val, float lo, float hi) {
    if (lo > 0 && val < lo) {
        val = lo;
    }
    if (hi > 0 && val > hi) {
        val = hi;
    }
    return val;
}

void VBoxLayout::add_widget(std::unique_ptr<Widget> widget, int stretch, Alignment h_align) {
    if (window_) {
        widget->set_window(window_);
    }
    items_.push_back({std::move(widget), stretch, h_align});
    if (rect_.width > 0 || rect_.height > 0) {
        apply_layout();
    }
}

void VBoxLayout::set_rect(Rect const &rect) {
    Widget::set_rect(rect);
}

void VBoxLayout::set_window(Window *w) {
    Widget::set_window(w);
    for (auto &item : items_) {
        item.widget->set_window(w);
    }
}

void VBoxLayout::apply_layout() {
    if (items_.empty()) {
        return;
    }

    // FIXME: short and non descriptive variables
    auto cx = rect_.x + margins_.left;
    auto cy = rect_.y + margins_.top;
    auto cw = rect_.width - margins_.left - margins_.right;
    auto ch = rect_.height - margins_.top - margins_.bottom;
    auto visible_count = 0;

    for (auto const &item : items_) {
        if (item.widget->is_visible()) {
            visible_count++;
        }
    }
    if (visible_count == 0) {
        return;
    }

    auto total_spacing = spacing_ * static_cast<float>(visible_count - 1);
    auto available = ch - total_spacing;
    auto fixed_h = 0.0f;
    auto total_stretch = 0;

    for (auto const &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        if (item.stretch == 0) {
            fixed_h += item.widget->size_hint().height;
        } else {
            total_stretch += item.stretch;
        }
    }

    auto remaining = std::max(0.0f, available - fixed_h);
    auto stretch_unit = total_stretch > 0 ? remaining / static_cast<float>(total_stretch) : 0.0f;
    auto y = cy;

    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }

        auto item_w = cw;
        auto item_x = cx;
        auto mins = item.widget->min_size();
        auto maxs = item.widget->max_size();
        auto item_h = item.stretch == 0 ? item.widget->size_hint().height
                                        : stretch_unit * static_cast<float>(item.stretch);
        item_h = clamp_dim(item_h, mins.height, maxs.height);

        if (item.h_align != Alignment::Fill) {
            float hint_w = item.widget->size_hint().width;
            if (hint_w > 0 && hint_w < cw) {
                item_w = hint_w;
            }
            switch (item.h_align) {
            case Alignment::Center:
                item_x = cx + (cw - item_w) / 2.0f;
                break;
            case Alignment::End:
                item_x = cx + cw - item_w;
                break;
            case Alignment::Start:
            default:
                break;
            }
        }
        item_w = clamp_dim(item_w, mins.width, maxs.width);
        item.widget->set_rect({item_x, y, item_w, item_h});
        y += item_h + spacing_;
    }
}

void VBoxLayout::paint(Painter &painter) {
    if (layout_dirty) {
        apply_layout();
        layout_dirty = false;
    }

    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        auto r = item.widget->rect();
        painter.push_clip(r);
        item.widget->paint(painter);
        painter.pop_clip();
    }
}

bool VBoxLayout::handle_mouse(MouseEvent const &event) {
    auto handled = false;
    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        if (item.widget->handle_mouse(event)) {
            handled = true;
        }
    }
    return handled;
}

Size VBoxLayout::size_hint() const {
    auto w = 0.0f;
    auto h = 0.0f;
    auto visible_count = 0.0f;

    for (auto const &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        auto hint = item.widget->size_hint();
        w = std::max(w, hint.width);
        h += hint.height;
        visible_count++;
    }
    if (visible_count > 1) {
        h += spacing_ * static_cast<float>(visible_count - 1);
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

Widget *VBoxLayout::find_focusable_at(Point p) {
    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        if (auto *w = item.widget->find_focusable_at(p)) {
            return w;
        }
    }
    return nullptr;
}

Widget *VBoxLayout::widget_at(Point p) {
    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        if (auto *w = item.widget->widget_at(p)) {
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

void HBoxLayout::add_widget(std::unique_ptr<Widget> widget, int stretch, Alignment v_align) {
    if (window_) {
        widget->set_window(window_);
    }
    items_.push_back({std::move(widget), stretch, v_align});
    if (rect_.width > 0 || rect_.height > 0) {
        apply_layout();
    }
}

void HBoxLayout::set_rect(Rect const &rect) {
    Widget::set_rect(rect);
}

void HBoxLayout::set_window(Window *w) {
    Widget::set_window(w);
    for (auto &item : items_) {
        item.widget->set_window(w);
    }
}

void HBoxLayout::apply_layout() {
    if (items_.empty()) {
        return;
    }

    // FIXME: short and non descriptive variable names
    auto cx = rect_.x + margins_.left;
    auto cy = rect_.y + margins_.top;
    auto cw = rect_.width - margins_.left - margins_.right;
    auto ch = rect_.height - margins_.top - margins_.bottom;
    auto visible_count = 0;
    for (auto const &item : items_) {
        if (item.widget->is_visible()) {
            visible_count++;
        }
    }
    if (visible_count == 0) {
        return;
    }

    auto total_spacing = spacing_ * static_cast<float>(visible_count - 1);
    auto available = cw - total_spacing;
    auto fixed_w = 0.0f;
    auto total_stretch = 0.0f;
    for (auto const &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        if (item.stretch == 0) {
            fixed_w += item.widget->size_hint().width;
        } else {
            total_stretch += item.stretch;
        }
    }

    auto remaining = std::max(0.0f, available - fixed_w);
    auto stretch_unit = total_stretch > 0 ? remaining / static_cast<float>(total_stretch) : 0.0f;
    auto x = cx;
    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }

        auto item_h = ch;
        auto item_y = cy;
        auto mins = item.widget->min_size();
        auto maxs = item.widget->max_size();
        auto item_w = item.stretch == 0 ? item.widget->size_hint().width
                                        : stretch_unit * static_cast<float>(item.stretch);
        item_w = clamp_dim(item_w, mins.width, maxs.width);

        if (item.v_align != Alignment::Fill) {
            auto hint_h = item.widget->size_hint().height;
            if (hint_h > 0 && hint_h < ch) {
                item_h = hint_h;
            }
            switch (item.v_align) {
            case Alignment::Center:
                item_y = cy + (ch - item_h) / 2.0f;
                break;
            case Alignment::End:
                item_y = cy + ch - item_h;
                break;
            case Alignment::Start:
            default:
                break;
            }
        }
        item_h = clamp_dim(item_h, mins.height, maxs.height);

        item.widget->set_rect({x, item_y, item_w, item_h});
        x += item_w + spacing_;
    }
}

void HBoxLayout::paint(Painter &painter) {
    if (layout_dirty) {
        apply_layout();
        layout_dirty = false;
    }

    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        auto r = item.widget->rect();
        painter.push_clip(r);
        item.widget->paint(painter);
        painter.pop_clip();
    }
}

bool HBoxLayout::handle_mouse(MouseEvent const &event) {
    bool handled = false;
    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        if (item.widget->handle_mouse(event)) {
            handled = true;
        }
    }
    return handled;
}

Size HBoxLayout::size_hint() const {
    float w = 0;
    float h = 0;
    int visible_count = 0;
    for (auto const &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        auto hint = item.widget->size_hint();
        h = std::max(h, hint.height);
        w += hint.width;
        visible_count++;
    }
    if (visible_count > 1) {
        w += spacing_ * static_cast<float>(visible_count - 1);
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

Widget *HBoxLayout::find_focusable_at(Point p) {
    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        if (auto *w = item.widget->find_focusable_at(p)) {
            return w;
        }
    }
    return nullptr;
}

Widget *HBoxLayout::widget_at(Point p) {
    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        if (auto *w = item.widget->widget_at(p)) {
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
