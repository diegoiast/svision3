#include "toolkit/tab_widget.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/button.hpp"
#include "toolkit/window.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace toolkit {

TabWidget::TabWidget() {
    set_focusable(true);
    prev_button_ = std::make_unique<Button>("<");
    prev_button_->set_flat(true);
    prev_button_->on_click = [this]() { scroll_by(-100); };
    prev_button_->set_parent(this);

    next_button_ = std::make_unique<Button>(">");
    next_button_->set_flat(true);
    next_button_->on_click = [this]() { scroll_by(100); };
    next_button_->set_parent(this);
}

void TabWidget::add_tab(std::string title, std::unique_ptr<Widget> content) {
    content->set_parent(this);
    if (window_) content->set_window(window_);
    tabs_.push_back({std::move(title), std::move(content)});
    if (rect_.width > 0 || rect_.height > 0)
        layout_content();
}

void TabWidget::set_current(int index) {
    if (index >= 0 && index < static_cast<int>(tabs_.size())) {
        current_ = index;
        scroll_to_tab(index);
        layout_content();
    }
}

void TabWidget::scroll_by(float delta) {
    scroll_offset_ += delta;
    update_scroll_bounds();
}

void TabWidget::scroll_to_tab(int index) {
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return;

    bool vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);
    float available = vertical ? rect_.height : rect_.width;
    if (leading_widget_) available -= vertical ? leading_widget_->size_hint().height : leading_widget_->size_hint().width;
    if (trailing_widget_) available -= vertical ? trailing_widget_->size_hint().height : trailing_widget_->size_hint().width;

    if (show_scroll_buttons_) {
        Size sz_prev = prev_button_->size_hint();
        Size sz_next = next_button_->size_hint();
        float btn_size = vertical ? std::max(sz_prev.height, sz_next.height) : std::max(sz_prev.width, sz_next.width);
        available -= btn_size * 2;
    }

    if (available <= 0) return;

    float tab_start = 0;
    for (int i = 0; i < index; i++) {
        tab_start += tab_size(i);
    }
    float tab_end = tab_start + tab_size(index);

    if (tab_start < scroll_offset_) {
        scroll_offset_ = tab_start;
    } else if (tab_end > scroll_offset_ + available) {
        scroll_offset_ = tab_end - available;
    }
    update_scroll_bounds();
}

void TabWidget::update_scroll_bounds() {
    bool vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);
    float lead_off = leading_widget_ ? (vertical ? leading_widget_->size_hint().height : leading_widget_->size_hint().width) : 0;
    float trail_off = trailing_widget_ ? (vertical ? trailing_widget_->size_hint().height : trailing_widget_->size_hint().width) : 0;
    float bar_dim = vertical ? rect_.height : rect_.width;
    float base_available = bar_dim - lead_off - trail_off;

    float total_tabs = 0;
    for (int i = 0; i < static_cast<int>(tabs_.size()); i++) {
        total_tabs += tab_size(i);
    }

    if (total_tabs <= base_available) {
        show_scroll_buttons_ = false;
        scroll_offset_ = 0;
        prev_button_->set_visible(false);
        next_button_->set_visible(false);
        if (window_) window_->request_redraw();
        return;
    }

    show_scroll_buttons_ = true;
    Size sz_prev = prev_button_->size_hint();
    Size sz_next = next_button_->size_hint();
    float btn_size_prev = vertical ? sz_prev.height : sz_prev.width;
    float btn_size_next = vertical ? sz_next.height : sz_next.width;

    bool was_prev = prev_button_->is_visible();
    
    // Determine visibility. This is a bit recursive because available space depends on visibility.
    // If offset > 0, prev button is visible.
    bool now_prev = scroll_offset_ > 0;
    if (now_prev != was_prev) {
        // Jitter compensation: when prev button appears, it pushes the viewport start.
        // We adjust scroll_offset to keep the same tab physically pinned.
        if (now_prev) scroll_offset_ += btn_size_prev;
        else scroll_offset_ -= btn_size_prev;
    }

    float available = base_available - (now_prev ? btn_size_prev : 0);
    // If we assume next is visible:
    float max_scroll_with_next = std::max(0.0f, total_tabs - (available - btn_size_next));
    bool now_next = scroll_offset_ < max_scroll_with_next;
    
    if (now_next) available -= btn_size_next;
    float max_scroll = std::max(0.0f, total_tabs - available);
    
    scroll_offset_ = std::clamp(scroll_offset_, 0.0f, max_scroll);
    
    // Re-verify visibility after clamp
    now_prev = scroll_offset_ > 0;
    now_next = scroll_offset_ < max_scroll;

    prev_button_->set_visible(now_prev);
    next_button_->set_visible(now_next);

    if (window_) window_->request_redraw();
}

