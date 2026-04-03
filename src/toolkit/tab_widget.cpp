// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/tab_widget.hpp"
#include "toolkit/button.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace toolkit {

TabWidget::TabWidget() {
    state.focusable = true;
    state.non_focus_input = true;

    prev_button_ = std::make_unique<Button>("<");
    prev_button_->set_flat(true);
    prev_button_->on_click = [this]() { scroll_by(-100); };
    prev_button_->set_parent(this);

    next_button_ = std::make_unique<Button>(">");
    next_button_->set_flat(true);
    next_button_->on_click = [this]() { scroll_by(100); };
    next_button_->set_parent(this);
}

TabWidget &TabWidget::add_tab(std::string title, std::unique_ptr<Widget> content) {
    content->set_parent(this);
    if (window_) {
        content->set_window(window_);
    }
    tabs_.push_back({std::move(title), std::move(content)});
    if (rect_.width > 0 || rect_.height > 0) {
        layout_content();
    }
    return *this;
}

TabWidget &TabWidget::set_current(int index) {
    if (index >= 0 && index < static_cast<int>(tabs_.size())) {
        current_ = index;
        scroll_to_tab(index);
        layout_content();
    }
    return *this;
}

void TabWidget::scroll_by(float delta) {
    scroll_offset_ += delta;
    update_scroll_bounds();
}

void TabWidget::scroll_to_tab(int index) {
    auto vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);
    auto available = vertical ? rect_.height : rect_.width;
    auto tab_start = 0.0f;
    auto tab_end = 0.0f;

    if (index < 0 || index >= static_cast<int>(tabs_.size())) {
        return;
    }

    if (leading_widget_) {
        auto sz = leading_widget_->size_hint();
        available -= vertical ? sz.height : sz.width;
    }
    if (trailing_widget_) {
        auto sz = trailing_widget_->size_hint();
        available -= vertical ? sz.height : sz.width;
    }

    if (show_scroll_buttons_) {
        auto sz_prev = prev_button_->size_hint();
        auto sz_next = next_button_->size_hint();
        auto btn_size = vertical ? std::max(sz_prev.height, sz_next.height)
                                 : std::max(sz_prev.width, sz_next.width);
        available -= btn_size * 2;
    }

    if (available <= 0) {
        return;
    }

    for (auto i = 0; i < index; i++) {
        tab_start += tab_size(i);
    }
    tab_end = tab_start + tab_size(index);

    if (tab_start < scroll_offset_) {
        scroll_offset_ = tab_start;
    } else if (tab_end > scroll_offset_ + available) {
        scroll_offset_ = tab_end - available;
    }
    update_scroll_bounds();
}

void TabWidget::update_scroll_bounds() {
    auto vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);
    auto lead_off = leading_widget_ ? (vertical ? leading_widget_->size_hint().height
                                                : leading_widget_->size_hint().width)
                                    : 0.0f;
    auto trail_off = trailing_widget_ ? (vertical ? trailing_widget_->size_hint().height
                                                  : trailing_widget_->size_hint().width)
                                      : 0.0f;
    auto bar_dim = vertical ? rect_.height : rect_.width;
    auto base_available = bar_dim - lead_off - trail_off;
    auto total_tabs = 0.0f;

    for (auto i = 0; i < static_cast<int>(tabs_.size()); i++) {
        total_tabs += tab_size(i);
    }

    if (total_tabs <= base_available) {
        show_scroll_buttons_ = false;
        scroll_offset_ = 0;
        prev_button_->set_visible(false);
        next_button_->set_visible(false);
        if (window_) {
            window_->request_redraw("tab change");
        }
        return;
    }

    show_scroll_buttons_ = true;
    auto sz_prev = prev_button_->size_hint();
    auto sz_next = next_button_->size_hint();
    auto btn_size_prev = vertical ? sz_prev.height : sz_prev.width;
    auto btn_size_next = vertical ? sz_next.height : sz_next.width;
    auto was_prev = prev_button_->is_visible();
    auto now_prev = scroll_offset_ > 0;

    if (now_prev != was_prev) {
        if (now_prev) {
            scroll_offset_ += btn_size_prev;
        } else {
            scroll_offset_ -= btn_size_prev;
        }
    }

    auto available = base_available - (now_prev ? btn_size_prev : 0);
    auto max_scroll_with_next = std::max(0.0f, total_tabs - (available - btn_size_next));
    auto now_next = scroll_offset_ < max_scroll_with_next;

    if (now_next) {
        available -= btn_size_next;
    }
    auto max_scroll = std::max(0.0f, total_tabs - available);

    scroll_offset_ = std::clamp(scroll_offset_, 0.0f, max_scroll);

    now_prev = scroll_offset_ > 0;
    now_next = scroll_offset_ < max_scroll;

    prev_button_->set_visible(now_prev);
    next_button_->set_visible(now_next);

    if (window_) {
        window_->request_redraw("tab change");
    }
}

