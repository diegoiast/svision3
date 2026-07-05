// X-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/icon_grid.hpp"
#include "toolkit/stopwatch.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <cmath>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace toolkit {

IconGrid::IconGrid(std::shared_ptr<ItemModel> model) : model_(std::move(model)) {
    state.focusable = true;
    if (model_) {
        model_->on_data_changed = [this] {
            scroll_x_ = scroll_y_ = 0;
            if (model_ && model_->row_count() > 0) {
                set_selected(size_t{0});
            } else {
                set_selected(std::nullopt);
            }
            invalidate_layout();
        };
    }
}

nlohmann::json IconGrid::to_json() const {
    auto j = Widget::to_json();
    j["icon_size"] = icon_size_;
    j["scale_icons"] = scale_icons_;
    j["selected_index"] = cursor_ ? nlohmann::json(*cursor_) : nlohmann::json(nullptr);
    j["selected_indices"] = selected_indices_;
    if (model_) {
        j["row_count"] = model_->row_count();
    }
    return j;
}

void IconGrid::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("icon_size")) {
        set_icon_size(j["icon_size"]);
    }
    if (j.contains("scale_icons")) {
        set_scale_icons(j["scale_icons"]);
    }
    if (j.contains("selected_index") && !j["selected_index"].is_null()) {
        set_selected(j["selected_index"].get<size_t>());
    }
}

IconGrid &IconGrid::set_model(std::shared_ptr<ItemModel> model) {
    model_ = std::move(model);
    if (model_) {
        model_->on_data_changed = [this] {
            scroll_x_ = scroll_y_ = 0;
            if (model_ && model_->row_count() > 0) {
                set_selected(size_t{0});
            } else {
                set_selected(std::nullopt);
            }
            invalidate_layout();
            if (window()) {
                window()->request_redraw("data changed");
            }
        };
    }
    invalidate_layout();
    return *this;
}

IconGrid &IconGrid::set_icon_size(int size) {
    icon_size_ = size;
    invalidate_layout();
    clamp_scroll();
    return *this;
}

IconGrid &IconGrid::set_scale_icons(bool scale) {
    scale_icons_ = scale;
    invalidate_layout();
    return *this;
}

int IconGrid::display_icon_size() const {
    if (scale_icons_) {
        return icon_size_;
    }

    static int standard_sizes[] = {16, 22, 24, 32, 48, 64, 128, 256};
    auto best = standard_sizes[0];
    auto best_diff = std::abs(standard_sizes[0] - icon_size_);
    for (auto s : standard_sizes) {
        auto diff = std::abs(s - icon_size_);
        if (diff < best_diff) {
            best_diff = diff;
            best = s;
        }
    }
    return best;
}

IconGrid &IconGrid::set_selected(std::optional<size_t> index) {
    if (cursor_ == index) {
        return *this;
    }
    auto old_indices = selected_indices_;
    cursor_ = index;
    selected_indices_.clear();
    if (cursor_) {
        selected_indices_.insert(*cursor_);
    }
    if (selected_indices_ != old_indices && on_selection_changed) {
        on_selection_changed(selected_indices_);
    }
    if (cursor_) {
        scroll_to(*cursor_);
    }
    if (window_) {
        window_->request_redraw();
    }
    return *this;
}

IconGrid &IconGrid::toggle_selection(size_t index) {
    if (selected_indices_.contains(index)) {
        selected_indices_.erase(index);
    } else {
        selected_indices_.insert(index);
    }
    cursor_ = index;
    if (on_selection_changed) {
        on_selection_changed(selected_indices_);
    }
    if (window_) {
        window_->request_redraw();
    }
    return *this;
}

IconGrid &IconGrid::select_range(size_t from, size_t to) {
    auto start = std::min(from, to);
    auto end = std::max(from, to);
    selected_indices_.clear();
    for (auto i = start; i <= end; ++i) {
        selected_indices_.insert(i);
    }
    cursor_ = to;
    if (on_selection_changed) {
        on_selection_changed(selected_indices_);
    }
    scroll_to(to);
    if (window_) {
        window_->request_redraw();
    }
    return *this;
}

