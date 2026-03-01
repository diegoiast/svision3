// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/combobox.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>
#include <cmath>

namespace toolkit {

Combobox::Combobox(std::vector<std::string> items) : items_(std::move(items)) {}

void Combobox::set_items(std::vector<std::string> items) {
    items_ = std::move(items);
    if (selected_index_ >= static_cast<int>(items_.size())) {
        selected_index_ = items_.empty() ? -1 : 0;
    }
}

void Combobox::set_selected(int index) {
    if (index >= 0 && index < static_cast<int>(items_.size())) {
        selected_index_ = index;
    }
}

std::string Combobox::selected_text() const {
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(items_.size())) {
        return items_[selected_index_];
    }
    return {};
}

float Combobox::dropdown_item_height() const {
    auto const &style = Theme::current().combobox;
    return style.font_size + style.item_padding * 2.0f;
}

Rect Combobox::dropdown_bounds() const {
    auto item_h = dropdown_item_height();
    auto drop_h = item_h * static_cast<float>(drop_max_visible_);
    auto drop_y = rect_.y + rect_.height;

    // If it was flipped above, recalculate
    if (window()) {
        auto win_h = window()->size().height;
        auto space_below = win_h - (rect_.y + rect_.height);
        auto full_h = item_h * static_cast<float>(items_.size());
        if (full_h > space_below && rect_.y > space_below) {
            drop_y = rect_.y - drop_h;
        }
    }

    return {rect_.x, drop_y, rect_.width, drop_h};
}

int Combobox::item_index_at(Point p) const {
    // FIXME: this is a nice utility - move to widget? or Point? Rect?
    // FIXME: variable name is too short and non descriptive
    auto db = dropdown_bounds();
    if (!db.contains(p)) {
        return -1;
    }
    auto item_h = dropdown_item_height();
    auto idx = static_cast<int>((p.y - db.y + drop_scroll_) / item_h);
    if (idx < 0 || idx >= static_cast<int>(items_.size())) {
        return -1;
    }
    return idx;
}

void Combobox::open_dropdown() {
    if (!window() || items_.empty()) {
        return;
    }

    auto item_h = dropdown_item_height();
    auto total = static_cast<int>(items_.size());
    auto win_h = window()->size().height;
    auto space_below = win_h - (rect_.y + rect_.height);
    auto space_above = rect_.y;
    auto best_space = std::max(space_below, space_above);
    drop_max_visible_ = total;
    open_ = true;
    hovered_index_ = selected_index_;
    drop_scroll_ = 0;

    auto max_fit = std::max(1, static_cast<int>(best_space / item_h));
    if (max_fit < total) {
        drop_max_visible_ = max_fit;
    }

    // Scroll so the selected item is visible
    if (selected_index_ >= 0 && selected_index_ >= drop_max_visible_) {
        drop_scroll_ = item_h * static_cast<float>(selected_index_ - drop_max_visible_ + 1);
    }

    Popup popup;
    popup.bounds = dropdown_bounds();
    popup.on_paint = [this](Painter &p) { paint_dropdown(p); };
    popup.on_mouse = [this](MouseEvent const &e) { return handle_dropdown_mouse(e); };
    popup.on_key = [this](KeyEvent const &e) { return handle_dropdown_key(e); };
    window()->open_popup(std::move(popup));
}

void Combobox::close_dropdown() {
    open_ = false;
    if (window()) {
        window()->close_popup();
    }
}

void Combobox::paint(Painter &painter) {
    auto const &style = Theme::current().combobox;

    Color border = focused_ ? style.border_focused : style.border;
    painter.draw_frame(rect_, style.background, border, style, true);

    auto fm = painter.font_metrics(style.font_size);
    float baseline_y = rect_.y + (rect_.height - fm.height) / 2.0f + fm.ascent;
    float text_x = rect_.x + style.padding.left;

    auto txt = selected_text();
    if (!txt.empty()) {
        float clip_w = rect_.width - style.padding.left - style.padding.right - 16.0f;
        painter.push_clip({text_x, rect_.y, clip_w, rect_.height});
        painter.draw_text(txt, {text_x, baseline_y}, style.text, style.font_size);
        painter.pop_clip();
    }

    float arrow_x = rect_.x + rect_.width - style.padding.right - 8.0f;
    float arrow_y = rect_.y + rect_.height / 2.0f;
    float aw = 4.0f;
    painter.draw_line({arrow_x - aw, arrow_y - 2.0f}, {arrow_x, arrow_y + 2.0f}, style.arrow, 1.5f);
    painter.draw_line({arrow_x, arrow_y + 2.0f}, {arrow_x + aw, arrow_y - 2.0f}, style.arrow, 1.5f);

    if (focused_ && !open_) {
        painter.draw_focus_ring(rect_, style.corner_radius);
    }
}

