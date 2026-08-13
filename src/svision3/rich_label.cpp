// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/rich_label.hpp"
#include "svision3/theme.hpp"
#include "svision3/utf8.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/fmt/fmt.h>

namespace svision3 {

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

nlohmann::json RichLabel::to_json() const {
    auto j = Widget::to_json();
    j["text"] = text_;
    return j;
}

void RichLabel::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("text")) {
        set_markdown(j["text"]);
    }
}

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

} // namespace svision3
