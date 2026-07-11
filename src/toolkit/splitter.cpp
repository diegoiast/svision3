// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/splitter.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>

namespace toolkit {

Splitter::Splitter(Orientation o) : orientation_(o) {}

// ---------------------------------------------------------------------------
// Serialisation
// ---------------------------------------------------------------------------

nlohmann::json Splitter::to_json() const {
    auto j = Widget::to_json();
    j["orientation"] = static_cast<int>(orientation_);
    j["ratios"] = ratios_;
    auto arr = nlohmann::json::array();
    for (auto const &child : children_) {
        if (child) arr.push_back(child->to_json());
    }
    j["children"] = arr;
    return j;
}

void Splitter::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("orientation")) {
        orientation_ = static_cast<Orientation>(j["orientation"].get<int>());
    }
    if (j.contains("ratios")) {
        ratios_ = j["ratios"].get<std::vector<float>>();
    }
}

// ---------------------------------------------------------------------------
// Child management
// ---------------------------------------------------------------------------

Splitter &Splitter::add_child(std::unique_ptr<Widget> w) {
    if (!children_.empty()) {
        auto N = children_.size();
        ratios_.push_back(static_cast<float>(N) / static_cast<float>(N + 1));
        locked_dividers_.push_back({});
    }
    stretch_factors_.push_back(1.0f);
    if (w) {
        w->set_parent(this);
        w->set_window(window_);
    }
    children_.push_back(std::move(w));
    layout_children();
    return *this;
}

Widget *Splitter::child_at(size_t index) {
    if (index >= children_.size()) return nullptr;
    return children_[index].get();
}

// ---------------------------------------------------------------------------
// Per-divider ratio / lock
// ---------------------------------------------------------------------------

Splitter &Splitter::set_ratio(int divider, float r) {
    if (divider < 0 || divider >= (int)ratios_.size()) return *this;
    ratios_[divider] = std::clamp(r, 0.0f, 1.0f);
    // Force a locked divider to re-anchor from the new ratio.
    locked_dividers_[divider].px = std::numeric_limits<float>::quiet_NaN();
    layout_children();
    if (window_) window_->request_redraw("splitter ratio");
    return *this;
}

float Splitter::ratio(int divider) const {
    if (divider < 0 || divider >= (int)ratios_.size()) return 0.5f;
    return ratios_[divider];
}

Splitter &Splitter::set_divider_locked(int divider, bool locked) {
    if (divider < 0 || divider >= (int)locked_dividers_.size()) return *this;
    locked_dividers_[divider].locked = locked ? 1u : 0u;
    locked_dividers_[divider].px = std::numeric_limits<float>::quiet_NaN();
    if (locked && dragging_divider_ == divider) dragging_divider_.reset();
    cursor_ = CursorShape::Arrow;
    hovered_divider_.reset();
    layout_children();
    if (window_) window_->request_redraw("splitter lock");
    return *this;
}

bool Splitter::is_divider_locked(int divider) const {
    if (divider < 0 || divider >= (int)locked_dividers_.size()) return false;
    return locked_dividers_[divider].locked != 0;
}

Splitter &Splitter::set_stretch_factor(int child, float factor) {
    if (child < 0 || child >= (int)stretch_factors_.size()) return *this;
    stretch_factors_[child] = std::max(0.0f, factor);
    return *this;
}

float Splitter::stretch_factor(int child) const {
    if (child < 0 || child >= (int)stretch_factors_.size()) return 1.0f;
    return stretch_factors_[child];
}

// ---------------------------------------------------------------------------
// Single-divider convenience
// ---------------------------------------------------------------------------

Splitter &Splitter::set_locked(bool locked) {
    for (auto &lk : locked_dividers_) {
        lk.locked = locked ? 1u : 0u;
        lk.px = std::numeric_limits<float>::quiet_NaN();
    }
    if (locked) dragging_divider_.reset();
    cursor_ = CursorShape::Arrow;
    hovered_divider_.reset();
    layout_children();
    if (window_) window_->request_redraw("splitter lock");
    return *this;
}

