// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/html_view.hpp"
#include <string>

namespace svision3 {

class RichLabel : public HtmlView, public Fluent<RichLabel> {
    DECLARE_WIDGET(RichLabel)
  public:
    RichLabel();
    explicit RichLabel(std::string text);

    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    RichLabel &set_text(std::string const &text);
    RichLabel &set_markdown(std::string const &markdown);
    std::string const &text() const { return text_; }
    void on_theme_changed() override;

  private:
    std::string text_;
};

} // namespace svision3
