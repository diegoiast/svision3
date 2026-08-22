// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/toolbar.hpp"
#include "svision3/button.hpp"
#include "svision3/theme.hpp"
#include "svision3/window.hpp"
#include <algorithm>
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

// Divides the toolbar across its short axis, so it runs vertically on a
// horizontal toolbar and horizontally on a vertical one.
class ToolbarSeparator : public Widget {
    DECLARE_WIDGET(ToolbarSeparator)
  public:
    explicit ToolbarSeparator(Orientation toolbar_orientation = Orientation::Horizontal)
        : toolbar_orientation_(toolbar_orientation) {}

    void set_toolbar_orientation(Orientation o) { toolbar_orientation_ = o; }

    void paint(Painter &painter) override {
        auto const &theme = Theme::current();
        auto const &style = theme.style;
        auto const &palette = theme.palette;
        auto horizontal_bar = toolbar_orientation_ == Orientation::Horizontal;

        // The line is centered on the short axis and inset on the long one.
        auto from =
            horizontal_bar ? Point{rect_.width / 2.0f, 4.0f} : Point{4.0f, rect_.height / 2.0f};
        auto to = horizontal_bar ? Point{rect_.width / 2.0f, rect_.height - 4.0f}
                                 : Point{rect_.width - 4.0f, rect_.height / 2.0f};

        if (style.beveled) {
            auto shift = horizontal_bar ? Point{-1.0f, 0.0f} : Point{0.0f, -1.0f};
            painter.draw_line({from.x + shift.x, from.y + shift.y},
                              {to.x + shift.x, to.y + shift.y}, palette.shadow, 1.0f);
            painter.draw_line(from, to, palette.highlight, 1.0f);
        } else {
            painter.draw_line(from, to, palette.window.darken(0.15f), 1.0f);
        }
    }
    bool handle_mouse(MouseEvent const &) override { return false; }
    Size size_hint() const override {
        return toolbar_orientation_ == Orientation::Horizontal ? Size{8.0f, 0.0f}
                                                               : Size{0.0f, 8.0f};
    }

  private:
    Orientation toolbar_orientation_ = Orientation::Horizontal;
};

auto create_toolbar_separator(Orientation toolbar_orientation) -> std::unique_ptr<Widget> {
    return std::make_unique<ToolbarSeparator>(toolbar_orientation);
}

Toolbar::Toolbar() { rebuild_layout(); }

void Toolbar::rebuild_layout() {
    auto horizontal = orientation_ == Orientation::Horizontal;

    if (horizontal) {
        auto box = std::make_unique<HBoxLayout>();
        for (auto const &entry : entries_) {
            box->add_widget(entry.widget, entry.stretch);
        }
        layout_ = std::move(box);
    } else {
        auto box = std::make_unique<VBoxLayout>();
        for (auto const &entry : entries_) {
            box->add_widget(entry.widget, entry.stretch);
        }
        layout_ = std::move(box);
    }

    layout_->set_parent(this);
    layout_->set_margins(horizontal ? Margins{2, 4, 2, 4} : Margins{4, 2, 4, 2});
    layout_->set_spacing(2);
    layout_->set_window(window_);
    layout_->set_rect({0, 0, rect_.width, rect_.height});

    // Toolbar::size_hint() is the layout's, so every add, removal and reorientation
    // changes what this widget asks for -- ancestors have to re-measure, not just
    // repaint. Every mutation funnels through here, so this is the one place that
    // needs to say so. It also schedules the redraw the callers used to ask for
    // individually, and that some of them (add_command, add_widget) never did.
    invalidate_layout();
}

Toolbar &Toolbar::set_orientation(Orientation o) {
    if (orientation_ == o) {
        return *this;
    }
    orientation_ = o;
    for (auto const &entry : entries_) {
        if (auto *sep = dynamic_cast<ToolbarSeparator *>(entry.widget.get())) {
            sep->set_toolbar_orientation(o);
        }
    }
    rebuild_layout();
    return *this;
}

std::weak_ptr<Widget> Toolbar::insert_entry(int index, std::shared_ptr<Widget> w, int stretch) {
    auto ref = std::weak_ptr<Widget>(w);
    index = std::clamp(index, 0, static_cast<int>(entries_.size()));
    entries_.insert(entries_.begin() + index, Entry{std::move(w), stretch});
    rebuild_layout();
    return ref;
}

void Toolbar::add_command(Command::Ptr cmd) {
    insert_entry(item_count(), std::make_shared<ToolButton>(std::move(cmd)), 0);
}

std::weak_ptr<Widget> Toolbar::add_widget(std::shared_ptr<Widget> w, float stretch) {
    return insert_entry(item_count(), std::move(w), static_cast<int>(stretch));
}

void Toolbar::add_separator() {
    insert_entry(item_count(), std::make_shared<ToolbarSeparator>(orientation_), 0);
}

void Toolbar::insert_command(int index, Command::Ptr cmd) {
    insert_entry(index, std::make_shared<ToolButton>(std::move(cmd)), 0);
}

void Toolbar::insert_separator(int index) {
    insert_entry(index, std::make_shared<ToolbarSeparator>(orientation_), 0);
}

void Toolbar::remove_range(int index, int count) {
    if (index < 0 || count <= 0 || index >= static_cast<int>(entries_.size())) {
        return;
    }
    count = std::min(count, static_cast<int>(entries_.size()) - index);
    entries_.erase(entries_.begin() + index, entries_.begin() + index + count);
    rebuild_layout();
}

int Toolbar::item_count() const { return static_cast<int>(entries_.size()); }

void Toolbar::clear() {
    entries_.clear();
    rebuild_layout();
}

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
    // Keep a usable thickness across the short axis even when empty
    if (orientation_ == Orientation::Horizontal) {
        hint.height = std::max(hint.height, 32.0f);
    } else {
        hint.width = std::max(hint.width, 32.0f);
    }
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
    j["orientation"] = orientation_ == Orientation::Vertical ? "vertical" : "horizontal";
    return j;
}

void Toolbar::from_json(nlohmann::json const &j) {
    Widget::from_json(j);

    if (j.contains("orientation")) {
        orientation_ =
            j["orientation"] == "vertical" ? Orientation::Vertical : Orientation::Horizontal;
        rebuild_layout();
    }
    if (!j.contains("layout")) {
        return;
    }
    layout_->from_json(j["layout"]);

    // The layout just gained children that the toolbar knows nothing about; adopt
    // them so the next rebuild_layout() does not drop them on the floor.
    entries_.clear();
    if (orientation_ == Orientation::Horizontal) {
        for (auto const &item : static_cast<HBoxLayout *>(layout_.get())->items()) {
            entries_.push_back({item.widget, item.stretch});
        }
    } else {
        for (auto const &item : static_cast<VBoxLayout *>(layout_.get())->items()) {
            entries_.push_back({item.widget, item.stretch});
        }
    }
}

} // namespace svision3