void Combobox::paint_dropdown(Painter &painter) {
    auto const &style = Theme::current().combobox;
    auto db = dropdown_bounds();
    float item_h = dropdown_item_height();
    int total = static_cast<int>(items_.size());
    bool scrollable = drop_max_visible_ < total;

    painter.fill_rect(db, style.dropdown_bg);
    painter.push_clip(db);

    auto fm = painter.font_metrics(style.font_size);

    for (int i = 0; i < total; i++) {
        float iy = db.y + item_h * static_cast<float>(i) - drop_scroll_;
        if (iy + item_h < db.y || iy > db.y + db.height) {
            continue;
        }

        Rect item_rect{db.x, iy, db.width, item_h};

        if (i == hovered_index_) {
            painter.fill_rect(item_rect, style.item_hovered);
        }

        float baseline = iy + (item_h - fm.height) / 2.0f + fm.ascent;
        Color tc = (i == hovered_index_) ? style.item_text_hovered : style.text;
        painter.draw_text(items_[i], {db.x + style.padding.left, baseline}, tc, style.font_size);
    }

    if (scrollable) {
        float content_h = item_h * static_cast<float>(total);
        float bar_h = std::max(12.0f, db.height * (db.height / content_h));
        float bar_y = db.y + (drop_scroll_ / content_h) * db.height;
        Rect sb{db.x + db.width - 5.0f, bar_y, 3.0f, bar_h};
        painter.fill_rounded_rect(sb, Color::rgba(style.text.r, style.text.g, style.text.b, 0.3f),
                                  1.5f);
    }

    painter.pop_clip();

    if (style.corner_radius > 0) {
        painter.draw_rounded_rect(db, style.border, style.corner_radius, style.border_width);
    } else {
        painter.draw_rect(db, style.border, style.border_width);
    }
}

void Combobox::clamp_drop_scroll() {
    float item_h = dropdown_item_height();
    float content_h = item_h * static_cast<float>(items_.size());
    float visible_h = item_h * static_cast<float>(drop_max_visible_);
    float max_scroll = std::max(0.0f, content_h - visible_h);
    drop_scroll_ = std::clamp(drop_scroll_, 0.0f, max_scroll);
}

void Combobox::ensure_hovered_visible() {
    if (hovered_index_ < 0) {
        return;
    }
    float item_h = dropdown_item_height();
    float top = item_h * static_cast<float>(hovered_index_);
    float bot = top + item_h;
    float visible_h = item_h * static_cast<float>(drop_max_visible_);
    if (bot > drop_scroll_ + visible_h) {
        drop_scroll_ = bot - visible_h;
    }
    if (top < drop_scroll_) {
        drop_scroll_ = top;
    }
    clamp_drop_scroll();
}

bool Combobox::handle_dropdown_mouse(MouseEvent const &event) {
    auto db = dropdown_bounds();

    if (event.type == MouseEvent::Type::Scroll && db.contains(event.position)) {
        drop_scroll_ -= event.scroll_dy;
        clamp_drop_scroll();
        return true;
    }

    if (event.type == MouseEvent::Type::Move || event.type == MouseEvent::Type::Drag) {
        hovered_index_ = item_index_at(event.position);
        return db.contains(event.position);
    }

    if (event.type == MouseEvent::Type::Press) {
        int idx = item_index_at(event.position);
        if (idx >= 0) {
            selected_index_ = idx;
            if (on_change) {
                on_change(selected_index_);
            }
            close_dropdown();
            return true;
        }
        close_dropdown();
        return false;
    }

    return false;
}

bool Combobox::handle_dropdown_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }

    switch (event.key) {
    case Key::Down:
        if (hovered_index_ < static_cast<int>(items_.size()) - 1) {
            hovered_index_++;
            ensure_hovered_visible();
        }
        return true;
    case Key::Up:
        if (hovered_index_ > 0) {
            hovered_index_--;
            ensure_hovered_visible();
        }
        return true;
    case Key::Enter:
        if (hovered_index_ >= 0) {
            selected_index_ = hovered_index_;
            if (on_change) {
                on_change(selected_index_);
            }
        }
        close_dropdown();
        return true;
    case Key::Escape:
        close_dropdown();
        return true;
    default:
        return false;
    }
}

bool Combobox::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Press && hit_test(event.position)) {
        if (open_) {
            close_dropdown();
        } else {
            open_dropdown();
        }
        return true;
    }
    return false;
}

bool Combobox::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }

    if (event.key == Key::Enter || (!event.text.empty() && event.text[0] == ' ')) {
        if (!open_) {
            open_dropdown();
        }
        return true;
    }

    if (!open_) {
        if (event.key == Key::Down && selected_index_ < static_cast<int>(items_.size()) - 1) {
            selected_index_++;
            if (on_change) {
                on_change(selected_index_);
            }
            return true;
        }
        if (event.key == Key::Up && selected_index_ > 0) {
            selected_index_--;
            if (on_change) {
                on_change(selected_index_);
            }
            return true;
        }
    }

    return false;
}

Size Combobox::size_hint() const {
    auto const &style = Theme::current().combobox;
    auto fm = Painter::measure_font_metrics(style.font_size);

    float max_w = 0;
    for (auto const &item : items_) {
        max_w = std::max(max_w, Painter::measure_text(item, style.font_size).width);
    }

    return {max_w + style.padding.left + style.padding.right + 20.0f,
            fm.height + style.padding.top + style.padding.bottom};
}

} // namespace toolkit
