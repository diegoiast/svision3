// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/splitter.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>

namespace toolkit {

Splitter::Splitter(Orientation o) : orientation_(o) {}

nlohmann::json Splitter::to_json() const {
    auto j = Widget::to_json();
    j["orientation"] = static_cast<int>(orientation_);
    j["ratio"] = ratio_;
    if (first_) {
        j["first"] = first_->to_json();
    }
    if (second_) {
        j["second"] = second_->to_json();
    }
    return j;
}

void Splitter::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("orientation")) {
        orientation_ = static_cast<Orientation>(j["orientation"].get<int>());
    }
    if (j.contains("ratio")) {
        set_ratio(j["ratio"]);
    }
}

Splitter &Splitter::set_first(std::unique_ptr<Widget> w) {
    first_ = std::move(w);
    if (first_) {
        first_->set_parent(this);
        first_->set_window(window_);
    }
    layout_children();
    return *this;
}

Splitter &Splitter::set_second(std::unique_ptr<Widget> w) {
    second_ = std::move(w);
    if (second_) {
        second_->set_parent(this);
        second_->set_window(window_);
    }
    layout_children();
    return *this;
}

Splitter &Splitter::set_ratio(float r) {
    ratio_ = std::clamp(r, 0.0f, 1.0f);
    layout_children();
    if (window_) {
        window_->request_redraw("splitter ratio");
    }
    return *this;
}

Splitter &Splitter::set_locked(bool locked) {
    locked_ = locked;
    if (locked_ && dragging_) {
        dragging_ = false;
    }
    cursor_ = CursorShape::Arrow;
    if (window_) {
        window_->request_redraw("splitter lock");
    }
    return *this;
}

float Splitter::split_pos() const {
    if (orientation_ == Orientation::Horizontal) {
        auto total = rect_.width;
        auto min_first = first_ ? first_->size_hint().width : 0.0f;
        auto min_second = second_ ? second_->size_hint().width : 0.0f;
        auto pos = total * ratio_ - kHandleSize / 2.0f;
        pos = std::max(pos, min_first);
        pos = std::min(pos, total - kHandleSize - min_second);
        return std::max(pos, 0.0f);
    } else {
        auto total = rect_.height;
        auto min_first = first_ ? first_->size_hint().height : 0.0f;
        auto min_second = second_ ? second_->size_hint().height : 0.0f;
        auto pos = total * ratio_ - kHandleSize / 2.0f;
        pos = std::max(pos, min_first);
        pos = std::min(pos, total - kHandleSize - min_second);
        return std::max(pos, 0.0f);
    }
}

Rect Splitter::handle_rect() const {
    auto pos = split_pos();
    if (orientation_ == Orientation::Horizontal) {
        return {pos, 0, kHandleSize, rect_.height};
    } else {
        return {0, pos, rect_.width, kHandleSize};
    }
}

void Splitter::layout_children() {
    if (!first_ && !second_) {
        return;
    }
    auto pos = split_pos();
    if (orientation_ == Orientation::Horizontal) {
        if (first_) {
            first_->set_rect({0, 0, pos, rect_.height});
        }
        if (second_) {
            auto x2 = pos + kHandleSize;
            second_->set_rect({x2, 0, rect_.width - x2, rect_.height});
        }
    } else {
        if (first_) {
            first_->set_rect({0, 0, rect_.width, pos});
        }
        if (second_) {
            auto y2 = pos + kHandleSize;
            second_->set_rect({0, y2, rect_.width, rect_.height - y2});
        }
    }
}

void Splitter::paint(Painter &painter) {
    auto h = handle_rect();
    auto const &pal = Theme::current().palette;
    // Draw a subtle 1px divider line in the centre of the handle
    auto line_color = pal.border.with_alpha(0.6f);
    if (orientation_ == Orientation::Horizontal) {
        float cx = h.x + h.width / 2.0f;
        painter.draw_line({cx, h.y}, {cx, h.y + h.height}, line_color, 1.0f);
    } else {
        float cy = h.y + h.height / 2.0f;
        painter.draw_line({h.x, cy}, {h.x + h.width, cy}, line_color, 1.0f);
    }

    if (first_) {
        first_->draw(painter);
    }
    if (second_) {
        second_->draw(painter);
    }

    if (active_pane_ == 0 && first_) {
        painter.draw_rect(first_->rect(), pal.accent, kBorderWidth);
    } else if (active_pane_ == 1 && second_) {
        painter.draw_rect(second_->rect(), pal.accent, kBorderWidth);
    }
}

