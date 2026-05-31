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
            update_scroll_state();
            if (window()) {
                window()->request_redraw("list selection");
            }
        };
    }
}

ListView &ListView::set_model(std::shared_ptr<ItemModel> model) {
    model_ = std::move(model);
    selection_.clear();
    anchor_ = std::nullopt;
    cursor_ = std::nullopt;
    scroll_x_ = scroll_y_ = 0;
    if (model_) {
        model_->on_data_changed = [this] {
            update_scroll_state();
            if (window()) {
                window()->request_redraw("list selection");
            }
        };
    }
    return *this;
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
    auto ih = item_height();
    auto top = ih * static_cast<float>(index);
    auto bottom = top + ih;
    auto visible_h = viewport_rect().height;
    if (bottom > scroll_y_ + visible_h) {
        scroll_y_ = bottom - visible_h;
    }
    if (top < scroll_y_) {
        scroll_y_ = top;
    }
    update_scroll_state();
}

void ListView::set_rect(Rect const &r) {
    Widget::set_rect(r);
    update_scroll_state();
}

void ListView::update_scroll_state() {
    update_scrollbars({0, total_content_height()});
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
    return item_height() * static_cast<float>(model_->row_count());
}

std::optional<size_t> ListView::item_at_y(float y) const {
    if (!model_) {
        return std::nullopt;
    }
    auto const &palette = Theme::current().palette;
    auto bw = palette.border_width;

    auto local_y = y - bw + scroll_y_;
    if (local_y < 0) {
        return std::nullopt;
    }
    auto idx = static_cast<size_t>(local_y / item_height());
    if (idx >= model_->row_count()) {
        return std::nullopt;
    }
    return idx;
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
        draw_scrollbars(painter);
        return;
    }

    auto const &style = theme.list_view;
    auto const &palette = theme.palette;
    auto ih = item_height();
    auto n = model_->row_count();
    auto bw = palette.border_width;

    auto vr = viewport_rect();
    auto first_visible = static_cast<size_t>(scroll_y_ / ih);
    auto last_visible = std::min(n - 1, static_cast<size_t>((scroll_y_ + vr.height) / ih));

    painter.push_clip(vr);
    for (auto i = first_visible; i <= last_visible; i++) {
        auto iy = bw + ih * static_cast<float>(i) - scroll_y_;
        auto item_rect = Rect{bw, iy, vr.width, ih};
        auto selected = is_selected(i);
        auto hovered = (hovered_ == i) && !selected;
        auto alt_row = alternating_ && (i % 2 == 1);

        theme.draw_list_item(painter, item_rect, model_->cell_text(i, 0), {}, selected, hovered,
                             alt_row);
    }
    painter.pop_clip();

    draw_scrollbars(painter);
}

bool ListView::handle_mouse(MouseEvent const &event) {
    if (!model_) {
        return false;
    }

    if (handle_scrollbar_mouse(event)) {
        return true;
    }

    auto const local_rect = Rect{0, 0, rect_.width, rect_.height};

    if (event.type == MouseEvent::Type::Move) {
        if (local_rect.contains(event.position)) {
            hovered_ = item_at_y(event.position.y);
            return true;
        }
        hovered_ = std::nullopt;
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

} // namespace toolkit
