// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/layout.hpp"
#include "toolkit/widget_loader.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>

namespace toolkit {

static auto clamp_dim(float val, float lo, float hi) -> float {
    if (lo > 0 && val < lo) {
        val = lo;
    }
    if (hi > 0 && val > hi) {
        val = hi;
    }
    return val;
}

static auto parse_alignment(std::string const &s) -> Alignment {
    if (s == "start") {
        return Alignment::Start;
    }
    if (s == "center") {
        return Alignment::Center;
    }
    if (s == "end") {
        return Alignment::End;
    }
    return Alignment::Fill;
}

void AbstractLayout::set_rect(Rect const &rect) {
    Widget::set_rect(rect);
    apply_layout();
    state.layout_dirty = false;
}

void AbstractLayout::set_window(Window *win) {
    Widget::set_window(win);
    for_each_child([win](Widget *child) { child->set_window(win); });
}

void AbstractLayout::paint(Painter &painter) {
    if (state.layout_dirty) {
        apply_layout();
        state.layout_dirty = false;
    }
    for_each_child([&painter](Widget *child) { child->draw(painter); });
}

auto AbstractLayout::handle_mouse(MouseEvent const &event) -> bool {
    auto handled = false;
    auto stop = false;

    if (state.layout_dirty) {
        apply_layout();
        state.layout_dirty = false;
    }

    for_each_child([&](Widget *child) {
        if (stop || !child->is_visible()) {
            return;
        }
        if (Widget::dispatch_mouse_event(child, event)) {
            handled = true;
            if (event.type != MouseEvent::Type::Move && event.type != MouseEvent::Type::Drag) {
                stop = true;
            }
        }
    });
    return handled;
}

bool AbstractLayout::handle_key(KeyEvent const &event) {
    auto result = false;
    auto stop = false;
    for_each_child([&](Widget *child) {
        if (stop || !child->is_visible()) {
            return;
        }
        if (!child->is_focused() && !child->can_get_non_focus_input()) {
            return;
        }
        if (child->handle_key(event)) {
            result = true;
            stop = true;
        }
    });
    return result;
}

void AbstractLayout::collect_focusables(std::vector<Widget *> &out) {
    for_each_child([&](Widget *child) {
        if (child->is_visible()) {
            child->collect_focusables(out);
        }
    });
}

void AbstractLayout::collect_mnemonics(std::vector<Widget *> &out) {
    for_each_child([&](Widget *child) {
        if (child->is_visible()) {
            child->collect_mnemonics(out);
        }
    });
}

auto AbstractLayout::find_focusable_at(Point p) -> Widget * {
    if (state.layout_dirty) {
        apply_layout();
        state.layout_dirty = false;
    }
    Widget *result = nullptr;
    for_each_child([&](Widget *child) {
        if (result || !child->is_visible()) {
            return;
        }
        auto local_p = p;
        local_p.x -= child->rect().x;
        local_p.y -= child->rect().y;
        result = child->find_focusable_at(local_p);
    });
    return result;
}

auto AbstractLayout::widget_at(Point p) -> Widget * {
    if (!is_visible()) {
        return nullptr;
    }
    if (state.layout_dirty) {
        apply_layout();
        state.layout_dirty = false;
    }
    Widget *result = nullptr;
    for_each_child([&](Widget *child) {
        if (result || !child->is_visible()) {
            return;
        }
        auto local_p = p;
        local_p.x -= child->rect().x;
        local_p.y -= child->rect().y;
        result = child->widget_at(local_p);
    });
    return result;
}

VBoxLayout::VBoxLayout() {}

void VBoxLayout::for_each_child(std::function<void(Widget *)> const &callback) {
    for (auto &item : items_) {
        callback(item.widget.get());
    }
}

auto VBoxLayout::release_item(int index) -> std::unique_ptr<Widget> {
    if (index < 0 || index >= static_cast<int>(items_.size())) {
        return nullptr;
    }
    auto widget = std::move(items_[index].widget);
    items_.erase(items_.begin() + index);
    widget->set_parent(nullptr);
    widget->set_window(nullptr);
    return widget;
}

void VBoxLayout::add_widget(std::unique_ptr<Widget> widget, int stretch, Alignment h_align) {
    widget->set_parent(this);
    if (window_) {
        widget->set_window(window_);
    }
    items_.push_back({std::move(widget), stretch, h_align});
    if (rect_.width > 0 || rect_.height > 0) {
        apply_layout();
    }
}

void VBoxLayout::apply_layout() {
    auto content_x = margins_.left;
    auto content_y = margins_.top;
    auto content_w = rect_.width - margins_.left - margins_.right;
    auto content_h = rect_.height - margins_.top - margins_.bottom;
    auto visible_count = 0;
    auto total_spacing = 0.0f;
    auto available_height = 0.0f;
    auto fixed_height = 0.0f;
    auto total_stretch = 0;
    auto remaining_height = 0.0f;
    auto stretch_unit = 0.0f;
    auto current_y = 0.0f;

    if (items_.empty()) {
        return;
    }

    for (auto const &item : items_) {
        if (item.widget->is_visible()) {
            visible_count++;
        }
    }
    if (visible_count == 0) {
        return;
    }

    total_spacing = spacing_ * (visible_count - 1);
    available_height = content_h - total_spacing;

    for (auto const &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        if (item.stretch == 0) {
            auto mins = item.widget->min_size();
            auto maxs = item.widget->max_size();
            fixed_height += clamp_dim(item.widget->size_hint().height, mins.height, maxs.height);
        } else {
            total_stretch += item.stretch;
        }
    }

    remaining_height = std::max(0.0f, available_height - fixed_height);
    stretch_unit = total_stretch > 0 ? remaining_height / total_stretch : 0.0f;
    current_y = content_y;

    for (auto &item : items_) {
        auto item_w = 0.0f;
        auto item_h = 0.0f;
        auto item_x = 0.0f;
        auto mins = Size{};
        auto maxs = Size{};

        if (!item.widget->is_visible()) {
            continue;
        }

        item_w = content_w;
        item_x = content_x;
        mins = item.widget->min_size();
        maxs = item.widget->max_size();
        item_h = item.stretch == 0 ? item.widget->size_hint().height : stretch_unit * item.stretch;
        item_h = clamp_dim(item_h, mins.height, maxs.height);

        if (item.h_align != Alignment::Fill) {
            item_w = item.widget->size_hint().width;
        } else {
            item_w = content_w;
        }
        item_w = clamp_dim(item_w, mins.width, maxs.width);

        if (item.h_align != Alignment::Fill) {
            switch (item.h_align) {
            case Alignment::Center:
                item_x = content_x + (content_w - item_w) / 2.0f;
                break;
            case Alignment::End:
                item_x = content_x + content_w - item_w;
                break;
            case Alignment::Start:
            default:
                break;
            }
        }
        item.widget->set_rect({item_x, current_y, item_w, item_h});
        current_y += item_h + spacing_;
    }
}

auto VBoxLayout::size_hint() const -> Size {
    auto w = 0.0f;
    auto h = 0.0f;
    auto visible_count = 0;

    for (auto const &item : items_) {
        auto hint = Size{};

        if (!item.widget->is_visible()) {
            continue;
        }
        hint = item.widget->size_hint();
        w = std::max(w, hint.width);
        h += hint.height;
        visible_count++;
    }
    if (visible_count > 1) {
        h += spacing_ * (visible_count - 1);
    }
    h += margins_.top + margins_.bottom;
    w += margins_.left + margins_.right;
    return {w, h};
}

nlohmann::json VBoxLayout::to_json() const {
    auto j = Widget::to_json();
    j["spacing"] = spacing_;
    j["margins"] = {{"top", margins_.top},
                    {"bottom", margins_.bottom},
                    {"left", margins_.left},
                    {"right", margins_.right}};
    auto children = nlohmann::json::array();
    for (auto const &item : items_) {
        auto child = item.widget->to_json();
        child["stretch"] = item.stretch;
        static constexpr const char *alignment_names[] = {"fill", "start", "center", "end"};
        child["h_align"] = alignment_names[static_cast<int>(item.h_align)];
        children.push_back(child);
    }
    j["children"] = children;
    return j;
}

void VBoxLayout::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("spacing")) {
        set_spacing(j["spacing"]);
    }
    if (j.contains("margins")) {
        auto const &m = j["margins"];
        margins_.top = m.value("top", 0.0f);
        margins_.bottom = m.value("bottom", 0.0f);
        margins_.left = m.value("left", 0.0f);
        margins_.right = m.value("right", 0.0f);
    }
    if (j.contains("children") && j["children"].is_array()) {
        items_.clear();
        for (auto const &child_j : j["children"]) {
            auto child = WidgetLoader::instance().create_widget(child_j);
            if (child) {
                auto stretch = child_j.value("stretch", 0);
                auto h_align = parse_alignment(child_j.value("h_align", std::string{"fill"}));
                add_widget(std::move(child), stretch, h_align);
            }
        }
    }
}

