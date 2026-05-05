// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/list_view.hpp"
#include "toolkit/application.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>

namespace toolkit {

ListView::ListView(std::shared_ptr<ItemModel> model) {
    model_ = std::move(model);
    state.focusable = true;
    if (model_) {
        model_->on_data_changed = [this] {
            sync_widget_windows();
            clamp_scroll();
            if (window()) {
                window()->request_redraw("list data changed");
            }
        };
    }
}

ListView &ListView::set_model(std::shared_ptr<ItemModel> model) {
    model_ = std::move(model);
    selection_.clear();
    anchor_ = std::nullopt;
    cursor_ = std::nullopt;
    pressed_widget_row_ = std::nullopt;
    scroll_offset_ = 0;
    if (model_) {
        model_->on_data_changed = [this] {
            sync_widget_windows();
            clamp_scroll();
            if (window()) {
                window()->request_redraw("list data changed");
            }
        };
    }
    sync_widget_windows();
    return *this;
}

void ListView::set_window(Window *w) {
    Widget::set_window(w);
    sync_widget_windows();
}

ListView &ListView::set_selected(std::optional<size_t> index) {
    if (!model_) {
        return *this;
    }
    if (!index || *index >= model_->row_count()) {
        clear_selection();
        return *this;
    }
    selection_.clear();
    selection_.insert(*index);
    anchor_ = index;
    cursor_ = index;
    notify_selection();
    return *this;
}

ListView &ListView::set_selection(std::set<size_t> indices) {
    selection_ = std::move(indices);
    if (!selection_.empty()) {
        anchor_ = *selection_.begin();
        cursor_ = *selection_.rbegin();
    } else {
        anchor_ = cursor_ = std::nullopt;
    }
    notify_selection();
    return *this;
}

ListView &ListView::select_all() {
    if (!model_) {
        return *this;
    }
    auto n = model_->row_count();
    selection_.clear();
    for (auto i = size_t{0}; i < n; i++) {
        selection_.insert(i);
    }
    anchor_ = size_t{0};
    cursor_ = n > 0 ? std::optional<size_t>{n - 1} : std::nullopt;
    notify_selection();
    return *this;
}

ListView &ListView::clear_selection() {
    selection_.clear();
    anchor_ = cursor_ = std::nullopt;
    notify_selection();
    return *this;
}

void ListView::select_range_from_anchor() {
    if (!anchor_ || !cursor_) {
        return;
    }
    selection_.clear();
    auto lo = std::min(*anchor_, *cursor_);
    auto hi = std::max(*anchor_, *cursor_);
    for (auto i = lo; i <= hi; i++) {
        selection_.insert(i);
    }
}

void ListView::notify_selection() {
    if (on_selection_changed) {
        on_selection_changed(cursor_);
    }
}

void ListView::scroll_to(size_t index) {
    auto const &palette = Theme::current().palette;
    auto bw = palette.border_width;
    auto visible_h = rect_.height - bw * 2;

    float top, bottom;
    auto *wm = dynamic_cast<WidgetItemModel *>(model_.get());
    if (wm) {
        top = wm->row_top(index);
        auto *w = wm->widget_at(index);
        bottom = top + (w ? w->size_hint().height : 0.0f);
    } else {
        auto ih = item_height();
        top = ih * static_cast<float>(index);
        bottom = top + ih;
    }

    if (bottom > scroll_offset_ + visible_h) {
        scroll_offset_ = bottom - visible_h;
    }
    if (top < scroll_offset_) {
        scroll_offset_ = top;
    }
    clamp_scroll();
}

float ListView::item_height() const {
    auto const &style = Theme::current().list_view;
    auto const &palette = Theme::current().palette;
    auto fm = font_metrics(palette.fonts.size);
    return fm.height + style.item_padding * 2;
}

float ListView::total_content_height() const {
    if (!model_) {
        return 0;
    }
    auto *wm = dynamic_cast<WidgetItemModel *>(model_.get());
    if (wm) {
        return wm->total_height();
    }
    return item_height() * static_cast<float>(model_->row_count());
}

void ListView::clamp_scroll() {
    auto const &palette = Theme::current().palette;
    auto bw = palette.border_width;
    auto visible = rect_.height - bw * 2;
    auto content = total_content_height();
    auto max_scroll = std::max(0.0f, content - visible);
    scroll_offset_ = std::clamp(scroll_offset_, 0.0f, max_scroll);
    this->window()->request_redraw("ListView::clamp_scroll");
}

std::optional<size_t> ListView::item_at_y(float y) const {
    if (!model_) {
        return std::nullopt;
    }
    auto const &palette = Theme::current().palette;
    auto bw = palette.border_width;
    auto content_y = y - bw + scroll_offset_;
    if (content_y < 0) {
        return std::nullopt;
    }

    auto *wm = dynamic_cast<WidgetItemModel *>(model_.get());
    if (wm) {
        auto const &tops = wm->row_tops();
        if (tops.empty()) {
            return std::nullopt;
        }
        // Binary search: find last row whose top <= content_y.
        auto it = std::upper_bound(tops.begin(), tops.end(), content_y);
        if (it == tops.begin()) {
            return std::nullopt;
        }
        --it;
        auto idx = static_cast<size_t>(std::distance(tops.begin(), it));
        auto *w = const_cast<WidgetItemModel *>(wm)->widget_at(idx);
        float row_h = w ? w->size_hint().height : 0.0f;
        if (content_y >= tops[idx] + row_h) {
            return std::nullopt;
        }
        return idx;
    }

    auto idx = static_cast<size_t>(content_y / item_height());
    if (idx >= model_->row_count()) {
        return std::nullopt;
    }
    return idx;
}

void ListView::sync_widget_windows() {
    auto *wm = dynamic_cast<WidgetItemModel *>(model_.get());
    if (!wm) {
        return;
    }
    for (size_t i = 0; i < wm->row_count(); i++) {
        if (auto *w = wm->widget_at(i)) {
            w->set_window(window_);
        }
    }
}

bool ListView::dispatch_to_widget(size_t row, MouseEvent event) {
    auto *wm = dynamic_cast<WidgetItemModel *>(model_.get());
    if (!wm) {
        return false;
    }
    auto *w = wm->widget_at(row);
    if (!w) {
        return false;
    }
    auto bw = Theme::current().palette.border_width;
    event.position.x -= bw;
    event.position.y -= (bw + wm->row_top(row) - scroll_offset_);
    return w->handle_mouse(event);
}

void ListView::paint_text_items(Painter &painter) {
    auto const &theme = Theme::current();
    auto const &palette = theme.palette;
    auto ih = item_height();
    auto n = model_->row_count();
    auto bw = palette.border_width;
    auto inner_w = rect_.width - bw * 2;
    auto inner_h = rect_.height - bw * 2;
    auto first_visible = static_cast<size_t>(scroll_offset_ / ih);
    auto last_visible = std::min(n - 1, static_cast<size_t>((scroll_offset_ + inner_h) / ih));

    auto body_clip = Rect{bw, bw, inner_w, inner_h};
    painter.push_clip(body_clip);
    for (auto i = first_visible; i <= last_visible; i++) {
        auto iy = bw + ih * static_cast<float>(i) - scroll_offset_;
        auto item_rect = Rect{bw, iy, inner_w, ih};
        auto selected = is_selected(i);
        auto hovered = (hovered_ == i) && !selected;
        auto alt_row = alternating_ && (i % 2 == 1);

        theme.draw_list_item(painter, item_rect, model_->cell_text(i, 0), {}, selected, hovered,
                             alt_row);
    }
    painter.pop_clip();
}

void ListView::paint_widget_items(Painter &painter, WidgetItemModel *wm) {
    auto const &theme = Theme::current();
    auto const &palette = theme.palette;
    auto bw = palette.border_width;
    auto inner_w = rect_.width - bw * 2;
    auto inner_h = rect_.height - bw * 2;
    auto n = wm->row_count();

    auto body_clip = Rect{bw, bw, inner_w, inner_h};
    painter.push_clip(body_clip);
    for (size_t i = 0; i < n; i++) {
        auto *w = wm->widget_at(i);
        if (!w) {
            continue;
        }
        auto row_h = w->size_hint().height;
        auto row_y = bw + wm->row_top(i) - scroll_offset_;

        if (row_y + row_h < bw || row_y > bw + inner_h) {
            continue;
        }

        auto item_rect = Rect{bw, row_y, inner_w, row_h};
        auto selected = is_selected(i);
        auto hovered = (hovered_ == i) && !selected;
        auto alt_row = alternating_ && (i % 2 == 1);
        theme.draw_list_item_background(painter, item_rect, selected, hovered, alt_row);

        w->set_rect(item_rect);
        w->draw(painter);
    }
    painter.pop_clip();
}

void ListView::paint(Painter &painter) {
    auto const &theme = Theme::current();
    auto wstate = WidgetState{
        .interaction   = ButtonState::Normal,
        .focused       = is_focused(),
        .enabled       = is_enabled(),
        .window_active = window_ ? window_->is_active() : true,
    };
    theme.draw_list_background(painter, {0, 0, rect_.width, rect_.height}, wstate);

    if (!model_ || model_->row_count() == 0) {
        return;
    }

    auto *wm = dynamic_cast<WidgetItemModel *>(model_.get());
    if (wm) {
        paint_widget_items(painter, wm);
    } else {
        paint_text_items(painter);
    }

    auto const &palette = theme.palette;
    auto bw = palette.border_width;
    auto inner_h = rect_.height - bw * 2;
    auto content_h = total_content_height();
    if (content_h > inner_h) {
        auto bar_h = std::max(20.0f, inner_h * (inner_h / content_h));
        auto bar_y = bw + (scroll_offset_ / content_h) * inner_h;
        auto bar_x = rect_.width - bw - 6.0f;
        auto sb = Rect{bar_x, bar_y, 4.0f, bar_h};
        painter.fill_rounded_rect(sb, palette.text, 2.0f);
    }
}

bool ListView::handle_mouse(MouseEvent const &event) {
    if (!model_) {
        return false;
    }

    auto const local_rect = Rect{0, 0, rect_.width, rect_.height};

    if (event.type == MouseEvent::Type::Scroll) {
        if (!local_rect.contains(event.position)) {
            return false;
        }
        scroll_offset_ -= event.scroll_dy;
        clamp_scroll();
        return true;
    }

    if (event.type == MouseEvent::Type::Move) {
        if (local_rect.contains(event.position)) {
            auto new_hovered = item_at_y(event.position.y);
            if (new_hovered != hovered_) {
                if (hovered_) {
                    auto leave_event = event;
                    leave_event.type = MouseEvent::Type::Leave;
                    dispatch_to_widget(*hovered_, leave_event);
                }
                hovered_ = new_hovered;
            }
            if (hovered_) {
                dispatch_to_widget(*hovered_, event);
            }
            return true;
        }
        if (hovered_) {
            auto leave_event = event;
            leave_event.type = MouseEvent::Type::Leave;
            dispatch_to_widget(*hovered_, leave_event);
            hovered_ = std::nullopt;
        }
        return false;
    }

    if (event.type == MouseEvent::Type::Leave) {
        if (hovered_) {
            auto leave_event = event;
            leave_event.type = MouseEvent::Type::Leave;
            dispatch_to_widget(*hovered_, leave_event);
            hovered_ = std::nullopt;
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Release) {
        if (pressed_widget_row_) {
            dispatch_to_widget(*pressed_widget_row_, event);
            pressed_widget_row_ = std::nullopt;
            if (window_) {
                window_->request_redraw("list widget release");
            }
            return true;
        }
        return false;
    }

    if (event.type == MouseEvent::Type::Press) {
        if (!local_rect.contains(event.position)) {
            return false;
        }
        auto idx = item_at_y(event.position.y);
        if (!idx) {
            return false;
        }

        auto *wm = dynamic_cast<WidgetItemModel *>(model_.get());
        if (wm) {
            cursor_ = idx;
            pressed_widget_row_ = idx;
            dispatch_to_widget(*idx, event);
            if (window_) {
                window_->request_redraw("list widget press");
            }
            return true;
        }

        auto toggle_mod = event.super || event.ctrl;

        if (multi_select_ && event.shift && anchor_) {
            cursor_ = idx;
            select_range_from_anchor();
            notify_selection();
        } else if (multi_select_ && toggle_mod) {
            if (is_selected(*idx)) {
                selection_.erase(*idx);
            } else {
                selection_.insert(*idx);
            }
            anchor_ = idx;
            cursor_ = idx;
            notify_selection();
        } else {
            selection_.clear();
            selection_.insert(*idx);
            anchor_ = idx;
            cursor_ = idx;
            notify_selection();
        }
        return true;
    }

    return false;
}

bool ListView::handle_key(KeyEvent const &event) {
    if (!is_focused() || !model_ || event.type != KeyEvent::Type::Press) {
        return false;
    }

    auto n = model_->row_count();
    if (n == 0) {
        return false;
    }

    switch (event.key) {
    case Key::Down: {
        auto next = cursor_ ? std::min(*cursor_ + 1, n - 1) : size_t{0};
        if (multi_select_ && event.shift) {
            if (!anchor_) {
                anchor_ = next;
            }
            cursor_ = next;
            select_range_from_anchor();
        } else {
            set_selected(next);
        }
        scroll_to(*cursor_);
        notify_selection();
        return true;
    }
    case Key::Up: {
        auto next = (cursor_ && *cursor_ > 0) ? *cursor_ - 1 : size_t{0};
        if (multi_select_ && event.shift) {
            if (!anchor_) {
                anchor_ = next;
            }
            cursor_ = next;
            select_range_from_anchor();
        } else {
            set_selected(next);
        }
        scroll_to(*cursor_);
        notify_selection();
        return true;
    }
    case Key::Home: {
        if (multi_select_ && event.shift) {
            if (!anchor_) {
                anchor_ = size_t{0};
            }
            cursor_ = size_t{0};
            select_range_from_anchor();
            notify_selection();
        } else {
            set_selected(size_t{0});
        }
        scroll_to(0);
        return true;
    }
    case Key::End: {
        auto last = n - 1;
        if (multi_select_ && event.shift) {
            if (!anchor_) {
                anchor_ = last;
            }
            cursor_ = last;
            select_range_from_anchor();
            notify_selection();
        } else {
            set_selected(last);
        }
        scroll_to(last);
        return true;
    }
    case Key::PageDown: {
        auto bw = Theme::current().palette.border_width;
        auto page = std::max(size_t{1}, static_cast<size_t>((rect_.height - bw * 2) / item_height()));
        auto next = cursor_ ? std::min(*cursor_ + page, n - 1) : size_t{0};
        if (multi_select_ && event.shift) {
            if (!anchor_) {
                anchor_ = next;
            }
            cursor_ = next;
            select_range_from_anchor();
        } else {
            set_selected(next);
        }
        scroll_to(*cursor_);
        notify_selection();
        return true;
    }
    case Key::PageUp: {
        auto bw = Theme::current().palette.border_width;
        auto page = std::max(size_t{1}, static_cast<size_t>((rect_.height - bw * 2) / item_height()));
        auto next = (cursor_ && *cursor_ >= page) ? *cursor_ - page : size_t{0};
        if (multi_select_ && event.shift) {
            if (!anchor_) {
                anchor_ = next;
            }
            cursor_ = next;
            select_range_from_anchor();
        } else {
            set_selected(next);
        }
        scroll_to(*cursor_);
        notify_selection();
        return true;
    }
    default:
        break;
    }

    // For widget rows, route non-navigation keys to the widget at cursor_.
    auto *wm = dynamic_cast<WidgetItemModel *>(model_.get());
    if (wm && cursor_) {
        if (auto *w = wm->widget_at(*cursor_)) {
            if (w->handle_key(event)) {
                return true;
            }
        }
    }

    if (multi_select_ && event.text == "a" && (event.super || event.ctrl)) {
        select_all();
        return true;
    }

    return false;
}

Size ListView::size_hint() const {
    auto item_measured_height = item_height();
    // FIXME: what is this 8 here...?
    auto hz = item_measured_height * 8;
    return {0, hz};
}

void ListView::for_each_child(std::function<void(Widget *)> const &callback) {
    auto *wm = dynamic_cast<WidgetItemModel *>(model_.get());
    if (!wm) {
        return;
    }

    auto const &palette = Theme::current().palette;
    auto bw = palette.border_width;
    auto inner_h = rect_.height - bw * 2;

    for (size_t i = 0; i < wm->row_count(); i++) {
        auto *w = wm->widget_at(i);
        if (!w) {
            continue;
        }

        auto row_h = w->size_hint().height;
        auto row_y = bw + wm->row_top(i) - scroll_offset_;

        if (row_y + row_h < bw || row_y > bw + inner_h) {
            continue;
        }

        callback(w);
    }
}

} // namespace toolkit
