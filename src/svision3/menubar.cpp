// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/menubar.hpp"
#include "svision3/theme.hpp"
#include "svision3/window.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace svision3 {

MenuBar::MenuBar() {
    set_focusable(false);
    state.non_focus_input = true;
}

void MenuBar::set_show_mnemonics(bool show) {
    if (show_mnemonics_ == show) {
        return;
    }
    show_mnemonics_ = show;
    if (window()) {
        window()->request_redraw();
    }
}

void MenuBar::add_menu(std::shared_ptr<Menu> menu) {
    menus_.push_back(std::move(menu));
    invalidate_layout();
}

std::shared_ptr<Menu> MenuBar::add_menu(std::string title) {
    auto m = std::make_shared<Menu>(std::move(title));
    add_menu(m);
    return m;
}

int MenuBar::find_menu(std::string_view title) const {
    for (auto i = 0; i < static_cast<int>(menus_.size()); ++i) {
        if (menus_[i]->display_title() == title) {
            return i;
        }
    }
    return -1;
}

void MenuBar::paint(Painter &painter) {
    auto const &theme = Theme::current();
    auto wstate = WidgetState{
        .interaction = ButtonState::Normal,
        .focused = false,
        .enabled = true,
        .window_active = window_ ? window_->is_active() : true,
    };
    theme.draw_menubar_background(painter, rect_, wstate);

    auto x = 0.0f;

    for (auto i = 0; i < static_cast<int>(menus_.size()); i++) {
        auto const &menu = menus_[i];
        auto item_w = theme.measure_menubar_item(menu->display_title()).width;
        auto item_rect = Rect{x, 0, item_w, rect_.height};

        theme.draw_menubar_item(painter, item_rect, menu->title(), i == hovered_, i == active_,
                                show_mnemonics_);

        x += item_w;
    }
}

float MenuBar::get_menu_x(int index) const {
    auto const &theme = Theme::current();
    auto x = 0.0f;
    for (auto i = 0; i < index; ++i) {
        x += theme.measure_menubar_item(menus_[i]->display_title()).width;
    }
    return x;
}

int MenuBar::menu_at(Point p) const {
    if (!hit_test(p)) {
        return -1;
    }
    auto const &theme = Theme::current();
    auto x = 0.0f;

    for (auto i = 0; i < static_cast<int>(menus_.size()); i++) {
        auto const &menu = menus_[i];
        auto item_w = theme.measure_menubar_item(menu->display_title()).width;
        if (p.x >= x && p.x < x + item_w) {
            return i;
        }
        x += item_w;
    }
    return -1;
}

bool MenuBar::handle_mouse(MouseEvent const &event) {
    auto inside = hit_test(event.position);

    switch (event.type) {
    case MouseEvent::Type::Move:
    case MouseEvent::Type::Drag:
        hovered_ = menu_at(event.position);
        if (menu_bar_keyboard_active_) {
            if (active_ != -1 && hovered_ != -1 && hovered_ != active_) {
                toggle_menu(hovered_);
            }
            return inside;
        }
        if (active_ != -1 && hovered_ != -1 && hovered_ != active_) {
            toggle_menu(hovered_);
        }
        return inside;

    case MouseEvent::Type::Press:
        if (menu_bar_keyboard_active_) {
            auto h = menu_at(event.position);
            if (active_ != -1 && h == active_) { // Click on active menu, close it
                menus_[active_]->close();
            } else if (h != -1) {
                toggle_menu(h);
            } else {
                if (window()) {
                    window()->close_all_popups();
                }
                menu_bar_keyboard_active_ = false;
                set_show_mnemonics(false);
            }
            return inside;
        }
        active_ = menu_at(event.position);
        if (active_ != -1) {
            toggle_menu(active_);
            return true;
        }
        return inside;

    case MouseEvent::Type::Release:
        return inside;

    case MouseEvent::Type::Leave:
        if (!menu_bar_keyboard_active_) {
            hovered_ = -1;
        }
        return true;
    default:
        return false;
    }
}