HBoxLayout::HBoxLayout() {}

void HBoxLayout::for_each_child(std::function<void(Widget *)> const &callback) {
    for (auto &item : items_) {
        callback(item.widget.get());
    }
}

auto HBoxLayout::release_item(int index) -> std::unique_ptr<Widget> {
    if (index < 0 || index >= static_cast<int>(items_.size())) {
        return nullptr;
    }
    auto widget = std::move(items_[index].widget);
    items_.erase(items_.begin() + index);
    widget->set_parent(nullptr);
    widget->set_window(nullptr);
    return widget;
}

void HBoxLayout::add_widget(std::unique_ptr<Widget> widget, int stretch, Alignment v_align) {
    widget->set_parent(this);
    if (window_) {
        widget->set_window(window_);
    }
    items_.push_back({std::move(widget), stretch, v_align});
    if (rect_.width > 0 || rect_.height > 0) {
        apply_layout();
    }
}

void HBoxLayout::apply_layout() {
    auto content_x = margins_.left;
    auto content_y = margins_.top;
    auto content_w = rect_.width - margins_.left - margins_.right;
    auto content_h = rect_.height - margins_.top - margins_.bottom;
    auto visible_count = 0;
    auto total_spacing = 0.0f;
    auto available_width = 0.0f;
    auto fixed_width = 0.0f;
    auto total_stretch = 0;
    auto remaining_width = 0.0f;
    auto stretch_unit = 0.0f;
    auto current_x = 0.0f;

    if (items_.empty()) {
        return;
    }

    for (auto const &item : items_) {
        if (item.widget->is_visible()) {
            visible_count++;
        }
    }
    if (visible_count == 0) {
        return;
    }

    total_spacing = spacing_ * (visible_count - 1);
    available_width = content_w - total_spacing;

    for (auto const &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        if (item.stretch == 0) {
            auto mins = item.widget->min_size();
            auto maxs = item.widget->max_size();
            fixed_width += clamp_dim(item.widget->size_hint().width, mins.width, maxs.width);
        } else {
            total_stretch += item.stretch;
        }
    }

    remaining_width = std::max(0.0f, available_width - fixed_width);
    stretch_unit = total_stretch > 0 ? remaining_width / total_stretch : 0.0f;
    current_x = content_x;

    for (auto &item : items_) {
        auto item_h = 0.0f;
        auto item_y = 0.0f;
        auto mins = Size{};
        auto maxs = Size{};
        auto item_w = 0.0f;

        if (!item.widget->is_visible()) {
            continue;
        }

        item_h = content_h;
        item_y = content_y;
        mins = item.widget->min_size();
        maxs = item.widget->max_size();
        item_w = item.stretch == 0 ? item.widget->size_hint().width : stretch_unit * item.stretch;
        item_w = clamp_dim(item_w, mins.width, maxs.width);

        if (item.v_align != Alignment::Fill) {
            item_h = item.widget->size_hint().height;
        } else {
            item_h = content_h;
        }
        item_h = clamp_dim(item_h, mins.height, maxs.height);

        if (item.v_align != Alignment::Fill) {
            switch (item.v_align) {
            case Alignment::Center:
                item_y = content_y + (content_h - item_h) / 2.0f;
                break;
            case Alignment::End:
                item_y = content_y + content_h - item_h;
                break;
            case Alignment::Start:
            default:
                break;
            }
        }

        item.widget->set_rect({current_x, item_y, item_w, item_h});
        current_x += item_w + spacing_;
    }
}