IconGrid &IconGrid::select_in_rect(Rect const &r) {
    if (!model_) {
        return *this;
    }

    auto old_indices = selected_indices_;
    auto const &theme = Theme::current();
    auto const &style = theme.style.iconGrid;
    auto layout = compute_layout();
    auto count = model_->row_count();

    for (auto i = size_t{0}; i < count; ++i) {
        auto col = i % layout.columns;
        auto row = i / layout.columns;

        auto ix =
            style.padding.left + static_cast<float>(col) * (layout.item_width + style.spacing);
        auto iy =
            style.padding.top + static_cast<float>(row) * (layout.item_height + style.spacing);
        auto iw = layout.item_width;
        auto ih = layout.item_height;

        auto intersects =
            (ix < r.x + r.width) && (ix + iw > r.x) && (iy < r.y + r.height) && (iy + ih > r.y);
        if (intersects) {
            selected_indices_.insert(i);
        }
    }

    if (selected_indices_ != old_indices && on_selection_changed && !rubber_selecting_) {
        on_selection_changed(selected_indices_);
    }
    if (window_) {
        window_->request_redraw();
    }
    return *this;
}

Rect IconGrid::rubber_selection_rect() const {
    if (!rubber_selecting_) {
        return {};
    }
    auto x = std::min(rubber_start_.x, rubber_end_.x);
    auto y = std::min(rubber_start_.y, rubber_end_.y);
    auto w = std::abs(rubber_end_.x - rubber_start_.x);
    auto h = std::abs(rubber_end_.y - rubber_start_.y);
    return {x, y, w, h};
}

void IconGrid::on_scroll(float /*x*/, float /*y*/) {
    if (window()) {
        window()->request_redraw("scroll");
    }
}

void IconGrid::paint(Painter &painter) {
    if (!model_) {
        return;
    }

    auto const &theme = Theme::current();
    auto const &style = theme.style.iconGrid;
    auto const &palette = theme.palette;
    auto wstate = WidgetState{
        .interaction = ButtonState::Normal,
        .focused = is_focused(),
        .enabled = is_enabled(),
        .window_active = window() ? window()->is_active() : true,
    };
    theme.draw_list_background(painter, {0, 0, rect_.width, rect_.height}, wstate);

    auto layout = compute_layout();
    auto count = model_->row_count();
    auto vr = viewport_rect();

    painter.push_clip(vr);
    painter.push_translation({vr.x - scroll_x_, vr.y - scroll_y_});

    auto disp = display_icon_size();
    auto stats_enabled = window() && window()->is_statistics_logging_enabled();
    auto t_icon = 0.0, t_text = 0.0;
    auto visible_count = size_t{0};
    auto sw = Stopwatch{};

    for (auto i = size_t{0}; i < count; ++i) {
        auto col = i % layout.columns;
        auto row = i / layout.columns;

        auto item_rect =
            Rect{style.padding.left + static_cast<float>(col) * (layout.item_width + style.spacing),
                 style.padding.top + static_cast<float>(row) * (layout.item_height + style.spacing),
                 layout.item_width, layout.item_height};

        if (item_rect.y + item_rect.height < scroll_y_) {
            continue;
        }
        if (item_rect.y > scroll_y_ + vr.height) {
            break;
        }

        ++visible_count;
        auto is_selected = selected_indices_.contains(i);
        auto is_hovered = hovered_.has_value() && *hovered_ == i;

        if (stats_enabled) {
            sw.reset();
        }
        auto icon = model_->icon_at(i, 0, disp);
        if (stats_enabled) {
            t_icon += sw.elapsed_ms();
        }

        if (stats_enabled) {
            sw.reset();
        }
        theme.draw_icon_grid_item(painter, item_rect, model_->cell_text(i, 0), icon, is_selected,
                                  is_hovered, disp, scale_icons_);
        if (stats_enabled) {
            t_text += sw.elapsed_ms();
        }
    }

    if (stats_enabled) {
        spdlog::info("IconGrid::paint items={} visible={} icon={:.2f}ms text={:.2f}ms", count,
                     visible_count, t_icon, t_text);
    }

    if (rubber_selecting_) {
        auto rr = rubber_selection_rect();
        painter.fill_rect(rr, {0.2f, 0.2f, 0.6f, 0.3f});
        painter.draw_rect(rr, {0.2f, 0.2f, 0.6f, 1.0f}, 1.0f);
    }

    painter.pop_translation();
    painter.pop_clip();

    draw_scrollbars(painter);
}

