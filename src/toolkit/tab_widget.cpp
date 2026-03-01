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

float TabWidget::tab_bar_height() const {
    auto const &style = Theme::current().tab_widget;
    auto fm = Painter::measure_font_metrics(style.font_size);
    return fm.height + style.tab_padding_v * 2;
}

float TabWidget::tab_width(int i) const {
    auto const &style = Theme::current().tab_widget;
    float tw = Painter::measure_text(tabs_[i].title, style.font_size).width;
    return tw + close_btn_size_ + close_btn_gap_ + style.tab_padding_h * 2;
}

void TabWidget::layout_content() {
    if (tabs_.empty()) return;
    float bar_h = tab_bar_height();
    Rect content_rect{rect_.x, rect_.y + bar_h,
                      rect_.width, rect_.height - bar_h};
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size()))
        tabs_[current_].content->set_rect(content_rect);
}

TabWidget::HitResult TabWidget::hit_test_tab(Point p) const {
    auto const &style = Theme::current().tab_widget;
    float bar_h = tab_bar_height();
    if (p.y < rect_.y || p.y >= rect_.y + bar_h) return {};

    float x = rect_.x;
    for (int i = 0; i < static_cast<int>(tabs_.size()); i++) {
        float w = tab_width(i);
        if (p.x >= x && p.x < x + w) {
            float text_w = Painter::measure_text(tabs_[i].title, style.font_size).width;
            float close_x = x + style.tab_padding_h + text_w + close_btn_gap_;
            float close_cy = rect_.y + bar_h / 2.0f;
            float hr = close_btn_size_ / 2.0f + 2.0f;
            bool on_close = (p.x >= close_x - 2.0f && p.x <= close_x + close_btn_size_ + 2.0f &&
                             p.y >= close_cy - hr && p.y <= close_cy + hr);
            return {i, on_close};
        }
        x += w;
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

    painter.push_clip(rect_);

    auto const &style = Theme::current().tab_widget;
    float bar_h = tab_bar_height();
    auto fm = painter.font_metrics(style.font_size);

    painter.fill_rect({rect_.x, rect_.y, rect_.width, bar_h}, style.tab_inactive_bg);

    auto paint_tab = [&](int i, float draw_x) {
        float tw_total = tab_width(i);
        Rect tab_rect{draw_x, rect_.y, tw_total, bar_h};
        bool active = (i == current_);
        bool hovered = (i == hovered_tab_ && !active);

        Color bg = active  ? style.tab_active_bg
                 : hovered ? style.tab_hover_bg
                           : style.tab_inactive_bg;
        Color text_col = active ? style.tab_active_text : style.tab_inactive_text;

        if (active && style.corner_radius > 0) {
            float r = style.corner_radius;
            Rect top{tab_rect.x, tab_rect.y, tab_rect.width, r};
            painter.fill_rounded_rect(top, bg, r);
            Rect bottom{tab_rect.x, tab_rect.y + r, tab_rect.width, tab_rect.height - r};
            painter.fill_rect(bottom, bg);
        } else {
            painter.fill_rect(tab_rect, bg);
        }

        float text_y = tab_rect.y + (bar_h - fm.height) / 2.0f + fm.ascent;
        painter.draw_text(tabs_[i].title,
                         {tab_rect.x + style.tab_padding_h, text_y},
                         text_col, style.font_size);

        float text_w = painter.text_size(tabs_[i].title, style.font_size).width;
        float close_x = tab_rect.x + style.tab_padding_h + text_w + close_btn_gap_;
        float close_cy = tab_rect.y + bar_h / 2.0f;
        float close_cx = close_x + close_btn_size_ / 2.0f;

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

        if (active) {
            painter.fill_rect({tab_rect.x, tab_rect.y + bar_h - 2, tab_rect.width, 2},
                              Theme::current().combobox.border_focused);
        }
    };

    // Draw non-dragged tabs first, then the dragged tab on top
    float x = rect_.x;
    float drag_draw_x = 0;
    for (int i = 0; i < static_cast<int>(tabs_.size()); i++) {
        float tw_total = tab_width(i);
        if (dragging_ && i == drag_tab_) {
            drag_draw_x = x + drag_offset_x_;
        } else {
            paint_tab(i, x);
        }
        x += tw_total;
    }
    if (dragging_ && drag_tab_ >= 0)
        paint_tab(drag_tab_, drag_draw_x);

    if (style.border_width > 0) {
        painter.draw_line({rect_.x, rect_.y + bar_h},
                         {rect_.x + rect_.width, rect_.y + bar_h},
                         style.border, style.border_width);
    }

    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
        auto &content = tabs_[current_].content;
        auto r = content->rect();
        painter.push_clip(r);
        content->paint(painter);
        painter.pop_clip();
    }
    painter.pop_clip();
}