TabWidget &TabWidget::set_orientation(TabOrientation o) {
    if (orientation_ == o) {
        return *this;
    }
    orientation_ = o;
    if (orientation_ == TabOrientation::North || orientation_ == TabOrientation::South) {
        prev_button_->set_text("<");
        next_button_->set_text(">");
    } else {
        prev_button_->set_text("^");
        next_button_->set_text("v");
    }
    layout_content();
    return *this;
}

TabWidget &TabWidget::set_leading_widget(std::unique_ptr<Widget> widget) {
    widget->set_parent(this);
    leading_widget_ = std::move(widget);
    if (leading_widget_ && window_) {
        leading_widget_->set_window(window_);
    }
    layout_content();
    return *this;
}

TabWidget &TabWidget::set_trailing_widget(std::unique_ptr<Widget> widget) {
    widget->set_parent(this);
    trailing_widget_ = std::move(widget);
    if (trailing_widget_ && window_) {
        trailing_widget_->set_window(window_);
    }
    layout_content();
    return *this;
}

auto TabWidget::tab_bar_thickness() const -> float {
    auto const &theme = Theme::current();
    auto const &palette = theme.palette;
    auto const &style = theme.tab_widget;
    auto fm = Painter::measure_font_metrics(palette.fonts.size);
    return fm.height + style.tab_padding_v * 2;
}

auto TabWidget::tab_size(int i) const -> float {
    auto const &theme = Theme::current();
    auto const &palette = theme.palette;
    auto const &style = theme.tab_widget;
    auto tw = Painter::measure_text(tabs_[i].title, palette.fonts.size).width;
    return tw + close_btn_size_ + close_btn_gap_ + style.tab_padding_h * 2;
}