bool IconGrid::handle_mouse(MouseEvent const &event) {
    if (!model_) {
        return false;
    }

    if (handle_scrollbar_mouse(event)) {
        return true;
    }

    auto vr = viewport_rect();
    if (!vr.contains(event.position)) {
        return false;
    }

    auto p = event.position;
    p.x -= vr.x;
    p.y -= vr.y;

    auto p_scrolled = p;
    p_scrolled.x += scroll_x_;
    p_scrolled.y += scroll_y_;
    auto index = item_at(p_scrolled);

    switch (event.type) {
    case MouseEvent::Type::Press:
        set_focused(true);
        if (event.button == 3 && on_back_requested) {
            on_back_requested();
            return true;
        }
        if (event.button == 0) {
            if (event.click_count == 2 && index && on_item_activated) {
                on_item_activated(*index);
                return true;
            }
            if (index) {
                if (event.ctrl) {
                    toggle_selection(*index);
                } else if (event.shift && cursor_) {
                    select_range(*cursor_, *index);
                } else {
                    set_selected(index);
                }
            } else {
                if (!event.ctrl) {
                    selected_indices_.clear();
                }
            }
            rubber_selecting_ = true;
            rubber_add_ = event.ctrl;
            rubber_start_ = p_scrolled;
            rubber_end_ = p_scrolled;
            select_in_rect(rubber_selection_rect());
        }
        return true;
    case MouseEvent::Type::Move:
        if (rubber_selecting_) {
            rubber_end_ = p_scrolled;
            auto rr = rubber_selection_rect();
            if (!rubber_add_) {
                selected_indices_.clear();
            }
            select_in_rect(rr);
            if (!selected_indices_.empty()) {
                cursor_ = *selected_indices_.begin();
            }
        } else {
            if (hovered_ != index) {
                hovered_ = index;
                set_tooltip(hovered_ ? model_->tooltip(*hovered_) : std::string{});
            }
        }
        if (window()) {
            window()->request_redraw("mouse move");
        }
        return true;
    case MouseEvent::Type::Drag:
        if (rubber_selecting_) {
            rubber_end_ = p_scrolled;
            auto rr = rubber_selection_rect();
            if (!rubber_add_) {
                selected_indices_.clear();
            }
            select_in_rect(rr);
            if (!selected_indices_.empty()) {
                cursor_ = *selected_indices_.begin();
            }
        }
        if (window()) {
            window()->request_redraw("mouse drag");
        }
        return true;
    case MouseEvent::Type::Release:
        if (rubber_selecting_) {
            rubber_selecting_ = false;
            rubber_add_ = false;
            if (on_selection_changed) {
                on_selection_changed(selected_indices_);
            }
        }
        if (window()) {
            window()->request_redraw("mouse release");
        }
        return true;
    case MouseEvent::Type::Leave:
        hovered_ = std::nullopt;
        set_tooltip("");
        if (window()) {
            window()->request_redraw("mouse leave");
        }
        return true;
    default:
        break;
    }

    return true;
}

bool IconGrid::handle_key(KeyEvent const &event) {
    if (!model_ || event.type != KeyEvent::Type::Press) {
        return false;
    }

    auto layout = compute_layout();
    auto count = model_->row_count();
    if (count == 0) {
        return false;
    }

    auto new_cursor = cursor_;

    switch (event.key) {
    case Key::Up:
        if (!new_cursor) {
            new_cursor = size_t{0};
        } else if (*new_cursor >= layout.columns) {
            *new_cursor -= layout.columns;
        }
        break;
    case Key::Down:
        if (!new_cursor) {
            new_cursor = size_t{0};
        } else {
            *new_cursor += layout.columns;
            if (*new_cursor >= count) {
                *new_cursor = count - 1;
            }
        }
        break;
    case Key::Left:
        if (!new_cursor) {
            new_cursor = size_t{0};
        } else if (*new_cursor > 0) {
            *new_cursor -= 1;
        }
        break;
    case Key::Right:
        if (!new_cursor) {
            new_cursor = size_t{0};
        } else if (*new_cursor < count - 1) {
            *new_cursor += 1;
        }
        break;
    case Key::Home:
        new_cursor = size_t{0};
        break;
    case Key::End:
        new_cursor = count - 1;
        break;
    case Key::A:
        if (event.ctrl) {
            selected_indices_.clear();
            for (auto i = size_t{0}; i < count; ++i) {
                selected_indices_.insert(i);
            }
            cursor_ = size_t{0};
            if (on_selection_changed) {
                on_selection_changed(selected_indices_);
            }
            if (window_) {
                window_->request_redraw();
            }
            return true;
        }
        break;
    case Key::Enter:
        if (cursor_ && on_item_activated) {
            on_item_activated(*cursor_);
        }
        return true;
    default:
        return false;
    }

    if (new_cursor != cursor_) {
        if (event.shift && cursor_) {
            auto anchor = selection_anchor_.value_or(*cursor_);
            select_range(anchor, *new_cursor);
        } else {
            selection_anchor_ = new_cursor;
            set_selected(new_cursor);
        }
        return true;
    }

    return false;
}

