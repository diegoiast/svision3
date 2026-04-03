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
    state_handler_.on_state_change_callback = [this] {
        if (window_) {
            window_->request_redraw("menu state");
        }
    };
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
    auto const &theme = Theme::current();
    auto const &palette = theme.palette;
    auto const &style = Theme::current().menu;
    auto max_name_w = 0.0f;
    auto max_shortcut_w = 0.0f;

    item_height_ = palette.fonts.size + style.item_padding * 2.0f + 4.0f;
    for (auto const &item : items_) {
        if (item.type == MenuItem::Type::Separator) {
            continue;
        }
        auto name_w = Painter::measure_text(item.command->name(), palette.fonts.size).width;

        max_name_w = std::max(max_name_w, name_w);
        if (!item.command->shortcut_string().empty()) {
            auto shortcut_w =
                Painter::measure_text(item.command->shortcut_string(), palette.fonts.size).width;
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
    auto const &theme = Theme::current();
    auto const &palette = theme.palette;

    theme.draw_menu_background(painter, {0, 0, bounds_.width, bounds_.height});
    auto y = 2.0f;
    for (auto i = 0; i < static_cast<int>(items_.size()); i++) {
        auto const &item = items_[i];

        if (item.type == MenuItem::Type::Separator) {
            Theme::current().draw_menu_separator(painter, {0, y, bounds_.width, separator_height_});
            y += separator_height_;
            continue;
        }

        auto enabled = item.command->is_enabled();
        auto item_rect = Rect{2, y, bounds_.width - 4, item_height_};
        auto icon_data = item.command->icon_image();
        auto shortcut = item.command->shortcut_string();
        auto text = strip_mnemonic(item.command->name());

        theme.draw_menu_item(painter, item_rect, text, icon_data, shortcut, i == hovered_, enabled,
                             false, false);
        if (item.type == MenuItem::Type::Submenu) {
            auto const &style = Theme::current().combobox;
            auto fm = painter.font_metrics(palette.fonts.size);
            auto baseline = y + (item_height_ - fm.height) / 2.0f + fm.ascent;
            auto arrow_x = bounds_.width - 15.0f;
            painter.draw_text(">", {arrow_x, baseline}, style.text, palette.fonts.size);
        }
        y += item_height_;
    }
}

bool Menu::handle_mouse(MouseEvent const &event) {
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

        if (hovered_ != -1 && previously_hovered != hovered_) {
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
            if (items_[idx].is_action()) {
                state_handler_.on_mouse_click(event);
                pressed_item_ = idx;
                return true;
            } else if (items_[idx].is_submenu()) {
                open_submenu(idx);
                return true;
            }
        }
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