void TabWidget::layout_content() {
    auto thickness = tab_bar_thickness();
    auto vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);
    auto bar_rect = Rect{0, 0, rect_.width, rect_.height};
    auto content_rect = Rect{0, 0, rect_.width, rect_.height};
    auto total_tabs = 0.0f;
    auto available = vertical ? rect_.height : rect_.width;

    if (vertical) {
        bar_rect.width = thickness;
        if (orientation_ == TabOrientation::East) {
            bar_rect.x = rect_.width - thickness;
            content_rect.width -= thickness;
        } else {
            content_rect.x += thickness;
            content_rect.width -= thickness;
        }
    } else {
        bar_rect.height = thickness;
        if (orientation_ == TabOrientation::South) {
            bar_rect.y = rect_.height - thickness;
            content_rect.height -= thickness;
        } else {
            content_rect.y += thickness;
            content_rect.height -= thickness;
        }
    }

    if (leading_widget_) {
        auto sz = leading_widget_->size_hint();
        if (vertical) {
            leading_widget_->set_rect({bar_rect.x, bar_rect.y, thickness, sz.height});
        } else {
            leading_widget_->set_rect({bar_rect.x, bar_rect.y, sz.width, thickness});
        }
    }

    if (trailing_widget_) {
        auto sz = trailing_widget_->size_hint();
        if (vertical) {
            trailing_widget_->set_rect(
                {bar_rect.x, bar_rect.y + bar_rect.height - sz.height, thickness, sz.height});
        } else {
            trailing_widget_->set_rect(
                {bar_rect.x + bar_rect.width - sz.width, bar_rect.y, sz.width, thickness});
        }
    }

    if (leading_widget_) {
        auto sz = leading_widget_->size_hint();
        available -= vertical ? sz.height : sz.width;
    }
    if (trailing_widget_) {
        auto sz = trailing_widget_->size_hint();
        available -= vertical ? sz.height : sz.width;
    }

    for (auto i = 0; i < static_cast<int>(tabs_.size()); i++) {
        total_tabs += tab_size(i);
    }

    if (total_tabs > available) {
        show_scroll_buttons_ = true;
        auto sz_prev = prev_button_->size_hint();
        auto sz_next = next_button_->size_hint();
        auto btn_size = vertical ? std::max(sz_prev.height, sz_next.height)
                                 : std::max(sz_prev.width, sz_next.width);
        auto lead_off = leading_widget_ ? (vertical ? leading_widget_->size_hint().height
                                                    : leading_widget_->size_hint().width)
                                        : 0.0f;
        auto trail_off = trailing_widget_ ? (vertical ? trailing_widget_->size_hint().height
                                                      : trailing_widget_->size_hint().width)
                                          : 0.0f;

        if (vertical) {
            prev_button_->set_rect({bar_rect.x, bar_rect.y + lead_off, thickness, btn_size});
            next_button_->set_rect({bar_rect.x, bar_rect.y + bar_rect.height - trail_off - btn_size,
                                    thickness, btn_size});
        } else {
            prev_button_->set_rect({bar_rect.x + lead_off, bar_rect.y, btn_size, thickness});
            next_button_->set_rect({bar_rect.x + bar_rect.width - trail_off - btn_size, bar_rect.y,
                                    btn_size, thickness});
        }
    } else {
        show_scroll_buttons_ = false;
        prev_button_->set_visible(false);
        next_button_->set_visible(false);
    }

    update_scroll_bounds();

    for (auto i = 0; i < static_cast<int>(tabs_.size()); i++) {
        bool is_current = (i == current_);
        tabs_[i].content->set_visible(is_current);
        if (is_current) {
            tabs_[i].content->set_rect(content_rect);
        }
    }
}

auto TabWidget::hit_test_tab(Point p) const -> HitResult {
    auto const &theme = Theme::current();
    auto const &palette = theme.palette;
    auto const &style = theme.tab_widget;
    auto thickness = tab_bar_thickness();
    auto vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);
    auto x = 0.0f;
    auto y = 0.0f;

    if (orientation_ == TabOrientation::South) {
        y = rect_.height - thickness;
    }
    if (orientation_ == TabOrientation::East) {
        x = rect_.width - thickness;
    }

    auto start_pos = 0.0f;
    if (leading_widget_) {
        auto sz = leading_widget_->size_hint();
        start_pos += vertical ? sz.height : sz.width;
    }

    auto bar_start = start_pos;
    auto bar_end = vertical ? rect_.height : rect_.width;
    if (trailing_widget_) {
        auto sz = trailing_widget_->size_hint();
        bar_end -= vertical ? sz.height : sz.width;
    }

    if (show_scroll_buttons_) {
        auto sz_prev = prev_button_->size_hint();
        auto sz_next = next_button_->size_hint();
        auto btn_size_prev = vertical ? sz_prev.height : sz_prev.width;
        auto btn_size_next = vertical ? sz_next.height : sz_next.width;

        if (prev_button_->is_visible()) {
            bar_start += btn_size_prev;
        }
        if (next_button_->is_visible()) {
            bar_end -= btn_size_next;
        }
    }

    if (vertical) {
        if (p.x < x || p.x >= x + thickness) {
            return {};
        }
        if (p.y < bar_start || p.y >= bar_end) {
            return {};
        }
        auto draw_y = bar_start - scroll_offset_;
        for (auto i = 0; i < static_cast<int>(tabs_.size()); i++) {
            auto h = tab_size(i);
            if (p.y >= draw_y && p.y < draw_y + h) {
                auto text_w = Painter::measure_text(tabs_[i].title, palette.fonts.size).width;
                auto close_cx = x + thickness / 2.0f;
                auto close_cy = 0.0f;
                if (orientation_ == TabOrientation::West) {
                    close_cy = draw_y + h - style.tab_padding_h - text_w - close_btn_gap_ -
                               close_btn_size_ / 2.0f;
                } else {
                    close_cy = draw_y + style.tab_padding_h + text_w + close_btn_gap_ +
                               close_btn_size_ / 2.0f;
                }
                auto hr = close_btn_size_ / 2.0f + 2.0f;
                auto on_close = (p.x >= close_cx - hr && p.x <= close_cx + hr &&
                                 p.y >= close_cy - hr && p.y <= close_cy + hr);
                return {i, on_close};
            }
            draw_y += h;
        }
    } else {
        if (p.y < y || p.y >= y + thickness) {
            return {};
        }
        if (p.x < bar_start || p.x >= bar_end) {
            return {};
        }
        auto draw_x = bar_start - scroll_offset_;
        for (auto i = 0; i < static_cast<int>(tabs_.size()); i++) {
            auto w = tab_size(i);
            if (p.x >= draw_x && p.x < draw_x + w) {
                auto text_w = Painter::measure_text(tabs_[i].title, palette.fonts.size).width;
                auto close_x = draw_x + style.tab_padding_h + text_w + close_btn_gap_;
                auto close_cy = y + thickness / 2.0f;
                auto hr = close_btn_size_ / 2.0f + 2.0f;
                auto on_close = (p.x >= close_x - 2.0f && p.x <= close_x + close_btn_size_ + 2.0f &&
                                 p.y >= close_cy - hr && p.y <= close_cy + hr);
                return {i, on_close};
            }
            draw_x += w;
        }
    }
    return {};
}

