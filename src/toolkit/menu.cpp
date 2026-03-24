// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/menu.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>

namespace toolkit {

MenuItem MenuItem::submenu_item(std::string name, std::shared_ptr<class Menu> menu) {
    if (name.empty() && menu) {
        name = menu->title();
    }
    auto cmd = std::make_shared<Command>(std::move(name), nullptr);
    return {Type::Submenu, std::move(cmd), std::move(menu)};
}

static char get_mnemonic(std::string_view name) {
    auto pos = name.find('&');
    if (pos != std::string_view::npos && pos + 1 < name.size()) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(name[pos + 1])));
    }
    return 0;
}

static std::string strip_mnemonic(std::string_view name) {
    auto pos = name.find('&');
    if (pos != std::string_view::npos) {
        std::string res(name.substr(0, pos));
        res += name.substr(pos + 1);
        return res;
    }
    return std::string(name);
}

Menu::Menu(std::string title) : title_(std::move(title)), mnemonic_index_(-1) {
    mnemonic_key_ = get_mnemonic(title_);
    auto pos = title_.find('&');
    if (pos != std::string::npos) {
        mnemonic_index_ = static_cast<int>(pos);
    }
    display_title_ = strip_mnemonic(title_);
}

void Menu::add_action(std::shared_ptr<Command> cmd) {
    items_.push_back(MenuItem::action(std::move(cmd)));
}

void Menu::add_action(std::string name, std::function<void()> execute, bool enabled) {
    items_.push_back(MenuItem::action(std::move(name), std::move(execute), enabled));
}

void Menu::add_separator() { items_.push_back(MenuItem::sep()); }

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
    auto max_name_w = 0.0f;
    auto max_shortcut_w = 0.0f;

    item_height_ = style.font_size + style.item_padding * 2.0f + 4.0f;
    for (auto const &item : items_) {
        if (item.type == MenuItem::Type::Separator) {
            continue;
        }
        auto name_w = Painter::measure_text(item.command->name(), style.font_size).width;

        max_name_w = std::max(max_name_w, name_w);
        if (!item.command->shortcut_string().empty()) {
            auto shortcut_w =
                Painter::measure_text(item.command->shortcut_string(), style.font_size).width;
            max_shortcut_w = std::max(max_shortcut_w, shortcut_w);
        }
    }

    auto width = max_name_w + style.padding.left + style.padding.right + 20.0f;
    auto height = menu_total_height(items_, item_height_, separator_height_) + 4.0f;
    auto win_size = window_->size();
    auto x = position.x;
    auto y = position.y;

    if (max_shortcut_w > 0) {
        width += max_shortcut_w + 20.0f; // Add gap and shortcut width
    }
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
    popup.on_close = [this] {
        if (on_close_callback) {
            on_close_callback();
        }
    };
    window_->open_popup(std::move(popup));
}

void Menu::close() {
    if (!window_) {
        return;
    }
    if (on_close_callback) {
        on_close_callback();
    }
    if (window_) {
        window_->close_popup();
    }
    window_ = nullptr;
}