auto HBoxLayout::size_hint() const -> Size {
    auto w = 0.0f;
    auto h = 0.0f;
    auto visible_count = 0;

    for (auto const &item : items_) {
        auto hint = Size{};

        if (!item.widget->is_visible()) {
            continue;
        }
        hint = item.widget->size_hint();
        h = std::max(h, hint.height);
        w += hint.width;
        visible_count++;
    }
    if (visible_count > 1) {
        w += spacing_ * (visible_count - 1);
    }
    w += margins_.left + margins_.right;
    h += margins_.top + margins_.bottom;
    return {w, h};
}

nlohmann::json HBoxLayout::to_json() const {
    auto j = Widget::to_json();
    j["spacing"] = spacing_;
    j["margins"] = {{"top", margins_.top},
                    {"bottom", margins_.bottom},
                    {"left", margins_.left},
                    {"right", margins_.right}};
    auto children = nlohmann::json::array();
    for (auto const &item : items_) {
        auto child = item.widget->to_json();
        child["stretch"] = item.stretch;
        static constexpr const char *alignment_names[] = {"fill", "start", "center", "end"};
        child["v_align"] = alignment_names[static_cast<int>(item.v_align)];
        children.push_back(child);
    }
    j["children"] = children;
    return j;
}

void HBoxLayout::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("spacing")) {
        set_spacing(j["spacing"]);
    }
    if (j.contains("margins")) {
        auto const &m = j["margins"];
        margins_.top = m.value("top", 0.0f);
        margins_.bottom = m.value("bottom", 0.0f);
        margins_.left = m.value("left", 0.0f);
        margins_.right = m.value("right", 0.0f);
    }
    if (j.contains("children") && j["children"].is_array()) {
        items_.clear();
        for (auto const &child_j : j["children"]) {
            auto child = WidgetLoader::instance().create_widget(child_j);
            if (child) {
                auto stretch = child_j.value("stretch", 0);
                auto v_align = parse_alignment(child_j.value("v_align", std::string{"center"}));
                add_widget(std::move(child), stretch, v_align);
            }
        }
    }
}

