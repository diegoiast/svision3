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

#include <assert.h>
#include <memory>

namespace toolkit {

class GnomeTitleBar : public Widget {
  public:
    GnomeTitleBar(Window *window) : window_(window) { set_on_top(true); }

    void paint(Painter &painter) override {
        auto const &p = Theme::current().palette;
        auto r = rect_;
        auto active = window_->is_active();
        auto bg = active ? p.window : p.window_inactive.value_or(p.window);

        painter.fill_rect(r, bg);
        auto fm = painter.font_metrics(p.fonts.size);
        auto text_y = (r.height - fm.height) / 2.0f + fm.ascent;
        painter.draw_text(std::string{window_->title()}, {r.x + 10.0f, text_y}, p.text,
                          p.fonts.size);
    }

    bool handle_mouse(MouseEvent const &event) override {
        if (event.type == MouseEvent::Type::Press) {
            if (event.click_count == 2) {
                if (window_->is_maximized()) {
                    window_->restore();
                } else {
                    window_->maximize();
                }
                return true;
            }
            window_->start_system_move(event.serial);
            return true;
        }
        return false;
    }

    Size size_hint() const override {
        auto const &m = Theme::current().palette.window_decoration;
        return {100.0f, m.top};
    }

  private:
    Window *window_;
};

MaterialTheme::MaterialTheme(ColorScheme scheme, std::optional<Palette> p)
    : BaseTheme(scheme, std::move(p)) {
    if (!p) {
        palette = this->default_palette(scheme);
    }
    name = "Material";
    menu.padding = {4, 4, 4, 4};
    menubar.padding = {4, 12, 4, 12};
    slider.handle_size = 18.0f;
    slider.groove_thickness = 4.0f;
    focus_ring_margin = 3.0f;
    focus_ring_corner_radius = 3.0f;
    tab_widget.indicator_weight = 2.0f;
}

Palette MaterialTheme::default_palette(ColorScheme scheme) const {
    auto material_purple = Color::from_rgb(0x6750A4);
    Palette p = BaseTheme::default_palette(scheme);
    p.corner_radius = 4.0f;
    p.tab_radius = 0.0f;
    p.window_decoration = {32, 0, 0, 0};

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
        p.tab_radius = 4.0f;
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
        p.tab_radius = 4.0f;
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

    button.padding = {10, 16, 10, 16};

    slider.handle_size = 22.0f;
    slider.groove_thickness = 6.0f;

    focus_ring_margin = 2.0f;
    focus_ring_corner_radius = 2.0f;

    scrollbar.show_buttons = false;
    scrollbar.thickness = 6.0f;
    scrollbar.show_frame = false;
    scrollbar.padding = {0, 0, 0, 0};

    tab_widget.indicator_weight = 4.0f;
}

Palette GnomeTheme::default_palette(ColorScheme scheme) const {
    auto adwaita_color = Color::from_argb(0xFF3465A4);
    Palette p = BaseTheme::default_palette(scheme);
    p.corner_radius = 4.0f;
    p.inline_scrollbars = false;

    p.bottom_shadow = true;
    p.tab_radius = 4.0f;
    p.window_decoration = {32, 0, 0, 0};

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_rgb(0xe1dedb);
        p.base = Color::from_rgb(0xedecea);
        p.alternate = Color::from_rgb(0xf6f6f6);
        p.text = Color::from_rgb(0x2e3436);
        p.text_disabled = Color::from_rgb(0x888a85);
        p.placeholder = Color::from_rgb(0xaaaaaa);
        p.highlight = adwaita_color;
        p.highlighted_text = Color::from_rgb(0xffffff);
        p.border = Color::from_rgb(0xcbcbcb);
        p.accent = adwaita_color;
        p.link = adwaita_color;
        p.shadow = Color::rgba(1, 1, 1, 0.8f);     // Top inner highlight
        p.dark_shadow = Color::from_rgb(0xb0b0b0); // Bottom silver lines
        p.tooltip = Color::rgb(0.25f, 0.25f, 0.22f);
        p.background_pressed = Color::from_rgb(0xd6d6d1);
        p.background_hovered = Color::from_rgb(0xfdfdfd);
        p.tooltip = p.base;
        p.success = Color::from_rgb(0x2e7d32);
        p.warning = Color::from_rgb(0xfbc02d);
        p.error = Color::from_rgb(0xc62828);
        p.tab_select_background = Color::from_rgb(0xebe9e7);
        p.tab_background = p.window;
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
        p.dark_shadow = Color::from_argb(0x99000000);
        p.tooltip = Color::rgb(0.25f, 0.25f, 0.22f);
        p.background_pressed = Color::from_argb(0xFF484848);
        p.background_hovered = Color::from_argb(0xFF565656);
        p.tooltip = p.base;
        p.success = Color::from_argb(0xFF2E7D32);
        p.warning = Color::from_argb(0xFFFBC02D);
        p.error = Color::from_argb(0xFFC62828);
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        p.bottom_shadow = true;
        break;
    }
    return p;
}

std::unique_ptr<Widget> GnomeTheme::create_title_bar(Window *window) const {
    // return std::make_unique<GnomeTitleBar>(window);
    return BaseTheme::create_title_bar(window);
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
