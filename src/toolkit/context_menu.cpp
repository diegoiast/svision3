// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/context_menu.hpp"
#include "toolkit/menu.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>

namespace toolkit {

ContextMenu::ContextMenu(std::vector<MenuItem> items) : items_(std::move(items)) {
    state_handler_.on_state_change_callback = [this] {
        if (window_) {
            window_->request_redraw("context menu state");
        }
    };
}

static auto menu_total_height(std::vector<MenuItem> const &items, float item_h, float sep_h)
    -> float {
    auto h = 0.0f;

    for (auto const &item : items) {
        h += (item.type == MenuItem::Type::Separator) ? sep_h : item_h;
    }
    return h;
}

void ContextMenu::show(Window *win, Point position) {
    window_ = win;
    if (!window_ || items_.empty()) {
        return;
    }

    auto const &style = Theme::current().style.combo;
    auto const &theme = Theme::current();
    auto const &palette = theme.palette;
    auto max_name_w = 0.0f;
    auto max_shortcut_w = 0.0f;

    item_height_ = detail::current_platform()->font_metrics(palette.fonts.size).height +
                   style.item_padding * 2.0f;
    for (auto const &item : items_) {
        if (item.type == MenuItem::Type::Separator) {
            continue;
        }
        auto name_w = detail::current_platform()
                          ->measure_text(item.command->name(), palette.fonts.size)
                          .width;
        max_name_w = std::max(max_name_w, name_w);

        auto shortcut = item.command->printable_shortcut();
        if (!shortcut.empty()) {
            auto shortcut_w =
                detail::current_platform()->measure_text(shortcut, palette.fonts.size).width;
            max_shortcut_w = std::max(max_shortcut_w, shortcut_w);
        }
    }

    // FIXME: what is this extra 20.0f padding?
    auto width = max_name_w + style.padding.left + style.padding.right + 20.0f;
    if (max_shortcut_w > 0) {
        width += max_shortcut_w + 20.0f; // Add gap and shortcut width
    }
    auto height = menu_total_height(items_, item_height_, separator_height_);
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

    auto popup = Popup{};
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

void ContextMenu::paint(Painter &painter) {
    auto const &theme = Theme::current();
    auto const &palette = theme.palette;

    theme.draw_menu_background(painter, {0, 0, bounds_.width, bounds_.height});

    auto y = 2.0f;

    for (auto i = 0; i < static_cast<int>(items_.size()); i++) {
        auto const &item = items_[i];

        if (item.type == MenuItem::Type::Separator) {
            theme.draw_menu_separator(painter, {0, y, bounds_.width, separator_height_});
            y += separator_height_;
            continue;
        }

        auto enabled = item.command->is_enabled();
        auto item_rect = Rect{2, y, bounds_.width - 4, item_height_};
        auto icon_data = item.command->icon_image();
        auto shortcut = item.command->printable_shortcut();

        theme.draw_menu_item(painter, item_rect, item.command->name(), icon_data,
                             shortcut, i == hovered_, enabled, false, false);

        if (item.type == MenuItem::Type::Submenu) {
            auto const &style = Theme::current().style.combo;
            auto fm = painter.font_metrics(palette.fonts.size);
            auto baseline = y + (item_height_ - fm.height) / 2.0f + fm.ascent;
            auto arrow_x = bounds_.width - 15.0f;
            painter.draw_text(">", {arrow_x, baseline}, palette.text, palette.fonts.size);
        }

        y += item_height_;
    }
}

bool ContextMenu::handle_mouse(MouseEvent const &event) {
    auto local_bounds = Rect{0, 0, bounds_.width, bounds_.height};
    auto inside = local_bounds.contains(event.position);

    if (event.type == MouseEvent::Type::Move || event.type == MouseEvent::Type::Drag) {
        auto previously_hovered = hovered_;
        hovered_ = item_at(event.position);

        if (inside) {
            state_handler_.on_mouse_enter();
        } else {
            state_handler_.on_mouse_leave();
        }

        if (inside && hovered_ != -1 && previously_hovered != hovered_) {
            if (open_submenu_index_ != -1 && hovered_ != open_submenu_index_) {
                if (window_) {
                    while (window_->num_popups() > 0) {
                        window_->close_popup();
                        if (open_submenu_index_ == -1) {
                            break;
                        }
                        break;
                    }
                }
                open_submenu_index_ = -1;
            }
            if (items_[hovered_].is_submenu()) {
                open_submenu(hovered_);
            }
        }

        return inside;
    }

    if (event.type == MouseEvent::Type::Leave) {
        hovered_ = -1;
        state_handler_.on_mouse_leave();
        return true;
    }

    if (event.type == MouseEvent::Type::Press) {
        auto idx = item_at(event.position);
        if (idx >= 0 && items_[idx].command->is_enabled()) {
            if (items_[idx].is_submenu()) {
                if (open_submenu_index_ == idx) {
                    return true;
                }
                open_submenu(idx);
                return true;
            }
            state_handler_.on_mouse_click(event);
            pressed_item_ = idx;
            return true;
        }
        close();
        return false;
    }

    if (event.type == MouseEvent::Type::Release) {
        auto idx = item_at(event.position);
        if (state_handler_.button_state == ButtonState::ClickedInside && idx == pressed_item_) {
            if (idx >= 0 && items_[idx].is_action() && items_[idx].command->is_enabled()) {
                auto cmd = items_[idx].command;
                close();
                cmd->execute();
            }
        }
        state_handler_.on_mouse_click(event);
        pressed_item_ = -1;
        return inside;
    }

    return false;
}

bool ContextMenu::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }

    auto next_enabled = [&](int from, int dir) -> int {
        auto n = static_cast<int>(items_.size());
        for (auto step = 0; step < n; step++) {
            from = (from + dir + n) % n;
            if (items_[from].type != MenuItem::Type::Separator &&
                items_[from].command->is_enabled()) {
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
    case Key::Right:
        if (hovered_ != -1 && items_[hovered_].is_submenu()) {
            open_submenu(hovered_);
        }
        return true;
    case Key::Enter:
        if (hovered_ >= 0 && items_[hovered_].is_submenu()) {
            open_submenu(hovered_);
            return true;
        }
        if (hovered_ >= 0 && items_[hovered_].is_action() &&
            items_[hovered_].command->is_enabled()) {
            auto cmd = items_[hovered_].command;
            close();
            cmd->execute();
        }
        return true;
    case Key::Escape:
        if (open_submenu_index_ != -1) {
            if (window_) {
                window_->close_popup();
            }
            open_submenu_index_ = -1;
            return true;
        }
        close();
        return true;
    default:
        return false;
    }
}

void ContextMenu::open_submenu(int index) {
    if (index < 0 || index >= items_.size() || !items_[index].is_submenu()) {
        return;
    }

    open_submenu_index_ = index;
    auto const &item = items_[index];
    auto y = 2.0f;

    for (auto i = 0; i < index; ++i) {
        y += (items_[i].is_separator()) ? separator_height_ : item_height_;
    }

    item.submenu->set_parent_menu(nullptr);
    auto pos = Point{bounds_.x + bounds_.width, bounds_.y + y};
    item.submenu->on_close_callback = [this] {
        if (open_submenu_index_ != -1) {
            open_submenu_index_ = -1;
            close();
        }
    };
    item.submenu->show(window_, pos);
    item.submenu->select_first();
}

} // namespace toolkit
