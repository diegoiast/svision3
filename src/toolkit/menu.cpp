// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/menu.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>

namespace toolkit {

MenuItem MenuItem::submenu_item(std::string name, std::shared_ptr<class Menu> menu) {
    auto cmd = std::make_shared<Command>(std::move(name), nullptr);
    return {Type::Submenu, std::move(cmd), std::move(menu)};
}

Menu::Menu(std::string title) : title_(std::move(title)) {}

void Menu::add_action(std::shared_ptr<Command> cmd) {
    items_.push_back(MenuItem::action(std::move(cmd)));
}

void Menu::add_action(std::string name, std::function<void()> execute, bool enabled) {
    items_.push_back(MenuItem::action(std::move(name), std::move(execute), enabled));
}

void Menu::add_separator() {
    items_.push_back(MenuItem::sep());
}

void Menu::add_submenu(std::string name, std::shared_ptr<Menu> submenu) {
    items_.push_back(MenuItem::submenu_item(std::move(name), std::move(submenu)));
}

static auto menu_total_height(std::vector<MenuItem> const &items, float item_h, float sep_h)
    -> float {
    auto h = 0.0f;
    for (auto const &item : items) {
        h += (item.type == MenuItem::Type::Separator) ? sep_h : item_h;
    }
    return h;
}

void Menu::show(Window *win, Point position) {
    window_ = win;
    if (!window_ || items_.empty()) {
        return;
    }

    auto const &style = Theme::current().menu;

    item_height_ = style.font_size + style.item_padding * 2.0f + 4.0f;
    float max_name_w = 0.0f;
    float max_shortcut_w = 0.0f;
    for (auto const &item : items_) {
        if (item.type == MenuItem::Type::Separator) {
            continue;
        }
        auto name_w = Painter::measure_text(item.command->name(), style.font_size).width;
        max_name_w = std::max(max_name_w, name_w);
        if (!item.command->shortcut_string().empty()) {
            auto shortcut_w = Painter::measure_text(item.command->shortcut_string(), style.font_size).width;
            max_shortcut_w = std::max(max_shortcut_w, shortcut_w);
        }
    }

    auto width = max_name_w + style.padding.left + style.padding.right + 20.0f;
    if (max_shortcut_w > 0) {
        width += max_shortcut_w + 20.0f; // Add gap and shortcut width
    }
    auto height = menu_total_height(items_, item_height_, separator_height_) + 4.0f;
    auto win_size = window_->size();
    auto x = position.x;
    auto y = position.y;

    if (x + width > win_size.width) {
        x = win_size.width - width;
    }
    if (y + height > win_size.height) {
        y = win_size.height - height;
    }
    x = std::max(0.0f, x);
    y = std::max(0.0f, y);

    bounds_ = {x, y, width, height};
    hovered_ = -1;

    auto popup = Popup{};
    popup.bounds = bounds_;
    popup.on_paint = [this](Painter &p) { paint(p); };
    popup.on_mouse = [this](MouseEvent const &e) { return handle_mouse(e); };
    popup.on_key = [this](KeyEvent const &e) { return handle_key(e); };
    window_->open_popup(std::move(popup));
}

void Menu::close() {
    if (window_) {
        window_->close_popup();
    }
    window_ = nullptr;
}

int Menu::item_at(Point p) const {
    auto local_bounds = Rect{0, 0, bounds_.width, bounds_.height};
    if (!local_bounds.contains(p)) {
        return -1;
    }
    auto y = 2.0f;
    for (auto i = 0; i < static_cast<int>(items_.size()); i++) {
        auto h = (items_[i].type == MenuItem::Type::Separator) ? separator_height_ : item_height_;
        if (p.y >= y && p.y < y + h && items_[i].type != MenuItem::Type::Separator) {
            return i;
        }
        y += h;
    }
    return -1;
}

void Menu::paint(Painter &painter) {
    auto const &style = Theme::current().combobox;
    auto shadow = Color::rgba(0, 0, 0, 0.12f);
    auto fm = painter.font_metrics(style.font_size);
    auto y = 2.0f;
    auto local_bounds = Rect{0, 0, bounds_.width, bounds_.height};

    painter.fill_rounded_rect({1, 1, bounds_.width, bounds_.height}, shadow, style.corner_radius);
    painter.fill_rounded_rect(local_bounds, style.dropdown_bg, style.corner_radius);
    painter.draw_rounded_rect(local_bounds, style.border, style.corner_radius, style.border_width);

    for (auto i = 0; i < static_cast<int>(items_.size()); i++) {
        auto const &item = items_[i];

        if (item.type == MenuItem::Type::Separator) {
            auto mid_y = y + separator_height_ / 2.0f;
            auto sep_col = style.border;
            sep_col.a *= 0.5f;
            painter.draw_line({8, mid_y}, {bounds_.width - 8, mid_y}, sep_col, 0.5f);
            y += separator_height_;
            continue;
        }

        auto enabled = item.command->is_enabled();
        auto item_rect = Rect{2, y, bounds_.width - 4, item_height_};
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
        painter.draw_text(item.command->name(), {style.padding.left + 4, baseline}, text_col,
                          style.font_size);

        if (!item.command->shortcut_string().empty()) {
            auto shortcut_w = painter.text_size(item.command->shortcut_string(), style.font_size).width;
            auto shortcut_x = bounds_.width - style.padding.right - shortcut_w - 10.0f;
            painter.draw_text(item.command->shortcut_string(), {shortcut_x, baseline}, text_col,
                              style.font_size);
        }

        if (item.type == MenuItem::Type::Submenu) {
            // Draw arrow for submenu
            auto arrow_x = bounds_.width - 15.0f;
            painter.draw_text(">", {arrow_x, baseline}, text_col, style.font_size);
        }

        y += item_height_;
    }
}

bool Menu::handle_mouse(MouseEvent const &event) {
    auto local_bounds = Rect{0, 0, bounds_.width, bounds_.height};

    if (event.type == MouseEvent::Type::Move || event.type == MouseEvent::Type::Drag) {
        hovered_ = item_at(event.position);
        return local_bounds.contains(event.position);
    }

    if (event.type == MouseEvent::Type::Press) {
        auto idx = item_at(event.position);
        if (idx >= 0 && items_[idx].command->is_enabled()) {
            if (items_[idx].type == MenuItem::Type::Action) {
                auto cmd = items_[idx].command;
                close();
                cmd->execute();
                return true;
            } else if (items_[idx].type == MenuItem::Type::Submenu) {
                // Submenus not fully implemented in the stack yet, but let's at least show them
                // This would need section 5.1 of the plan (popup stack)
                return true;
            }
        }
        close();
        return false;
    }

    return false;
}

bool Menu::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }

    auto next_enabled = [&](int from, int dir) -> int {
        auto n = static_cast<int>(items_.size());
        for (auto step = 0; step < n; step++) {
            from = (from + dir + n) % n;
            if (items_[from].type != MenuItem::Type::Separator && items_[from].command->is_enabled()) {
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
            if (items_[hovered_].type == MenuItem::Type::Action) {
                auto cmd = items_[hovered_].command;
                close();
                cmd->execute();
            }
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
