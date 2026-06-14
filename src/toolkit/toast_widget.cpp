// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/toast_widget.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/rich_label.hpp"
#include "toolkit/theme.hpp"

namespace toolkit {

ToastBuilder &ToastBuilder::text(std::string t) {
    text_ = std::move(t);
    rich_text_ = false;
    return *this;
}

ToastBuilder &ToastBuilder::rich_text(std::string t) {
    text_ = std::move(t);
    rich_text_ = true;
    return *this;
}

ToastBuilder &ToastBuilder::title(std::string t) {
    title_ = std::move(t);
    return *this;
}

ToastBuilder &ToastBuilder::background(Color c) {
    background_ = c;
    return *this;
}

ToastBuilder &ToastBuilder::timeout(float t) {
    timeout_ = t;
    return *this;
}

auto ToastBuilder::build() const -> std::unique_ptr<ToastWidget> {
    auto widget = std::make_unique<ToastWidget>(text_, title_, "", timeout_, rich_text_);
    if (background_) {
        widget->set_background_override(*background_);
    }
    return widget;
}

ToastWidget::ToastWidget(std::string text, std::string title, std::string icon_path, float timeout,
                         bool rich_text)
    : timeout_(timeout), remaining_time_(timeout), rich_text_(rich_text) {
    set_on_top(true);

    auto vbox = std::make_unique<VBoxLayout>();
    vbox->set_parent(this);

    auto close_button = std::make_unique<Button>("X");
    close_button->set_flat(true);
    close_button->on_click = [this] {
        if (on_close_) {
            on_close_();
        }
    };
    close_button_ = close_button.get();

    if (!title.empty()) {
        auto const &palette = Theme::current().palette;

        auto title_hbox = std::make_unique<HBoxLayout>();
        title_hbox->set_margins({2, 2, 2, 2});
        title_hbox->set_background_color(palette.accent);

        auto title_label = std::make_unique<Label>(std::move(title));
        title_label->set_alignment(Alignment::Start);
        title_label_ = title_label.get();
        title_hbox->add_widget(std::move(title_label), 1, Alignment::Fill);
        title_hbox->add_widget(std::move(close_button), 0, Alignment::Fill);
        vbox->add_widget(std::move(title_hbox), 0);

        auto content_hbox = std::make_unique<HBoxLayout>();
        content_hbox->set_margins({4, 4, 4, 4});
        if (rich_text_) {
            auto text_label = std::make_unique<RichLabel>(std::move(text));
            content_hbox->add_widget(std::move(text_label), 1, Alignment::Fill);
        } else {
            auto text_label = std::make_unique<Label>(std::move(text));
            text_label->set_alignment(Alignment::Start);
            text_label_ = text_label.get();
            content_hbox->add_widget(std::move(text_label), 1, Alignment::Fill);
        }
        vbox->add_widget(std::move(content_hbox), 1);
    } else {
        auto hbox = std::make_unique<HBoxLayout>();
        hbox->set_margins({4, 4, 4, 4});
        hbox->set_spacing(0.4f);
        if (rich_text_) {
            auto text_label = std::make_unique<RichLabel>(std::move(text));
            hbox->add_widget(std::move(text_label), 1, Alignment::Fill);
        } else {
            auto text_label = std::make_unique<Label>(std::move(text));
            text_label->set_alignment(Alignment::Start);
            text_label_ = text_label.get();
            hbox->add_widget(std::move(text_label), 1, Alignment::Fill);
        }
        hbox->add_widget(std::move(close_button), 0, Alignment::Fill);
        vbox->add_widget(std::move(hbox), 1);
    }

    layout_ = std::move(vbox);
}

void ToastWidget::paint(Painter &painter) {
    auto const &palette = Theme::current().palette;
    Rect r = {0, 0, rect_.width, rect_.height};

    auto bg = background_override_.value_or(palette.tooltip);
    painter.fill_rounded_rect(r, bg, Theme::current().style.corner_radius);
    painter.draw_rounded_rect(r, palette.border, Theme::current().style.corner_radius, 1);

    // Progress Bar
    auto progress_width = r.width - 20;
    auto progress_height = 4;
    Rect p_r{10, r.height - 10, progress_width * (remaining_time_ / timeout_),
             static_cast<float>(progress_height)};
    painter.fill_rect(p_r, palette.highlight);

    layout_->draw(painter);
}

bool ToastWidget::handle_mouse(MouseEvent const &event) { return layout_->handle_mouse(event); }

Size ToastWidget::size_hint() const { return {300.0f, title_label_ ? 120.0f : 100.0f}; }

void ToastWidget::set_rect(Rect const &rect) {
    Widget::set_rect(rect);
    layout_->set_rect({0, 0, rect.width, rect.height});
}

void ToastWidget::for_each_child(std::function<void(Widget *)> const &callback) {
    layout_->for_each_child(callback);
}

void ToastWidget::set_on_close(std::function<void()> on_close) { on_close_ = std::move(on_close); }

void ToastWidget::update_remaining_time(float delta) { remaining_time_ -= delta; }

} // namespace toolkit