void TabWidget::set_orientation(TabOrientation o) {
    if (orientation_ == o) return;
    orientation_ = o;
    if (orientation_ == TabOrientation::North || orientation_ == TabOrientation::South) {
        prev_button_->set_text("<");
        next_button_->set_text(">");
    } else {
        prev_button_->set_text("^");
        next_button_->set_text("v");
    }
    layout_content();
}

void TabWidget::set_leading_widget(std::unique_ptr<Widget> widget) {
    widget->set_parent(this);
    leading_widget_ = std::move(widget);
    if (leading_widget_ && window_) leading_widget_->set_window(window_);
    layout_content();
}

void TabWidget::set_trailing_widget(std::unique_ptr<Widget> widget) {
    widget->set_parent(this);
    trailing_widget_ = std::move(widget);
    if (trailing_widget_ && window_) trailing_widget_->set_window(window_);
    layout_content();
}

float TabWidget::tab_bar_thickness() const {
    auto const &style = Theme::current().tab_widget;
    auto fm = Painter::measure_font_metrics(style.font_size);
    return fm.height + style.tab_padding_v * 2;
}

float TabWidget::tab_size(int i) const {
    auto const &style = Theme::current().tab_widget;
    float tw = Painter::measure_text(tabs_[i].title, style.font_size).width;
    return tw + close_btn_size_ + close_btn_gap_ + style.tab_padding_h * 2;
}

void TabWidget::layout_content() {
    float thickness = tab_bar_thickness();
    bool vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);

    Rect bar_rect = rect_;
    Rect content_rect = rect_;

    if (vertical) {
        bar_rect.width = thickness;
        if (orientation_ == TabOrientation::East) {
            bar_rect.x = rect_.x + rect_.width - thickness;
            content_rect.width -= thickness;
        } else {
            content_rect.x += thickness;
            content_rect.width -= thickness;
        }
    } else {
        bar_rect.height = thickness;
        if (orientation_ == TabOrientation::South) {
            bar_rect.y = rect_.y + rect_.height - thickness;
            content_rect.height -= thickness;
        } else {
            content_rect.y += thickness;
            content_rect.height -= thickness;
        }
    }

    if (leading_widget_) {
        Size sz = leading_widget_->size_hint();
        if (vertical) {
            leading_widget_->set_rect({bar_rect.x, bar_rect.y, thickness, sz.height});
        } else {
            leading_widget_->set_rect({bar_rect.x, bar_rect.y, sz.width, thickness});
        }
    }

    if (trailing_widget_) {
        Size sz = trailing_widget_->size_hint();
        if (vertical) {
            trailing_widget_->set_rect({bar_rect.x, bar_rect.y + bar_rect.height - sz.height, thickness, sz.height});
        } else {
            trailing_widget_->set_rect({bar_rect.x + bar_rect.width - sz.width, bar_rect.y, sz.width, thickness});
        }
    }

    float available = vertical ? rect_.height : rect_.width;
    if (leading_widget_) available -= vertical ? leading_widget_->size_hint().height : leading_widget_->size_hint().width;
    if (trailing_widget_) available -= vertical ? trailing_widget_->size_hint().height : trailing_widget_->size_hint().width;

    float total_tabs = 0;
    for (int i = 0; i < static_cast<int>(tabs_.size()); i++) {
        total_tabs += tab_size(i);
    }

    if (total_tabs > available) {
        show_scroll_buttons_ = true;
        Size sz_prev = prev_button_->size_hint();
        Size sz_next = next_button_->size_hint();

        float btn_size = vertical ? std::max(sz_prev.height, sz_next.height) : std::max(sz_prev.width, sz_next.width);
        float lead_off = leading_widget_ ? (vertical ? leading_widget_->size_hint().height : leading_widget_->size_hint().width) : 0;
        float trail_off = trailing_widget_ ? (vertical ? trailing_widget_->size_hint().height : trailing_widget_->size_hint().width) : 0;

        if (vertical) {
            prev_button_->set_rect({bar_rect.x, bar_rect.y + lead_off, thickness, btn_size});
            next_button_->set_rect({bar_rect.x, bar_rect.y + bar_rect.height - trail_off - btn_size, thickness, btn_size});
        } else {
            prev_button_->set_rect({bar_rect.x + lead_off, bar_rect.y, btn_size, thickness});
            next_button_->set_rect({bar_rect.x + bar_rect.width - trail_off - btn_size, bar_rect.y, btn_size, thickness});
        }
    } else {
        show_scroll_buttons_ = false;
        prev_button_->set_visible(false);
        next_button_->set_visible(false);
    }

    update_scroll_bounds();

    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size()))
        tabs_[current_].content->set_rect(content_rect);
}