GridLayout::GridLayout() {}

void GridLayout::for_each_child(std::function<void(Widget *)> const &callback) {
    for (auto &item : items_) {
        callback(item.widget.get());
    }
}

void GridLayout::add_widget(std::unique_ptr<Widget> widget, int row, int col, int rowspan,
                            int colspan, Alignment h_align, Alignment v_align) {
    widget->set_parent(this);
    if (window_) {
        widget->set_window(window_);
    }
    items_.push_back({std::move(widget), row, col, rowspan, colspan, h_align, v_align});
    if (rect_.width > 0 || rect_.height > 0) {
        apply_layout();
    }
}

void GridLayout::apply_layout() {
    auto num_rows = 0;
    auto num_cols = 0;

    for (auto const &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        num_rows = std::max(num_rows, item.row + item.rowspan);
        num_cols = std::max(num_cols, item.col + item.colspan);
    }
    if (num_rows == 0 || num_cols == 0) {
        return;
    }

    auto content_x = margins_.left;
    auto content_y = margins_.top;
    auto content_w = rect_.width - margins_.left - margins_.right;
    auto content_h = rect_.height - margins_.top - margins_.bottom;

    auto row_heights = std::vector<float>(num_rows, 0.0f);
    auto col_widths = std::vector<float>(num_cols, 0.0f);

    for (auto const &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        auto hint = item.widget->size_hint();
        if (item.rowspan == 1) {
            row_heights[item.row] = std::max(row_heights[item.row], hint.height);
        }
        if (item.colspan == 1) {
            col_widths[item.col] = std::max(col_widths[item.col], hint.width);
        }
    }

    auto total_col_spacing = col_spacing_ * (num_cols - 1);
    auto total_row_spacing = row_spacing_ * (num_rows - 1);
    auto fixed_w = 0.0f;
    auto fixed_h = 0.0f;
    auto total_col_stretch = 0;
    auto total_row_stretch = 0;

    for (auto c = 0; c < num_cols; ++c) {
        auto it = col_stretch_.find(c);
        if (it != col_stretch_.end() && it->second > 0) {
            total_col_stretch += it->second;
        } else {
            fixed_w += col_widths[c];
        }
    }
    for (auto r = 0; r < num_rows; ++r) {
        auto it = row_stretch_.find(r);
        if (it != row_stretch_.end() && it->second > 0) {
            total_row_stretch += it->second;
        } else {
            fixed_h += row_heights[r];
        }
    }

    auto remaining_w = std::max(0.0f, content_w - total_col_spacing - fixed_w);
    auto remaining_h = std::max(0.0f, content_h - total_row_spacing - fixed_h);
    auto col_stretch_unit = total_col_stretch > 0 ? remaining_w / total_col_stretch : 0.0f;
    auto row_stretch_unit = total_row_stretch > 0 ? remaining_h / total_row_stretch : 0.0f;

    for (auto c = 0; c < num_cols; ++c) {
        auto it = col_stretch_.find(c);
        if (it != col_stretch_.end() && it->second > 0) {
            col_widths[c] = col_stretch_unit * it->second;
        }
    }
    for (auto r = 0; r < num_rows; ++r) {
        auto it = row_stretch_.find(r);
        if (it != row_stretch_.end() && it->second > 0) {
            row_heights[r] = row_stretch_unit * it->second;
        }
    }

    auto col_offsets = std::vector<float>(num_cols);
    auto row_offsets = std::vector<float>(num_rows);

    col_offsets[0] = content_x;
    for (auto c = 1; c < num_cols; ++c) {
        col_offsets[c] = col_offsets[c - 1] + col_widths[c - 1] + col_spacing_;
    }
    row_offsets[0] = content_y;
    for (auto r = 1; r < num_rows; ++r) {
        row_offsets[r] = row_offsets[r - 1] + row_heights[r - 1] + row_spacing_;
    }

    for (auto &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }

        auto cell_x = col_offsets[item.col];
        auto cell_y = row_offsets[item.row];
        auto cell_w = 0.0f;
        auto cell_h = 0.0f;

        for (auto c = item.col; c < item.col + item.colspan && c < num_cols; ++c) {
            cell_w += col_widths[c];
            if (c > item.col) {
                cell_w += col_spacing_;
            }
        }
        for (auto r = item.row; r < item.row + item.rowspan && r < num_rows; ++r) {
            cell_h += row_heights[r];
            if (r > item.row) {
                cell_h += row_spacing_;
            }
        }

        auto hint = item.widget->size_hint();
        auto mins = item.widget->min_size();
        auto maxs = item.widget->max_size();
        auto item_x = cell_x;
        auto item_y = cell_y;
        auto item_w = cell_w;
        auto item_h = cell_h;

        if (item.h_align != Alignment::Fill) {
            item_w = clamp_dim(hint.width, mins.width, maxs.width);
            switch (item.h_align) {
            case Alignment::Center:
                item_x = cell_x + (cell_w - item_w) / 2.0f;
                break;
            case Alignment::End:
                item_x = cell_x + cell_w - item_w;
                break;
            default:
                break;
            }
        } else {
            item_w = clamp_dim(cell_w, mins.width, maxs.width);
        }

        if (item.v_align != Alignment::Fill) {
            item_h = clamp_dim(hint.height, mins.height, maxs.height);
            switch (item.v_align) {
            case Alignment::Center:
                item_y = cell_y + (cell_h - item_h) / 2.0f;
                break;
            case Alignment::End:
                item_y = cell_y + cell_h - item_h;
                break;
            default:
                break;
            }
        } else {
            item_h = clamp_dim(cell_h, mins.height, maxs.height);
        }

        item.widget->set_rect({item_x, item_y, item_w, item_h});
    }
}