void TabWidget::set_rect(Rect const &rect) {
    rect_ = rect;
    layout_content();
}

void TabWidget::set_window(Window *w) {
    window_ = w;
    for (auto &tab : tabs_) {
        tab.content->set_window(w);
    }
    if (leading_widget_) {
        leading_widget_->set_window(w);
    }
    if (trailing_widget_) {
        trailing_widget_->set_window(w);
    }
    prev_button_->set_window(w);
    next_button_->set_window(w);
}

void TabWidget::paint(Painter &painter) {
    if (tabs_.empty()) {
        return;
    }

    if (state.layout_dirty) {
        layout_content();
        state.layout_dirty = false;
    }

    auto const &theme = Theme::current();
    auto const &palette = theme.palette;
    auto const &style = theme.tab_widget;
    auto fm = Painter::measure_font_metrics(palette.fonts.size);
    auto thickness = tab_bar_thickness();
    auto vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);
    auto bar_x = 0.0f;
    auto bar_y = 0.0f;
    auto bar_w = rect_.width;
    auto bar_h = rect_.height;

    if (vertical) {
        bar_w = thickness;
        if (orientation_ == TabOrientation::East) {
            bar_x = rect_.width - thickness;
        }
    } else {
        bar_h = thickness;
        if (orientation_ == TabOrientation::South) {
            bar_y = rect_.height - thickness;
        }
    }

    Theme::current().draw_tab_bar_background(painter, {bar_x, bar_y, bar_w, bar_h});

    auto paint_tab = [&](int i, float draw_x, float draw_y) {
        auto size = tab_size(i);
        auto tab_rect =
            vertical ? Rect{bar_x, draw_y, thickness, size} : Rect{draw_x, bar_y, size, thickness};
        auto active = (i == current_);
        auto hovered = (i == hovered_tab_ && !active);
        auto hovered_close = (i == hovered_close_);

        Theme::current().draw_tab(painter, tab_rect, tabs_[i].title, active, hovered, true,
                                  orientation_, true, hovered_close);
    };

    auto start_pos = 0.0f;
    if (leading_widget_) {
        auto sz = leading_widget_->size_hint();
        start_pos += vertical ? sz.height : sz.width;
        leading_widget_->draw(painter);
    }

    auto bar_start = start_pos;
    auto bar_end = vertical ? rect_.height : rect_.width;
    if (trailing_widget_) {
        auto sz = trailing_widget_->size_hint();
        bar_end -= vertical ? sz.height : sz.width;
        trailing_widget_->draw(painter);
    }

    if (show_scroll_buttons_) {
        auto sz_prev = prev_button_->size_hint();
        auto sz_next = next_button_->size_hint();
        auto btn_size_prev = vertical ? sz_prev.height : sz_prev.width;
        auto btn_size_next = vertical ? sz_next.height : sz_next.width;

        if (prev_button_->is_visible()) {
            prev_button_->draw(painter);
            bar_start += btn_size_prev;
        }
        if (next_button_->is_visible()) {
            next_button_->draw(painter);
            bar_end -= btn_size_next;
        }
    }

    auto scrollable_rect = vertical ? Rect{bar_x, bar_start, thickness, bar_end - bar_start}
                                    : Rect{bar_start, bar_y, bar_end - bar_start, thickness};

    painter.push_clip(scrollable_rect);

    auto cur_x = vertical ? bar_x : bar_start - scroll_offset_;
    auto cur_y = vertical ? bar_start - scroll_offset_ : bar_y;
    auto drag_draw_x = 0.0f;
    auto drag_draw_y = 0.0f;
    for (auto i = 0; i < static_cast<int>(tabs_.size()); i++) {
        auto size = tab_size(i);
        if (dragging_ && i == drag_tab_) {
            if (vertical) {
                drag_draw_x = bar_x;
                drag_draw_y = cur_y + drag_offset_x_;
            } else {
                drag_draw_x = cur_x + drag_offset_x_;
                drag_draw_y = bar_y;
            }
        } else {
            auto visible = vertical ? (cur_y + size > bar_start && cur_y < bar_end)
                                    : (cur_x + size > bar_start && cur_x < bar_end);
            if (visible) {
                paint_tab(i, cur_x, cur_y);
            }
        }
        if (vertical) {
            cur_y += size;
        } else {
            cur_x += size;
        }
    }
    if (dragging_ && drag_tab_ >= 0) {
        paint_tab(drag_tab_, drag_draw_x, drag_draw_y);
    }

    painter.pop_clip();

    if (style.border_width > 0) {
        if (orientation_ == TabOrientation::North) {
            painter.draw_line({0, thickness}, {rect_.width, thickness}, style.border,
                              style.border_width);
        } else if (orientation_ == TabOrientation::South) {
            painter.draw_line({0, rect_.height - thickness},
                              {rect_.width, rect_.height - thickness}, style.border,
                              style.border_width);
        } else if (orientation_ == TabOrientation::West) {
            painter.draw_line({thickness, 0}, {thickness, rect_.height}, style.border,
                              style.border_width);
        } else if (orientation_ == TabOrientation::East) {
            painter.draw_line({rect_.width - thickness, 0}, {rect_.width - thickness, rect_.height},
                              style.border, style.border_width);
        }
    }

    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
        tabs_[current_].content->draw(painter);
    }
}

