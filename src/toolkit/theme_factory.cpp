// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_factory.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_base.hpp"
#include "toolkit/theme_macos.hpp"
#include "toolkit/theme_plasma.hpp"
#include "toolkit/theme_win11.hpp"
#include "toolkit/theme_win95.hpp"
#include "toolkit/types.hpp"
#include "toolkit/widget.hpp"
#include "toolkit/window.hpp"
#include "toolkit/window_title_bar.hpp"

#include <assert.h>
#include <memory>

namespace toolkit {

class GnomeTitleBar : public WindowTitleBar {
  public:
    GnomeTitleBar(Window *window) : WindowTitleBar(window) {
        // Anything else...?
    }

    void initializeTitleBar() {
        layout = create_title_layout();
        layout->set_window(window_);
        // 36px total height. 24px buttons, centered means (36-24)/2 = 8px vertical margin.
        layout->set_margins(Margins{8.0f, 10.0f, 8.0f, 10.0f});
        layout->set_spacing(10.0f);

        icon_widget = new TitleBarIcon(window_);
        icon_widget->set_min_size({22, 22});
        icon_widget->set_max_size({22, 22});
        icon_widget->set_image(window_->get_icon());

        // Set buttons to 32px height.
        close_btn = new TitlebarButton(DecorationButton::Close, "Close", {22, 22});
        close_btn->on_click = [this] { window_->close(); };
        close_btn->set_min_size({22, 22});
        close_btn->set_max_size({22, 22});
        sync_button_states();

        title_label = new Label(std::string{window_->title()});
        title_label->set_alignment(Alignment::Center).set_shrinkable(true).set_elide(true);

        layout->add_widget(std::unique_ptr<Widget>(icon_widget));
        layout->add_widget(std::unique_ptr<Label>(title_label), 1);
        layout->add_widget(std::unique_ptr<Widget>(close_btn));
    }

    void paint(Painter &painter) {
        auto const &pal = Theme::current().palette;
        auto active = window_->is_active();
        auto bg = active ? pal.window : pal.window_inactive.value_or(pal.window);
        auto fg = active ? pal.text : pal.text_disabled;

        painter.fill_rect({0, 0, rect_.width, rect_.height}, bg);
        sync_button_states();
        title_label->set_text(std::string(window_->title()));
        title_label->set_color(fg);
        layout->paint(painter);
    }
};
MaterialTheme::MaterialTheme(ColorScheme scheme, std::optional<Palette> p)
    : BaseTheme(scheme, std::move(p)) {
    if (!p) {
        palette = this->default_palette(scheme);
    }
    name = "Material";
    style.menu.padding = {4, 4, 4, 4};
    style.menuBar.padding = {4, 12, 4, 12};
    style.slider.handle_size = 18.0f;
    style.slider.groove_thickness = 4.0f;
    style.ringFocus.margin = 3.0f;
    style.ringFocus.corner_radius = 3.0f;
    style.tabWidget.indicator_weight = 2.0f;
    style.corner_radius = 10.0f;
    style.tabWidget.tab_radius = 4.0f;
    style.window_decoration = {32, 0, 0, 0};
}

Palette MaterialTheme::default_palette(ColorScheme scheme) const {
    auto material_purple = Color::from_rgb(0x6750A4);
    Palette p = BaseTheme::default_palette(scheme);

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_rgb(0xFFFBFE);
        p.base = Color::from_rgb(0xFFFFFF);
        p.alternate = Color::from_rgb(0xF7F2FA);
        p.text = Color::from_rgb(0x1C1B1F);
        p.text_disabled = Color::from_rgb(0x9E9E9E);
        p.placeholder = Color::from_rgb(0x79747E);
        p.highlight = material_purple;
        p.highlighted_text = Color::from_rgb(0x000000);
        p.border = Color::from_rgb(0xE7E0EC);
        p.accent = material_purple;
        p.link = Color::from_rgb(0xFF2962FF);
        p.shadow = Color::from_rgb(0x000000);
        p.dark_shadow = Color::from_rgb(0x000000);
        p.background_pressed = Color::from_rgb(0xE8DEF8);
        p.background_hovered = Color::from_rgb(0xF3EDF7);
        p.tooltip = Color::from_rgb(0xE6E1E5);
        p.success = Color::from_rgb(0x2E7D32);
        p.warning = Color::from_rgb(0xF9A825);
        p.error = Color::from_rgb(0xB3261E);
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        break;
    case ColorScheme::Dark:
        p.window = Color::from_rgb(0x1C1B1F);
        p.base = Color::from_rgb(0x1C1B1F);
        p.alternate = Color::from_rgb(0x292529);
        p.text = Color::from_rgb(0xab8587);
        p.text_disabled = Color::from_rgb(0x9E9E9E);
        p.placeholder = Color::from_rgb(0x79747E);
        p.highlight = material_purple;
        p.highlighted_text = Color::from_rgb(0xFFFFFF);
        p.border = Color::from_rgb(0x49454F);
        p.accent = material_purple;
        p.link = Color::from_rgb(0x82B1FF);
        p.shadow = Color::from_rgb(0x000000);
        p.dark_shadow = Color::from_argb(0x66000000);
        p.background_pressed = Color::from_rgb(0x3E3748);
        p.background_hovered = Color::from_rgb(0x4A4256);
        p.tooltip = Color::from_rgb(0x2B2B2B);
        p.success = Color::from_rgb(0x2E7D32);
        p.warning = Color::from_rgb(0xF9A825);
        p.error = Color::from_rgb(0xB3261E);
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        break;
    }
    return p;
}

