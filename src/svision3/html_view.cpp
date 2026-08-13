// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/html_view.hpp"
#include "svision3/painter.hpp"
#include "svision3/theme.hpp"
#include "svision3/window.hpp"

#include <litehtml/litehtml.h>
#include <md4c-html.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

// clang-format off
// CSS braces escaped as {{ }} for fmt::format.
static constexpr auto MARKDOWN_PAGE = R"(<!DOCTYPE html>
<html><head><style>
body{{font-family:sans-serif;margin:{margin}px;{max_w}background:{bg};color:{fg};line-height:1.6}}
h1{{font-size:1.6em;margin-top:0.8em;margin-bottom:0.25em;border-bottom:1px solid rgba(128,128,128,0.3);padding-bottom:0.2em}}
h2{{font-size:1.3em;margin-top:0.8em;margin-bottom:0.25em}}
h3{{font-size:1.1em;margin-top:0.8em;margin-bottom:0.25em}}
a{{color:{link}}}
code{{font-family:monospace;background:rgba(128,128,128,0.15);padding:1px 4px;border-radius:3px}}
pre{{background:rgba(128,128,128,0.15);padding:10px;border-radius:4px;overflow-x:auto}}
pre code{{background:none;padding:0}}
blockquote{{border-left:3px solid rgba(128,128,128,0.4);margin:0;padding-left:12px;opacity:0.8}}
table{{border-collapse:collapse;margin:8px 0}}
th,td{{border:1px solid rgba(128,128,128,0.4);padding:4px 8px}}
th{{background:rgba(128,128,128,0.1)}}
ul,ol{{padding-left:24px}}li{{margin-bottom:2px}}
</style></head><body>{body}</body></html>)";
// clang-format on

static std::string css_color(svision3::Color c) {
    return fmt::format("rgba({},{},{},{:.2f})", static_cast<int>(c.r * 255),
                       static_cast<int>(c.g * 255), static_cast<int>(c.b * 255), c.a);
}

namespace svision3 {

static Color lh_color(litehtml::web_color c) {
    return Color::rgba(c.red / 255.0f, c.green / 255.0f, c.blue / 255.0f, c.alpha / 255.0f);
}

static Rect lh_rect(litehtml::position const &p) {
    return {static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.width),
            static_cast<float>(p.height)};
}

static svision3::FontFamily lh_font_family(const char *face) {
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
    svision3::FontFamily family;
    int decoration; // litehtml::text_decoration flags
    bool bold;
    bool italic;
};

class LitehtmlContainer : public litehtml::document_container {
  public:
    explicit LitehtmlContainer(HtmlView *view) : view_(view) {}