auto TabWidget::handle_tab_drag(MouseEvent const &event) -> bool {
    auto vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);

    if (event.type == MouseEvent::Type::Drag) {
        auto mouse_pos = vertical ? event.position.y : event.position.x;
        drag_offset_x_ = mouse_pos - drag_start_x_;

        auto start_pos = 0.0f;
        if (leading_widget_) {
            auto sz = leading_widget_->size_hint();
            start_pos += vertical ? sz.height : sz.width;
        }

        auto bar_start = start_pos;
        auto bar_end = vertical ? rect_.height : rect_.width;
        if (trailing_widget_) {
            auto sz = trailing_widget_->size_hint();
            bar_end -= vertical ? sz.height : sz.width;
        }
        if (show_scroll_buttons_) {
            auto sz_prev = prev_button_->size_hint();
            auto sz_next = next_button_->size_hint();
            auto btn_size_prev = vertical ? sz_prev.height : sz_prev.width;
            auto btn_size_next = vertical ? sz_next.height : sz_next.width;
            bar_start += btn_size_prev;
            bar_end -= btn_size_next;
        }

        auto scroll_speed = 0.0f;
        if (mouse_pos < bar_start + 20) {
            scroll_speed = -5;
        } else if (mouse_pos > bar_end - 20) {
            scroll_speed = 5;
        }

        if (scroll_speed != 0) {
            auto old_offset = scroll_offset_;
            scroll_offset_ += scroll_speed;
            update_scroll_bounds();
            drag_start_x_ -= (scroll_offset_ - old_offset);
            drag_offset_x_ = mouse_pos - drag_start_x_;
        }

        auto swapped = false;
        do {
            swapped = false;
            auto pos = bar_start - scroll_offset_;
            for (auto i = 0; i < drag_tab_; i++) {
                pos += tab_size(i);
            }
            auto dragged_center = pos + tab_size(drag_tab_) / 2.0f + drag_offset_x_;
            if (drag_tab_ > 0) {
                auto prev_size = tab_size(drag_tab_ - 1);
                auto prev_mid = pos - prev_size / 2.0f;
                if (dragged_center < prev_mid) {
                    std::swap(tabs_[drag_tab_], tabs_[drag_tab_ - 1]);
                    if (current_ == drag_tab_) {
                        current_ = drag_tab_ - 1;
                    } else if (current_ == drag_tab_ - 1) {
                        current_ = drag_tab_;
                    }
                    drag_tab_--;
                    drag_start_x_ -= prev_size;
                    drag_offset_x_ = mouse_pos - drag_start_x_;
                    swapped = true;
                }
            }
            if (!swapped && drag_tab_ < static_cast<int>(tabs_.size()) - 1) {
                auto cur_size = tab_size(drag_tab_);
                auto next_size = tab_size(drag_tab_ + 1);
                auto next_mid = pos + cur_size + next_size / 2.0f;
                if (dragged_center > next_mid) {
                    std::swap(tabs_[drag_tab_], tabs_[drag_tab_ + 1]);
                    if (current_ == drag_tab_) {
                        current_ = drag_tab_ + 1;
                    } else if (current_ == drag_tab_ + 1) {
                        current_ = drag_tab_;
                    }
                    drag_tab_++;
                    drag_start_x_ += next_size;
                    drag_offset_x_ = mouse_pos - drag_start_x_;
                    swapped = true;
                }
            }
        } while (swapped);

        if (window_) {
            window_->request_redraw("tab change");
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Release) {
        dragging_ = false;
        drag_offset_x_ = 0;
        drag_tab_ = -1;
        if (window_) {
            window_->request_redraw("tab change");
        }
        return true;
    }

    return true;
}

