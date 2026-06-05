// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/theme_win11.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/widget.hpp"
#include "toolkit/window.hpp"
#include "toolkit/window_title_bar.hpp"
#include <memory>

namespace toolkit {

class Win11TitleBar : public WindowTitleBar {
  public:
    using WindowTitleBar::WindowTitleBar;

    void initializeTitleBar() override {
        layout = new HBoxLayout();
        layout->set_spacing(0.0f);
        layout->set_margins({0, 0, 0, 0});

        title_label = new Label(std::string{window_->title()});
        title_label->set_alignment(Alignment::Start).set_shrinkable(true).set_elide(true);

/*        auto m = title_label->get_margins();
        m.left = 12.0f;
        title_label->set_margins(m);
*/
        layout->add_widget(std::unique_ptr<Label>(title_label), 1);

        auto const &decoration = Theme::current().palette.window_decoration;
        auto btn_size = Size{44, decoration.top};

        auto *min_btn = new TitlebarButton(DecorationButton::Minimize, "Minimize", btn_size);
        min_btn->on_click = [this] { window_->minimize(); };

        max_btn = new TitlebarButton(DecorationButton::Maximize, "Maximize", btn_size);
        max_btn->on_click = [this] {
            if (window_->is_maximized()) {
                window_->restore();
            } else {
                window_->maximize();
            }
        };

        auto *close_btn = new TitlebarButton(DecorationButton::Close, "Close", btn_size);
        close_btn->on_click = [this] { window_->close(); };

        layout->add_widget(std::unique_ptr<Widget>(min_btn));
        layout->add_widget(std::unique_ptr<Widget>(max_btn));
        layout->add_widget(std::unique_ptr<Widget>(close_btn));
    }

    void paint(Painter &painter) override {
        auto const &pal = Theme::current().palette;
        auto active = window_->is_active();
        auto bg = active ? pal.window : pal.window_inactive.value_or(pal.window);
        auto fg = active ? pal.text : pal.text_disabled;

        painter.fill_rect({0, 0, rect_.width, rect_.height}, bg);

        if (title_label) {
            title_label->set_text(std::string(window_->title()));
            title_label->set_color(fg);
        }

        layout->paint(painter);
    }
};

Win11Theme::Win11Theme(ColorScheme scheme, std::optional<Palette> p)
    : BaseTheme(scheme, std::move(p)) {
    if (!p) {
        palette = this->default_palette(scheme);
    }
    name = "Windows 11";
    focus_ring_margin = 3.0f;
    focus_ring_corner_radius = 4.0f;
    palette.corner_radius = 9.0f;

    button.padding = {5, 20, 5, 20};
    tab_widget.indicator_weight = {};
    radio.accent_fill = true;
}

std::unique_ptr<Widget> Win11Theme::create_title_bar(Window *window) const {
    auto b = std::make_unique<Win11TitleBar>(window);
    b->initializeTitleBar();
    return b;
}

Palette Win11Theme::default_palette(ColorScheme scheme) const {
    Palette p = BaseTheme::default_palette(scheme);
    auto windows_blue = Color::from_rgb(0x0078D4);
    p.tab_radius = 4.0f;
    p.corner_radius = 4.0f;
    p.chrome_lines = false;
    p.window_decoration = {32, 0, 0, 0};

    switch (scheme) {
    case ColorScheme::Light:
        p.window = Color::from_rgb(0xEEF4F9);
        p.window_inactive = Color::from_rgb(0xF3F3F3);
        p.base = Color::from_argb(0xFFFFFFFF);
        p.alternate = Color::from_argb(0xFFF9F9F9);
        p.text = Color::from_argb(0xFF000000);
        p.text_disabled = Color::from_argb(0xFF6D6D6D);
        p.placeholder = Color::from_argb(0xFF8A8A8A);
        p.highlight = windows_blue;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFFDCDCDC);
        p.accent = windows_blue;
        p.link = Color::from_argb(0xFF0067C0);
        p.shadow = Color::from_argb(0x20000000);
        p.dark_shadow = Color::from_argb(0x40000000);
        p.tooltip = Color::from_argb(0xFFFFFFFF);
        p.background_pressed = Color::from_argb(0xFFE5E5E5);
        p.background_hovered = Color::from_argb(0xFFEDEDED);
        p.success = Color::from_argb(0xFF107C10);
        p.warning = Color::from_argb(0xFFFFB900);
        p.error = Color::from_argb(0xFFD13438);
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        break;
    case ColorScheme::Dark:
        p.window = Color::from_argb(0xFF202020);
        p.base = Color::from_argb(0xFF2B2B2B);
        p.alternate = Color::from_argb(0xFF333333);
        p.text = Color::from_argb(0xFFFFFFFF);
        p.text_disabled = Color::from_argb(0xFF8A8A8A);
        p.placeholder = Color::from_argb(0xFF6D6D6D);
        p.highlight = windows_blue;
        p.highlighted_text = Color::from_argb(0xFFFFFFFF);
        p.border = Color::from_argb(0xFF3C3C3C);
        p.accent = windows_blue;
        p.link = Color::from_argb(0xFF4CC2FF);
        p.shadow = Color::from_argb(0x66000000);
        p.dark_shadow = Color::from_argb(0x99000000);
        p.tooltip = Color::from_argb(0xFF2B2B2B);
        p.background_pressed = Color::from_argb(0xFF3A3A3A);
        p.background_hovered = Color::from_argb(0xFF444444);
        p.success = Color::from_argb(0xFF6CCB5F);
        p.warning = Color::from_argb(0xFFFFC83D);
        p.error = Color::from_argb(0xFFFF5F5F);
        p.tab_select_background = p.base;
        p.tab_background = p.window;
        break;
    }
    return p;
}