TabWidget::HitResult TabWidget::hit_test_tab(Point p) const {
    auto const &style = Theme::current().tab_widget;
    float thickness = tab_bar_thickness();
    bool vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);

    float x = rect_.x, y = rect_.y;
    if (orientation_ == TabOrientation::South) y = rect_.y + rect_.height - thickness;
    if (orientation_ == TabOrientation::East) x = rect_.x + rect_.width - thickness;

    float start_pos = vertical ? rect_.y : rect_.x;
    if (leading_widget_) {
        Size sz = leading_widget_->size_hint();
        start_pos += vertical ? sz.height : sz.width;
    }

    float bar_start = start_pos;
    float bar_end = vertical ? rect_.y + rect_.height : rect_.x + rect_.width;
    if (trailing_widget_) {
        Size sz = trailing_widget_->size_hint();
        bar_end -= vertical ? sz.height : sz.width;
    }

    if (show_scroll_buttons_) {
        Size sz_prev = prev_button_->size_hint();
        Size sz_next = next_button_->size_hint();
        float btn_size_prev = vertical ? sz_prev.height : sz_prev.width;
        float btn_size_next = vertical ? sz_next.height : sz_next.width;

        if (prev_button_->is_visible()) {
            bar_start += btn_size_prev;
        }
        if (next_button_->is_visible()) {
            bar_end -= btn_size_next;
        }
    }

    if (vertical) {
        if (p.x < x || p.x >= x + thickness) return {};
        if (p.y < bar_start || p.y >= bar_end) return {};
        float draw_y = bar_start - scroll_offset_;
        for (int i = 0; i < static_cast<int>(tabs_.size()); i++) {
            float h = tab_size(i);
            if (p.y >= draw_y && p.y < draw_y + h) {
                float text_w = Painter::measure_text(tabs_[i].title, style.font_size).width;
                float close_cx = x + thickness / 2.0f;
                float close_cy;
                if (orientation_ == TabOrientation::West) {
                    close_cy = draw_y + h - style.tab_padding_h - text_w - close_btn_gap_ - close_btn_size_ / 2.0f;
                } else {
                    close_cy = draw_y + style.tab_padding_h + text_w + close_btn_gap_ + close_btn_size_ / 2.0f;
                }
                float hr = close_btn_size_ / 2.0f + 2.0f;
                bool on_close = (p.x >= close_cx - hr && p.x <= close_cx + hr &&
                                 p.y >= close_cy - hr && p.y <= close_cy + hr);
                return {i, on_close};
            }
            draw_y += h;
        }
    } else {
        if (p.y < y || p.y >= y + thickness) return {};
        if (p.x < bar_start || p.x >= bar_end) return {};
        float draw_x = bar_start - scroll_offset_;
        for (int i = 0; i < static_cast<int>(tabs_.size()); i++) {
            float w = tab_size(i);
            if (p.x >= draw_x && p.x < draw_x + w) {
                float text_w = Painter::measure_text(tabs_[i].title, style.font_size).width;
                float close_x = draw_x + style.tab_padding_h + text_w + close_btn_gap_;
                float close_cy = y + thickness / 2.0f;
                float hr = close_btn_size_ / 2.0f + 2.0f;
                bool on_close = (p.x >= close_x - 2.0f && p.x <= close_x + close_btn_size_ + 2.0f &&
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
    for (auto &tab : tabs_)
        tab.content->set_window(w);
    if (leading_widget_) leading_widget_->set_window(w);
    if (trailing_widget_) trailing_widget_->set_window(w);
    prev_button_->set_window(w);
    next_button_->set_window(w);
}

void TabWidget::paint(Painter &painter) {
    if (tabs_.empty()) return;

    auto const &style = Theme::current().tab_widget;
    float thickness = tab_bar_thickness();
    auto fm = painter.font_metrics(style.font_size);
    bool vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);

    float bar_x = rect_.x, bar_y = rect_.y;
    float bar_w = rect_.width, bar_h = rect_.height;

    if (vertical) {
        bar_w = thickness;
        if (orientation_ == TabOrientation::East) bar_x = rect_.x + rect_.width - thickness;
    } else {
        bar_h = thickness;
        if (orientation_ == TabOrientation::South) bar_y = rect_.y + rect_.height - thickness;
    }

    painter.fill_rect({bar_x, bar_y, bar_w, bar_h}, style.tab_inactive_bg);

    auto paint_tab = [&](int i, float draw_x, float draw_y) {
        float size = tab_size(i);
        Rect tab_rect = vertical ? Rect{bar_x, draw_y, thickness, size}
                                 : Rect{draw_x, bar_y, size, thickness};
        bool active = (i == current_);
        bool hovered = (i == hovered_tab_ && !active);

        Color bg = active  ? style.tab_active_bg
                 : hovered ? style.tab_hover_bg
                           : style.tab_inactive_bg;
        Color text_col = active ? style.tab_active_text : style.tab_inactive_text;

        if (active && style.corner_radius > 0) {
            float r = style.corner_radius;
            // Simplified: just draw rounded rect for now, properly clipping would be better
            painter.fill_rounded_rect(tab_rect, bg, r);
        } else {
            painter.fill_rect(tab_rect, bg);
        }

        painter.push_clip(tab_rect);
        if (vertical) {
            if (orientation_ == TabOrientation::West) {
                // Bottom to top (CCW): Baseline is on the right of the text
                float text_x = tab_rect.x + (tab_rect.width - fm.height) / 2.0f + fm.ascent;
                painter.draw_text(tabs_[i].title, {text_x, tab_rect.y + size - style.tab_padding_h},
                                  text_col, style.font_size, FontFamily::System,
                                  Painter::TextOrientation::VerticalCCW);
            } else {
                // Top to bottom (CW): Baseline is on the left of the text
                float text_x = tab_rect.x + (tab_rect.width + fm.height) / 2.0f - fm.ascent;
                painter.draw_text(tabs_[i].title, {text_x, tab_rect.y + style.tab_padding_h},
                                  text_col, style.font_size, FontFamily::System,
                                  Painter::TextOrientation::VerticalCW);
            }
        } else {
            float text_y = tab_rect.y + (tab_rect.height - fm.height) / 2.0f + fm.ascent;
            painter.draw_text(tabs_[i].title,
                             {tab_rect.x + style.tab_padding_h, text_y},
                             text_col, style.font_size);
        }

        float text_w = painter.text_size(tabs_[i].title, style.font_size).width;
        float close_cx, close_cy;
        if (vertical) {
            close_cx = tab_rect.x + tab_rect.width / 2.0f;
            if (orientation_ == TabOrientation::West) {
                close_cy = tab_rect.y + size - style.tab_padding_h - text_w - close_btn_gap_ - close_btn_size_ / 2.0f;
            } else {
                close_cy = tab_rect.y + style.tab_padding_h + text_w + close_btn_gap_ + close_btn_size_ / 2.0f;
            }
        } else {
            float close_x = tab_rect.x + style.tab_padding_h + text_w + close_btn_gap_;
            close_cy = tab_rect.y + tab_rect.height / 2.0f;
            close_cx = close_x + close_btn_size_ / 2.0f;
        }

        if (i == hovered_close_) {
            float cr = close_btn_size_ / 2.0f + 1.0f;
            Color hover_circle = Color::rgba(text_col.r, text_col.g, text_col.b, 0.15f);
            painter.fill_circle({close_cx, close_cy}, cr, hover_circle);
        }

        float cs = close_btn_size_ * 0.3f;
        Color x_col = (i == hovered_close_) ? text_col
                     : Color::rgba(text_col.r, text_col.g, text_col.b, 0.5f);
        painter.draw_line({close_cx - cs, close_cy - cs},
                         {close_cx + cs, close_cy + cs}, x_col, 1.5f);
        painter.draw_line({close_cx + cs, close_cy - cs},
                         {close_cx - cs, close_cy + cs}, x_col, 1.5f);
        painter.pop_clip();

        if (active) {
            Rect indicator;
            if (orientation_ == TabOrientation::North) indicator = {tab_rect.x, tab_rect.y + thickness - 3, tab_rect.width, 2};
            else if (orientation_ == TabOrientation::South) indicator = {tab_rect.x, tab_rect.y + 1, tab_rect.width, 2};
            else if (orientation_ == TabOrientation::West) indicator = {tab_rect.x + thickness - 3, tab_rect.y, 2, tab_rect.height};
            else if (orientation_ == TabOrientation::East) indicator = {tab_rect.x + 1, tab_rect.y, 2, tab_rect.height};
            painter.fill_rect(indicator, Theme::current().combobox.border_focused);
        }
    };

    float start_pos = vertical ? rect_.y : rect_.x;
    if (leading_widget_) {
        Size sz = leading_widget_->size_hint();
        start_pos += vertical ? sz.height : sz.width;
        leading_widget_->draw(painter);
    }

    float bar_start = start_pos;
    float bar_end = vertical ? rect_.y + rect_.height : rect_.x + rect_.width;
    if (trailing_widget_) {
        Size sz = trailing_widget_->size_hint();
        bar_end -= vertical ? sz.height : sz.width;
        trailing_widget_->draw(painter);
    }

    if (show_scroll_buttons_) {
        Size sz_prev = prev_button_->size_hint();
        Size sz_next = next_button_->size_hint();
        float btn_size_prev = vertical ? sz_prev.height : sz_prev.width;
        float btn_size_next = vertical ? sz_next.height : sz_next.width;

        if (prev_button_->is_visible()) {
            prev_button_->draw(painter);
            bar_start += btn_size_prev;
        }
        if (next_button_->is_visible()) {
            next_button_->draw(painter);
            bar_end -= btn_size_next;
        }
    }

    Rect scrollable_rect = vertical ? Rect{bar_x, bar_start, thickness, bar_end - bar_start}
                                    : Rect{bar_start, bar_y, bar_end - bar_start, thickness};

    painter.push_clip(scrollable_rect);

    float cur_x = vertical ? bar_x : bar_start - scroll_offset_;
    float cur_y = vertical ? bar_start - scroll_offset_ : bar_y;
    float drag_draw_x = 0, drag_draw_y = 0;
    for (int i = 0; i < static_cast<int>(tabs_.size()); i++) {
        float size = tab_size(i);
        if (dragging_ && i == drag_tab_) {
            if (vertical) {
                drag_draw_x = bar_x;
                drag_draw_y = cur_y + drag_offset_x_;
            } else {
                drag_draw_x = cur_x + drag_offset_x_;
                drag_draw_y = bar_y;
            }
        } else {
            // Only paint if at least partially visible
            bool visible = vertical ? (cur_y + size > bar_start && cur_y < bar_end)
                                    : (cur_x + size > bar_start && cur_x < bar_end);
            if (visible) {
                paint_tab(i, cur_x, cur_y);
            }
        }
        if (vertical) cur_y += size; else cur_x += size;
    }
    if (dragging_ && drag_tab_ >= 0) {
        paint_tab(drag_tab_, drag_draw_x, drag_draw_y);
    }

    painter.pop_clip();

    if (style.border_width > 0) {
        if (orientation_ == TabOrientation::North)
            painter.draw_line({rect_.x, rect_.y + thickness}, {rect_.x + rect_.width, rect_.y + thickness}, style.border, style.border_width);
        else if (orientation_ == TabOrientation::South)
            painter.draw_line({rect_.x, rect_.y + rect_.height - thickness}, {rect_.x + rect_.width, rect_.y + rect_.height - thickness}, style.border, style.border_width);
        else if (orientation_ == TabOrientation::West)
            painter.draw_line({rect_.x + thickness, rect_.y}, {rect_.x + thickness, rect_.y + rect_.height}, style.border, style.border_width);
        else if (orientation_ == TabOrientation::East)
            painter.draw_line({rect_.x + rect_.width - thickness, rect_.y}, {rect_.x + rect_.width - thickness, rect_.y + rect_.height}, style.border, style.border_width);
    }

    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
        tabs_[current_].content->draw(painter);
    }
}

bool TabWidget::handle_tab_drag(MouseEvent const &event) {
    bool vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);

    if (event.type == MouseEvent::Type::Drag) {
        float mouse_pos = vertical ? event.position.y : event.position.x;
        drag_offset_x_ = mouse_pos - drag_start_x_;

        float start_pos = vertical ? rect_.y : rect_.x;
        if (leading_widget_) {
            Size sz = leading_widget_->size_hint();
            start_pos += vertical ? sz.height : sz.width;
        }

        // Auto-scroll when dragging near edges
        float bar_start = start_pos;
        float bar_end = vertical ? rect_.y + rect_.height : rect_.x + rect_.width;
        if (trailing_widget_) {
            Size sz = trailing_widget_->size_hint();
            bar_end -= vertical ? sz.height : sz.width;
        }
        if (show_scroll_buttons_) {
            Size sz_prev = prev_button_->size_hint();
            Size sz_next = next_button_->size_hint();
            float btn_size_prev = vertical ? sz_prev.height : sz_prev.width;
            float btn_size_next = vertical ? sz_next.height : sz_next.width;
            bar_start += btn_size_prev;
            bar_end -= btn_size_next;
        }

        float scroll_speed = 0;
        if (mouse_pos < bar_start + 20)
            scroll_speed = -5;
        else if (mouse_pos > bar_end - 20)
            scroll_speed = 5;

        if (scroll_speed != 0) {
            float old_offset = scroll_offset_;
            scroll_offset_ += scroll_speed;
            update_scroll_bounds();
            drag_start_x_ -= (scroll_offset_ - old_offset);
            drag_offset_x_ = mouse_pos - drag_start_x_;
        }

        bool swapped;
        do {
            swapped = false;
            float pos = bar_start - scroll_offset_;
            for (int i = 0; i < drag_tab_; i++)
                pos += tab_size(i);
            float dragged_center = pos + tab_size(drag_tab_) / 2.0f + drag_offset_x_;
            if (drag_tab_ > 0) {
                float prev_size = tab_size(drag_tab_ - 1);
                float prev_mid = pos - prev_size / 2.0f;
                if (dragged_center < prev_mid) {
                    std::swap(tabs_[drag_tab_], tabs_[drag_tab_ - 1]);
                    if (current_ == drag_tab_)
                        current_ = drag_tab_ - 1;
                    else if (current_ == drag_tab_ - 1)
                        current_ = drag_tab_;
                    drag_tab_--;
                    drag_start_x_ -= prev_size;
                    drag_offset_x_ = mouse_pos - drag_start_x_;
                    swapped = true;
                }
            }
            if (!swapped && drag_tab_ < static_cast<int>(tabs_.size()) - 1) {
                float cur_size = tab_size(drag_tab_);
                float next_size = tab_size(drag_tab_ + 1);
                float next_mid = pos + cur_size + next_size / 2.0f;
                if (dragged_center > next_mid) {
                    std::swap(tabs_[drag_tab_], tabs_[drag_tab_ + 1]);
                    if (current_ == drag_tab_)
                        current_ = drag_tab_ + 1;
                    else if (current_ == drag_tab_ + 1)
                        current_ = drag_tab_;
                    drag_tab_++;
                    drag_start_x_ += next_size;
                    drag_offset_x_ = mouse_pos - drag_start_x_;
                    swapped = true;
                }
            }
        } while (swapped);

        if (window_) window_->request_redraw();
        return true;
    }

    if (event.type == MouseEvent::Type::Release) {
        dragging_ = false;
        drag_offset_x_ = 0;
        drag_tab_ = -1;
        if (window_) window_->request_redraw();
        return true;
    }

    // While dragging, we consume all mouse events to ensure we get the Release
    return true;
}

