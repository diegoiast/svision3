// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/menubar.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <spdlog/spdlog.h>

namespace toolkit {

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

    theme.draw_menubar_background(painter, rect_);

    auto x = 0.0f;

    for (auto i = 0; i < static_cast<int>(menus_.size()); i++) {
        auto const &menu = menus_[i];
        auto item_w = theme.measure_menubar_item(menu->display_title()).width;
        auto item_rect = Rect{x, 0, item_w, rect_.height};

        theme.draw_menubar_item(painter, item_rect, menu->display_title(), i == hovered_,
                                i == active_, show_mnemonics_, menu->mnemonic_index());

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
        // FIXME: this will not work with non ASCII letters
        char key = std::tolower(static_cast<unsigned char>(event.text[0]));
        if (trigger_mnemonic(key)) {
            set_show_mnemonics(false);
            return true;
        }
    }

    return false;
}

void MenuBar::collect_mnemonics(std::vector<Widget *> &out) { out.push_back(this); }

bool MenuBar::trigger_mnemonic(char key) {
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
    auto const &style = theme.menubar;
    auto const &palette = theme.palette;

    auto fm = Painter::measure_font_metrics(palette.fonts.size);
    auto w = 0.0f;
    for (auto const &m : menus_) {
        w += theme.measure_menubar_item(m->display_title()).width;
    }
    return {w, fm.height + style.padding.top + style.padding.bottom};
}

} // namespace toolkit
