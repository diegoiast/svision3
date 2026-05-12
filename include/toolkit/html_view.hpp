// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <functional>
#include <memory>
#include <string>

namespace litehtml {
class document;
}

namespace toolkit {

class LitehtmlContainer;

class HtmlView : public Widget {
  public:
    HtmlView();
    ~HtmlView() override;

    void set_html(std::string const &html, std::string const &base_url = "");
    void set_markdown(std::string const &markdown);
    void set_css(std::string light_css, std::string dark_css = {});
    std::string const &html() const { return html_; }

    std::function<void(std::string const &)> on_link_click;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    CursorShape cursor() const override { return cursor_shape_; }
    Size size_hint() const override;
    void on_theme_changed() override;
    void set_rect(Rect const &rect) override;

  private:
    friend class LitehtmlContainer;
    std::string html_;
    std::string base_url_;
    std::string markdown_;
    std::string markdown_css_light_;
    std::string markdown_css_dark_;
    CursorShape cursor_shape_ = CursorShape::Arrow;
    std::unique_ptr<LitehtmlContainer> container_;
    std::shared_ptr<litehtml::document> document_;

    void load_html_(std::string html, std::string base_url);
    void relayout();
};

} // namespace toolkit