bool TabWidget::handle_mouse(MouseEvent const &event) {
    bool vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);

    if (dragging_) {
        return handle_tab_drag(event);
    }

    if (leading_widget_ && leading_widget_->handle_mouse(event)) return true;
    if (trailing_widget_ && trailing_widget_->handle_mouse(event)) return true;

    if (show_scroll_buttons_) {
        if (prev_button_->handle_mouse(event)) return true;
        if (next_button_->handle_mouse(event)) return true;
    }

    float thickness = tab_bar_thickness();

    bool in_bar = false;
    float bar_x = rect_.x, bar_y = rect_.y;
    if (vertical) {
        if (orientation_ == TabOrientation::East) bar_x = rect_.x + rect_.width - thickness;
        in_bar = event.position.x >= bar_x && event.position.x < bar_x + thickness &&
                 event.position.y >= rect_.y && event.position.y < rect_.y + rect_.height;
    } else {
        if (orientation_ == TabOrientation::South) bar_y = rect_.y + rect_.height - thickness;
        in_bar = event.position.y >= bar_y && event.position.y < bar_y + thickness &&
                 event.position.x >= rect_.x && event.position.x < rect_.x + rect_.width;
    }

    if (event.type == MouseEvent::Type::Move) {
        if (in_bar) {
            auto hr = hit_test_tab(event.position);
            hovered_tab_ = hr.tab;
            hovered_close_ = hr.on_close ? hr.tab : -1;
            if (window_) window_->request_redraw();
            return true;
        }
        if (hovered_tab_ != -1 || hovered_close_ != -1) {
            hovered_tab_ = -1;
            hovered_close_ = -1;
            if (window_) window_->request_redraw();
        }
        if (current_ >= 0 && current_ < static_cast<int>(tabs_.size()))
            return tabs_[current_].content->handle_mouse(event);
        return false;
    }

    if (event.type == MouseEvent::Type::Leave) {
        if (hovered_tab_ != -1 || hovered_close_ != -1) {
            hovered_tab_ = -1;
            hovered_close_ = -1;
            if (window_) window_->request_redraw();
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Scroll && in_bar) {
        float delta = vertical ? event.scroll_dy : (event.scroll_dx + event.scroll_dy);
        scroll_offset_ -= delta * 20.0f; // Scroll speed
        update_scroll_bounds();
        return true;
    }

    if (event.type == MouseEvent::Type::Press && in_bar) {
        if (window_) window_->set_focused_widget(this);
        auto hr = hit_test_tab(event.position);
        if (hr.tab >= 0 && hr.on_close) {
            spdlog::info("Tab close requested: [{}] \"{}\"", hr.tab, tabs_[hr.tab].title);
            if (on_tab_close)
                on_tab_close(hr.tab, tabs_[hr.tab].title);
            return true;
        }
        if (hr.tab >= 0) {
            if (hr.tab != current_)
                set_current(hr.tab);
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

    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size()))
        return tabs_[current_].content->handle_mouse(event);
    return false;
}

