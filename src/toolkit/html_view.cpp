// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/html_view.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#include <litehtml/litehtml.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace toolkit {

static Color lh_color(litehtml::web_color c) {
    return Color::rgba(c.red / 255.0f, c.green / 255.0f, c.blue / 255.0f, c.alpha / 255.0f);
}

static Rect lh_rect(litehtml::position const &p) {
    return {static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.width),
            static_cast<float>(p.height)};
}

static toolkit::FontFamily lh_font_family(const char *face) {
    if (!face) {
        return FontFamily::System;
    }
    std::string f(face);
    std::transform(f.begin(), f.end(), f.begin(), ::tolower);
    if (f.find("mono") != std::string::npos || f.find("courier") != std::string::npos ||
        f.find("consolas") != std::string::npos) {
        return FontFamily::Monospace;
    }
    return FontFamily::System;
}

struct FontHandle {
    float size;
    toolkit::FontFamily family;
    int decoration; // litehtml::text_decoration flags
};

class LitehtmlContainer : public litehtml::document_container {
  public:
    explicit LitehtmlContainer(HtmlView *view) : view_(view) {}

    litehtml::uint_ptr create_font(const char *faceName, int size, int /*weight*/,
                                   litehtml::font_style /*italic*/, unsigned int decoration,
                                   litehtml::font_metrics *fm) override {
        auto *handle = new FontHandle{static_cast<float>(size), lh_font_family(faceName),
                                      static_cast<int>(decoration)};
        if (fm) {
            auto m = view_->font_metrics(handle->size, handle->family);
            fm->height = static_cast<int>(m.height);
            fm->ascent = static_cast<int>(m.ascent);
            fm->descent = static_cast<int>(m.descent);
            fm->x_height = static_cast<int>(m.ascent * 0.5f);
            fm->draw_spaces = true;
        }
        return reinterpret_cast<litehtml::uint_ptr>(handle);
    }

    void delete_font(litehtml::uint_ptr hFont) override {
        delete reinterpret_cast<FontHandle *>(hFont);
    }

    int text_width(const char *text, litehtml::uint_ptr hFont) override {
        if (!text || !hFont) {
            return 0;
        }
        auto *handle = reinterpret_cast<FontHandle *>(hFont);
        auto w = view_->measure_text(text, handle->size, handle->family).width;

        // Cairo text_extents measures ink bounds, not advance — spaces return 0.
        // Fall back to a proportional estimate when the result is zero.
        if (w < 0.5f) {
            auto all_spaces = true;
            for (auto p = text; *p; ++p) {
                if (*p != ' ' && *p != '\t') {
                    all_spaces = false;
                    break;
                }
            }

            // FIXME: what is this 0.28f?
            if (all_spaces) {
                w = handle->size * 0.28f * std::strlen(text);
            }
        }
        return static_cast<int>(w);
    }

    void draw_text(litehtml::uint_ptr hdc, const char *text, litehtml::uint_ptr hFont,
                   litehtml::web_color color, litehtml::position const &pos) override {
        if (!hdc || !text || !hFont) {
            return;
        }
        auto painter = reinterpret_cast<Painter *>(hdc);
        auto handle = reinterpret_cast<FontHandle *>(hFont);
        auto fm = painter->font_metrics(handle->size, handle->family);
        float baseline_y = static_cast<float>(pos.y) + fm.ascent;
        painter->draw_text(text, {static_cast<float>(pos.x), baseline_y}, lh_color(color),
                           handle->size, handle->family);

        if (handle->decoration & litehtml::font_decoration_underline) {
            float uy = static_cast<float>(pos.y) + fm.height;
            float uw = static_cast<float>(text_width(text, hFont));
            float px = static_cast<float>(pos.x);
            painter->draw_line({px, uy}, {px + uw, uy}, lh_color(color), 1.0f);
        }
        if (handle->decoration & litehtml::font_decoration_linethrough) {
            float ly = static_cast<float>(pos.y) + fm.height * 0.5f;
            float lw = static_cast<float>(text_width(text, hFont));
            float px = static_cast<float>(pos.x);
            painter->draw_line({px, ly}, {px + lw, ly}, lh_color(color), 1.0f);
        }
    }

    int pt_to_px(int pt) const override {
        // FIXME: add support for other DPI
        // 96 dpi: px = pt * 96 / 72
        return pt * 96 / 72;
    }

    int get_default_font_size() const override {
        return static_cast<int>(Theme::current().palette.fonts.size);
    }

    // FIXME: use the theme's default font
    const char *get_default_font_name() const override { return "sans-serif"; }