void IconGrid::set_rect(Rect const &rect) {
    if (rect_ == rect) {
        return;
    }

    // Attempt to keep the same top-left item visible
    auto layout = compute_layout();
    auto col = 0;
    auto row = static_cast<size_t>(
        std::floor(scroll_y_ / (layout.item_height + layout.item_height))); // Simplified

    ScrollableWidget::set_rect(rect);
    clamp_scroll();
}

Size IconGrid::size_hint() const { return {200, 200}; }

IconGrid::LayoutInfo IconGrid::compute_layout() const {
    auto const &theme = Theme::current();
    auto const &style = theme.style.iconGrid;
    auto sample_size = theme.measure_icon_grid_item("W", display_icon_size());

    auto item_w = sample_size.width;
    auto item_h = sample_size.height;

    auto available_w = rect_.width - style.padding.left - style.padding.right;
    auto cols = std::max(
        size_t{1}, static_cast<size_t>(std::floor(std::max(0.0f, available_w + style.spacing) /
                                                  (item_w + style.spacing))));

    auto count = model_ ? model_->row_count() : size_t{0};
    auto rows = (count + cols - 1) / cols;

    return {cols, rows, item_w, item_h};
}

std::optional<size_t> IconGrid::item_at(Point p) const {
    auto const &theme = Theme::current();
    auto const &style = theme.style.iconGrid;
    auto layout = compute_layout();

    auto x = p.x - style.padding.left;
    auto y = p.y - style.padding.top;

    if (x < 0 || y < 0) {
        return std::nullopt;
    }

    auto col = static_cast<size_t>(std::floor(x / (layout.item_width + style.spacing)));
    auto row = static_cast<size_t>(std::floor(y / (layout.item_height + style.spacing)));

    if (col >= layout.columns) {
        return std::nullopt;
    }

    auto item_x = static_cast<float>(col) * (layout.item_width + style.spacing);
    auto item_y = static_cast<float>(row) * (layout.item_height + style.spacing);

    if (x < item_x || x > item_x + layout.item_width || y < item_y ||
        y > item_y + layout.item_height) {
        return std::nullopt;
    }

    auto index = row * layout.columns + col;
    if (index >= (model_ ? model_->row_count() : size_t{0})) {
        return std::nullopt;
    }

    return index;
}

auto IconGrid::widget_at(Point p) -> Widget * {
    if (!hit_test(p)) {
        return nullptr;
    }
    return this;
}

void IconGrid::clamp_scroll() {
    auto const &theme = Theme::current();
    auto const &style = theme.style.iconGrid;
    auto layout = compute_layout();
    auto total_h = style.padding.top + style.padding.bottom +
                   static_cast<float>(layout.rows) * layout.item_height +
                   static_cast<float>(layout.rows > 0 ? layout.rows - 1 : 0) * style.spacing;

    update_scrollbars({0.0f, total_h});
}

void IconGrid::scroll_to(size_t index) {
    auto const &theme = Theme::current();
    auto const &style = theme.style.iconGrid;
    auto layout = compute_layout();
    auto row = index / layout.columns;
    auto item_y =
        style.padding.top + static_cast<float>(row) * (layout.item_height + style.spacing);

    auto vr = viewport_rect();
    if (item_y < scroll_y()) {
        ScrollableWidget::scroll_to(scroll_x(), item_y);
    } else if (item_y + layout.item_height > scroll_y() + vr.height) {
        ScrollableWidget::scroll_to(scroll_x(), item_y + layout.item_height - vr.height);
    }
}

} // namespace toolkit