void Win11Theme::draw_menubar_item(Painter &painter, Rect const &rect, std::string_view title,
                                   bool hovered, bool active, bool show_mnemonics,
                                   int mnemonic_index) const {
    auto const &style = menubar;
    auto padding = style.padding;
    auto fm = painter.font_metrics(palette.fonts.size);

    if (hovered || active) {
        Color bg = palette.highlight;
        auto hover_rect = rect.inset(2.0f);
        painter.fill_rounded_rect(hover_rect, bg, palette.corner_radius);
    }

    auto baseline = (rect.height - fm.height) / 2.0f + fm.ascent;
    auto text_c = palette.text;
    if (hovered || active) {
        text_c = palette.highlighted_text;
    }
    painter.draw_text(title, {rect.x + padding.left, baseline}, text_c, palette.fonts.size);
}

void Win11Theme::draw_tree_item(Painter &painter, Rect const &rect, std::string_view text,
                                int depth, bool has_children, bool expanded, bool selected,
                                bool hovered, bool alternate) const {
    auto const &style = tree_view;
    auto fm = painter.font_metrics(palette.fonts.size);
    auto indent = style.indent;

    auto x_offset = style.item_padding_h + depth * indent;

    if (has_children) {
        auto arrow_x = x_offset + 4;
        auto arrow_y = rect.y + rect.height / 2;
        auto arrow_size = 8.0f;

        if (expanded) {
            painter.fill_triangle({arrow_x, arrow_y - arrow_size / 2},
                                  {arrow_x + arrow_size, arrow_y},
                                  {arrow_x, arrow_y + arrow_size / 2}, palette.text);
        } else {
            painter.fill_triangle({arrow_x, arrow_y - arrow_size / 2},
                                  {arrow_x + arrow_size, arrow_y - arrow_size / 2},
                                  {arrow_x + arrow_size / 2, arrow_y + arrow_size / 2},
                                  palette.text);
        }
        x_offset += indent;
    }

    auto text_y = rect.y + (rect.height - fm.height) / 2.0f + fm.ascent;
    auto text_col = selected ? palette.accent : palette.text;
    painter.draw_text(text, {x_offset, text_y}, text_col, palette.fonts.size);
}

void Win11Theme::draw_tab_content_background(Painter &painter, Rect const &rect) const {
    painter.fill_rect(rect, palette.base);
}

void Win11Theme::draw_window_button(Painter &painter, Rect const &rect, DecorationButton button,
                                    WidgetState const &state) const {
    auto active = state.window_active;
    auto hovered = state.interaction == ButtonState::Hovered;
    auto pressed = state.interaction == ButtonState::ClickedInside;

    if (hovered || pressed) {
        Color bg;
        if (button == DecorationButton::Close) {
            bg = pressed ? Color::from_rgb(0xC42B1C) : Color::from_rgb(0xE81123);
        } else {
            if (pressed) {
                bg = palette.background_pressed.value_or(palette.base);
            } else {
                bg = palette.background_hovered.value_or(
                    palette.background_pressed.value_or(palette.base));
            }
        }
        painter.fill_rect(rect, bg);
    }

    auto symbol_c = palette.text;
    if (button == DecorationButton::Close && (hovered || pressed)) {
        symbol_c = Color::from_rgb(0xFFFFFF);
    } else if (!active) {
        symbol_c = palette.text_disabled;
    }

    auto center = Point{rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};
    auto s = 5.0f; // Half-size of the symbol

    switch (button) {
    case DecorationButton::Close:
        painter.draw_line({center.x - s, center.y - s}, {center.x + s, center.y + s}, symbol_c,
                          1.0f);
        painter.draw_line({center.x + s, center.y - s}, {center.x - s, center.y + s}, symbol_c,
                          1.0f);
        break;
    case DecorationButton::Minimize:
        painter.draw_line({center.x - s, center.y}, {center.x + s, center.y}, symbol_c, 1.0f);
        break;
    case DecorationButton::Maximize:
        painter.draw_rect({center.x - s, center.y - s, s * 2, s * 2}, symbol_c, 1.0f);
        break;
    case DecorationButton::Restore: {
        auto r1 = Rect{center.x - s + 2, center.y - s, s * 2 - 2, s * 2 - 2};
        auto r2 = Rect{center.x - s, center.y - s + 2, s * 2 - 2, s * 2 - 2};
        // Draw the back one first (r1)
        painter.draw_rect(r1, symbol_c, 1.0f);
        painter.draw_rect(r2, symbol_c, 1.0f);
        break;
    }
    case DecorationButton::Menu:
        // Windows 11 usually doesn't have a menu button in the title bar, but if it does:
        painter.draw_line({center.x - s, center.y - 3}, {center.x + s, center.y - 3}, symbol_c,
                          1.0f);
        painter.draw_line({center.x - s, center.y}, {center.x + s, center.y}, symbol_c, 1.0f);
        painter.draw_line({center.x - s, center.y + 3}, {center.x + s, center.y + 3}, symbol_c,
                          1.0f);
        break;
    }
}

} // namespace toolkit