    // ── backgrounds & borders ────────────────────────────────────────────────

    void draw_background(litehtml::uint_ptr hdc,
                         std::vector<litehtml::background_paint> const &bg) override {
        if (!hdc || bg.empty()) {
            return;
        }
        auto *painter = reinterpret_cast<Painter *>(hdc);

        for (auto it = bg.rbegin(); it != bg.rend(); ++it) {
            auto const &layer = *it;
            if (layer.color.alpha > 0) {
                painter->fill_rect(lh_rect(layer.clip_box), lh_color(layer.color));
            }

            // FIXME: Images are not supported in this minimal implementation.
        }
    }

    void draw_borders(litehtml::uint_ptr hdc, litehtml::borders const &borders,
                      litehtml::position const &pos, bool root) override {
        if (!hdc || root) {
            return;
        }
        auto *painter = reinterpret_cast<Painter *>(hdc);

        auto draw_side = [&](litehtml::border const &b, Point from, Point to) {
            if (b.width <= 0 || b.style == litehtml::border_style_none ||
                b.style == litehtml::border_style_hidden) {
                return;
            }
            auto c = lh_color(b.color);
            auto w = static_cast<float>(b.width);
            if (b.style == litehtml::border_style_dotted) {
                painter->set_line_style(Painter::LineStyle::Dotted);
            } else if (b.style == litehtml::border_style_dashed) {
                painter->set_line_style(Painter::LineStyle::Dashed);
            } else {
                painter->set_line_style(Painter::LineStyle::Solid);
            }
            painter->draw_line(from, to, c, w);
            painter->set_line_style(Painter::LineStyle::Solid);
        };

        auto x = pos.x, y = pos.y, r = pos.right(), b = pos.bottom();
        draw_side(borders.top, {x, y}, {r, y});
        draw_side(borders.right, {r, y}, {r, b});
        draw_side(borders.bottom, {x, b}, {r, b});
        draw_side(borders.left, {x, y}, {x, b});
    }

    void draw_list_marker(litehtml::uint_ptr hdc, litehtml::list_marker const &marker) override {
        if (!hdc) {
            return;
        }
        auto painter = reinterpret_cast<Painter *>(hdc);
        auto c = lh_color(marker.color);
        auto cx = marker.pos.x + marker.pos.width * 0.5f;
        auto cy = marker.pos.y + marker.pos.height * 0.5f;
        auto r = marker.pos.width * 0.5f;

        switch (marker.marker_type) {
        case litehtml::list_style_type_disc:
            painter->fill_circle({cx, cy}, r, c);
            break;
        case litehtml::list_style_type_circle:
            painter->draw_circle({cx, cy}, r, c);
            break;
        case litehtml::list_style_type_square:
            painter->fill_rect(lh_rect(marker.pos), c);
            break;
        default:
            // FIXME: Numeric markers would require rendering text; skip for now.
            break;
        }
    }

    void load_image(const char * /*src*/, const char * /*baseurl*/,
                    bool /*redraw_on_ready*/) override {
        // FIXME: add support for images
    }

    void get_image_size(const char * /*src*/, const char * /*baseurl*/,
                        litehtml::size &sz) override {
        // FIXME: add support for images
        sz.width = 0;
        sz.height = 0;
    }

    void set_clip(litehtml::position const &pos,
                  litehtml::border_radiuses const & /*bdr_radius*/) override {
        clip_stack_.push_back(pos);
        if (current_painter_) {
            current_painter_->push_clip(lh_rect(pos));
        }
    }

    void del_clip() override {
        if (!clip_stack_.empty()) {
            clip_stack_.pop_back();
            if (current_painter_) {
                current_painter_->pop_clip();
            }
        }
    }

    void set_caption(const char * /*caption*/) override {}

    void set_base_url(const char *base_url) override {
        if (base_url) {
            view_->base_url_ = base_url;
        }
    }

    void link(std::shared_ptr<litehtml::document> const & /*doc*/,
              litehtml::element::ptr const & /*el*/) override {}

    void on_anchor_click(const char *url, litehtml::element::ptr const & /*el*/) override {
        if (url && view_->on_link_click) {
            view_->on_link_click(url);
        }
    }

    void set_cursor(const char *cursor) override {
        auto shape = CursorShape::Arrow;
        if (cursor) {
            auto s = std::string_view(cursor);
            if (s == "pointer") {
                shape = CursorShape::Hand;
            } else if (s == "text" || s == "vertical-text") {
                shape = CursorShape::IBeam;
            } else if (s == "not-allowed" || s == "no-drop") {
                shape = CursorShape::NotAllowed;
            } else if (s == "ew-resize" || s == "col-resize") {
                shape = CursorShape::ResizeEW;
            }
        }
        view_->cursor_shape_ = shape;
    }

