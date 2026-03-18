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
        set_padding({4, 8, 4, 8});
        set_tooltip(cmd_->tooltip());
        on_click = [this] { cmd_->execute(); };
    }

    void paint(Painter &painter) override {
        // Sync state before paint
        set_enabled(cmd_->is_enabled());
        set_text(cmd_->display_text());
        set_tooltip(cmd_->tooltip());
        Button::paint(painter);
    }

  private:
    Command::Ptr cmd_;
};

class ToolbarSeparator : public Widget {
  public:
    void paint(Painter &painter) override {
        auto const &style = Theme::current().button;
        if (style.beveled) {
            float x = rect_.width / 2.0f;
            painter.draw_line({x - 1.0f, 4.0f}, {x - 1.0f, rect_.height - 4.0f}, style.shadow, 1.0f);
            painter.draw_line({x, 4.0f}, {x, rect_.height - 4.0f}, style.highlight, 1.0f);
        } else {
            auto color = Theme::current().window.background.darken(0.15f);
            painter.draw_line({rect_.width / 2.0f, 4.0f}, {rect_.width / 2.0f, rect_.height - 4.0f},
                              color, 1.0f);
        }
    }
    bool handle_mouse(MouseEvent const &) override { return false; }
    Size size_hint() const override { return {8.0f, 0.0f}; }
};

Toolbar::Toolbar() {
    layout_ = std::make_unique<HBoxLayout>();
    layout_->set_margins({2, 4, 2, 4});
    layout_->set_spacing(2);
}

void Toolbar::add_command(Command::Ptr cmd) {
    layout_->add_widget(std::make_unique<ToolButton>(std::move(cmd)));
}

void Toolbar::add_widget(std::unique_ptr<Widget> w, float stretch) {
    layout_->add_widget(std::move(w), stretch);
}

void Toolbar::add_separator() { layout_->add_widget(std::make_unique<ToolbarSeparator>()); }

void Toolbar::paint(Painter &painter) {
    auto const &style = Theme::current().button;
    if (style.beveled) {
        // Win95 style: highlight at top, shadow at bottom
        painter.draw_line({0, 0}, {rect_.width, 0}, style.highlight, 1.0f);
        painter.draw_line({0, rect_.height - 1.0f}, {rect_.width, rect_.height - 1.0f},
                          style.shadow, 1.0f);
    } else {
        // Flat style: subtle border at bottom
        auto border_c = Theme::current().window.background.darken(0.15f);
        painter.draw_line({0, rect_.height - 1.0f}, {rect_.width, rect_.height - 1.0f}, border_c, 1.0f);
    }

    layout_->draw(painter);
}

bool Toolbar::handle_mouse(MouseEvent const &event) { return layout_->handle_mouse(event); }

bool Toolbar::handle_key(KeyEvent const &event) { return layout_->handle_key(event); }

Widget *Toolbar::widget_at(Point p) {
    if (!is_visible() || !hit_test(p)) {
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
