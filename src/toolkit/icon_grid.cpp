// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/icon_grid.hpp"
#include "toolkit/application.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <cmath>

namespace toolkit {

SimpleIconGridModel::SimpleIconGridModel(std::vector<Item> items) : items_(std::move(items)) {}

std::string SimpleIconGridModel::text_at(size_t index) const {
    if (index >= items_.size()) {
        return "";
    }
    return items_[index].text;
}

Icon SimpleIconGridModel::icon_at(size_t index, int size, bool snap) const {
    if (index >= items_.size()) {
        return nullptr;
    }

    auto &item = const_cast<Item &>(items_[index]);
    auto target_size = size;

    if (snap) {
        // Snap to standard icon sizes for better loader compatibility
        auto best_size = 16;
        static const int standard_sizes[] = {16, 22, 24, 32, 48, 64, 128, 256};
        auto min_diff = std::abs(standard_sizes[0] - size);
        for (auto s : standard_sizes) {
            auto diff = std::abs(s - size);
            if (diff < min_diff) {
                min_diff = diff;
                best_size = s;
            }
        }
        target_size = best_size;
    }

    if (item.cached_icon && item.cached_size == target_size) {
        return item.cached_icon;
    }

    // Try multiple contexts to find the icon
    static const char* contexts[] = {"", "actions", "apps", "categories", "devices", "mimetypes", "places", "status"};
    for (auto const* ctx : contexts) {
        item.cached_icon = Application::instance().load_icon(item.icon_name, target_size, ctx);
        if (item.cached_icon) {
            break;
        }
    }
    
    item.cached_size = target_size;
    return item.cached_icon;
}

void SimpleIconGridModel::set_items(std::vector<Item> items) {
    items_ = std::move(items);
    if (on_data_changed) {
        on_data_changed();
    }
}

void SimpleIconGridModel::append(Item item) {
    items_.push_back(std::move(item));
    if (on_data_changed) {
        on_data_changed();
    }
}

IconGrid::IconGrid(std::shared_ptr<IconGridModel> model) : model_(std::move(model)) {
    state.focusable = true;
    if (model_) {
        model_->on_data_changed = [this] {
            invalidate_layout();
            if (window_) {
                window_->request_redraw();
            }
        };
    }
}

IconGrid &IconGrid::set_model(std::shared_ptr<IconGridModel> model) {
    model_ = std::move(model);
    if (model_) {
        model_->on_data_changed = [this] {
            invalidate_layout();
            if (window_) {
                window_->request_redraw();
            }
        };
    }
    invalidate_layout();
    return *this;
}

IconGrid &IconGrid::set_icon_size(int size) {
    icon_size_ = size;
    invalidate_layout();
    return *this;
}

IconGrid &IconGrid::set_scale_icons(bool scale) {
    scale_icons_ = scale;
    invalidate_layout();
    if (window_) {
        window_->request_redraw();
    }
    return *this;
}

int IconGrid::display_icon_size() const {
    if (scale_icons_) {
        return icon_size_;
    }
    static const int standard_sizes[] = {16, 22, 24, 32, 48, 64, 128, 256};
    int best = standard_sizes[0];
    int best_diff = std::abs(standard_sizes[0] - icon_size_);
    for (int s : standard_sizes) {
        int diff = std::abs(s - icon_size_);
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
    cursor_ = index;
    if (on_selection_changed) {
        on_selection_changed(cursor_);
    }
    if (cursor_) {
        scroll_to(*cursor_);
    }
    if (window_) {
        window_->request_redraw();
    }
    return *this;
}

void IconGrid::paint(Painter &painter) {
    if (!model_) {
        return;
    }

    auto const &theme = Theme::current();
    auto const &style = theme.icon_grid;
    theme.draw_list_background(painter, {0, 0, rect_.width, rect_.height}, is_focused());

    auto layout = compute_layout();
    auto count = model_->count();

    painter.push_clip({0, 0, rect_.width, rect_.height});
    painter.push_translation({0, -scroll_offset_});

    for (size_t i = 0; i < count; ++i) {
        auto col = i % layout.columns;
        auto row = i / layout.columns;

        auto item_rect = Rect{
            style.padding.left + static_cast<float>(col) * (layout.item_width + style.spacing),
            style.padding.top + static_cast<float>(row) * (layout.item_height + style.spacing),
            layout.item_width,
            layout.item_height
        };

        // Simple culling
        if (item_rect.y + item_rect.height < scroll_offset_) continue;
        if (item_rect.y > scroll_offset_ + rect_.height) break;

        auto is_selected = cursor_.has_value() && *cursor_ == i;
        auto is_hovered = hovered_.has_value() && *hovered_ == i;

        auto disp = display_icon_size();
        theme.draw_icon_grid_item(painter, item_rect, model_->text_at(i),
                                  model_->icon_at(i, disp, !scale_icons_), is_selected,
                                  is_hovered, disp, scale_icons_);
    }

    painter.pop_translation();
    painter.pop_clip();
}

bool IconGrid::handle_mouse(MouseEvent const &event) {
    if (!model_) return false;

    auto p = event.position;
    
    if (event.type == MouseEvent::Type::Scroll) {
        scroll_offset_ -= event.scroll_dy * 20.0f;
        clamp_scroll();
        if (window_) window_->request_redraw();
        return true;
    }

    p.y += scroll_offset_;
    auto index = item_at(p);

    switch (event.type) {
    case MouseEvent::Type::Press:
        set_focused(true);
        set_selected(index);
        return true;
    case MouseEvent::Type::Move:
        if (hovered_ != index) {
            hovered_ = index;
            if (hovered_) {
                set_tooltip(model_->text_at(*hovered_));
            } else {
                set_tooltip("");
            }
            if (window_) window_->request_redraw();
        }
        return true;
    case MouseEvent::Type::Leave:
        hovered_ = std::nullopt;
        set_tooltip("");
        if (window_) window_->request_redraw();
        return true;
    default:
        break;
    }

    return false;
}

bool IconGrid::handle_key(KeyEvent const &event) {
    if (!model_ || event.type != KeyEvent::Type::Press) return false;

    auto layout = compute_layout();
    auto count = model_->count();
    if (count == 0) return false;

    auto new_cursor = cursor_;

    switch (event.key) {
    case Key::Up:
        if (!new_cursor) new_cursor = 0;
        else if (*new_cursor >= layout.columns) *new_cursor -= layout.columns;
        break;
    case Key::Down:
        if (!new_cursor) new_cursor = 0;
        else {
            *new_cursor += layout.columns;
            if (*new_cursor >= count) *new_cursor = count - 1;
        }
        break;
    case Key::Left:
        if (!new_cursor) new_cursor = 0;
        else if (*new_cursor > 0) *new_cursor -= 1;
        break;
    case Key::Right:
        if (!new_cursor) new_cursor = 0;
        else if (*new_cursor < count - 1) *new_cursor += 1;
        break;
    case Key::Home:
        new_cursor = 0;
        break;
    case Key::End:
        new_cursor = count - 1;
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
        set_selected(new_cursor);
        return true;
    }

    return false;
}

void IconGrid::set_rect(Rect const &rect) {
    if (rect_ == rect) return;
    auto first = first_visible_item();
    Widget::set_rect(rect);
    scroll_to(first);
}

size_t IconGrid::first_visible_item() const {
    auto const &theme = Theme::current();
    auto const &style = theme.icon_grid;
    auto layout = compute_layout();

    auto y = scroll_offset_ - style.padding.top;
    if (y <= 0) return 0;
    
    auto row = static_cast<size_t>(std::floor(y / (layout.item_height + style.spacing)));
    return row * layout.columns;
}

Size IconGrid::size_hint() const {
    return {200, 200};
}

IconGrid::LayoutInfo IconGrid::compute_layout() const {
    auto const &theme = Theme::current();
    auto const &style = theme.icon_grid;
    auto sample_size = theme.measure_icon_grid_item("W", display_icon_size());
    
    auto item_w = sample_size.width;
    auto item_h = sample_size.height;
    
    auto available_w = rect_.width - style.padding.left - style.padding.right;
    auto cols = std::max(size_t{1}, static_cast<size_t>(std::floor((available_w + style.spacing) / (item_w + style.spacing))));
    
    auto count = model_ ? model_->count() : 0;
    auto rows = (count + cols - 1) / cols;

    return {cols, rows, item_w, item_h};
}

std::optional<size_t> IconGrid::item_at(Point p) const {
    auto const &theme = Theme::current();
    auto const &style = theme.icon_grid;
    auto layout = compute_layout();
    
    auto x = p.x - style.padding.left;
    auto y = p.y - style.padding.top;

    if (x < 0 || y < 0) return std::nullopt;
    
    auto col = static_cast<size_t>(std::floor(x / (layout.item_width + style.spacing)));
    auto row = static_cast<size_t>(std::floor(y / (layout.item_height + style.spacing)));
    
    if (col >= layout.columns) return std::nullopt;
    
    // Check if we are actually over the item (not in the spacing)
    auto item_x = static_cast<float>(col) * (layout.item_width + style.spacing);
    auto item_y = static_cast<float>(row) * (layout.item_height + style.spacing);
    
    if (x < item_x || x > item_x + layout.item_width ||
        y < item_y || y > item_y + layout.item_height) {
        return std::nullopt;
    }

    auto index = row * layout.columns + col;
    if (index >= (model_ ? model_->count() : 0)) return std::nullopt;
    
    return index;
}

void IconGrid::clamp_scroll() {
    auto const &theme = Theme::current();
    auto const &style = theme.icon_grid;
    auto layout = compute_layout();
    auto total_h = style.padding.top + style.padding.bottom + 
                   static_cast<float>(layout.rows) * layout.item_height +
                   static_cast<float>(std::max(size_t{0}, layout.rows - 1)) * style.spacing;
    auto max_scroll = std::max(0.0f, total_h - rect_.height);
    scroll_offset_ = std::clamp(scroll_offset_, 0.0f, max_scroll);
}

void IconGrid::scroll_to(size_t index) {
    auto const &theme = Theme::current();
    auto const &style = theme.icon_grid;
    auto layout = compute_layout();
    auto row = index / layout.columns;
    auto item_y = style.padding.top + static_cast<float>(row) * (layout.item_height + style.spacing);
    
    if (item_y < scroll_offset_) {
        scroll_offset_ = item_y;
    } else if (item_y + layout.item_height > scroll_offset_ + rect_.height) {
        scroll_offset_ = item_y + layout.item_height - rect_.height;
    }
    clamp_scroll();
}

} // namespace toolkit
