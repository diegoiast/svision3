// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/toolbar.hpp"
#include "svision3/button.hpp"
#include "svision3/theme.hpp"
#include "svision3/window.hpp"
#include <nlohmann/json.hpp>

namespace svision3 {

class ToolButton : public Button {
  public:
    explicit ToolButton(Command::Ptr cmd) : Button(cmd->display_text()), cmd_(std::move(cmd)) {
        set_flat(true);
        set_padding({4, 8, 4, 8});
        set_tooltip(cmd_->tooltip());

        if (!cmd_->icon().empty()) {
            auto icon_data = cmd_->icon_image();
            if (icon_data) {
                set_icon(icon_data);
            }
        }
        this->set_command(cmd_);
    }

    void paint(Painter &painter) override {
        set_enabled(cmd_->is_enabled());
        set_text(cmd_->display_text());
        set_tooltip(cmd_->tooltip());
        if (!cmd_->icon().empty()) {
            auto icon_data = cmd_->icon_image();
            if (icon_data) {
                set_icon(icon_data);
            }
        }
        Button::paint(painter);
    }

  private:
    Command::Ptr cmd_;
};

class ToolbarSeparator : public Widget {
    DECLARE_WIDGET(ToolbarSeparator)
  public:
    void paint(Painter &painter) override {
        auto const &theme = Theme::current();
        auto const &style = theme.style;
        auto const &palette = theme.palette;

        if (style.beveled) {
            auto x = rect_.width / 2.0f;
            painter.draw_line({x - 1.0f, 4.0f}, {x - 1.0f, rect_.height - 4.0f}, palette.shadow,
                              1.0f);
            painter.draw_line({x, 4.0f}, {x, rect_.height - 4.0f}, palette.highlight, 1.0f);
        } else {
            auto color = palette.window.darken(0.15f);
            painter.draw_line({rect_.width / 2.0f, 4.0f}, {rect_.width / 2.0f, rect_.height - 4.0f},
                              color, 1.0f);
        }
    }
    bool handle_mouse(MouseEvent const &) override { return false; }
    Size size_hint() const override { return {8.0f, 0.0f}; }
};

auto create_toolbar_separator() -> std::unique_ptr<Widget> {
    return std::make_unique<ToolbarSeparator>();
}

Toolbar::Toolbar() {
    layout_ = std::make_unique<HBoxLayout>();
    layout_->set_parent(this);
    layout_->set_margins({2, 4, 2, 4});
    layout_->set_spacing(2);
}

void Toolbar::add_command(Command::Ptr cmd) {
    layout_->add_widget(std::make_unique<ToolButton>(std::move(cmd)));
}

std::weak_ptr<Widget> Toolbar::add_widget(std::shared_ptr<Widget> w, float stretch) {
    return layout_->add_widget(std::move(w), stretch);
}

void Toolbar::add_separator() { layout_->add_widget(std::make_unique<ToolbarSeparator>()); }

void Toolbar::insert_command(int index, Command::Ptr cmd) {
    layout_->insert_widget(index, std::make_unique<ToolButton>(std::move(cmd)));
    if (window_) {
        window_->request_redraw("toolbar command inserted");
    }
}

void Toolbar::insert_separator(int index) {
    layout_->insert_widget(index, std::make_unique<ToolbarSeparator>());
    if (window_) {
        window_->request_redraw("toolbar separator inserted");
    }
}

void Toolbar::remove_range(int index, int count) {
    for (auto i = 0; i < count; ++i) {
        layout_->release_item(index);
    }
    if (count > 0 && window_) {
        window_->request_redraw("toolbar items removed");
    }
}

int Toolbar::item_count() const { return static_cast<int>(layout_->items().size()); }

void Toolbar::clear() { layout_->clear_items(); }

void Toolbar::paint(Painter &painter) {
    auto rect = Rect{0, 0, rect_.width, rect_.height};
    auto wstate = WidgetState{
        .interaction = ButtonState::Normal,
        .focused = false,
        .enabled = true,
        .window_active = window_ ? window_->is_active() : true,
    };
    Theme::current().draw_toolbar(painter, rect, wstate);
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

nlohmann::json Toolbar::to_json() const {
    nlohmann::json j = Widget::to_json();
    j["layout"] = layout_->to_json();
    return j;
}

void Toolbar::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("layout")) {
        layout_->from_json(j["layout"]);
    }
}

} // namespace svision3
