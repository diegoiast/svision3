// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/button.hpp"
#include "svision3/label.hpp"
#include "svision3/layout.hpp"
#include "svision3/widget.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace svision3 {

class ToastWidget;

class ToastBuilder {
  public:
    ToastBuilder &text(std::string t);
    ToastBuilder &rich_text(std::string t);
    ToastBuilder &title(std::string t);
    ToastBuilder &background(Color c);
    ToastBuilder &timeout(float t);

    std::unique_ptr<ToastWidget> build() const;

  private:
    std::string text_;
    std::string title_;
    float timeout_ = 10.0f;
    bool rich_text_ = false;
    std::optional<Color> background_;
};

class ToastWidget : public Widget {
    DECLARE_WIDGET(ToastWidget)
  public:
    ToastWidget(std::string text, std::string title = "", std::string icon_path = "",
                float timeout = 10.0f, bool rich_text = false);

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    Widget *find_focusable_at(Point p) override { return layout_->find_focusable_at(p); }
    Widget *widget_at(Point p) override { return layout_->widget_at(p); }
    Size size_hint() const override;
    void set_rect(Rect const &rect) override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

    void set_on_close(std::function<void()> on_close);
    float remaining_time() const { return remaining_time_; }
    void update_remaining_time(float delta);
    bool is_expired() const { return remaining_time_ <= 0; }
    void expire() { remaining_time_ = 0; }

    void set_background_override(Color c) { background_override_ = c; }

  private:
    float timeout_;
    float remaining_time_;
    std::function<void()> on_close_;
    bool rich_text_;

    std::unique_ptr<VBoxLayout> layout_;
    Label *title_label_ = nullptr;
    Label *text_label_ = nullptr;
    Button *close_button_;
    std::optional<Color> background_override_;
};

} // namespace svision3