auto TabWidget::handle_mouse(MouseEvent const &event) -> bool {
    auto vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);
    auto thickness = tab_bar_thickness();
    auto bar_x = 0.0f;
    auto bar_y = 0.0f;
    auto local_rect = Rect{0, 0, rect_.width, rect_.height};

    if (dragging_) {
        return handle_tab_drag(event);
    }

    if (leading_widget_ && Widget::dispatch_mouse_event(leading_widget_.get(), event)) {
        return true;
    }
    if (trailing_widget_ && Widget::dispatch_mouse_event(trailing_widget_.get(), event)) {
        return true;
    }

    if (show_scroll_buttons_) {
        if (Widget::dispatch_mouse_event(prev_button_.get(), event)) {
            return true;
        }
        if (Widget::dispatch_mouse_event(next_button_.get(), event)) {
            return true;
        }
    }

    if (vertical) {
        if (orientation_ == TabOrientation::East) {
            bar_x = rect_.width - thickness;
        }
    } else {
        if (orientation_ == TabOrientation::South) {
            bar_y = rect_.height - thickness;
        }
    }

    auto const bar_rect =
        vertical ? Rect{bar_x, 0, thickness, rect_.height} : Rect{0, bar_y, rect_.width, thickness};
    auto const in_bar = bar_rect.contains(event.position);

    if (event.type == MouseEvent::Type::Move) {
        if (in_bar) {
            auto hr = hit_test_tab(event.position);
            hovered_tab_ = hr.tab;
            hovered_close_ = hr.on_close ? hr.tab : -1;
            if (window_) {
                window_->request_redraw("tab change");
            }
            return true;
        }
        if (hovered_tab_ != -1 || hovered_close_ != -1) {
            hovered_tab_ = -1;
            hovered_close_ = -1;
            if (window_) {
                window_->request_redraw("tab change");
            }
        }
        if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
            return Widget::dispatch_mouse_event(tabs_[current_].content.get(), event);
        }
        return false;
    }

    if (event.type == MouseEvent::Type::Leave) {
        if (hovered_tab_ != -1 || hovered_close_ != -1) {
            hovered_tab_ = -1;
            hovered_close_ = -1;
            if (window_) {
                window_->request_redraw("tab change");
            }
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Scroll && in_bar) {
        auto delta = vertical ? event.scroll_dy : (event.scroll_dx + event.scroll_dy);
        scroll_offset_ -= delta * 20.0f;
        update_scroll_bounds();
        return true;
    }

    if (event.type == MouseEvent::Type::Press && in_bar) {
        if (window_) {
            window_->set_focused_widget(this);
        }
        auto hr = hit_test_tab(event.position);
        if (hr.tab >= 0 && hr.on_close) {
            spdlog::info("Tab close requested: [{}] \"{}\"", hr.tab, tabs_[hr.tab].title);
            if (on_tab_close) {
                on_tab_close(hr.tab, tabs_[hr.tab].title);
            }
            return true;
        }
        if (hr.tab >= 0) {
            if (hr.tab != current_) {
                set_current(hr.tab);
            }
            dragging_ = true;
            drag_tab_ = hr.tab;
            drag_start_x_ = vertical ? event.position.y : event.position.x;
            drag_offset_x_ = 0;
            return true;
        }
        return true;
    }

    hovered_tab_ = -1;
    hovered_close_ = -1;

    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
        return Widget::dispatch_mouse_event(tabs_[current_].content.get(), event);
    }
    return false;
}

