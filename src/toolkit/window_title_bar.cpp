// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/window_title_bar.hpp"
#include "toolkit/image_widget.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/widget.hpp"
#include "toolkit/window.hpp"

#include <algorithm>
#include <memory>
#include <spdlog/spdlog.h>

namespace toolkit {

// How far the pointer must travel from the press before a drag on a maximized window's title bar
// is treated as "the user wants to pull this window loose" rather than as a click.
static constexpr auto drag_threshold = 5.0f;

TitlebarButton::TitlebarButton(DecorationButton type, std::string tooltip, Size size_hint)
    : Button(""), type_(type), custom_size_hint(size_hint) {
    set_flat(true);
    set_tooltip(std::move(tooltip));
}

void TitlebarButton::paint(Painter &painter) {
    auto interaction = ButtonState::Normal;
    if (is_pressed()) {
        interaction = ButtonState::ClickedInside;
    } else if (is_hovered()) {
        interaction = ButtonState::Hovered;
    }
    auto wstate = WidgetState{
        .interaction = interaction,
        .focused = is_focused(),
        .enabled = is_enabled(),
        .window_active = window_ ? window_->is_active() : true,
        .checked = false,
    };
    Theme::current().draw_window_button(painter, {0, 0, rect_.width, rect_.height}, type_, wstate);
}

TitleBarIcon::TitleBarIcon(Window *w) : Button(""), window_(w) {
    set_focusable(false);
    set_flat(true);
}

void TitleBarIcon::set_image(Icon const &icon) {
    icon_image_ = icon;
    if (window_) {
        window_->request_redraw("title bar icon");
    }
}

void TitleBarIcon::paint(Painter &painter) {
    // Deliberately not Button::paint(): the window icon is an affordance, not a button face.
    // Button draws a hover/pressed background, which around a 16px icon reads as a stray frame
    // floating in the title bar -- and Windows' own title bar icon has no hover treatment at all.
    // Button is still the base class for its press/state handling, just not its chrome.
    if (!icon_image_ || icon_image_->width <= 0 || icon_image_->height <= 0) {
        return;
    }
    auto iw = static_cast<float>(icon_image_->width);
    auto ih = static_cast<float>(icon_image_->height);
    painter.draw_image(*icon_image_,
                       {(rect_.width - iw) / 2.0f, (rect_.height - ih) / 2.0f});
}

bool TitleBarIcon::handle_mouse(MouseEvent const &event) {
    // Deliberately not Button's on_click: a system menu opens on press, not on release, which is
    // what Windows itself does for the title bar icon. Everything else (hover/pressed state) is
    // left to Button.
    if (event.type == MouseEvent::Type::Press && hit_test(event.position)) {
        // map_to_window() walks the real parent chain (icon -> layout -> WindowTitleBar ->
        // root_), so it already lands in full window-local coordinates, shadow included --
        // no separate compensation needed (see WindowTitleBar::create_title_layout()).
        auto menu_pos = map_to_window({0, rect_.height});
        window_->platform_window()->show_system_menu(menu_pos);
        return true;
    }
    return Button::handle_mouse(event);
}

WindowTitleBar::WindowTitleBar(Window *w) { set_window(w); }

auto WindowTitleBar::create_title_layout() -> HBoxLayout * {
    layout = new HBoxLayout();
    layout->set_parent(this);
    return layout;
}

void WindowTitleBar::initializeTitleBar() {
    layout = create_title_layout();
    layout->set_window(window_);
    layout->set_margins(Margins{8.0f, 12.0, 8.0, 12.0f});
    layout->set_spacing(8.0f);

    icon_widget = new TitleBarIcon(window_);
    icon_widget->set_min_size({16, 16});
    icon_widget->set_max_size({16, 16});
    icon_widget->set_image(window_->get_icon());

    close_btn = new TitlebarButton(DecorationButton::Close, "Close");
    close_btn->on_click = [this] { window_->close(); };

    min_btn = new TitlebarButton(DecorationButton::Minimize, "Minimize");
    min_btn->on_click = [this] { window_->minimize(); };

    max_btn = new TitlebarButton(DecorationButton::Maximize, "Zoom");
    max_btn->on_click = [this] {
        if (window_->is_maximized()) {
            window_->restore();
        } else {
            window_->maximize();
        }
    };

    sync_button_states();

    layout->add_widget(std::unique_ptr<Widget>(icon_widget));
    title_label = new Label(std::string{window_->title()});
    title_label->set_alignment(Alignment::Center).set_shrinkable(true).set_elide(true);
    layout->add_widget(std::unique_ptr<Label>(title_label), 1);
    layout->add_widget(std::unique_ptr<Widget>(min_btn));
    layout->add_widget(std::unique_ptr<Widget>(max_btn));
    layout->add_widget(std::unique_ptr<Widget>(close_btn));
}

auto WindowTitleBar::create_btn(DecorationButton type) -> Button * {
    auto tooltip = std::string("");

    switch (type) {
    case DecorationButton::Close:
        tooltip = "Close";
        break;
    case DecorationButton::Minimize:
        tooltip = "Minimize";
        break;
    case DecorationButton::Maximize:
        tooltip = "Maximize";
        break;
    case DecorationButton::Restore:
        tooltip = "Restore";
        break;
    case DecorationButton::Menu:
        tooltip = "Menu";
        break;
    }

    auto *btn = new TitlebarButton(type, tooltip);
    btn->on_click = [this, type, btn] {
        switch (type) {
        case DecorationButton::Close:
            window_->close();
            break;
        case DecorationButton::Minimize:
            window_->minimize();
            break;
        case DecorationButton::Maximize:
            window_->maximize();
            break;
        case DecorationButton::Restore:
            window_->restore();
            break;
        case DecorationButton::Menu: {
            auto menu_pos = map_to_window({btn->rect().x, btn->rect().y + btn->rect().height});
            window_->platform_window()->show_system_menu(menu_pos);
            break;
        }
        }
    };
    return btn;
}

void WindowTitleBar::sync_button_states() {
    if (min_btn) {
        min_btn->set_enabled(window_->is_minimizable());
    }
    if (max_btn) {
        max_btn->set_enabled(window_->is_maximizable());
        max_btn->set_tooltip(window_->is_maximized() ? "Restore" : "Maximize");
        // The tooltip swap above only ever touched text -- the icon glyph itself was fixed to
        // whatever DecorationButton the theme constructed it with (always ::Maximize), so it
        // never actually showed "restore" once the window was maximized. max_btn is declared as
        // the generic Button* every other titlebar button uses, but every theme constructs it as
        // a TitlebarButton, so this cast is safe.
        if (auto *titlebar_btn = dynamic_cast<TitlebarButton *>(max_btn)) {
            titlebar_btn->set_type(window_->is_maximized() ? DecorationButton::Restore
                                                            : DecorationButton::Maximize);
        }
    }
    if (close_btn) {
        close_btn->set_enabled(window_->is_closable());
    }
}

Size WindowTitleBar::size_hint() const {
    auto const &m = Theme::current().Theme::current().style.window_decoration;
    return {100.0f, m.top};
}

void WindowTitleBar::paint(Painter &painter) {
    auto const &pal = Theme::current().palette;
    auto active = window_->is_active();
    auto bg = active ? pal.accent : pal.window;
    auto fg = active ? pal.highlighted_text : pal.text_disabled;

    // FIXME: it would be nice for this to be changable from other themes
    painter.fill_rect({0, 0, rect_.width, rect_.height}, bg);
    sync_button_states();

    // FIXME: update window label only when the window title changed
    title_label->set_text(std::string(window_->title()));
    // FIXME: update color on blur/active
    title_label->set_color(fg);
    layout->paint(painter);
}

void WindowTitleBar::set_rect(Rect const &rect) {
    Widget::set_rect(rect);
    if (layout) {
        layout->set_rect(rect);
    }
}

Widget *WindowTitleBar::widget_at(Point p) {
    if (!is_visible() || !hit_test(p)) {
        return nullptr;
    }
    // The layout is given this widget's own rect (see set_rect) and positions its children from
    // its own origin, so our local point can be handed straight down -- the same convention
    // handle_mouse() already relies on when it forwards events to the layout.
    //
    // Only resolve into a child that can actually act on the point (Widget::blocks_hit_test()).
    // title_label, for instance, stretches to fill the whole draggable middle of the bar but
    // never handles mouse input -- Press events go straight to whatever widget_at() resolves to
    // (no bubbling back up on rejection), so handing it a purely decorative child directly would
    // swallow the click before WindowTitleBar::handle_mouse() ever runs and the window could no
    // longer be dragged. Buttons/icon (which do handle input, or want tooltip hover resolution)
    // still resolve normally; anything that opts out is treated as part of the plain bar surface.
    if (layout) {
        if (auto *child = layout->widget_at(p); child && child->blocks_hit_test()) {
            return child;
        }
    }
    return this;
}

void WindowTitleBar::set_icon(Icon const &icon) {
    if (icon_widget) {
        icon_widget->set_image(icon);
    } else {
        spdlog::warn("WindowTitleBar::set_icon called but icon_widget is null!");
    }
}

void WindowTitleBar::pull_loose_from_maximized(MouseEvent const &event) {
    auto *platform = window_->platform_window();
    auto maximized_size = window_->size();

    // Where the pointer is on screen right now. Only knowable where the platform tracks window
    // positions at all -- on Wayland the restore below is all we can do, and the compositor is
    // the one that decides where the surface ends up.
    auto can_place = platform->can_set_position();
    auto pointer_in_window = map_to_window(event.position);
    auto origin = can_place ? platform->position() : Point{};

    window_->restore();

    if (!can_place) {
        return;
    }
    // On backends where unmaximizing is asynchronous the new size is not known yet, and placing
    // the window from the stale one would just shove it sideways for no reason.
    auto restored_size = window_->size();
    if (restored_size.width >= maximized_size.width) {
        return;
    }

    // Land the window under the pointer holding the same relative spot on the title bar that was
    // grabbed, which is what a native title bar does. The restored window carries a shadow inset
    // the maximized one does not, so the bar no longer starts at the window's own corner.
    auto const &style = Theme::current().style;
    auto inset = style.border_width + style.shadow.size;
    auto ratio = rect_.width > 0 ? press_position.x / rect_.width : 0.5f;
    auto grab_x = inset + ratio * std::max(0.0f, restored_size.width - 2 * inset);
    auto grab_y = inset + press_position.y;
    platform->set_position({origin.x + pointer_in_window.x - grab_x,
                            origin.y + pointer_in_window.y - grab_y});
}

bool WindowTitleBar::handle_mouse(MouseEvent const &event) {
    if (layout->handle_mouse(event)) {
        return true;
    }

    // Checked before the bounds test below: a drag that unmaximizes naturally pulls the pointer
    // off the bar, and the release ending it can land anywhere.
    if (pending_move) {
        if (event.type == MouseEvent::Type::Drag) {
            auto dx = event.position.x - press_position.x;
            auto dy = event.position.y - press_position.y;
            if (dx * dx + dy * dy < drag_threshold * drag_threshold) {
                return true;
            }
            pending_move = false;
            pull_loose_from_maximized(event);
            // The press serial, not the drag's: compositors validate a move request against the
            // input event that is meant to have started it.
            window_->start_system_move(press_serial);
            return true;
        }
        if (event.type == MouseEvent::Type::Release) {
            pending_move = false;
            return true;
        }
    }

    auto local_rect = Rect{0, 0, rect_.width, rect_.height};
    if (!local_rect.contains(event.position)) {
        return false;
    }

    if (event.type == MouseEvent::Type::Press) {
        if (event.button == 1) { // Right click
            auto menu_pos = map_to_window(event.position);
            window_->platform_window()->show_system_menu(menu_pos);
            return true;
        }
        if (event.click_count == 2) {
            if (window_->is_maximized()) {
                window_->restore();
            } else {
                window_->maximize();
            }
            return true;
        }
        if (window_->is_maximized() && !window_->platform_window()->system_move_unmaximizes()) {
            pending_move = true;
            press_position = event.position;
            press_serial = event.serial;
            return true;
        }
        // Handing the press straight over covers the maximized case too on platforms that pull
        // the window loose themselves -- including leaving it maximized when the press turns out
        // to be a plain click rather than a drag.
        window_->start_system_move(event.serial);
        return true;
    }
    return false;
}

} // namespace toolkit
