// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/rich_label.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/utf8.hpp"

#include <spdlog/fmt/fmt.h>

namespace toolkit {

static std::string build_css() {
    auto fs = Theme::current().palette.fonts.size;
    return fmt::format(
        "html,body{{background:transparent;margin:0;padding:0;color:inherit;font-size:{:.0f}px;}}"
        "code{{font-family:monospace;background:rgba(128,128,128,0.15);padding:1px 4px;}}",
        fs);
}

RichLabel::RichLabel() {
    set_draw_frame(false);
    set_css(build_css());
}

RichLabel::RichLabel(std::string text) : RichLabel() { set_markdown(text); }

RichLabel &RichLabel::set_text(std::string const &text) {
    text_ = text;
    auto fs = static_cast<int>(Theme::current().palette.fonts.size);
    set_html(fmt::format("<html><body style='margin:0;padding:0;font-size:{}px'>"
                         "<p style='margin:0'>{}</p></body></html>",
                         fs, html_escape(text)));
    return *this;
}

RichLabel &RichLabel::set_markdown(std::string const &markdown) {
    text_ = markdown;
    set_css(build_css());
    HtmlView::set_markdown(markdown);
    return *this;
}

void RichLabel::on_theme_changed() {
    set_css(build_css());
    HtmlView::on_theme_changed();
}

} // namespace toolkit