    litehtml::uint_ptr create_font(const char *faceName, int size, int weight,
                                   litehtml::font_style style, unsigned int decoration,
                                   litehtml::font_metrics *fm) override {
        auto *handle = new FontHandle{static_cast<float>(size), lh_font_family(faceName),
                                      static_cast<int>(decoration), weight >= 700,
                                      style == litehtml::font_style_italic};
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
        auto w =
            view_->measure_text(text, handle->size, handle->family, handle->bold, handle->italic)
                .width;

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
        float baseline_y = std::round(static_cast<float>(pos.y) + fm.ascent);
        painter->draw_text(text, {std::round(static_cast<float>(pos.x)), baseline_y},
                           lh_color(color), handle->size, handle->family,
                           Painter::TextOrientation::Horizontal, handle->bold, handle->italic);

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

    float screen_dpi() const {
        auto scale = view_->window() ? view_->window()->scale_factor() : 1.0f;
        return 96.0f * scale;
    }

    int pt_to_px(int pt) const override {
        return static_cast<int>(std::round(pt * screen_dpi() / 72.0f));
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
        draw_side(borders.top, {static_cast<float>(x), static_cast<float>(y)},
                  {static_cast<float>(r), static_cast<float>(y)});

        draw_side(borders.right, {static_cast<float>(r), static_cast<float>(y)},
                  {static_cast<float>(r), static_cast<float>(b)});

        draw_side(borders.bottom, {static_cast<float>(x), static_cast<float>(b)},
                  {static_cast<float>(r), static_cast<float>(b)});

        draw_side(borders.left, {static_cast<float>(x), static_cast<float>(y)},
                  {static_cast<float>(x), static_cast<float>(b)});
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
        media.resolution = static_cast<int>(screen_dpi());
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

void HtmlView::load_html_(std::string html, std::string base_url) {
    html_ = std::move(html);
    base_url_ = std::move(base_url);
    document_ = litehtml::document::createFromString(html_.c_str(), container_.get());
    relayout();
    invalidate_layout();
}

void HtmlView::set_html(std::string const &html, std::string const &base_url) {
    markdown_ = {};
    load_html_(html, base_url);
}

void HtmlView::set_markdown(std::string const &markdown) {
    markdown_ = markdown;

    std::string body;
    auto append = [](const MD_CHAR *text, MD_SIZE size, void *userdata) {
        static_cast<std::string *>(userdata)->append(text, size);
    };
    md_html(markdown.c_str(), static_cast<MD_SIZE>(markdown.size()), append, &body,
            MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS | MD_FLAG_TABLES, 0);

    auto const &pal = Theme::current().palette;
    auto bg = background_color_.value_or(pal.window);
    if (!markdown_css_light_.empty()) {
        auto lum = 0.299f * bg.r + 0.587f * bg.g + 0.114f * bg.b;
        auto const &css =
            (lum < 0.5f && !markdown_css_dark_.empty()) ? markdown_css_dark_ : markdown_css_light_;
        auto page =
            std::string(
                "<!DOCTYPE html><html><head><style>html,body{margin:0;padding:0;background:") +
            css_color(bg) + "}" + css + "</style></head><body><div class=\"markdown-body\">" +
            body + "</div></body></html>";
        load_html_(std::move(page), {});
    } else {
        auto max_w_css = content_max_width_ > 0
                             ? fmt::format("width:max-content;max-width:{}px;", content_max_width_)
                             : std::string{};
        auto generated_html = fmt::format(MARKDOWN_PAGE, fmt::arg("bg", css_color(bg)),
                                          fmt::arg("fg", css_color(pal.text)),
                                          fmt::arg("link", css_color(Color::rgb(0.0f, 0.4f, 0.8f))),
                                          fmt::arg("margin", content_margin_),
                                          fmt::arg("max_w", max_w_css), fmt::arg("body", body));
        load_html_(generated_html, {});
    }
}

void HtmlView::set_css(std::string light_css, std::string dark_css) {
    markdown_css_light_ = std::move(light_css);
    markdown_css_dark_ = std::move(dark_css);
    if (!markdown_.empty()) {
        set_markdown(markdown_);
    }
}

void HtmlView::relayout() {
    if (!document_) {
        return;
    }
    auto bw = content_inset();
    auto inner_w = rect_.width > 2 * bw ? rect_.width - 2 * bw : rect_.width;
    auto w = inner_w > 0 ? static_cast<int>(inner_w) : 800;
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
        auto bw = content_inset();
        auto inner_w = rect_.width - 2 * bw;
        auto inner_h = rect_.height - 2 * bw;
        painter.push_translation({bw, bw});
        litehtml::position clip{0, 0, static_cast<int>(inner_w), static_cast<int>(inner_h)};
        document_->draw(reinterpret_cast<litehtml::uint_ptr>(&painter), 0, 0, &clip);
        painter.pop_translation();
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

float HtmlView::content_width() const {
    if (document_) {
        return static_cast<float>(document_->content_width());
    }
    return 0;
}

Size HtmlView::size_hint() const {
    if (document_) {
        auto bw = content_inset();
        // Returning 0 width allows the layout system to shrink this widget as much as it wants.
        // The HTML document will re-render to this new width on set_rect.
        return {0.0f, static_cast<float>(document_->height()) + 2 * bw};
    }
    return {0, 0};
}

void HtmlView::on_theme_changed() {
    Widget::on_theme_changed();
    if (!markdown_.empty()) {
        set_markdown(markdown_);
        if (window_) {
            window_->request_redraw("markdown theme changed");
        }
    } else if (document_) {
        relayout();
    }
}

} // namespace svision3