auto GridLayout::size_hint() const -> Size {
    auto num_rows = 0;
    auto num_cols = 0;

    for (auto const &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        num_rows = std::max(num_rows, item.row + item.rowspan);
        num_cols = std::max(num_cols, item.col + item.colspan);
    }
    if (num_rows == 0 || num_cols == 0) {
        return {0, 0};
    }

    auto row_heights = std::vector<float>(num_rows, 0.0f);
    auto col_widths = std::vector<float>(num_cols, 0.0f);

    for (auto const &item : items_) {
        if (!item.widget->is_visible()) {
            continue;
        }
        auto hint = item.widget->size_hint();
        if (item.rowspan == 1) {
            row_heights[item.row] = std::max(row_heights[item.row], hint.height);
        }
        if (item.colspan == 1) {
            col_widths[item.col] = std::max(col_widths[item.col], hint.width);
        }
    }

    auto w = margins_.left + margins_.right;
    auto h = margins_.top + margins_.bottom;
    for (auto c = 0; c < num_cols; ++c) {
        w += col_widths[c];
    }
    for (auto r = 0; r < num_rows; ++r) {
        h += row_heights[r];
    }
    if (num_cols > 1) {
        w += col_spacing_ * (num_cols - 1);
    }
    if (num_rows > 1) {
        h += row_spacing_ * (num_rows - 1);
    }
    return {w, h};
}

