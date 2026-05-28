// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme.hpp"
#include "toolkit/application.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/theme_factory.hpp"
#include "toolkit/theme_macos.hpp"
#include "toolkit/theme_plasma.hpp"
#include "toolkit/theme_win11.hpp"
#include "toolkit/theme_win95.hpp"
#include "toolkit/types.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <spdlog/spdlog.h>

#include <vector>

namespace toolkit {

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
#if defined(__APPLE__)
    return ThemeStyle::MacOS;
#elif defined(_WIN32)
    return ThemeStyle::Win11;
#else
    const char *xdg = std::getenv("XDG_CURRENT_DESKTOP");
    if (xdg) {
        std::string s(xdg);
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (s.find("gnome") != std::string::npos) {
            return ThemeStyle::GNOME;
        }
        if (s.find("kde") != std::string::npos || s.find("plasma") != std::string::npos) {
            return ThemeStyle::Plasma6;
        }
    }
    return ThemeStyle::Material;
#endif
}

void Theme::set_current(std::unique_ptr<Theme> theme) {
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

    auto margin = focus_ring_margin;
    auto r = Rect{global_x - margin, global_y - margin, widget->rect().width + margin * 2,
                  widget->rect().height + margin * 2};
    auto corner_radius = palette.corner_radius + focus_ring_corner_radius;

    painter.set_line_style(focus_ring_line_style);
    draw_focus_ring(painter, r, corner_radius);
    painter.set_line_style(Painter::LineStyle::Solid);
}

} // namespace toolkit