bool MenuBar::handle_key(KeyEvent const &event) {
    if (event.type == KeyEvent::Type::Press) {
        alt_key_down_ = event.alt;
    } else if (event.type == KeyEvent::Type::Release) {
        alt_key_down_ = event.alt;
        if (menu_bar_keyboard_active_ && !window()->has_popup()) {
            menu_bar_keyboard_active_ = false;
            set_show_mnemonics(false);
        }
        return false;
    }

    if (event.alt && event.text.empty()) {
        if (!window()->has_popup() && !menu_bar_keyboard_active_) {
            menu_bar_keyboard_active_ = true;
            set_show_mnemonics(true);
        } else if (menu_bar_keyboard_active_ && !window()->has_popup()) {
            menu_bar_keyboard_active_ = false;
            set_show_mnemonics(false);
        }
        return true;
    }

    if (menu_bar_keyboard_active_) {
        if (event.key == Key::Escape) {
            if (window()->has_popup()) {
                window()->close_popup();
            } else {
                menu_bar_keyboard_active_ = false;
                set_show_mnemonics(false);
            }
            return true;
        }

        if (active_ == -1 && menus_.empty()) {
            return false;
        }
        // Navigation within the menubar when active_
        if (event.key == Key::Left) {
            auto prev_index = (active_ - 1 + menus_.size()) % menus_.size();
            toggle_menu(prev_index);
            return true;
        } else if (event.key == Key::Right) {
            auto next_index = (active_ + 1) % menus_.size();
            toggle_menu(next_index);
            return true;
        } else if (event.key == Key::Down || event.key == Key::Enter) {
            if (active_ != -1) {
                menus_[active_]->on_close_callback = [this] {
                    active_ = -1;
                    hovered_ = -1;
                    menu_bar_keyboard_active_ = false;
                    set_show_mnemonics(false);
                };
                menus_[active_]->show(window(), map_to_window({get_menu_x(active_), rect_.height}));
                return true;
            }
        }
    }

    // F10 handling
    if (event.key == Key::F10) {
        if (menu_bar_keyboard_active_ && window()->has_popup()) {
            // F10 closes if already active and open
            menus_[active_]->close();
            menu_bar_keyboard_active_ = false;
            set_show_mnemonics(false);
        } else if (!menu_bar_keyboard_active_) {
            // F10 activates if not active
            menu_bar_keyboard_active_ = true;
            toggle_menu(0);
            set_show_mnemonics(true);
        } else if (menu_bar_keyboard_active_ && !window()->has_popup()) {
            // F10 closes if active but no popup
            menu_bar_keyboard_active_ = false;
            set_show_mnemonics(false);
        }
        return true;
    }

    // Existing logic for global shortcuts (only if not in keyboard navigation mode)
    if (!menu_bar_keyboard_active_) {
        for (auto const &menu : menus_) {
            for (auto const &item : menu->items()) {
                if (item.type == MenuItem::Type::Action && item.command->is_enabled()) {
                    if (item.command->matches_key_event(event)) {
                        item.command->execute();
                        return true;
                    }
                } else if (item.type == MenuItem::Type::Submenu && item.submenu) {
                    // FIXME: We should ideally recurse here if we had nested submenus
                }
            }
        }
    }

    // Mnemonic handling
    if (alt_key_down_ && !event.text.empty()) {
        auto key = normalize_mnemonic_key(event.text);
        if (trigger_mnemonic(key)) {
            set_show_mnemonics(false);
            return true;
        }
    }

    return false;
}

void MenuBar::collect_mnemonics(std::vector<Widget *> &out) { out.push_back(this); }

bool MenuBar::trigger_mnemonic(std::string_view key) {
    for (size_t i = 0; i < menus_.size(); ++i) {
        if (menus_[i]->mnemonic_key() == key) {
            toggle_menu(i);
            set_show_mnemonics(false);
            return true;
        }
    }
    return false;
}