    void transform_text(litehtml::string &text, litehtml::text_transform tt) override {
        switch (tt) {
        case litehtml::text_transform_uppercase:
            std::transform(text.begin(), text.end(), text.begin(), ::toupper);
            break;
        case litehtml::text_transform_lowercase:
            std::transform(text.begin(), text.end(), text.begin(), ::tolower);
            break;
        case litehtml::text_transform_capitalize:
            if (!text.empty()) {
                text[0] = static_cast<char>(::toupper(static_cast<unsigned char>(text[0])));
            }
            break;
        default:
            break;
        }
    }

    void import_css(litehtml::string & /*text*/, litehtml::string const & /*url*/,
                    litehtml::string & /*baseurl*/) override {
        // FIXME: add suppport. Does it mean we need  network support?
    }

    litehtml::element::ptr
    create_element(const char * /*tag_name*/, litehtml::string_map const & /*attributes*/,
                   std::shared_ptr<litehtml::document> const & /*doc*/) override {
        // FIXME: add support for this
        return nullptr;
    }

    void get_client_rect(litehtml::position &client) const override {
        client.x = 0;
        client.y = 0;
        client.width = static_cast<int>(view_->rect().width);
        client.height = static_cast<int>(view_->rect().height);
    }

    void get_media_features(litehtml::media_features &media) const override {
        media.type = litehtml::media_type_screen;
        media.width = static_cast<int>(view_->rect().width);
        media.height = static_cast<int>(view_->rect().height);
        media.device_width = media.width;
        media.device_height = media.height;
        media.color = 8;
        media.resolution = 96;
    }

    void get_language(litehtml::string &language, litehtml::string &culture) const override {
        // FIXME: should we hook this to the locale of the system?
        language = "en";
        culture = "";
    }

    Painter *current_painter_ = nullptr;

  private:
    HtmlView *view_;
    std::vector<litehtml::position> clip_stack_;
};

HtmlView::HtmlView() : container_(std::make_unique<LitehtmlContainer>(this)) {}

HtmlView::~HtmlView() = default;

void HtmlView::set_html(std::string const &html, std::string const &base_url) {
    html_ = html;
    base_url_ = base_url;
    document_ = litehtml::document::createFromString(html_.c_str(), container_.get());
    relayout();
    invalidate_layout();
}

void HtmlView::relayout() {
    if (!document_) {
        return;
    }
    auto w = rect_.width > 0 ? static_cast<int>(rect_.width) : 800;
    document_->render(w);
}

void HtmlView::set_rect(Rect const &r) {
    auto width_changed = (r.width != rect_.width);
    Widget::set_rect(r);
    if (width_changed && document_) {
        relayout();
    }
}

void HtmlView::paint(Painter &painter) {
    container_->current_painter_ = &painter;
    if (document_) {
        litehtml::position clip{0, 0, static_cast<int>(rect_.width),
                                static_cast<int>(rect_.height)};
        document_->draw(reinterpret_cast<litehtml::uint_ptr>(&painter), 0, 0, &clip);
    }
    container_->current_painter_ = nullptr;
}

bool HtmlView::handle_mouse(MouseEvent const &event) {
    if (!document_) {
        return false;
    }
    auto x = static_cast<int>(event.position.x);
    auto y = static_cast<int>(event.position.y);

    litehtml::position::vector redraw_boxes;
    switch (event.type) {
    case MouseEvent::Type::Move:
    case MouseEvent::Type::Drag:
        document_->on_mouse_over(x, y, x, y, redraw_boxes);
        break;
    case MouseEvent::Type::Press:
        if (event.button == 0) {
            document_->on_lbutton_down(x, y, x, y, redraw_boxes);
        }
        break;
    case MouseEvent::Type::Release:
        if (event.button == 0) {
            document_->on_lbutton_up(x, y, x, y, redraw_boxes);
        }
        break;
    case MouseEvent::Type::Leave:
        document_->on_mouse_leave(redraw_boxes);
        break;
    default:
        break;
    }
    if (window_) {
        window_->request_redraw("html mouse");
    }
    return false;
}

Size HtmlView::size_hint() const {
    if (document_) {
        return {static_cast<float>(document_->width()), static_cast<float>(document_->height())};
    }
    return {0, 0};
}

void HtmlView::on_theme_changed() {
    Widget::on_theme_changed();
    if (document_) {
        relayout();
    }
}

} // namespace toolkit
