// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/tree_view.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

namespace toolkit {

SimpleTreeModel::SimpleTreeModel(std::vector<TreeNode> roots) : roots_(std::move(roots)) {}

TreeNode const &SimpleTreeModel::root_at(int index) const {
    static TreeNode empty;
    if (index < 0 || index >= static_cast<int>(roots_.size())) {
        return empty;
    }
    return roots_[index];
}

void SimpleTreeModel::set_roots(std::vector<TreeNode> roots) {
    roots_ = std::move(roots);
    if (on_data_changed) {
        on_data_changed();
    }
}

void SimpleTreeModel::clear() {
    roots_.clear();
    if (on_data_changed) {
        on_data_changed();
    }
}

TreeView::TreeView(std::shared_ptr<TreeModel> model) : model_(std::move(model)) {
    state.focusable = true;
    if (model_) {
        model_->on_data_changed = [this] {
            rebuild_flattened();
            clamp_scroll();
            if (window()) {
                window()->request_redraw("tree model changed");
            }
        };
        rebuild_flattened();
    }
}

TreeView &TreeView::set_model(std::shared_ptr<TreeModel> model) {
    model_ = std::move(model);
    selection_.clear();
    anchor_ = -1;
    cursor_ = -1;
    scroll_to(0, 0);
    if (model_) {
        model_->on_data_changed = [this] {
            rebuild_flattened();
            clamp_scroll();
            if (window()) {
                window()->request_redraw("tree model changed");
            }
        };
        rebuild_flattened();
    } else {
        flat_nodes_.clear();
    }
    return *this;
}

void TreeView::rebuild_flattened() {
    flat_nodes_.clear();
    if (!model_) {
        return;
    }
    auto n = model_->root_count();
    for (auto i = 0; i < n; i++) {
        auto &root = const_cast<TreeNode &>(model_->root_at(i));
        flatten_node(root, 0);
    }
}

void TreeView::flatten_node(TreeNode &node, int depth) {
    flat_nodes_.push_back({&node, depth});

    if (node.expanded) {
        for (auto &child : node.children) {
            flatten_node(child, depth + 1);
        }
    }
}

TreeView &TreeView::set_selected_node(int index) {
    if (!model_ || index < 0 || index >= static_cast<int>(flat_nodes_.size())) {
        clear_selection();
        return *this;
    }
    selection_.clear();
    selection_.insert(index);
    anchor_ = index;
    cursor_ = index;
    notify_selection();
    return *this;
}

TreeView &TreeView::set_selection(std::set<int> indices) {
    selection_ = std::move(indices);
    if (!selection_.empty()) {
        anchor_ = *selection_.begin();
        cursor_ = *selection_.rbegin();
    } else {
        anchor_ = cursor_ = -1;
    }
    notify_selection();
    return *this;
}

TreeView &TreeView::clear_selection() {
    selection_.clear();
    anchor_ = cursor_ = -1;
    notify_selection();
    return *this;
}

void TreeView::select_range_from_anchor() {
    auto lo = std::min(anchor_, cursor_);
    auto hi = std::max(anchor_, cursor_);

    selection_.clear();
    for (int i = lo; i <= hi; i++) {
        selection_.insert(i);
    }
}

void TreeView::notify_selection() {
    if (on_selection_changed) {
        on_selection_changed(cursor_);
    }
}

void TreeView::notify_expansion(int node_index) {
    if (on_node_expanded) {
        on_node_expanded(node_index);
    }
}

void TreeView::expand(int node_index) {
    if (!model_ || node_index < 0 || node_index >= static_cast<int>(flat_nodes_.size())) {
        return;
    }
    auto &flat = flat_nodes_[node_index];

    if (flat.node_ptr && !flat.node_ptr->children.empty()) {
        flat.node_ptr->expanded = true;
        rebuild_flattened();
        clamp_scroll();
        notify_expansion(node_index);
    }
}

void TreeView::collapse(int node_index) {
    if (!model_ || node_index < 0 || node_index >= static_cast<int>(flat_nodes_.size())) {
        return;
    }
    auto &flat = flat_nodes_[node_index];

    if (flat.node_ptr && !flat.node_ptr->children.empty()) {
        flat.node_ptr->expanded = false;
        rebuild_flattened();
        clamp_scroll();
        notify_expansion(node_index);
    }
}

void TreeView::toggle(int node_index) {
    if (!model_ || node_index < 0 || node_index >= static_cast<int>(flat_nodes_.size())) {
        return;
    }
    auto &flat = flat_nodes_[node_index];

    if (flat.node_ptr->expanded) {
        collapse(node_index);
    } else {
        expand(node_index);
    }
}

TreeView &TreeView::set_multi_select(bool enabled) {
    multi_select_ = enabled;
    return *this;
}

TreeView &TreeView::set_alternating_row_colors(bool enabled) {
    alternating_ = enabled;
    return *this;
}

void TreeView::on_scroll(float /*x*/, float /*y*/) {
    if (window()) {
        window()->request_redraw("tree scroll");
    }
}

float TreeView::row_height() const {
    auto const &theme = Theme::current();
    auto const &style = Theme::current().style.treeView;
    auto const &palette = theme.palette;
    auto fm = font_metrics(palette.fonts.size);
    return fm.height + style.item_padding * 2;
}

void TreeView::clamp_scroll() {
    update_scrollbars({0.0f, row_height() * static_cast<float>(flat_nodes_.size())});
}

void TreeView::scroll_to_node(int index) {
    auto rh = row_height();
    auto top = rh * static_cast<float>(index);
    auto bot = top + rh;
    auto vr = viewport_rect();
    if (bot > scroll_y() + vr.height) {
        scroll_to(scroll_x(), bot - vr.height);
    }
    if (top < scroll_y()) {
        scroll_to(scroll_x(), top);
    }
}

int TreeView::node_at_y(float y) const {
    if (flat_nodes_.empty()) {
        return -1;
    }

    auto local_y = y + scroll_y();
    if (local_y < 0) {
        return -1;
    }

    auto idx = static_cast<int>(local_y / row_height());
    if (idx < 0 || idx >= static_cast<int>(flat_nodes_.size())) {
        return -1;
    }
    return idx;
}

void TreeView::paint(Painter &painter) {
    if (!model_ || flat_nodes_.empty()) {
        return;
    }

    auto const &theme = Theme::current();
    auto const &style = theme.style.treeView;
    auto const &palette = theme.palette;
    auto rh = row_height();
    auto n = static_cast<int>(flat_nodes_.size());
    auto indent = style.indent;
    auto item_padding_h = style.item_padding_h;

    auto wstate = WidgetState{
        .interaction   = ButtonState::Normal,
        .focused       = is_focused(),
        .enabled       = is_enabled(),
        .window_active = window() ? window()->is_active() : true,
    };
    theme.draw_tree_background(painter, {0, 0, rect().width, rect().height}, wstate);

    auto vr = viewport_rect();
    painter.push_clip(vr);
    painter.push_translation({vr.x - scroll_x(), vr.y - scroll_y()});

    auto first_visible = std::max(0, static_cast<int>(scroll_y() / rh));
    auto last_visible = std::min(n - 1, static_cast<int>((scroll_y() + vr.height) / rh));

    for (auto i = first_visible; i <= last_visible; i++) {
        auto const &flat = flat_nodes_[i];
        auto iy = rh * i;
        auto item_rect = Rect{vr.x, vr.y + iy, rect().width, rh};
        auto selected = is_selected(i);
        auto hovered = (i == hovered_) && !selected;
        auto alt_row = alternating_ && (i % 2 == 1);

        if (selected) {
            painter.fill_rect(item_rect, palette.highlight);
        } else if (alt_row) {
            painter.fill_rect(item_rect, palette.alternate);
        }

        // FIXME: this should not be here.
        auto is_win95 = (theme.name == "Windows 95");
        if (is_win95) {
            painter.set_line_style(Painter::LineStyle::Dotted);
        }

        auto has_children = flat.node_ptr && !flat.node_ptr->children.empty();

        if (flat.depth > 0) {
            for (int d = 0; d < flat.depth; d++) {
                auto connector_x = vr.x + item_padding_h + d * indent + indent / 2;
                auto should_draw_line = false;
                for (auto j = i - 1; j >= 0; j--) {
                    if (flat_nodes_[j].depth == d) {
                        auto *parent_node = flat_nodes_[j].node_ptr;
                        if (parent_node && parent_node->expanded) {
                            should_draw_line = true;
                        }
                        break;
                    }
                }
                if (should_draw_line) {
                    painter.draw_line({connector_x, vr.y + iy}, {connector_x, vr.y + iy + rh}, palette.border,
                                      1.0f);
                }
            }

            auto has_next_sibling = false;
            if (i < n - 1) {
                auto next_depth = flat_nodes_[i + 1].depth;
                if (next_depth == flat.depth) {
                    has_next_sibling = true;
                } else if (next_depth < flat.depth) {
                    has_next_sibling = true;
                }
            }
            if (has_next_sibling) {
                auto connector_x = vr.x + item_padding_h + flat.depth * indent + indent / 2;
                painter.draw_line({connector_x, vr.y + iy}, {connector_x, vr.y + iy + rh}, palette.border, 1.0f);
            }

            auto handle_x = vr.x + item_padding_h + flat.depth * indent + indent / 2;
            auto handle_end_x = vr.x + item_padding_h + flat.depth * indent + indent;
            auto handle_y = vr.y + iy + rh / 2;
            painter.draw_line({handle_x, handle_y}, {handle_end_x, handle_y}, palette.border, 1.0f);
        } else {
            if (i < n - 1 && flat_nodes_[i + 1].depth == 0) {
                auto connector_x = vr.x + item_padding_h + indent / 2;
                painter.draw_line({connector_x, vr.y + iy}, {connector_x, vr.y + iy + rh}, palette.border, 1.0f);
            }

            auto handle_x = vr.x + item_padding_h + indent / 2;
            auto handle_end_x = vr.x + item_padding_h + indent;
            auto handle_y = vr.y + iy + rh / 2;
            painter.draw_line({handle_x, handle_y}, {handle_end_x, handle_y}, palette.border, 1.0f);
        }

        // FIXME: move this code to the theme
        if (is_win95) {
            painter.set_line_style(Painter::LineStyle::Solid);
        }

        theme.draw_tree_item(painter, item_rect, flat.node_ptr->text, flat.depth, has_children,
                             flat.node_ptr->expanded, selected, hovered, alt_row);
    }

    painter.pop_translation();
    painter.pop_clip();
    draw_scrollbars(painter);
}

bool TreeView::handle_mouse(MouseEvent const &event) {
    if (!model_ || flat_nodes_.empty()) {
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
    p.y += scroll_y();

    auto idx = node_at_y(p.y);
    if (idx < 0) {
        return false;
    }

    switch (event.type) {
    case MouseEvent::Type::Move:
        hovered_ = idx;
        return true;
    case MouseEvent::Type::Press:
        {
            auto const &flat = flat_nodes_[idx];
            auto *node_ptr = flat.node_ptr;
            auto has_children = node_ptr && !node_ptr->children.empty();
            auto indent = Theme::current().style.treeView.indent;
            auto click_x = p.x;
            auto expected_x = flat.depth * indent;

            if (has_children && click_x >= expected_x && click_x < expected_x + indent) {
                toggle(idx);
                return true;
            }

            if (event.click_count == 2 && has_children) {
                toggle(idx);
                return true;
            }

            auto toggle_mod = event.super || event.ctrl;

            if (multi_select_ && event.shift && anchor_ >= 0) {
                cursor_ = idx;
                select_range_from_anchor();
                notify_selection();
            } else if (multi_select_ && toggle_mod) {
                if (is_selected(idx)) {
                    selection_.erase(idx);
                } else {
                    selection_.insert(idx);
                }
                anchor_ = idx;
                cursor_ = idx;
                notify_selection();
            } else {
                selection_.clear();
                selection_.insert(idx);
                anchor_ = idx;
                cursor_ = idx;
                notify_selection();
            }
            return true;
        }
    default:
        break;
    }

    return false;
}

bool TreeView::handle_key(KeyEvent const &event) {
    if (!is_focused() || flat_nodes_.empty() || event.type != KeyEvent::Type::Press) {
        return false;
    }

    auto n = static_cast<int>(flat_nodes_.size());

    switch (event.key) {
    case Key::Down: {
        auto next = std::min((cursor_ < 0 ? 0 : cursor_ + 1), n - 1);
        if (multi_select_ && event.shift) {
            if (anchor_ < 0) {
                anchor_ = next;
            }
            cursor_ = next;
            select_range_from_anchor();
        } else {
            set_selected_node(next);
        }
        scroll_to_node(cursor_);
        notify_selection();
        window()->request_redraw("tree key");
        return true;
    }
    case Key::Up: {
        auto next = std::max((cursor_ < 0 ? 0 : cursor_ - 1), 0);
        if (multi_select_ && event.shift) {
            if (anchor_ < 0) {
                anchor_ = next;
            }
            cursor_ = next;
            select_range_from_anchor();
        } else {
            set_selected_node(next);
        }
        scroll_to_node(cursor_);
        notify_selection();
        window()->request_redraw("tree key");
        return true;
    }

    case Key::Home: {
        if (multi_select_ && event.shift) {
            if (anchor_ < 0) {
                anchor_ = 0;
            }
            cursor_ = 0;
            select_range_from_anchor();
            notify_selection();
        } else {
            set_selected_node(0);
        }
        scroll_to_node(0);
        window()->request_redraw("tree key");
        return true;
    }
    case Key::End: {
        if (multi_select_ && event.shift) {
            if (anchor_ < 0) {
                anchor_ = n - 1;
            }
            cursor_ = n - 1;
            select_range_from_anchor();
            notify_selection();
        } else {
            set_selected_node(n - 1);
        }
        scroll_to_node(n - 1);
        window()->request_redraw("tree key");
        return true;
    }

    case Key::Right: {
        if (cursor_ >= 0 && cursor_ < n) {
            auto const &flat = flat_nodes_[cursor_];
            auto *node_ptr = flat.node_ptr;
            auto has_children = node_ptr && !node_ptr->children.empty();
            if (has_children && !node_ptr->expanded) {
                expand(cursor_);
                window()->request_redraw("tree key");
                return true;
            }
        }
        return false;
    }

    case Key::Left: {
        if (cursor_ >= 0 && cursor_ < n) {
            auto const &flat = flat_nodes_[cursor_];
            auto *node_ptr = flat.node_ptr;
            auto has_children = node_ptr && !node_ptr->children.empty();
            if (has_children && node_ptr->expanded) {
                collapse(cursor_);
                window()->request_redraw("tree key");
                return true;
            }
        }
        return false;
    }
    default:
        break;
    }

    return false;
}

void TreeView::set_rect(Rect const &rect) {
    Widget::set_rect(rect);
    clamp_scroll();
}

Size TreeView::size_hint() const {
    auto measured_height = row_height() * 8;
    return {0, measured_height};
}

CursorShape TreeView::cursor() const { return CursorShape::Hand; }

} // namespace toolkit