bool TabWidget::handle_mouse(MouseEvent const &event) {
    float bar_h = tab_bar_height();
    bool in_bar = event.position.y >= rect_.y &&
                  event.position.y < rect_.y + bar_h &&
                  event.position.x >= rect_.x &&
                  event.position.x < rect_.x + rect_.width;

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
            drag_start_x_ = event.position.x;
            drag_offset_x_ = 0;
            return true;
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Drag && dragging_) {
        drag_offset_x_ = event.position.x - drag_start_x_;

        float x = rect_.x;
        for (int i = 0; i < drag_tab_; i++)
            x += tab_width(i);
        float dragged_center = x + tab_width(drag_tab_) / 2.0f + drag_offset_x_;

        if (drag_tab_ > 0) {
            float prev_x = x - tab_width(drag_tab_ - 1);
            float prev_mid = prev_x + tab_width(drag_tab_ - 1) / 2.0f;
            if (dragged_center < prev_mid) {
                std::swap(tabs_[drag_tab_], tabs_[drag_tab_ - 1]);
                if (current_ == drag_tab_) current_ = drag_tab_ - 1;
                else if (current_ == drag_tab_ - 1) current_ = drag_tab_;
                drag_tab_--;
                drag_start_x_ = event.position.x;
                drag_offset_x_ = 0;
            }
        }
        if (drag_tab_ < static_cast<int>(tabs_.size()) - 1) {
            float next_x = x + tab_width(drag_tab_);
            float next_mid = next_x + tab_width(drag_tab_ + 1) / 2.0f;
            if (dragged_center > next_mid) {
                std::swap(tabs_[drag_tab_], tabs_[drag_tab_ + 1]);
                if (current_ == drag_tab_) current_ = drag_tab_ + 1;
                else if (current_ == drag_tab_ + 1) current_ = drag_tab_;
                drag_tab_++;
                drag_start_x_ = event.position.x;
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
    float bar_h = tab_bar_height();
    float max_w = 0, max_h = 0;
    for (auto const &tab : tabs_) {
        auto hint = tab.content->size_hint();
        max_w = std::max(max_w, hint.width);
        max_h = std::max(max_h, hint.height);
    }
    float tabs_w = 0;
    for (int i = 0; i < static_cast<int>(tabs_.size()); i++)
        tabs_w += tab_width(i);
    return {std::max(max_w, tabs_w), max_h + bar_h};
}

Widget *TabWidget::find_focusable_at(Point p) {
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size()))
        return tabs_[current_].content->find_focusable_at(p);
    return nullptr;
}

Widget *TabWidget::widget_at(Point p) {
    if (!visible_ || !hit_test(p)) return nullptr;
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size())) {
        if (auto *w = tabs_[current_].content->widget_at(p))
            return w;
    }
    return this;
}

void TabWidget::collect_focusables(std::vector<Widget *> &out) {
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size()))
        tabs_[current_].content->collect_focusables(out);
}

void TabWidget::collect_mnemonics(std::vector<Widget *> &out) {
    if (current_ >= 0 && current_ < static_cast<int>(tabs_.size()))
        tabs_[current_].content->collect_mnemonics(out);
}

} // namespace toolkit