bool Splitter::handle_mouse(MouseEvent const &event) {
    auto h = handle_rect();

    switch (event.type) {
    case MouseEvent::Type::Move:
        if (!locked_ && h.contains(event.position)) {
            auto desired = orientation_ == Orientation::Horizontal ? CursorShape::ResizeEW
                                                                   : CursorShape::ResizeNS;
            if (cursor_ != desired) {
                cursor_ = desired;
                if (window_) {
                    window_->request_redraw("splitter cursor");
                }
            }
        } else if (!dragging_) {
            if (cursor_ != CursorShape::Arrow) {
                cursor_ = CursorShape::Arrow;
                if (window_) {
                    window_->request_redraw("splitter cursor");
                }
            }
        }
        break;

    case MouseEvent::Type::Press:
        if (!locked_ && h.contains(event.position)) {
            dragging_ = true;
            cursor_ = orientation_ == Orientation::Horizontal ? CursorShape::ResizeEW
                                                              : CursorShape::ResizeNS;
            return true;
        }
        break;

    case MouseEvent::Type::Drag:
        if (dragging_) {
            if (orientation_ == Orientation::Horizontal) {
                ratio_ = std::clamp(event.position.x / rect_.width, 0.05f, 0.95f);
            } else {
                ratio_ = std::clamp(event.position.y / rect_.height, 0.05f, 0.95f);
            }
            layout_children();
            if (window_) {
                window_->request_redraw("splitter drag");
            }
            return true;
        }
        break;

    case MouseEvent::Type::Release:
        if (dragging_) {
            dragging_ = false;
            cursor_ = CursorShape::Arrow;
            if (window_) {
                window_->request_redraw("splitter release");
            }
            return true;
        }
        break;

    case MouseEvent::Type::Leave:
        if (!dragging_) {
            cursor_ = CursorShape::Arrow;
        }
        break;

    default:
        break;
    }

    // Forward non-handle events to children; track active pane on press
    if (!dragging_) {
        // Returns true if the window's focused widget is inside `container`.
        auto focused_inside = [&](Widget *container) -> bool {
            if (!window_) {
                return false;
            }
            auto *fw = window_->focused_widget();
            while (fw) {
                if (fw == container) {
                    return true;
                }
                fw = fw->parent();
            }
            return false;
        };

        if (first_ && first_->rect().contains(event.position)) {
            auto shifted = event;
            shifted.position.x -= first_->rect().x;
            shifted.position.y -= first_->rect().y;
            auto result = first_->handle_mouse(shifted);
            if (event.type == MouseEvent::Type::Press && focused_inside(first_.get()) &&
                active_pane_ != 0) {
                active_pane_ = 0;
                if (window_) {
                    window_->request_redraw("splitter active pane");
                }
            }
            return result;
        }
        if (second_ && second_->rect().contains(event.position)) {
            auto shifted = event;
            shifted.position.x -= second_->rect().x;
            shifted.position.y -= second_->rect().y;
            auto result = second_->handle_mouse(shifted);
            if (event.type == MouseEvent::Type::Press && focused_inside(second_.get()) &&
                active_pane_ != 1) {
                active_pane_ = 1;
                if (window_) {
                    window_->request_redraw("splitter active pane");
                }
            }
            return result;
        }
    }
    return false;
}

void Splitter::set_rect(Rect const &rect) {
    Widget::set_rect(rect);
    layout_children();
}

void Splitter::set_window(Window *w) {
    Widget::set_window(w);
    if (first_) {
        first_->set_window(w);
    }
    if (second_) {
        second_->set_window(w);
    }
}

Size Splitter::size_hint() const {
    Size a = first_ ? first_->size_hint() : Size{};
    Size b = second_ ? second_->size_hint() : Size{};
    if (orientation_ == Orientation::Horizontal) {
        return {a.width + kHandleSize + b.width, std::max(a.height, b.height)};
    } else {
        return {std::max(a.width, b.width), a.height + kHandleSize + b.height};
    }
}

Widget *Splitter::find_focusable_at(Point p) {
    if (first_ && first_->rect().contains(p)) {
        auto shifted = Point{p.x - first_->rect().x, p.y - first_->rect().y};
        return first_->find_focusable_at(shifted);
    }
    if (second_ && second_->rect().contains(p)) {
        auto shifted = Point{p.x - second_->rect().x, p.y - second_->rect().y};
        return second_->find_focusable_at(shifted);
    }
    return nullptr;
}

Widget *Splitter::widget_at(Point p) {
    if (first_ && first_->rect().contains(p)) {
        auto shifted = Point{p.x - first_->rect().x, p.y - first_->rect().y};
        return first_->widget_at(shifted);
    }
    if (second_ && second_->rect().contains(p)) {
        auto shifted = Point{p.x - second_->rect().x, p.y - second_->rect().y};
        return second_->widget_at(shifted);
    }
    return this;
}

void Splitter::collect_focusables(std::vector<Widget *> &out) {
    if (first_) {
        first_->collect_focusables(out);
    }
    if (second_) {
        second_->collect_focusables(out);
    }
}

void Splitter::collect_mnemonics(std::vector<Widget *> &out) {
    if (first_) {
        first_->collect_mnemonics(out);
    }
    if (second_) {
        second_->collect_mnemonics(out);
    }
}

void Splitter::for_each_child(std::function<void(Widget *)> const &callback) {
    if (first_) {
        callback(first_.get());
    }
    if (second_) {
        callback(second_.get());
    }
}

void Splitter::on_theme_changed() {
    Widget::on_theme_changed();
    if (first_) {
        first_->on_theme_changed();
    }
    if (second_) {
        second_->on_theme_changed();
    }
}

} // namespace toolkit