void MenuBar::toggle_menu(int index) {
    if (index < 0 || index >= static_cast<int>(menus_.size())) {
        return;
    }
    if (active_ == index && window() && window()->has_popup()) {
        menu_bar_keyboard_active_ = false;
        menus_[active_]->close();
        return;
    }
    if (window()) {
        window()->close_all_popups();
    }

    auto const &theme = Theme::current();
    auto x = 0.0f;

    // Ensure keyboard navigation is active when a menu is opened
    menu_bar_keyboard_active_ = true;
    active_ = index;
    x = get_menu_x(index);

    auto pos = map_to_window({x, rect_.height});
    auto menu = menus_[index];
    menu->set_parent_menu(nullptr);
    menu->on_request_next_menu = [this, index] {
        auto next_index = (index + 1) % menus_.size();
        toggle_menu(next_index);
    };
    menu->on_request_prev_menu = [this, index] {
        auto prev_index = (index - 1 + menus_.size()) % menus_.size();
        toggle_menu(prev_index);
    };
    menu->on_close_callback = [this] {
        active_ = -1;
        hovered_ = -1;
        if (window() && !window()->has_popup()) {
            menu_bar_keyboard_active_ = false;
            set_show_mnemonics(false);
        }
    };
    menu->show(window(), pos);
}

Size MenuBar::size_hint() const {
    auto const &theme = Theme::current();
    auto const &style = theme.style.menuBar;
    auto const &palette = theme.palette;

    auto fm = font_metrics(palette.fonts.size);
    auto w = 0.0f;
    for (auto const &m : menus_) {
        w += theme.measure_menubar_item(m->display_title()).width;
    }
    return {w, fm.height + style.padding.top + style.padding.bottom};
}

auto MenuBar::item_to_json(MenuItem const &item) -> nlohmann::json {
    auto j = nlohmann::json::object();
    switch (item.type) {
    case MenuItem::Type::Action:
        j["type"] = "action";
        if (item.command) {
            j["command"] = item.command->to_json();
        }
        break;
    case MenuItem::Type::Separator:
        j["type"] = "separator";
        break;
    case MenuItem::Type::Submenu:
        j["type"] = "submenu";
        if (item.command) {
            j["command"] = item.command->to_json();
        }
        if (item.submenu) {
            auto items = nlohmann::json::array();
            for (auto const &sub : item.submenu->items()) {
                items.push_back(item_to_json(sub));
            }
            j["items"] = items;
        }
        break;
    }
    return j;
}

void MenuBar::item_from_json(nlohmann::json const &j, Menu &menu) {
    auto type = j.value("type", std::string{});
    if (type == "action") {
        auto cmd = Command::create("", nullptr);
        if (j.contains("command")) {
            cmd->from_json(j["command"]);
        }
        menu.add_action(cmd);
    } else if (type == "separator") {
        menu.add_separator();
    } else if (type == "submenu") {
        auto cmd = Command::create("", nullptr);
        if (j.contains("command")) {
            cmd->from_json(j["command"]);
        }
        auto submenu = std::make_shared<Menu>(cmd->name());
        if (j.contains("items")) {
            for (auto const &sub_json : j["items"]) {
                item_from_json(sub_json, *submenu);
            }
        }
        menu.add_submenu(cmd->name(), submenu);
    }
}

nlohmann::json MenuBar::to_json() const {
    auto j = Widget::to_json();
    auto menus_json = nlohmann::json::array();
    for (auto const &menu : menus_) {
        auto menu_json = nlohmann::json::object();
        menu_json["title"] = menu->title();
        auto items_json = nlohmann::json::array();
        for (auto const &item : menu->items()) {
            items_json.push_back(item_to_json(item));
        }
        menu_json["items"] = items_json;
        menus_json.push_back(menu_json);
    }
    j["menus"] = menus_json;
    return j;
}

void MenuBar::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (!j.contains("menus")) {
        return;
    }
    for (auto const &menu_json : j["menus"]) {
        auto title = menu_json.value("title", std::string{});
        auto menu = std::make_shared<Menu>(title);
        if (menu_json.contains("items")) {
            for (auto const &item_json : menu_json["items"]) {
                item_from_json(item_json, *menu);
            }
        }
        add_menu(menu);
    }
}

} // namespace svision3