void Menu::select_first() {
    for (auto i = 0; i < static_cast<int>(items_.size()); ++i) {
        if (items_[i].type != MenuItem::Type::Separator) {
            hovered_ = i;
            return;
        }
    }
    hovered_ = -1;
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
        auto text_col = style.text;
        auto baseline = y + (item_height_ - fm.height) / 2.0f + fm.ascent;

        if (i == hovered_ && enabled) {
            painter.fill_rounded_rect(item_rect, style.item_hovered, style.corner_radius * 0.5f);
        }

        auto icon_data = item.command->icon_image();
        if (icon_data) {
            auto icon_x = style.padding.left + 4;
            auto icon_y = y + (item_height_ - 16.0f) / 2.0f;
            painter.draw_image(*icon_data, Point{icon_x, icon_y});
        }

        if (i == hovered_ && enabled) {
            text_col = style.item_text_hovered;
        } else if (!enabled) {
            text_col.a *= 0.4f;
        }

        auto display_name = strip_mnemonic(item.command->name());
        auto mnemonic_idx = -1;
        auto ampersand_pos = item.command->name().find('&');
        if (ampersand_pos != std::string::npos) {
            mnemonic_idx = static_cast<int>(ampersand_pos);
        }

        auto text_x = style.padding.left + 4;
        if (!item.command->icon().empty()) {
            text_x += 20;
        }
        painter.draw_text(display_name, {text_x, baseline}, text_col, style.font_size);

        if (mnemonic_idx != -1) {
            auto m_char = display_name.substr(mnemonic_idx, 1);
            auto text_before_m = display_name.substr(0, mnemonic_idx);
            auto x_before = painter.text_size(text_before_m, style.font_size).width;
            auto m_size = painter.text_size(m_char, style.font_size);
            auto underline_y = baseline + 2.0f;
            painter.draw_line({text_x + x_before, underline_y},
                              {text_x + x_before + m_size.width, underline_y}, text_col, 1.0f);
        }

        if (!item.command->shortcut_string().empty()) {
            auto shortcut_w =
                painter.text_size(item.command->shortcut_string(), style.font_size).width;
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
        auto previously_hovered = hovered_;
        hovered_ = item_at(event.position);
        if (hovered_ != -1 && previously_hovered != hovered_) {
            // If we moved to a different item, and we have a submenu open,
            // we should close it.
            if (open_submenu_index_ != -1 && hovered_ != open_submenu_index_) {
                if (window_) {
                    // Close everything above this menu because we moved away
                    // from the item that opened the child.
                    // Since Window::handle_mouse only closes if size changes,
                    // we need to close it here.
                    while (window_->num_popups() > 0) {
                        // How to know if we are at popups_[i]?
                        // This is tricky. Let's use a simpler heuristic:
                        // we just close the popups above us.
                        // But Menu doesn't know its index in the window.

                        // Let's assume for now that if we are handling the mouse,
                        // and we have an open child, it's the one at the top.
                        window_->close_popup();
                        if (open_submenu_index_ == -1) {
                            break;
                        }
                        break; // Close only one level
                    }
                }
                open_submenu_index_ = -1;
            }

            if (items_[hovered_].is_submenu()) {
                open_submenu(hovered_);
            }
        }
        return local_bounds.contains(event.position);
    }

    if (event.type == MouseEvent::Type::Leave) {
        // We don't close submenus on Leave, because the mouse might be
        // moving into the submenu itself.
        hovered_ = -1;
        return true;
    }

    if (event.type == MouseEvent::Type::Press) {
        auto idx = item_at(event.position);
        if (idx >= 0 && items_[idx].command->is_enabled()) {
            if (items_[idx].is_action()) {
                auto cmd = items_[idx].command;
                close();
                cmd->execute();
                return true;
            } else if (items_[idx].is_submenu()) {
                open_submenu(idx);
                return true;
            }
        }
        return false;
    }

    return false;
}

bool Menu::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }

    auto next_enabled = [&](int current, int dir) -> int {
        auto n = static_cast<int>(items_.size());
        if (n == 0) {
            return -1;
        }

        int start = current;
        if (start == -1) {
            start = (dir == 1) ? n - 1 : 0;
        }

        int idx = start;
        for (auto step = 0; step < n; step++) {
            idx = (idx + dir + n) % n;
            if (items_[idx].type != MenuItem::Type::Separator &&
                items_[idx].command->is_enabled()) {
                return idx;
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
        } else if (on_request_next_menu) {
            on_request_next_menu();
        }
        return true;
    case Key::Left:
        if (parent_menu_) {
            close();
        } else if (on_request_prev_menu) {
            on_request_prev_menu();
        }
        return true;
    case Key::Escape:
        close();
        return true;
    case Key::Enter:
        if (hovered_ != -1 && items_[hovered_].is_submenu()) {
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
    default:
        if (!event.text.empty()) {
            auto key = static_cast<char>(std::tolower(static_cast<unsigned char>(event.text[0])));
            for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
                if (items_[i].is_separator()) {
                    continue;
                }
                if (get_mnemonic(items_[i].command->name()) == key) {
                    if (items_[i].is_action()) {
                        auto cmd = items_[i].command;
                        close();
                        cmd->execute();
                        return true;
                    } else if (items_[i].is_submenu()) {
                        open_submenu(i);
                        return true;
                    }
                }
            }
        }
        return false;
    }
}

void Menu::open_submenu(int index) {
    if (index < 0 || index >= items_.size() || !items_[index].is_submenu()) {
        return;
    }
    open_submenu_index_ = index;
    auto const &item = items_[index];
    auto y = 2.0f;

    item.submenu->set_parent_menu(this);
    for (auto i = 0; i < index; ++i) {
        y += (items_[i].is_separator()) ? separator_height_ : item_height_;
    }
    auto pos = Point{bounds_.x + bounds_.width, bounds_.y + y};
    item.submenu->show(window_, pos);
    item.submenu->select_first();
}

} // namespace toolkit
