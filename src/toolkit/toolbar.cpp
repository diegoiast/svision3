// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/toolbar.hpp"
#include "toolkit/button.hpp"
#include "toolkit/theme.hpp"

namespace toolkit {

class ToolButton : public Button {
  public:
    explicit ToolButton(Command::Ptr cmd) : Button(cmd->display_text()), cmd_(std::move(cmd)) {
        set_flat(true);
        set_tooltip(cmd_->tooltip());
        on_click = [this] { cmd_->execute(); };
    }

    void paint(Painter &painter) override {
        Button::paint(painter);
    }

  private:
    Command::Ptr cmd_;
};

class ToolbarSeparator : public Widget {
  public:
    void paint(Painter &painter) override {
        auto const &style = Theme::current().window;
        auto color = style.background.darken(0.15f);
        painter.draw_line({rect_.width / 2.0f, 4.0f}, {rect_.width / 2.0f, rect_.height - 4.0f},
                          color, 1.0f);
    }
    bool handle_mouse(MouseEvent const &) override { return false; }
    Size size_hint() const override { return {8.0f, 0.0f}; }
};

Toolbar::Toolbar() {
    layout_ = std::make_unique<HBoxLayout>();
    layout_->set_margins({2, 4, 2, 4});
    layout_->set_spacing(2);
    set_background_color(Theme::current().window.background.darken(0.02f));
}

void Toolbar::add_command(Command::Ptr cmd) {
    layout_->add_widget(std::make_unique<ToolButton>(std::move(cmd)));
}

void Toolbar::add_widget(std::unique_ptr<Widget> w, float stretch) {
    layout_->add_widget(std::move(w), stretch);
}

void Toolbar::add_separator() { layout_->add_widget(std::make_unique<ToolbarSeparator>()); }

void Toolbar::paint(Painter &painter) {
    if (background_color_) {
        painter.fill_rect({0, 0, rect_.width, rect_.height}, *background_color_);
    }
    // Draw bottom border
    auto border_c = Theme::current().window.background.darken(0.15f);
    painter.draw_line({0, rect_.height - 1.0f}, {rect_.width, rect_.height - 1.0f}, border_c, 1.0f);

    layout_->draw(painter);
}

bool Toolbar::handle_mouse(MouseEvent const &event) { return layout_->handle_mouse(event); }

bool Toolbar::handle_key(KeyEvent const &event) { return layout_->handle_key(event); }

Widget *Toolbar::widget_at(Point p) {
    if (!visible_ || !hit_test(p)) {
        return nullptr;
    }
    // Check children first
    auto local_p = Point{p.x, p.y};
    if (auto child = layout_->widget_at(local_p)) {
        return child;
    }
    return this;
}

Size Toolbar::size_hint() const {
    auto hint = layout_->size_hint();
    // Ensure it has a reasonable height even if empty
    hint.height = std::max(hint.height, 32.0f);
    return hint;
}

void Toolbar::set_rect(Rect const &rect) {
    Widget::set_rect(rect);
    layout_->set_rect({0, 0, rect.width, rect.height});
}

void Toolbar::set_window(Window *w) {
    Widget::set_window(w);
    layout_->set_window(w);
}

void Toolbar::for_each_child(std::function<void(Widget *)> const &callback) {
    layout_->for_each_child(callback);
}

} // namespace toolkit