GnomeTheme::GnomeTheme(ColorScheme scheme, std::optional<Palette> p)
    : BaseTheme(scheme, std::move(p)) {
    if (!p) {
        palette = this->default_palette(scheme);
    }
    name = "GNOME";

    style.chrome_lines = false;
    style.corner_radius = 10.0f;
    style.inline_scrollbars = false;
    style.tabWidget.tab_radius = 10.0f;
    style.tabWidget.tab_padding = 4.0f;
    style.tabWidget.tab_fully_rounded = true;
    style.window_decoration = {36, 0, 0, 0};
    style.bottom_shadow = true;

    style.button.padding = {10, 16, 10, 16};
    style.slider.handle_size = 22.0f;
    style.slider.groove_thickness = 6.0f;
    style.ringFocus.margin = 2.0f;
    style.ringFocus.corner_radius = 2.0f;
    style.scrollbar.show_buttons = false;
    style.scrollbar.thickness = 6.0f;
    style.scrollbar.show_frame = false;
    style.scrollbar.padding = {0, 0, 0, 0};
    style.tabWidget.indicator_weight = 0.0f;
    style.tabWidget.tab_padding_v = 12.0f;
    style.bottom_shadow = true;
    style.shadow.opacity = 0.2f;
    style.shadow.size = 32;
}

