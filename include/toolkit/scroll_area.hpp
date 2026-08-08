// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/scrollable_widget.hpp"
#include <memory>

namespace toolkit {

// A generic scrollable container.  Set any widget as content with set_content();
// the widget receives its natural size (from size_hint()) and ScrollArea clips
// and scrolls it within the available viewport.
class ScrollArea : public ScrollableWidget {
    DECLARE_WIDGET(ScrollArea)
  public:
    ScrollArea();
    ~ScrollArea() override = default;

    nlohmann::json to_json() const override;

    std::weak_ptr<Widget> set_content(std::shared_ptr<Widget> widget);
    std::weak_ptr<Widget> content() const { return content_; }

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    CursorShape cursor() const override {
        return content_ ? content_->cursor() : CursorShape::Arrow;
    }
    Size size_hint() const override { return {0, 0}; }
    void set_rect(Rect const &rect) override;
    void on_theme_changed() override;
    void collect_focusables(std::vector<Widget *> &out) override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

  protected:
    void on_scroll(float x, float y) override;

  private:
    std::shared_ptr<Widget> content_;

    void update_content_rect();
};

} // namespace toolkit