bool Splitter::locked() const {
    for (auto const &lk : locked_dividers_) {
        if (lk.locked) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Internal geometry helpers
// ---------------------------------------------------------------------------

float Splitter::effective_thickness(int divider) const {
    if (is_divider_locked(divider)) return 0.0f;
    return Theme::current().style.splitter.thickness;
}

std::vector<float> Splitter::compute_positions() const {
    auto const N = (int)children_.size();
    if (N <= 1) return {};

    auto const M = N - 1;
    auto const total =
        (orientation_ == Orientation::Horizontal) ? rect_.width : rect_.height;

    auto child_min = [&](int i) -> float {
        if (i < 0 || i >= N || !children_[i]) return 0.0f;
        auto h = children_[i]->size_hint();
        return (orientation_ == Orientation::Horizontal) ? h.width : h.height;
    };

    std::vector<float> pos(M);
    float prev_end = 0.0f;

    for (int i = 0; i < M; i++) {
        float hs = effective_thickness(i);
        float r = (i < (int)ratios_.size()) ? ratios_[i]
                                             : static_cast<float>(i + 1) / static_cast<float>(N);
        float raw = total * r - hs / 2.0f;

        if (is_divider_locked(i)) {
            // A locked divider is fixed by intent (used to collapse a pane).
            // On the first layout after locking, capture its pixel distance
            // from the nearest edge; later layouts keep that distance so the
            // collapsed pane never drifts when the splitter is resized (a
            // ratio would scale with the new total and leave a gap). Child
            // minimums are ignored so a pane can shrink all the way to zero.
            auto &lk = locked_dividers_[i];
            if (std::isnan(lk.px) && total > 0.0f) {
                auto p = std::clamp(raw, 0.0f, total);
                lk.from_end = p > total / 2.0f ? 1u : 0u;
                lk.px = lk.from_end ? total - p : p;
            }
            if (!std::isnan(lk.px)) {
                raw = lk.from_end ? total - lk.px : lk.px;
            }
            raw = std::clamp(raw, prev_end, total);
        } else {
            float min_i = child_min(i);
            float min_after = 0.0f;
            for (int j = i + 1; j < N; j++) {
                min_after += child_min(j);
                if (j < N - 1) min_after += effective_thickness(j);
            }
            raw = std::max(raw, prev_end + min_i);
            raw = std::min(raw, total - hs - min_after);
            raw = std::max(raw, prev_end);
        }
        pos[i] = std::round(raw);
        prev_end = pos[i] + hs;
    }
    return pos;
}

Rect Splitter::handle_rect(int divider, std::vector<float> const &positions) const {
    if (divider < 0 || divider >= (int)positions.size()) return {};
    auto const pos = positions[divider];
    auto const hs = effective_thickness(divider);
    if (orientation_ == Orientation::Horizontal) {
        return {pos - kHitRadius, 0.0f, hs + 2 * kHitRadius, rect_.height};
    } else {
        return {0.0f, pos - kHitRadius, rect_.width, hs + 2 * kHitRadius};
    }
}

void Splitter::redistribute_stretch(float new_total) {
    auto const N = (int)children_.size();
    auto const M = N - 1;
    if (M <= 0 || last_total_ <= 0.0f) return;

    // Reconstruct each child's current pixel size from ratios_ + the old total.
    std::vector<float> old_size(N, 0.0f);
    float prev_end = 0.0f;
    for (int i = 0; i < M; i++) {
        float hs = effective_thickness(i);
        float r = (i < (int)ratios_.size()) ? ratios_[i]
                                             : static_cast<float>(i + 1) / static_cast<float>(N);
        float pos = last_total_ * r - hs / 2.0f;
        old_size[i] = pos - prev_end;
        prev_end = pos + hs;
    }
    old_size[N - 1] = last_total_ - prev_end;

    float delta = new_total - last_total_;
    float sum_factors = 0.0f;
    for (int i = 0; i < N; i++) {
        sum_factors += (i < (int)stretch_factors_.size()) ? stretch_factors_[i] : 1.0f;
    }

    std::vector<float> new_size(N);
    if (sum_factors > 0.0f) {
        for (int i = 0; i < N; i++) {
            float factor = (i < (int)stretch_factors_.size()) ? stretch_factors_[i] : 1.0f;
            new_size[i] = std::max(0.0f, old_size[i] + delta * (factor / sum_factors));
        }
    } else {
        // No child can grow: scale everyone proportionally so there's no gap.
        float scale = new_total / last_total_;
        for (int i = 0; i < N; i++) {
            new_size[i] = std::max(0.0f, old_size[i] * scale);
        }
    }

    float pos = 0.0f;
    for (int i = 0; i < M; i++) {
        float hs = effective_thickness(i);
        pos += new_size[i];
        ratios_[i] = std::clamp((pos + hs / 2.0f) / new_total, 0.0f, 1.0f);
        pos += hs;
    }
}

void Splitter::layout_children() {
    auto const N = (int)children_.size();
    if (N == 0) return;

    auto const total = (orientation_ == Orientation::Horizontal) ? rect_.width : rect_.height;
    if (total > 0.0f) {
        if (last_total_ >= 0.0f && std::abs(total - last_total_) > 0.5f) {
            redistribute_stretch(total);
        }
        last_total_ = total;
    }

    auto const positions = compute_positions();

    if (orientation_ == Orientation::Horizontal) {
        float start = 0.0f;
        for (int i = 0; i < N; i++) {
            float end = (i < (int)positions.size()) ? positions[i] : rect_.width;
            float hs = (i < (int)positions.size()) ? effective_thickness(i) : 0.0f;
            if (children_[i]) {
                children_[i]->set_rect({start, 0.0f, end - start, rect_.height});
            }
            start = end + hs;
        }
    } else {
        float start = 0.0f;
        for (int i = 0; i < N; i++) {
            float end = (i < (int)positions.size()) ? positions[i] : rect_.height;
            float hs = (i < (int)positions.size()) ? effective_thickness(i) : 0.0f;
            if (children_[i]) {
                children_[i]->set_rect({0.0f, start, rect_.width, end - start});
            }
            start = end + hs;
        }
    }
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void Splitter::paint(Painter &painter) {
    for (auto const &child : children_) {
        if (child) child->draw(painter);
    }

    auto const positions = compute_positions();
    auto const M = (int)positions.size();
    auto const &pal = Theme::current().palette;

    for (int i = 0; i < M; i++) {
        if (is_divider_locked(i)) continue;

        auto const pos = positions[i];
        auto const hs = effective_thickness(i);
        auto const hovered = (hovered_divider_ == i) || (dragging_divider_ == i);

        if (orientation_ == Orientation::Horizontal) {
            painter.fill_rect({pos, 0.0f, hs, rect_.height}, pal.window);
        } else {
            painter.fill_rect({0.0f, pos, rect_.width, hs}, pal.window);
        }

        auto const line_color =
            hovered ? pal.accent.with_alpha(0.6f) : pal.border.with_alpha(0.5f);
        if (orientation_ == Orientation::Horizontal) {
            painter.draw_line({pos, 0.0f}, {pos, rect_.height}, line_color, 1.0f);
            painter.draw_line({pos + hs, 0.0f}, {pos + hs, rect_.height}, line_color, 1.0f);
        } else {
            painter.draw_line({0.0f, pos}, {rect_.width, pos}, line_color, 1.0f);
            painter.draw_line({0.0f, pos + hs}, {rect_.width, pos + hs}, line_color, 1.0f);
        }

        auto const center = pos + hs / 2.0f;
        Theme::current().draw_splitter_handle(painter, center, rect_, orientation_, hovered);
    }

}

// ---------------------------------------------------------------------------
// Mouse handling
// ---------------------------------------------------------------------------

bool Splitter::handle_mouse(MouseEvent const &event) {
    auto positions = compute_positions();
    auto const M = (int)positions.size();

    auto find_hit = [&](Point p) -> std::optional<int> {
        for (int i = 0; i < M; i++) {
            if (!is_divider_locked(i) && handle_rect(i, positions).contains(p)) return i;
        }
        return std::nullopt;
    };

    switch (event.type) {
    case MouseEvent::Type::Move:
        if (!dragging_divider_) {
            auto hit = find_hit(event.position);
            if (hit != hovered_divider_) {
                hovered_divider_ = hit;
                cursor_ = hit ? (orientation_ == Orientation::Horizontal ? CursorShape::ResizeEW
                                                                          : CursorShape::ResizeNS)
                              : CursorShape::Arrow;
                if (window_) window_->request_redraw("splitter cursor");
            }
        }
        break;

    case MouseEvent::Type::Press: {
        auto hit = find_hit(event.position);
        if (hit) {
            dragging_divider_ = hit;
            cursor_ = (orientation_ == Orientation::Horizontal) ? CursorShape::ResizeEW
                                                                 : CursorShape::ResizeNS;
            return true;
        }
        break;
    }

    case MouseEvent::Type::Drag:
        if (dragging_divider_ && *dragging_divider_ < (int)ratios_.size()) {
            auto total =
                (orientation_ == Orientation::Horizontal) ? rect_.width : rect_.height;
            auto centre =
                (orientation_ == Orientation::Horizontal) ? event.position.x : event.position.y;
            if (total > 0.0f) {
                ratios_[*dragging_divider_] = std::clamp(centre / total, 0.05f, 0.95f);
            }
            layout_children();
            if (window_) window_->request_redraw("splitter drag");
            return true;
        }
        break;

    case MouseEvent::Type::Release:
        if (dragging_divider_) {
            dragging_divider_.reset();
            cursor_ = CursorShape::Arrow;
            if (window_) window_->request_redraw("splitter release");
            return true;
        }
        break;

    case MouseEvent::Type::Leave:
        if (!dragging_divider_) {
            cursor_ = CursorShape::Arrow;
            hovered_divider_.reset();
        }
        break;

    default:
        break;
    }

    // Forward to children when not dragging a handle.
    if (!dragging_divider_) {
        auto focused_inside = [&](Widget *container) -> bool {
            if (!window_) return false;
            auto *fw = window_->focused_widget();
            while (fw) {
                if (fw == container) return true;
                fw = fw->parent();
            }
            return false;
        };

        for (int i = 0; i < (int)children_.size(); i++) {
            auto &child = children_[i];
            if (child && child->rect().contains(event.position)) {
                auto shifted = event;
                shifted.position.x -= child->rect().x;
                shifted.position.y -= child->rect().y;
                return child->handle_mouse(shifted);
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Layout / window / size
// ---------------------------------------------------------------------------

void Splitter::set_rect(Rect const &rect) {
    Widget::set_rect(rect);
    layout_children();
}

void Splitter::set_window(Window *w) {
    Widget::set_window(w);
    for (auto &child : children_) {
        if (child) child->set_window(w);
    }
}

Size Splitter::size_hint() const {
    auto const N = (int)children_.size();
    Size result{};
    for (int i = 0; i < N; i++) {
        float hs = (i < N - 1) ? effective_thickness(i) : 0.0f;
        Size ch = children_[i] ? children_[i]->size_hint() : Size{};
        if (orientation_ == Orientation::Horizontal) {
            result.width += ch.width + hs;
            result.height = std::max(result.height, ch.height);
        } else {
            result.width = std::max(result.width, ch.width);
            result.height += ch.height + hs;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Focus / widget traversal
// ---------------------------------------------------------------------------

Widget *Splitter::find_focusable_at(Point p) {
    auto const positions = compute_positions();
    auto const M = (int)positions.size();
    for (int i = 0; i < M; i++) {
        if (!is_divider_locked(i) && handle_rect(i, positions).contains(p)) return this;
    }
    for (auto const &child : children_) {
        if (child && child->rect().contains(p)) {
            auto shifted = Point{p.x - child->rect().x, p.y - child->rect().y};
            return child->find_focusable_at(shifted);
        }
    }
    return nullptr;
}

Widget *Splitter::widget_at(Point p) {
    auto const positions = compute_positions();
    auto const M = (int)positions.size();
    for (int i = 0; i < M; i++) {
        if (!is_divider_locked(i) && handle_rect(i, positions).contains(p)) return this;
    }
    for (auto const &child : children_) {
        if (child && child->rect().contains(p)) {
            auto shifted = Point{p.x - child->rect().x, p.y - child->rect().y};
            return child->widget_at(shifted);
        }
    }
    return this;
}

void Splitter::collect_focusables(std::vector<Widget *> &out) {
    for (auto const &child : children_) {
        if (child) child->collect_focusables(out);
    }
}

void Splitter::collect_mnemonics(std::vector<Widget *> &out) {
    for (auto const &child : children_) {
        if (child) child->collect_mnemonics(out);
    }
}

void Splitter::for_each_child(std::function<void(Widget *)> const &callback) {
    for (auto const &child : children_) {
        if (child) callback(child.get());
    }
}

void Splitter::on_theme_changed() {
    Widget::on_theme_changed();
    for (auto const &child : children_) {
        if (child) child->on_theme_changed();
    }
}

} // namespace toolkit