Palette GnomeTheme::default_palette(ColorScheme scheme) const {
    auto adwaita_color = Color::from_argb(0xFF3465A4);
    Palette p = BaseTheme::default_palette(scheme);

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_rgb(0xffffff);
        p.base = Color::from_rgb(0xebebed);
        p.alternate = Color::from_rgb(0xf6f6f6);
        p.text = Color::from_rgb(0x2e3436);
        p.text_disabled = Color::from_rgb(0x888a85);
        p.placeholder = Color::from_rgb(0xaaaaaa);
        p.highlight = adwaita_color;
        p.highlighted_text = Color::from_rgb(0xffffff);
        p.border = Color::from_rgb(0xcbcbcb);
        p.accent = adwaita_color;
        p.link = adwaita_color;
        p.shadow = Color::rgba(1, 1, 1, 0.8f);
        p.dark_shadow = Color::from_rgb(0xb0b0b0);
        p.light = p.shadow;
        p.background_pressed = Color::from_rgb(0xdedee0);
        p.background_hovered = Color::from_rgb(0xdedee0);
        p.tooltip = p.base;
        p.success = Color::from_rgb(0x2e7d32);
        p.warning = Color::from_rgb(0xfbc02d);
        p.error = Color::from_rgb(0xc62828);
        p.tab_select_background = Color::from_rgb(0xd8d8db);
        p.tab_background = p.base;
        break;
    case ColorScheme::Dark:
        p.window = Color::from_argb(0xFF2E3436);
        p.base = Color::from_argb(0xFF3B3B3B);
        p.alternate = Color::from_argb(0xFF4A4A4A);
        p.text = Color::from_argb(0xFFECECEC);
        p.text_disabled = Color::from_argb(0xFF888A85);
        p.placeholder = Color::from_argb(0xFFAAAAAA);
        p.highlight = adwaita_color;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFF555555);
        p.accent = adwaita_color;
        p.link = adwaita_color;
        p.shadow = Color::from_argb(0x66000000);
        p.dark_shadow = p.shadow;
        p.light = p.shadow;
        p.tooltip = Color::rgb(0.25f, 0.25f, 0.22f);
        p.background_pressed = Color::from_argb(0xFF484848);
        p.background_hovered = Color::from_argb(0xFF565656);
        p.tooltip = p.base;
        p.success = Color::from_argb(0xFF2E7D32);
        p.warning = Color::from_argb(0xFFFBC02D);
        p.error = Color::from_argb(0xFFC62828);
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        break;
    }
    return p;
}

void GnomeTheme::draw_window_button(Painter &painter, Rect const &rect, DecorationButton button,
                                    WidgetState const &state) const {
    auto &style = this->style;
    auto bg = palette.base;
    if (state.interaction == ButtonState::Hovered) {
        bg = palette.tab_select_background;
    }
    if (state.interaction == ButtonState::ClickedInside) {
        bg = palette.tab_select_background;
    }

    painter.fill_rounded_rect(rect, bg, rect.height);

    if (button == DecorationButton::Close) {
        auto padding = 15.0f;
        painter.draw_line({rect.x + padding, rect.y + padding},
                          {rect.x + rect.width - padding, rect.y + rect.height - padding},
                          palette.text);
        painter.draw_line({rect.x + rect.width - padding, rect.y + padding},
                          {rect.x + padding, rect.y + rect.height - padding}, palette.text);
    } else if (button == DecorationButton::Menu) {
        auto padding = 2.0f;
        auto spacing = 2.0f;
        auto y = rect.y + rect.height / 2.0f;
        painter.draw_line({rect.x + padding, y - spacing},
                          {rect.x + rect.width - padding, y - spacing}, palette.text);
        painter.draw_line({rect.x + padding, y}, {rect.x + rect.width - padding, y}, palette.text);
        painter.draw_line({rect.x + padding, y + spacing},
                          {rect.x + rect.width - padding, y + spacing}, palette.text);
    }
}

std::unique_ptr<Widget> GnomeTheme::create_title_bar(Window *window) const {
    // return std::make_unique<GnomeTitleBar>(window);
    auto p = std::make_unique<toolkit::GnomeTitleBar>(window);
    p->initializeTitleBar();
    return p;
}

namespace ThemeFactory {
std::unique_ptr<Theme> create(ThemeStyle style, ColorScheme scheme) {
    switch (style) {
    case ThemeStyle::System: {
        auto s = Theme::detect_system_style();
        return create(s, scheme);
    }
    case ThemeStyle::MacOS:
        return std::make_unique<MacOSTheme>(scheme);
    case ThemeStyle::Material:
        return std::make_unique<MaterialTheme>(scheme);
    case ThemeStyle::Win11:
        return std::make_unique<Win11Theme>(scheme);
    case ThemeStyle::Win95:
        return std::make_unique<Win95Theme>(scheme);
    case ThemeStyle::Plasma6:
        return std::make_unique<Plasma6Theme>(scheme);
    case ThemeStyle::GNOME:
        return std::make_unique<GnomeTheme>(scheme);
    }

    return std::make_unique<MaterialTheme>(scheme);
}
} // namespace ThemeFactory

} // namespace toolkit
