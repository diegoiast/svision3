#include "toolkit/tab_widget.hpp"
#include "toolkit/theme.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace toolkit {

void TabWidget::add_tab(std::string title, std::unique_ptr<Widget> content) {
    if (window_) content->set_window(window_);
    tabs_.push_back({std::move(title), std::move(content)});
    if (rect_.width > 0 || rect_.height > 0)
        layout_content();
}

void TabWidget::set_current(int index) {
    if (index >= 0 && index < static_cast<int>(tabs_.size())) {
        current_ = index;
        layout_content();
    }
}

void TabWidget::set_orientation(TabOrientation o) {
    if (orientation_ == o) return;
    orientation_ = o;
    layout_content();
}

void TabWidget::set_leading_widget(std::unique_ptr<Widget> widget) {
    leading_widget_ = std::move(widget);
    if (leading_widget_ && window_) leading_widget_->set_window(window_);
    layout_content();
}

void TabWidget::set_trailing_widget(std::unique_ptr<Widget> widget) {
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

    if (vertical) {
        if (p.x < x || p.x >= x + thickness) return {};
        float draw_y = start_pos;
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
        float draw_x = start_pos;
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
    if (trailing_widget_) {
        trailing_widget_->draw(painter);
    }

    float cur_x = vertical ? bar_x : start_pos;
    float cur_y = vertical ? start_pos : bar_y;
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
            paint_tab(i, cur_x, cur_y);
        }
        if (vertical) cur_y += size; else cur_x += size;
    }
    if (dragging_ && drag_tab_ >= 0) {
        paint_tab(drag_tab_, drag_draw_x, drag_draw_y);
    }

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

bool TabWidget::handle_mouse(MouseEvent const &event) {
    if (leading_widget_ && leading_widget_->handle_mouse(event)) return true;
    if (trailing_widget_ && trailing_widget_->handle_mouse(event)) return true;

    float thickness = tab_bar_thickness();
    bool vertical = (orientation_ == TabOrientation::East || orientation_ == TabOrientation::West);

    bool in_bar = false;
    if (vertical) {
        float bar_x = (orientation_ == TabOrientation::West) ? rect_.x : rect_.x + rect_.width - thickness;
        in_bar = event.position.x >= bar_x && event.position.x < bar_x + thickness &&
                 event.position.y >= rect_.y && event.position.y < rect_.y + rect_.height;
    } else {
        float bar_y = (orientation_ == TabOrientation::North) ? rect_.y : rect_.y + rect_.height - thickness;
        in_bar = event.position.y >= bar_y && event.position.y < bar_y + thickness &&
                 event.position.x >= rect_.x && event.position.x < rect_.x + rect_.width;
    }

    if (event.type == MouseEvent::Type::Move) {
        if (in_bar) {
            auto hr = hit_test_tab(event.position);
            hovered_tab_ = hr.tab;
            hovered_close_ = hr.on_close ? hr.tab : -1;
            return true;
        }
        hovered_tab_ = -1;
        hovered_close_ = -1;
        if (current_ >= 0 && current_ < static_cast<int>(tabs_.size()))
            return tabs_[current_].content->handle_mouse(event);
        return false;
    }

    if (event.type == MouseEvent::Type::Press && in_bar) {
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

    if (event.type == MouseEvent::Type::Drag && dragging_) {
        drag_offset_x_ = (vertical ? event.position.y : event.position.x) - drag_start_x_;

        float pos = vertical ? rect_.y : rect_.x;
        for (int i = 0; i < drag_tab_; i++)
            pos += tab_size(i);
        float dragged_center = pos + tab_size(drag_tab_) / 2.0f + drag_offset_x_;

        if (drag_tab_ > 0) {
            float prev_pos = pos - tab_size(drag_tab_ - 1);
            float prev_mid = prev_pos + tab_size(drag_tab_ - 1) / 2.0f;
            if (dragged_center < prev_mid) {
                std::swap(tabs_[drag_tab_], tabs_[drag_tab_ - 1]);
                if (current_ == drag_tab_) current_ = drag_tab_ - 1;
                else if (current_ == drag_tab_ - 1) current_ = drag_tab_;
                drag_tab_--;
                drag_start_x_ = vertical ? event.position.y : event.position.x;
                drag_offset_x_ = 0;
            }
        }
        if (drag_tab_ < static_cast<int>(tabs_.size()) - 1) {
            float next_pos = pos + tab_size(drag_tab_);
            float next_mid = next_pos + tab_size(drag_tab_ + 1) / 2.0f;
            if (dragged_center > next_mid) {
                std::swap(tabs_[drag_tab_], tabs_[drag_tab_ + 1]);
                if (current_ == drag_tab_) current_ = drag_tab_ + 1;
                else if (current_ == drag_tab_ + 1) current_ = drag_tab_;
                drag_tab_++;
                drag_start_x_ = vertical ? event.position.y : event.position.x;
                drag_offset_x_ = 0;
            }
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Release && dragging_) {
        dragging_ = false;
        drag_offset_x_ = 0;
        drag_tab_ = -1;
        return true;
    }

    hovered_tab_ = -1;
    hovered_close_ = -1;

    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size()))
        return tabs_[current_].content->handle_mouse(event);
    return false;
}

bool TabWidget::handle_key(KeyEvent const &event) {
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

    if (orientation_ == TabOrientation::North || orientation_ == TabOrientation::South) {
        return {std::max(max_w, total_tab_size), max_h + thickness};
    } else {
        return {max_w + thickness, std::max(max_h, total_tab_size)};
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
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
        if (auto *w = tabs_[current_].content->widget_at(p))
            return w;
    }
    return this;
}

void TabWidget::collect_focusables(std::vector<Widget *> &out) {
    if (leading_widget_) leading_widget_->collect_focusables(out);
    if (trailing_widget_) trailing_widget_->collect_focusables(out);
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size()))
        tabs_[current_].content->collect_focusables(out);
}

void TabWidget::collect_mnemonics(std::vector<Widget *> &out) {
    if (leading_widget_) leading_widget_->collect_mnemonics(out);
    if (trailing_widget_) trailing_widget_->collect_mnemonics(out);
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size()))
        tabs_[current_].content->collect_mnemonics(out);
}

void TabWidget::for_each_child(std::function<void(Widget *)> const &callback) {
    if (leading_widget_) callback(leading_widget_.get());
    if (trailing_widget_) callback(trailing_widget_.get());
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
        callback(tabs_[current_].content.get());
    }
}

} // namespace toolkit
