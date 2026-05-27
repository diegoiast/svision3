// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/combobox.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>

namespace toolkit {

Combobox::Combobox(std::vector<std::string> items) : items_(std::move(items)) {
    state.focusable = true;
}

Combobox &Combobox::set_items(std::vector<std::string> items) {
    items_ = std::move(items);
    if (selected_index_ >= static_cast<int>(items_.size())) {
        selected_index_ = items_.empty() ? -1 : 0;
    }
    return *this;
}

Combobox &Combobox::set_selected(int index) {
    if (index >= 0 && index < static_cast<int>(items_.size())) {
        if (selected_index_ != index) {
            selected_index_ = index;
        }
    }
    return *this;
}

auto Combobox::selected_text() const -> std::string {
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(items_.size())) {
        return items_[selected_index_];
    }
    return {};
}

auto Combobox::dropdown_item_height() const -> float {
    auto const &theme = Theme::current();
    auto const &style = Theme::current().combobox;
    auto fm = font_metrics(theme.palette.fonts.size);
    return fm.height + style.item_padding * 2.0f;
}

auto Combobox::dropdown_bounds() const -> Rect {
    auto item_h = dropdown_item_height();
    auto drop_h = item_h * static_cast<float>(drop_max_visible_);

    // Convert local coordinates to window coordinates
    auto pos = map_to_window({0.0f, rect_.height});

    // If it was flipped above, recalculate
    if (window()) {
        auto win_h = window()->size().height;
        auto space_below = win_h - pos.y;
        auto full_h = item_h * static_cast<float>(items_.size());
        if (full_h > space_below && pos.y > space_below) {
            pos.y = pos.y - drop_h;
        }
    }

    return {pos.x, pos.y, rect_.width, drop_h};
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
    auto rect = Rect{0, 0, rect_.width, rect_.height};
    auto wstate = WidgetState{
        .interaction = ButtonState::Normal,
        .focused = is_focused(),
        .enabled = is_enabled(),
        .window_active = window_ ? window_->is_active() : true,
    };
    Theme::current().draw_combobox(painter, rect, selected_text(), wstate, open_);
}

void Combobox::paint_dropdown(Painter &painter) {
    // auto const &style = Theme::current().combobox;
    auto db = dropdown_bounds();
    auto local_db = Rect{0, 0, db.width, db.height};
    auto item_h = dropdown_item_height();
    auto total = static_cast<int>(items_.size());
    auto scrollable = drop_max_visible_ < total;

    auto const &palette = Theme::current().palette;
    painter.fill_rect(local_db, palette.base);
    painter.push_clip(local_db);

    for (auto i = 0; i < total; i++) {
        auto iy = item_h * static_cast<float>(i) - drop_scroll_;
        auto item_rect = Rect{0, iy, db.width, item_h};

        if (iy + item_h < 0 || iy > db.height) {
            continue;
        }

        Theme::current().draw_combobox_item(painter, item_rect, items_[i], i == hovered_index_);
    }

    if (scrollable) {
        auto content_h = item_h * static_cast<float>(total);
        auto bar_h = std::max(12.0f, db.height * (db.height / content_h));
        auto bar_y = (drop_scroll_ / content_h) * db.height;
        auto sb = Rect{db.width - 5.0f, bar_y, 3.0f, bar_h};
        // painter.fill_rounded_rect(sb, palette.text, 1.5f);
        painter.fill_rounded_rect(sb, Color::from_argb(0x00ff00), 1.5f);
    }

    painter.pop_clip();

    if (palette.corner_radius > 0) {
        painter.draw_rounded_rect(local_db, palette.border, palette.corner_radius,
                                  palette.border_width);
    } else {
        painter.draw_rect(local_db, palette.border, palette.border_width);
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
    auto const &theme = Theme::current();
    auto const &style = theme.combobox;
    auto const &palette = theme.palette;
    auto fm = font_metrics(palette.fonts.size);
    auto max_w = 0.0f;

    for (auto const &item : items_) {
        max_w = std::max(max_w, measure_text(item, palette.fonts.size).width);
    }

    return {max_w + style.padding.left + style.padding.right + 20.0f,
            fm.height + style.padding.top + style.padding.bottom};
}

nlohmann::json Combobox::to_json() const {
    auto j = Widget::to_json();
    j["current_index"] = selected_index_;
    j["items"] = items_;
    return j;
}
} // namespace toolkit