nlohmann::json GridLayout::to_json() const {
    auto j = Widget::to_json();
    j["col_spacing"] = col_spacing_;
    j["row_spacing"] = row_spacing_;
    j["margins"] = {{"top", margins_.top},
                    {"bottom", margins_.bottom},
                    {"left", margins_.left},
                    {"right", margins_.right}};

    auto col_stretch_j = nlohmann::json::object();
    for (auto const &[col, stretch] : col_stretch_) {
        col_stretch_j[std::to_string(col)] = stretch;
    }
    j["col_stretch"] = col_stretch_j;

    auto row_stretch_j = nlohmann::json::object();
    for (auto const &[row, stretch] : row_stretch_) {
        row_stretch_j[std::to_string(row)] = stretch;
    }
    j["row_stretch"] = row_stretch_j;

    static constexpr const char *alignment_names[] = {"fill", "start", "center", "end"};
    auto children = nlohmann::json::array();
    for (auto const &item : items_) {
        auto child = item.widget->to_json();
        child["row"] = item.row;
        child["col"] = item.col;
        child["rowspan"] = item.rowspan;
        child["colspan"] = item.colspan;
        child["h_align"] = alignment_names[static_cast<int>(item.h_align)];
        child["v_align"] = alignment_names[static_cast<int>(item.v_align)];
        children.push_back(child);
    }
    j["children"] = children;
    return j;
}

void GridLayout::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("col_spacing")) {
        col_spacing_ = j["col_spacing"];
    }
    if (j.contains("row_spacing")) {
        row_spacing_ = j["row_spacing"];
    }
    if (j.contains("margins")) {
        auto const &m = j["margins"];
        margins_.top = m.value("top", 0.0f);
        margins_.bottom = m.value("bottom", 0.0f);
        margins_.left = m.value("left", 0.0f);
        margins_.right = m.value("right", 0.0f);
    }
    if (j.contains("col_stretch") && j["col_stretch"].is_object()) {
        for (auto const &[key, val] : j["col_stretch"].items()) {
            col_stretch_[std::stoi(key)] = val.get<int>();
        }
    }
    if (j.contains("row_stretch") && j["row_stretch"].is_object()) {
        for (auto const &[key, val] : j["row_stretch"].items()) {
            row_stretch_[std::stoi(key)] = val.get<int>();
        }
    }
    if (j.contains("children") && j["children"].is_array()) {
        items_.clear();
        for (auto const &child_j : j["children"]) {
            auto child = WidgetLoader::instance().create_widget(child_j);
            if (child) {
                auto row = child_j.value("row", 0);
                auto col = child_j.value("col", 0);
                auto rowspan = child_j.value("rowspan", 1);
                auto colspan = child_j.value("colspan", 1);
                auto h_align = parse_alignment(child_j.value("h_align", std::string{"fill"}));
                auto v_align = parse_alignment(child_j.value("v_align", std::string{"fill"}));
                add_widget(std::move(child), row, col, rowspan, colspan, h_align, v_align);
            }
        }
    }
}

StackedLayout::StackedLayout() {}

void StackedLayout::for_each_child(std::function<void(Widget *)> const &callback) {
    for (auto &item : items_) {
        callback(item.get());
    }
}

void StackedLayout::add_widget(std::unique_ptr<Widget> widget) {
    widget->set_parent(this);
    if (window_) {
        widget->set_window(window_);
    }
    widget->set_visible(false);
    items_.push_back(std::move(widget));
    if (current_ < 0) {
        set_current(0);
    }
}