bool TabWidget::handle_key(KeyEvent const &event) {
    if (event.type == KeyEvent::Type::Press && event.ctrl) {
        if (event.key == Key::PageUp) {
            if (event.shift) {
                if (current_ > 0) {
                    std::swap(tabs_[current_], tabs_[current_ - 1]);
                    set_current(current_ - 1);
                }
            } else {
                int next = (current_ - 1 + static_cast<int>(tabs_.size())) % static_cast<int>(tabs_.size());
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
                int next = (current_ + 1) % static_cast<int>(tabs_.size());
                set_current(next);
            }
            return true;
        }
    }

    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size()))
        return tabs_[current_].content->handle_key(event);
    return false;
}

Size TabWidget::size_hint() const {
    float thickness = tab_bar_thickness();
    float max_w = 0, max_h = 0;
    for (auto const &tab : tabs_) {
        auto hint = tab.content->size_hint();
        max_w = std::max(max_w, hint.width);
        max_h = std::max(max_h, hint.height);
    }
    float total_tab_size = 0;
    for (int i = 0; i < static_cast<int>(tabs_.size()); i++)
        total_tab_size += tab_size(i);

    bool vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);
    float lead_size = 0, trail_size = 0;
    if (leading_widget_) {
        Size sz = leading_widget_->size_hint();
        lead_size = vertical ? sz.height : sz.width;
    }
    if (trailing_widget_) {
        Size sz = trailing_widget_->size_hint();
        trail_size = vertical ? sz.height : sz.width;
    }

    float bar_size = total_tab_size + lead_size + trail_size;

    if (orientation_ == TabOrientation::North || orientation_ == TabOrientation::South) {
        return {std::max(max_w, bar_size), max_h + thickness};
    } else {
        return {max_w + thickness, std::max(max_h, bar_size)};
    }
}

