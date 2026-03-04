// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/context_menu.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>

namespace toolkit {

ContextMenu::ContextMenu(std::vector<MenuItem> items) : items_(std::move(items)) {}

float menu_total_height(std::vector<MenuItem> const &items, float item_h, float sep_h) {
    auto h = 0;
    for (auto const &item : items) {
        h += item.separator ? sep_h : item_h;
    }
    return h;
}

void ContextMenu::show(Window *win, Point position) {
    window_ = win;
    if (!window_ || items_.empty()) {
        return;
    }

    auto const &style = Theme::current().combobox;
    auto max_w = 0.0f;

    item_height_ = style.font_size + style.item_padding * 2.0f + 4.0f;
    for (auto const &item : items_) {
        if (item.separator) {
            continue;
        }
        float w = Painter::measure_text(item.command->name(), style.font_size).width;
        max_w = std::max(max_w, w);
    }

    auto width = max_w + style.padding.left + style.padding.right + 20.0f;
    auto height = menu_total_height(items_, item_height_, separator_height_) + 4.0f;
    auto win_size = window_->size();
    auto x = position.x;
    auto y = position.y;

    if (x + width > win_size.width) {
        x = win_size.width - width;
    }
    if (y + height > win_size.height) {
        y = position.y - height;
    }
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }

    bounds_ = {x, y, width, height};
    hovered_ = -1;

    Popup popup;
    popup.bounds = bounds_;
    popup.on_paint = [this](Painter &p) { paint(p); };
    popup.on_mouse = [this](MouseEvent const &e) { return handle_mouse(e); };
    popup.on_key = [this](KeyEvent const &e) { return handle_key(e); };
    window_->open_popup(std::move(popup));
}

void ContextMenu::close() {
    if (window_) {
        window_->close_popup();
    }
    window_ = nullptr;
}

int ContextMenu::item_at(Point p) const {
    if (!bounds_.contains(p)) {
        return -1;
    }
    auto y = bounds_.y + 2.0f;
    for (auto i = 0; i < static_cast<int>(items_.size()); i++) {
        auto h = items_[i].separator ? separator_height_ : item_height_;
        if (p.y >= y && p.y < y + h && !items_[i].separator) {
            return i;
        }
        y += h;
    }
    return -1;
}

void ContextMenu::paint(Painter &painter) {
    auto const &style = Theme::current().combobox;
    auto shadow = Color::rgba(0, 0, 0, 0.12f);
    auto fm = painter.font_metrics(style.font_size);
    auto y = bounds_.y + 2.0f;

    painter.fill_rounded_rect({bounds_.x + 1, bounds_.y + 1, bounds_.width, bounds_.height}, shadow,
                              style.corner_radius);
    painter.fill_rounded_rect(bounds_, style.dropdown_bg, style.corner_radius);
    painter.draw_rounded_rect(bounds_, style.border, style.corner_radius, style.border_width);

    for (auto i = 0; i < static_cast<int>(items_.size()); i++) {
        auto const &item = items_[i];

        if (item.separator) {
            float mid_y = y + separator_height_ / 2.0f;
            Color sep_col = style.border;
            sep_col.a *= 0.5f;
            painter.draw_line({bounds_.x + 8, mid_y}, {bounds_.x + bounds_.width - 8, mid_y},
                              sep_col, 0.5f);
            y += separator_height_;
            continue;
        }

        auto enabled = item.command->is_enabled();
        auto item_rect = Rect{bounds_.x + 2, y, bounds_.width - 4, item_height_};
        if (i == hovered_ && enabled) {
            painter.fill_rounded_rect(item_rect, style.item_hovered, style.corner_radius * 0.5f);
        }

        auto text_col = style.text;
        if (i == hovered_ && enabled) {
            text_col = style.item_text_hovered;
        } else if (!enabled) {
            text_col.a *= 0.4f;
        }

        auto baseline = y + (item_height_ - fm.height) / 2.0f + fm.ascent;
        painter.draw_text(item.command->name(), {bounds_.x + style.padding.left + 4, baseline},
                          text_col, style.font_size);
        y += item_height_;
    }
}

bool ContextMenu::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Move || event.type == MouseEvent::Type::Drag) {
        hovered_ = item_at(event.position);
        return bounds_.contains(event.position);
    }

    if (event.type == MouseEvent::Type::Press) {
        int idx = item_at(event.position);
        if (idx >= 0 && items_[idx].command->is_enabled()) {
            auto cmd = items_[idx].command;
            close();
            cmd->execute();
            return true;
        }
        close();
        return false;
    }

    return false;
}

bool ContextMenu::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }

    auto next_enabled = [&](int from, int dir) -> int {
        int n = static_cast<int>(items_.size());
        for (int step = 0; step < n; step++) {
            from = (from + dir + n) % n;
            if (!items_[from].separator && items_[from].command->is_enabled()) {
                return from;
            }
        }
        return -1;
    };

    switch (event.key) {
    case Key::Down:
        hovered_ = next_enabled(hovered_, 1);
        return true;
    case Key::Up:
        hovered_ = next_enabled(hovered_, -1);
        return true;
    case Key::Enter:
        if (hovered_ >= 0 && items_[hovered_].command->is_enabled()) {
            auto cmd = items_[hovered_].command;
            close();
            cmd->execute();
        }
        return true;
    case Key::Escape:
        close();
        return true;
    default:
        return false;
    }
}

} // namespace toolkit
