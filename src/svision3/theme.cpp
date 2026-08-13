// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/theme.hpp"
#include "svision3/application.hpp"
#include "svision3/painter.hpp"
#include "svision3/platform.hpp"
#include "svision3/theme_factory.hpp"
#include "svision3/theme_macos.hpp"
#include "svision3/theme_plasma.hpp"
#include "svision3/theme_win11.hpp"
#include "svision3/theme_win95.hpp"
#include "svision3/types.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <spdlog/spdlog.h>

#include <vector>

namespace svision3 {

static std::vector<std::function<void(const Theme &)>> &get_theme_observers() {
    static std::vector<std::function<void(const Theme &)>> observers;
    return observers;
}

void Theme::add_theme_observer(std::function<void(const Theme &)> observer) {
    get_theme_observers().push_back(std::move(observer));
}

void Theme::notify_theme_changed() {
    auto const &t = current();
    for (auto &observer : get_theme_observers()) {
        observer(t);
    }
    if (Application::has_instance()) {
        Application::instance().notify_theme_changed();
    }
}

static std::unique_ptr<Theme> &mutable_current_ptr() {
    static std::unique_ptr<Theme> instance;
    if (!instance) {
        instance = ThemeFactory::create(ThemeStyle::System);
#ifdef _WIN32
        if (instance) {
            instance->style.shadow.size = 0;
        }
#endif
    }
    return instance;
}

const Theme &Theme::current() { return *mutable_current_ptr(); }

const char *Theme::style_name(ThemeStyle style) {
    switch (style) {
    case ThemeStyle::System:
        return "System";
    case ThemeStyle::MacOS:
        return "macOS";
    case ThemeStyle::Material:
        return "Material";
    case ThemeStyle::Win11:
        return "Windows 11";
    case ThemeStyle::Win95:
        return "Windows 95";
    case ThemeStyle::Plasma6:
        return "Plasma 6";
    case ThemeStyle::GNOME:
        return "GNOME";
    default:
        return "Unknown";
    }
}

ThemeStyle Theme::detect_system_style() {
    switch (detect_desktop_environment()) {
    case DesktopEnvironment::MacOS:
        return ThemeStyle::MacOS;
    case DesktopEnvironment::Windows11:
        return ThemeStyle::Win11;
    case DesktopEnvironment::GNOME:
        return ThemeStyle::GNOME;
    case DesktopEnvironment::Plasma:
        return ThemeStyle::Plasma6;
    case DesktopEnvironment::Unknown:
        return ThemeStyle::Material;
    }
    return ThemeStyle::Material;
}

void Theme::set_current(std::unique_ptr<Theme> theme) {
#ifdef _WIN32
    if (theme) {
        theme->style.shadow.size = 0;
    }
#endif
    mutable_current_ptr() = std::move(theme);
    notify_theme_changed();
}

void Theme::init_fonts(Palette &p) {
    p.fonts.system = "sans-serif";
    p.fonts.monospace = "monospace";
    p.fonts.size = 14.0f;
    p.fonts.auto_repeat_delay = 0.5f;
    p.fonts.auto_repeat_interval = 0.4f;

    if (auto *plat = detail::current_platform()) {
        auto sf = plat->system_fonts();
        if (!sf.system.empty()) {
            p.fonts.system = sf.system;
        }
        if (!sf.monospace.empty()) {
            p.fonts.monospace = sf.monospace;
        }
        if (sf.size > 0) {
            p.fonts.size = std::floor(sf.size * (96.0f / 72.0f));
        }
        if (sf.auto_repeat_delay > 0) {
            p.fonts.auto_repeat_delay = sf.auto_repeat_delay;
        }
        if (sf.auto_repeat_interval > 0) {
            p.fonts.auto_repeat_interval = sf.auto_repeat_interval;
        }
    }
    p.auto_repeat_delay = p.fonts.auto_repeat_delay;
    p.auto_repeat_interval = p.fonts.auto_repeat_interval;
}

void Theme::draw_focus_ring_for_widget(Painter &painter, Widget const *widget) const {
    if (!widget) {
        return;
    }

    auto global_x = 0.0f;
    auto global_y = 0.0f;
    auto const *w = widget;
    while (w) {
        global_x += w->rect().x;
        global_y += w->rect().y;
        w = w->parent();
    }

    auto margin = style.ringFocus.margin;
    auto r = Rect{global_x - margin, global_y - margin, widget->rect().width + margin * 2,
                  widget->rect().height + margin * 2};
    auto corner_radius = style.corner_radius + style.ringFocus.corner_radius;

    painter.set_line_style(style.ringFocus.line_style);
    draw_focus_ring(painter, r, corner_radius);
    painter.set_line_style(Painter::LineStyle::Solid);
}

} // namespace svision3