Widget *TabWidget::find_focusable_at(Point p) {
    if (leading_widget_) {
        auto *w = leading_widget_->find_focusable_at(p);
        if (w) return w;
    }
    if (trailing_widget_) {
        auto *w = trailing_widget_->find_focusable_at(p);
        if (w) return w;
    }
    if (show_scroll_buttons_) {
        if (auto *w = prev_button_->find_focusable_at(p)) return w;
        if (auto *w = next_button_->find_focusable_at(p)) return w;
    }
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size()))
        return tabs_[current_].content->find_focusable_at(p);
    return nullptr;
}

Widget *TabWidget::widget_at(Point p) {
    if (!visible_ || !hit_test(p)) return nullptr;
    if (leading_widget_) {
        auto *w = leading_widget_->widget_at(p);
        if (w) return w;
    }
    if (trailing_widget_) {
        auto *w = trailing_widget_->widget_at(p);
        if (w) return w;
    }
    if (show_scroll_buttons_) {
        if (auto *w = prev_button_->widget_at(p)) return w;
        if (auto *w = next_button_->widget_at(p)) return w;
    }
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
        if (auto *w = tabs_[current_].content->widget_at(p))
            return w;
    }
    return this;
}

void TabWidget::collect_focusables(std::vector<Widget *> &out) {
    if (focusable() && enabled_ && visible_) {
        out.push_back(this);
    }
    if (leading_widget_) leading_widget_->collect_focusables(out);
    if (trailing_widget_) trailing_widget_->collect_focusables(out);
    if (show_scroll_buttons_) {
        prev_button_->collect_focusables(out);
        next_button_->collect_focusables(out);
    }
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size()))
        tabs_[current_].content->collect_focusables(out);
}

void TabWidget::collect_mnemonics(std::vector<Widget *> &out) {
    if (leading_widget_) leading_widget_->collect_mnemonics(out);
    if (trailing_widget_) trailing_widget_->collect_mnemonics(out);
    if (show_scroll_buttons_) {
        prev_button_->collect_mnemonics(out);
        next_button_->collect_mnemonics(out);
    }
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size()))
        tabs_[current_].content->collect_mnemonics(out);
}

void TabWidget::for_each_child(std::function<void(Widget *)> const &callback) {
    if (leading_widget_) callback(leading_widget_.get());
    if (trailing_widget_) callback(trailing_widget_.get());
    if (show_scroll_buttons_) {
        callback(prev_button_.get());
        callback(next_button_.get());
    }
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
        callback(tabs_[current_].content.get());
    }
}

} // namespace toolkit