auto TabWidget::handle_key(KeyEvent const &event) -> bool {
    if (event.type == KeyEvent::Type::Press && event.ctrl) {
        if (event.key == Key::PageUp) {
            if (event.shift) {
                if (current_ > 0) {
                    std::swap(tabs_[current_], tabs_[current_ - 1]);
                    set_current(current_ - 1);
                }
            } else {
                auto next = (current_ - 1 + static_cast<int>(tabs_.size())) %
                            static_cast<int>(tabs_.size());
                set_current(next);
            }
            return true;
        } else if (event.key == Key::PageDown) {
            if (event.shift) {
                if (current_ < static_cast<int>(tabs_.size()) - 1) {
                    std::swap(tabs_[current_], tabs_[current_ + 1]);
                    set_current(current_ + 1);
                }
            } else {
                auto next = (current_ + 1) % static_cast<int>(tabs_.size());
                set_current(next);
            }
            return true;
        }
    }

    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
        return tabs_[current_].content->handle_key(event);
    }
    return false;
}

auto TabWidget::size_hint() const -> Size {
    auto thickness = tab_bar_thickness();
    auto max_w = 0.0f;
    auto max_h = 0.0f;
    auto vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);
    auto lead_size = 0.0f;
    auto trail_size = 0.0f;

    for (auto const &tab : tabs_) {
        auto hint = tab.content->size_hint();
        max_w = std::max(max_w, hint.width);
        max_h = std::max(max_h, hint.height);
    }

    if (leading_widget_) {
        auto sz = leading_widget_->size_hint();
        lead_size = vertical ? sz.height : sz.width;
    }
    if (trailing_widget_) {
        auto sz = trailing_widget_->size_hint();
        trail_size = vertical ? sz.height : sz.width;
    }

    auto bar_size = lead_size + trail_size;
    if (!tabs_.empty()) {
        bar_size += tab_size(0);
        auto sz_prev = prev_button_->size_hint();
        auto sz_next = next_button_->size_hint();
        bar_size += vertical ? (sz_prev.height + sz_next.height) : (sz_prev.width + sz_next.width);
    }

    if (orientation_ == TabOrientation::North || orientation_ == TabOrientation::South) {
        return {std::max(max_w, bar_size), max_h + thickness};
    } else {
        return {max_w + thickness, std::max(max_h, bar_size)};
    }
}

