#include "toolkit/radio_button.hpp"
#include "toolkit/theme.hpp"
#include <algorithm>

namespace toolkit {

// --- RadioGroup ---

void RadioGroup::add(RadioButton *rb) {
    buttons_.push_back(rb);
}

void RadioGroup::select(RadioButton *rb) {
    int idx = 0;
    for (auto *b : buttons_) {
        b->set_selected(b == rb);
        if (b == rb && on_change) on_change(idx);
        idx++;
    }
}

// --- RadioButton ---

RadioButton::RadioButton(std::string text, RadioGroup &group)
    : text_(std::move(text)), group_(group) {
    focusable_ = true;
    group_.add(this);
}

void RadioButton::paint(Painter &painter) {
    auto const &style = Theme::current().radio;
    auto fm = painter.font_metrics(style.font_size);
    float r = style.box_size / 2.0f;

    Point center{rect_.x + r, rect_.y + rect_.height / 2.0f};

    painter.fill_circle(center, r, style.background);
    painter.draw_circle(center, r, style.border, style.border_width);

    if (style.beveled) {
        painter.draw_circle(center, r - 1.0f, style.shadow, 1.0f);
    }

    if (selected_) {
        painter.fill_circle(center, r * 0.45f, style.indicator);
    }

    float text_x = rect_.x + style.box_size + style.spacing;
    float baseline_y = rect_.y + (rect_.height - fm.height) / 2.0f + fm.ascent;
    painter.draw_text(text_, {text_x, baseline_y}, style.text, style.font_size);

    if (focused_)
        painter.draw_focus_ring(rect_, style.corner_radius);
}

bool RadioButton::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Press && hit_test(event.position)) {
        group_.select(this);
        return true;
    }
    return false;
}

bool RadioButton::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) return false;
    if (event.key == Key::Enter || (!event.text.empty() && event.text[0] == ' ')) {
        group_.select(this);
        return true;
    }
    return false;
}

Size RadioButton::size_hint() const {
    auto const &style = Theme::current().radio;
    auto fm = Painter::measure_font_metrics(style.font_size);
    auto tw = Painter::measure_text(text_, style.font_size).width;
    float h = std::max(style.box_size, fm.height);
    return {style.box_size + style.spacing + tw, h + 4.0f};
}

} // namespace toolkit
