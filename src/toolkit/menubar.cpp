// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/menubar.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

namespace toolkit {

MenuBar::MenuBar() { set_focusable(false); }

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
        if (menus_[i]->title() == title) {
            return i;
        }
    }
    return -1;
}

void MenuBar::paint(Painter &painter) {
    auto const &style = Theme::current().button;
    auto padding = style.padding;
    auto fm = painter.font_metrics(style.font_size);
    auto x = 0.0f;

    for (auto i = 0; i < static_cast<int>(menus_.size()); i++) {
        auto const &menu = menus_[i];
        auto text_w = painter.text_size(menu->title(), style.font_size).width;
        auto item_w = text_w + padding.left + padding.right;
        auto item_rect = Rect{x, 0, item_w, rect_.height};

        if (i == hovered_ || i == active_) {
            painter.fill_rect(item_rect,
                              style.background_hovered.value_or(style.background.darken(0.1f)));
        }

        auto baseline = (rect_.height - fm.height) / 2.0f + fm.ascent;
        painter.draw_text(menu->title(), {x + padding.left, baseline}, style.text, style.font_size);

        x += item_w;
    }

    // Bottom border
    auto border_c = Theme::current().window.background.darken(0.15f);
    painter.draw_line({0, rect_.height - 1.0f}, {rect_.width, rect_.height - 1.0f}, border_c, 1.0f);
}

int MenuBar::menu_at(Point p) const {
    if (!hit_test(p)) {
        return -1;
    }
    auto const &style = Theme::current().button;
    auto padding = style.padding;
    auto x = 0.0f;

    for (auto i = 0; i < static_cast<int>(menus_.size()); i++) {
        auto const &menu = menus_[i];
        auto text_w = Painter::measure_text(menu->title(), style.font_size).width;
        auto item_w = text_w + padding.left + padding.right;
        if (p.x >= x && p.x < x + item_w) {
            return i;
        }
        x += item_w;
    }
    return -1;
}

bool MenuBar::handle_mouse(MouseEvent const &event) {
    if (window() && !window()->has_popup()) {
        active_ = -1;
    }
    bool inside = hit_test(event.position);
    switch (event.type) {
    case MouseEvent::Type::Move:
        hovered_ = menu_at(event.position);
        if (active_ != -1 && hovered_ != -1 && hovered_ != active_) {
            open_menu(hovered_);
        }
        return inside;
    case MouseEvent::Type::Press:
        active_ = menu_at(event.position);
        if (active_ != -1) {
            open_menu(active_);
            return true;
        }
        return false;
    case MouseEvent::Type::Leave:
        hovered_ = -1;
        return true;
    default:
        return false;
    }
}

bool MenuBar::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }

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

    // MenuBar usually doesn't handle keys unless Alt is pressed,
    // which is handled by Window mnemonics.
    return false;
}

void MenuBar::open_menu(int index) {
    if (index < 0 || index >= static_cast<int>(menus_.size())) {
        return;
    }

    active_ = index;
    auto const &style = Theme::current().button;
    auto padding = style.padding;
    auto x = 0.0f;
    for (int i = 0; i < index; ++i) {
        x += Painter::measure_text(menus_[i]->title(), style.font_size).width + padding.left +
             padding.right;
    }

    auto pos = map_to_window({x, rect_.height});
    menus_[index]->show(window(), pos);
}

Size MenuBar::size_hint() const {
    auto const &style = Theme::current().button;
    auto padding = style.padding;
    auto fm = Painter::measure_font_metrics(style.font_size);
    auto w = 0.0f;
    for (auto const &m : menus_) {
        w +=
            Painter::measure_text(m->title(), style.font_size).width + padding.left + padding.right;
    }
    return {w, fm.height + padding.top + padding.bottom};
}

} // namespace toolkit