auto TabWidget::find_focusable_at(Point p) -> Widget * {
    if (leading_widget_) {
        auto local_p = p;
        local_p.x -= leading_widget_->rect().x;
        local_p.y -= leading_widget_->rect().y;
        if (auto *w = leading_widget_->find_focusable_at(local_p)) {
            return w;
        }
    }
    if (trailing_widget_) {
        auto local_p = p;
        local_p.x -= trailing_widget_->rect().x;
        local_p.y -= trailing_widget_->rect().y;

        if (auto *w = trailing_widget_->find_focusable_at(local_p)) {
            return w;
        }
    }
    if (show_scroll_buttons_) {
        auto p_local = p;
        auto n_local = p;
        p_local.x -= prev_button_->rect().x;
        p_local.y -= prev_button_->rect().y;
        if (auto *w = prev_button_->find_focusable_at(p_local)) {
            return w;
        }
        n_local.x -= next_button_->rect().x;
        n_local.y -= next_button_->rect().y;
        if (auto *w = next_button_->find_focusable_at(n_local)) {
            return w;
        }
    }
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
        auto local_p = p;
        local_p.x -= tabs_[current_].content->rect().x;
        local_p.y -= tabs_[current_].content->rect().y;
        return tabs_[current_].content->find_focusable_at(local_p);
    }
    return nullptr;
}

auto TabWidget::widget_at(Point p) -> Widget * {
    if (!is_visible() || !hit_test(p)) {
        return nullptr;
    }
    if (leading_widget_) {
        auto local_p = p;
        local_p.x -= leading_widget_->rect().x;
        local_p.y -= leading_widget_->rect().y;
        if (auto *w = leading_widget_->widget_at(local_p)) {
            return w;
        }
    }
    if (trailing_widget_) {
        auto local_p = p;
        local_p.x -= trailing_widget_->rect().x;
        local_p.y -= trailing_widget_->rect().y;
        if (auto *w = trailing_widget_->widget_at(local_p)) {
            return w;
        }
    }
    if (show_scroll_buttons_) {
        auto p_local = p;
        auto n_local = p;
        p_local.x -= prev_button_->rect().x;
        p_local.y -= prev_button_->rect().y;
        if (auto *w = prev_button_->widget_at(p_local)) {
            return w;
        }
        n_local.x -= next_button_->rect().x;
        n_local.y -= next_button_->rect().y;
        if (auto *w = next_button_->widget_at(n_local)) {
            return w;
        }
    }
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
        auto local_p = p;
        local_p.x -= tabs_[current_].content->rect().x;
        local_p.y -= tabs_[current_].content->rect().y;
        if (auto *w = tabs_[current_].content->widget_at(local_p)) {
            return w;
        }
    }
    return this;
}

void TabWidget::collect_focusables(std::vector<Widget *> &out) {
    if (is_focusable() && is_enabled() && is_visible()) {
        out.push_back(this);
    }
    if (leading_widget_) {
        leading_widget_->collect_focusables(out);
    }
    if (trailing_widget_) {
        trailing_widget_->collect_focusables(out);
    }
    if (show_scroll_buttons_) {
        prev_button_->collect_focusables(out);
        next_button_->collect_focusables(out);
    }
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
        tabs_[current_].content->collect_focusables(out);
    }
}

void TabWidget::collect_mnemonics(std::vector<Widget *> &out) {
    if (leading_widget_) {
        leading_widget_->collect_mnemonics(out);
    }
    if (trailing_widget_) {
        trailing_widget_->collect_mnemonics(out);
    }
    if (show_scroll_buttons_) {
        prev_button_->collect_mnemonics(out);
        next_button_->collect_mnemonics(out);
    }
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
        tabs_[current_].content->collect_mnemonics(out);
    }
}

void TabWidget::for_each_child(std::function<void(Widget *)> const &callback) {
    if (leading_widget_) {
        callback(leading_widget_.get());
    }
    if (trailing_widget_) {
        callback(trailing_widget_.get());
    }
    if (show_scroll_buttons_) {
        callback(prev_button_.get());
        callback(next_button_.get());
    }
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
        callback(tabs_[current_].content.get());
    }
}

} // namespace toolkit
