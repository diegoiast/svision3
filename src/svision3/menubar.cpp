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

bool MenuBar::mnemonics_visible() const {
    return show_mnemonics_ || Theme::current().style.menuBar.always_show_mnemonics;
}

bool MenuBar::has_open_menu() const { return window() && window()->has_popup(); }

void MenuBar::set_keyboard_active(bool active) {
    menu_bar_keyboard_active_ = active;
    if (!window()) {
        return;
    }
    // While being driven from the keyboard the menubar holds the keyboard, so the
    // arrows reach it rather than whatever widget happens to be focused. Without
    // this, a focused editor swallows Left/Right/Down and the menubar looks stuck.
    if (active) {
        window()->set_key_grab(this);
    } else if (window()->key_grab() == this) {
        window()->set_key_grab(nullptr);
    }
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

static auto find_shortcut_in(Menu const &menu, KeyEvent const &event) -> Command::Ptr {
    for (auto const &item : menu.items()) {
        if (item.is_submenu() && item.submenu) {
            if (auto found = find_shortcut_in(*item.submenu, event)) {
                return found;
            }
            continue;
        }
        if (!item.is_action() || !item.command || !item.command->is_enabled()) {
            continue;
        }
        if (item.command->matches_key_event(event)) {
            return item.command;
        }
    }
    return nullptr;
}

Command::Ptr MenuBar::find_shortcut(KeyEvent const &event) const {
    for (auto const &menu : menus_) {
        if (auto found = find_shortcut_in(*menu, event)) {
            return found;
        }
    }
    return nullptr;
}

int MenuBar::find_menu(std::string_view title) const {
    for (auto i = 0; i < static_cast<int>(menus_.size()); ++i) {
        if (menus_[i]->display_title() == title) {
            return i;
        }
    }
    return -1;
}

bool MenuBar::is_menu_shown(int index) const {
    if (index < 0 || index >= static_cast<int>(menus_.size())) {
        return false;
    }
    // An empty menu has nothing to drop down -- Menu::show() bails on one -- so
    // showing its title would leave a dead word in the menubar. Menus that fill
    // from the focused widget start out empty and appear once they have content.
    return !menus_[index]->items().empty();
}

int MenuBar::next_shown_menu(int from, int dir) const {
    auto n = static_cast<int>(menus_.size());
    if (n == 0) {
        return -1;
    }
    auto index = ((from + dir) % n + n) % n;
    for (auto step = 0; step < n; ++step) {
        if (is_menu_shown(index)) {
            return index;
        }
        index = ((index + dir) % n + n) % n;
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
        if (!is_menu_shown(i)) {
            continue;
        }
        auto const &menu = menus_[i];
        auto item_w = theme.measure_menubar_item(menu->display_title()).width;
        auto item_rect = Rect{x, 0, item_w, rect_.height};

        theme.draw_menubar_item(painter, item_rect, menu->title(), i == hovered_, i == active_,
                                mnemonics_visible());

        x += item_w;
    }
}

float MenuBar::get_menu_x(int index) const {
    auto const &theme = Theme::current();
    auto x = 0.0f;
    for (auto i = 0; i < index; ++i) {
        if (!is_menu_shown(i)) {
            continue;
        }
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
        if (!is_menu_shown(i)) {
            continue;
        }
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
                set_keyboard_active(false);
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
    auto is_alt_key = event.key == Key::LeftAlt || event.key == Key::RightAlt;

    if (event.type == KeyEvent::Type::Release) {
        alt_key_down_ = event.alt;
        // Tapping Alt on its own activates the menubar, matching F10. Releasing it
        // after a chord (Alt+F) must not, or every mnemonic would toggle twice.
        if (is_alt_key && alt_pressed_alone_) {
            alt_pressed_alone_ = false;
            toggle_keyboard_activation();
            return true;
        }
        alt_pressed_alone_ = false;
        return false;
    }

    if (event.type != KeyEvent::Type::Press) {
        return false;
    }

    alt_key_down_ = event.alt;
    if (is_alt_key) {
        // Preview the mnemonics while Alt is held; the release decides whether we
        // actually enter menu navigation -- or leave it, if we are already there.
        alt_pressed_alone_ = true;
        if (!menu_bar_keyboard_active_) {
            set_show_mnemonics(true);
        }
        return true;
    }
    alt_pressed_alone_ = false;

    if (menu_bar_keyboard_active_) {
        if (event.key == Key::Escape) {
            if (has_open_menu()) {
                window()->close_popup();
            } else {
                set_keyboard_active(false);
                active_ = -1;
                hovered_ = -1;
                set_show_mnemonics(false);
            }
            return true;
        }

        if (menus_.empty()) {
            return false;
        }

        if (event.key == Key::Left || event.key == Key::Right) {
            auto n = static_cast<int>(menus_.size());
            auto dir = (event.key == Key::Right) ? 1 : -1;
            // With nothing selected yet, Right starts at the first menu and Left
            // wraps around to the last one.
            auto base = (active_ == -1) ? ((dir == 1) ? -1 : 0) : active_;
            // Steps over menus that are currently empty, so the arrows only visit
            // titles the user can actually see.
            auto next_index = next_shown_menu(base, dir);
            if (next_index < 0) {
                return true;
            }

            // Sideways movement re-opens the neighbour only when a menu is already
            // dropped down; with the menubar merely focused it just moves the
            // highlight, so the top level can be walked without popups appearing.
            if (has_open_menu()) {
                toggle_menu(next_index);
            } else {
                active_ = next_index;
                if (window()) {
                    window()->request_redraw("menubar selection");
                }
            }
            return true;
        }

        if (event.key == Key::Down || event.key == Key::Enter) {
            if (active_ != -1 && !has_open_menu()) {
                toggle_menu(active_);
                menus_[active_]->select_first();
                return true;
            }
        }
    }

    if (event.key == Key::F10) {
        toggle_keyboard_activation();
        return true;
    }

    // Existing logic for global shortcuts (only if not in keyboard navigation mode)
    if (!menu_bar_keyboard_active_) {
        if (auto cmd = find_shortcut(event)) {
            cmd->execute();
            return true;
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
        if (!is_menu_shown(static_cast<int>(i))) {
            continue;
        }
        if (menus_[i]->mnemonic_key() == key) {
            // The window routes mnemonics straight here, so handle_key never sees
            // the letter that turned Alt into a chord. Without this, releasing Alt
            // afterwards would read as a bare tap and close the menu just opened.
            alt_pressed_alone_ = false;
            toggle_menu(i);
            set_show_mnemonics(false);
            return true;
        }
    }
    return false;
}

void MenuBar::toggle_keyboard_activation() {
    if (menu_bar_keyboard_active_) {
        if (active_ != -1 && has_open_menu()) {
            menus_[active_]->close();
        }
        set_keyboard_active(false);
        active_ = -1;
        hovered_ = -1;
        set_show_mnemonics(false);
        return;
    }
    // F10 and a bare Alt land on the first menu with anything in it.
    auto first = 0;
    while (first < static_cast<int>(menus_.size()) && !is_menu_shown(first)) {
        first++;
    }
    if (first >= static_cast<int>(menus_.size())) {
        return;
    }
    set_keyboard_active(true);
    set_show_mnemonics(true);
    toggle_menu(first);
}

void MenuBar::toggle_menu(int index) {
    if (!is_menu_shown(index)) {
        return;
    }
    if (active_ == index && window() && window()->has_popup()) {
        set_keyboard_active(false);
        menus_[active_]->close();
        return;
    }
    if (window()) {
        window()->close_all_popups();
    }

    auto const &theme = Theme::current();
    auto x = 0.0f;

    // Ensure keyboard navigation is active when a menu is opened
    set_keyboard_active(true);
    active_ = index;
    // A dropped-down menu means the user is in menu-land, so every menubar title
    // reveals its mnemonic -- not just the one that happens to be open.
    set_show_mnemonics(true);
    x = get_menu_x(index);

    auto pos = map_to_window({x, rect_.height});
    auto menu = menus_[index];
    menu->set_parent_menu(nullptr);
    // Left/Right inside an open dropdown walks the menubar the same way the
    // menubar's own arrow handling does -- skipping empty menus. Stepping by a
    // plain +/-1 would hand toggle_menu() an empty neighbour, which it refuses to
    // open, leaving navigation stuck one move away from where it started. Landing
    // back on the menu already open (the only one with content) is left alone
    // rather than toggled shut.
    menu->on_request_next_menu = [this, index] {
        auto next_index = next_shown_menu(index, 1);
        if (next_index >= 0 && next_index != index) {
            toggle_menu(next_index);
        }
    };
    menu->on_request_prev_menu = [this, index] {
        auto prev_index = next_shown_menu(index, -1);
        if (prev_index >= 0 && prev_index != index) {
            toggle_menu(prev_index);
        }
    };
    menu->on_close_callback = [this, index] {
        hovered_ = -1;
        if (window() && window()->has_popup()) {
            // A submenu of this menu is still on screen.
            return;
        }
        // Escape backs out of the dropdown but stays on the menubar, so the arrow
        // keys keep walking the top level; anything else leaves menu navigation.
        if (menus_[index]->last_close_reason() == Menu::CloseReason::EscapePressed) {
            active_ = index;
            return;
        }
        active_ = -1;
        set_keyboard_active(false);
        set_show_mnemonics(false);
    };
    menu->show(window(), pos);
}

Size MenuBar::size_hint() const {
    auto const &theme = Theme::current();
    auto const &style = theme.style.menuBar;
    auto const &palette = theme.palette;

    auto fm = font_metrics(palette.fonts.size);
    auto w = 0.0f;
    for (auto i = 0; i < static_cast<int>(menus_.size()); i++) {
        if (!is_menu_shown(i)) {
            continue;
        }
        w += theme.measure_menubar_item(menus_[i]->display_title()).width;
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
