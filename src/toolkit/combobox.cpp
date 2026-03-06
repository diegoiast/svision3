// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/combobox.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>
#include <cmath>

namespace toolkit {

Combobox::Combobox(std::vector<std::string> items) : items_(std::move(items)) {
    focusable_ = true;
}

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

auto Combobox::selected_text() const -> std::string {
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(items_.size())) {
        return items_[selected_index_];
    }
    return {};
}

auto Combobox::dropdown_item_height() const -> float {
    auto const &style = Theme::current().combobox;
    return style.font_size + style.item_padding * 2.0f;
}

auto Combobox::dropdown_bounds() const -> Rect {
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

auto Combobox::item_index_at(Point p) const -> int {
    auto db = dropdown_bounds();
    auto local_db = Rect{0, 0, db.width, db.height};

    if (!local_db.contains(p)) {
        return -1;
    }
    auto item_h = dropdown_item_height();
    auto idx = static_cast<int>((p.y + drop_scroll_) / item_h);

    if (idx < 0 || idx >= static_cast<int>(items_.size())) {
        return -1;
    }
    return idx;
}

void Combobox::open_dropdown() {
    auto item_h = dropdown_item_height();
    auto total = static_cast<int>(items_.size());

    if (!window() || items_.empty()) {
        return;
    }

    auto win_h = window()->size().height;
    auto space_below = win_h - (rect_.y + rect_.height);
    auto space_above = rect_.y;
    auto best_space = std::max(space_below, space_above);
    auto max_fit = std::max(1, static_cast<int>(best_space / item_h));
    auto popup = Popup{};

    drop_max_visible_ = total;
    open_ = true;
    hovered_index_ = selected_index_;
    drop_scroll_ = 0;

    if (max_fit < total) {
        drop_max_visible_ = max_fit;
    }

    // Scroll so the selected item is visible
    if (selected_index_ >= 0 && selected_index_ >= drop_max_visible_) {
        drop_scroll_ = item_h * static_cast<float>(selected_index_ - drop_max_visible_ + 1);
    }

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
    auto border = focused_ ? style.border_focused : style.border;
    auto local_rect = Rect{0, 0, rect_.width, rect_.height};
    auto fm = painter.font_metrics(style.font_size);
    auto baseline_y = (rect_.height - fm.height) / 2.0f + fm.ascent;
    auto text_x = style.padding.left;
    auto txt = selected_text();
    auto arrow_x = rect_.width - style.padding.right - 8.0f;
    auto arrow_y = rect_.height / 2.0f;
    auto aw = 4.0f;

    painter.draw_frame(local_rect, style.background, border, style, true);

    if (!txt.empty()) {
        auto clip_w = rect_.width - style.padding.left - style.padding.right - 16.0f;
        painter.push_clip({text_x, 0, clip_w, rect_.height});
        painter.draw_text(txt, {text_x, baseline_y}, style.text, style.font_size);
        painter.pop_clip();
    }

    painter.draw_line({arrow_x - aw, arrow_y - 2.0f}, {arrow_x, arrow_y + 2.0f}, style.arrow, 1.5f);
    painter.draw_line({arrow_x, arrow_y + 2.0f}, {arrow_x + aw, arrow_y - 2.0f}, style.arrow, 1.5f);

    if (focused_ && !open_) {
        painter.draw_focus_ring(local_rect, style.corner_radius);
    }
}

void Combobox::paint_dropdown(Painter &painter) {
    auto const &style = Theme::current().combobox;
    auto db = dropdown_bounds();
    auto local_db = Rect{0, 0, db.width, db.height};
    auto item_h = dropdown_item_height();
    auto total = static_cast<int>(items_.size());
    auto scrollable = drop_max_visible_ < total;
    auto fm = painter.font_metrics(style.font_size);

    painter.fill_rect(local_db, style.dropdown_bg);
    painter.push_clip(local_db);

    for (auto i = 0; i < total; i++) {
        auto iy = item_h * static_cast<float>(i) - drop_scroll_;
        auto item_rect = Rect{0, iy, db.width, item_h};
        auto baseline = iy + (item_h - fm.height) / 2.0f + fm.ascent;
        auto tc = (i == hovered_index_) ? style.item_text_hovered : style.text;

        if (iy + item_h < 0 || iy > db.height) {
            continue;
        }

        if (i == hovered_index_) {
            painter.fill_rect(item_rect, style.item_hovered);
        }

        painter.draw_text(items_[i], {style.padding.left, baseline}, tc, style.font_size);
    }

    if (scrollable) {
        auto content_h = item_h * static_cast<float>(total);
        auto bar_h = std::max(12.0f, db.height * (db.height / content_h));
        auto bar_y = (drop_scroll_ / content_h) * db.height;
        auto sb = Rect{db.width - 5.0f, bar_y, 3.0f, bar_h};
        painter.fill_rounded_rect(sb, Color::rgba(style.text.r, style.text.g, style.text.b, 0.3f),
                                  1.5f);
    }

    painter.pop_clip();

    if (style.corner_radius > 0) {
        painter.draw_rounded_rect(local_db, style.border, style.corner_radius, style.border_width);
    } else {
        painter.draw_rect(local_db, style.border, style.border_width);
    }
}

void Combobox::clamp_drop_scroll() {
    auto item_h = dropdown_item_height();
    auto content_h = item_h * static_cast<float>(items_.size());
    auto visible_h = item_h * static_cast<float>(drop_max_visible_);
    auto max_scroll = std::max(0.0f, content_h - visible_h);

    drop_scroll_ = std::clamp(drop_scroll_, 0.0f, max_scroll);
}

void Combobox::ensure_hovered_visible() {
    auto item_h = dropdown_item_height();
    auto top = item_h * static_cast<float>(hovered_index_);
    auto bot = top + item_h;
    auto visible_h = item_h * static_cast<float>(drop_max_visible_);

    if (hovered_index_ < 0) {
        return;
    }
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
    auto local_db = Rect{0, 0, db.width, db.height};

    if (event.type == MouseEvent::Type::Scroll && local_db.contains(event.position)) {
        drop_scroll_ -= event.scroll_dy;
        clamp_drop_scroll();
        return true;
    }

    if (event.type == MouseEvent::Type::Move || event.type == MouseEvent::Type::Drag) {
        hovered_index_ = item_index_at(event.position);
        return local_db.contains(event.position);
    }

    if (event.type == MouseEvent::Type::Press) {
        auto idx = item_index_at(event.position);
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
    auto local_rect = Rect{0, 0, rect_.width, rect_.height};

    if (event.type == MouseEvent::Type::Press && local_rect.contains(event.position)) {
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

auto Combobox::size_hint() const -> Size {
    auto const &style = Theme::current().combobox;
    auto fm = Painter::measure_font_metrics(style.font_size);
    auto max_w = 0.0f;

    for (auto const &item : items_) {
        max_w = std::max(max_w, Painter::measure_text(item, style.font_size).width);
    }

    return {max_w + style.padding.left + style.padding.right + 20.0f,
            fm.height + style.padding.top + style.padding.bottom};
}

} // namespace toolkit