void StackedLayout::remove_widget(int index) {
    if (index < 0 || index >= static_cast<int>(items_.size())) {
        return;
    }
    items_.erase(items_.begin() + index);
    auto n = static_cast<int>(items_.size());
    if (n == 0) {
        current_ = -1;
    } else if (current_ >= n) {
        current_ = n - 1;
        items_[current_]->set_visible(true);
        apply_layout();
    } else if (current_ == index) {
        current_ = std::min(index, n - 1);
        items_[current_]->set_visible(true);
        apply_layout();
    } else if (current_ > index) {
        current_--;
    }
}

void StackedLayout::swap_widgets(int a, int b) {
    auto n = static_cast<int>(items_.size());
    if (a < 0 || a >= n || b < 0 || b >= n || a == b) {
        return;
    }
    std::swap(items_[a], items_[b]);
    if (current_ == a) {
        current_ = b;
    } else if (current_ == b) {
        current_ = a;
    }
}

StackedLayout &StackedLayout::set_current(int index) {
    if (index < 0 || index >= static_cast<int>(items_.size())) {
        return *this;
    }
    if (current_ >= 0 && current_ < static_cast<int>(items_.size())) {
        items_[current_]->set_visible(false);
    }
    current_ = index;
    items_[current_]->set_visible(true);
    apply_layout();
    return *this;
}

void StackedLayout::apply_layout() {
    if (current_ < 0 || current_ >= static_cast<int>(items_.size())) {
        return;
    }
    auto x = margins_.left;
    auto y = margins_.top;
    auto w = rect_.width - margins_.left - margins_.right;
    auto h = rect_.height - margins_.top - margins_.bottom;
    items_[current_]->set_rect({x, y, w, h});
}

auto StackedLayout::size_hint() const -> Size {
    if (current_ >= 0 && current_ < static_cast<int>(items_.size())) {
        auto hint = items_[current_]->size_hint();
        return {hint.width + margins_.left + margins_.right,
                hint.height + margins_.top + margins_.bottom};
    }
    return {margins_.left + margins_.right, margins_.top + margins_.bottom};
}

nlohmann::json StackedLayout::to_json() const {
    auto j = Widget::to_json();
    j["current"] = current_;
    j["margins"] = {{"top", margins_.top},
                    {"bottom", margins_.bottom},
                    {"left", margins_.left},
                    {"right", margins_.right}};
    auto children = nlohmann::json::array();
    for (auto const &item : items_) {
        children.push_back(item->to_json());
    }
    j["children"] = children;
    return j;
}

void StackedLayout::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("margins")) {
        auto const &m = j["margins"];
        margins_.top = m.value("top", 0.0f);
        margins_.bottom = m.value("bottom", 0.0f);
        margins_.left = m.value("left", 0.0f);
        margins_.right = m.value("right", 0.0f);
    }
    if (j.contains("children") && j["children"].is_array()) {
        items_.clear();
        current_ = -1;
        for (auto const &child_j : j["children"]) {
            auto child = WidgetLoader::instance().create_widget(child_j);
            if (child) {
                add_widget(std::move(child));
            }
        }
    }
    if (j.contains("current")) {
        set_current(j["current"].get<int>());
    }
}

FormLayout::FormLayout() {}

void FormLayout::for_each_child(std::function<void(Widget *)> const &callback) {
    for (auto &row : rows_) {
        if (row.label) {
            callback(row.label.get());
        }
        if (row.field) {
            callback(row.field.get());
        }
    }
}

void FormLayout::add_row(std::unique_ptr<Widget> label, std::unique_ptr<Widget> field) {
    if (label) {
        label->set_parent(this);
        if (window_) {
            label->set_window(window_);
        }
    }
    if (field) {
        field->set_parent(this);
        if (window_) {
            field->set_window(window_);
        }
    }
    rows_.push_back({std::move(label), std::move(field)});
    if (rect_.width > 0 || rect_.height > 0) {
        apply_layout();
    }
}

void FormLayout::apply_layout() {
    if (rows_.empty()) {
        return;
    }

    auto content_x = margins_.left;
    auto content_y = margins_.top;
    auto content_w = rect_.width - margins_.left - margins_.right;
    auto label_w = 0.0f;
    auto visible_count = 0;

    for (auto const &row : rows_) {
        auto label_vis = row.label && row.label->is_visible();
        auto field_vis = row.field && row.field->is_visible();
        if (!label_vis && !field_vis) {
            continue;
        }
        visible_count++;
        if (label_vis) {
            label_w = std::max(label_w, row.label->size_hint().width);
        }
    }
    if (visible_count == 0) {
        return;
    }

    auto field_x = content_x + label_w + label_spacing_;
    auto field_w = content_w - label_w - label_spacing_;
    auto current_y = content_y;

    for (auto &row : rows_) {
        auto label_vis = row.label && row.label->is_visible();
        auto field_vis = row.field && row.field->is_visible();
        if (!label_vis && !field_vis) {
            continue;
        }

        auto row_h = 0.0f;
        if (label_vis) {
            row_h = std::max(row_h, row.label->size_hint().height);
        }
        if (field_vis) {
            row_h = std::max(row_h, row.field->size_hint().height);
        }

        if (label_vis) {
            auto mins = row.label->min_size();
            auto maxs = row.label->max_size();
            auto h = clamp_dim(row.label->size_hint().height, mins.height, maxs.height);
            auto y = current_y + (row_h - h) / 2.0f;
            row.label->set_rect({content_x, y, label_w, h});
        }
        if (field_vis) {
            auto mins = row.field->min_size();
            auto maxs = row.field->max_size();
            auto h = clamp_dim(row.field->size_hint().height, mins.height, maxs.height);
            auto y = current_y + (row_h - h) / 2.0f;
            row.field->set_rect({field_x, y, field_w, h});
        }

        current_y += row_h + spacing_;
    }
}

auto FormLayout::size_hint() const -> Size {
    auto label_w = 0.0f;
    auto field_w = 0.0f;
    auto h = 0.0f;
    auto visible_count = 0;

    for (auto const &row : rows_) {
        auto label_vis = row.label && row.label->is_visible();
        auto field_vis = row.field && row.field->is_visible();
        if (!label_vis && !field_vis) {
            continue;
        }
        auto row_h = 0.0f;
        if (label_vis) {
            label_w = std::max(label_w, row.label->size_hint().width);
            row_h = std::max(row_h, row.label->size_hint().height);
        }
        if (field_vis) {
            field_w = std::max(field_w, row.field->size_hint().width);
            row_h = std::max(row_h, row.field->size_hint().height);
        }
        h += row_h;
        visible_count++;
    }

    if (visible_count > 1) {
        h += spacing_ * (visible_count - 1);
    }
    h += margins_.top + margins_.bottom;
    auto w = margins_.left + margins_.right + label_w + label_spacing_ + field_w;
    return {w, h};
}

nlohmann::json FormLayout::to_json() const {
    auto j = Widget::to_json();
    j["spacing"] = spacing_;
    j["label_spacing"] = label_spacing_;
    j["margins"] = {{"top", margins_.top},
                    {"bottom", margins_.bottom},
                    {"left", margins_.left},
                    {"right", margins_.right}};
    auto rows_j = nlohmann::json::array();
    for (auto const &row : rows_) {
        auto row_j = nlohmann::json::object();
        if (row.label) {
            row_j["label"] = row.label->to_json();
        }
        if (row.field) {
            row_j["field"] = row.field->to_json();
        }
        rows_j.push_back(row_j);
    }
    j["rows"] = rows_j;
    return j;
}

void FormLayout::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("spacing")) {
        spacing_ = j["spacing"];
    }
    if (j.contains("label_spacing")) {
        label_spacing_ = j["label_spacing"];
    }
    if (j.contains("margins")) {
        auto const &m = j["margins"];
        margins_.top = m.value("top", 0.0f);
        margins_.bottom = m.value("bottom", 0.0f);
        margins_.left = m.value("left", 0.0f);
        margins_.right = m.value("right", 0.0f);
    }
    if (j.contains("rows") && j["rows"].is_array()) {
        rows_.clear();
        for (auto const &row_j : j["rows"]) {
            std::unique_ptr<Widget> label;
            std::unique_ptr<Widget> field;
            if (row_j.contains("label")) {
                label = WidgetLoader::instance().create_widget(row_j["label"]);
            }
            if (row_j.contains("field")) {
                field = WidgetLoader::instance().create_widget(row_j["field"]);
            }
            add_row(std::move(label), std::move(field));
        }
    }
}

} // namespace toolkit
