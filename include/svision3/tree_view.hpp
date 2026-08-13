// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/scrollable_widget.hpp"
#include "svision3/widget.hpp"
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace svision3 {

struct TreeNode {
    std::string text;
    std::vector<TreeNode> children;
    bool expanded = false;
    int parent_index = -1;
    int depth = 0;
};

class TreeModel {
  public:
    virtual ~TreeModel() = default;
    virtual int root_count() const = 0;
    virtual TreeNode const &root_at(int index) const = 0;
    std::function<void()> on_data_changed;
};

class SimpleTreeModel : public TreeModel {
  public:
    explicit SimpleTreeModel(std::vector<TreeNode> roots = {});

    int root_count() const override { return static_cast<int>(roots_.size()); }
    TreeNode const &root_at(int index) const override;

    void set_roots(std::vector<TreeNode> roots);
    void clear();

  private:
    std::vector<TreeNode> roots_;
};

class TreeView : public ScrollableWidget, public Fluent<TreeView> {
    DECLARE_WIDGET(TreeView)
  public:
    explicit TreeView(std::shared_ptr<TreeModel> model);

    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    TreeView &set_model(std::shared_ptr<TreeModel> model);
    std::shared_ptr<TreeModel> model() const { return model_; }

    int selected_node() const { return cursor_; }
    std::set<int> const &selection() const { return selection_; }
    TreeView &set_selected_node(int index);
    TreeView &set_selection(std::set<int> indices);
    TreeView &clear_selection();
    bool is_selected(int index) const { return selection_.count(index) > 0; }

    bool multi_select() const { return multi_select_; }
    TreeView &set_multi_select(bool enabled);

    bool alternating_row_colors() const { return alternating_; }
    TreeView &set_alternating_row_colors(bool enabled);

    std::function<void(int node_index)> on_selection_changed;
    std::function<void(int node_index)> on_node_expanded;

    void expand(int node_index);
    void collapse(int node_index);
    void toggle(int node_index);

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    void set_rect(Rect const &rect) override;
    Size size_hint() const override;
    CursorShape cursor() const override;

  protected:
    void on_scroll(float x, float y) override;

  private:
    struct FlatNode {
        TreeNode *node_ptr;
        int depth;
    };

    void rebuild_flattened();
    void flatten_node(TreeNode &node, int depth);
    float row_height() const;
    void clamp_scroll();
    int node_at_y(float y) const;
    void scroll_to_node(int index);
    void select_range_from_anchor();
    void notify_selection();
    void notify_expansion(int node_index);

    std::shared_ptr<TreeModel> model_;
    std::vector<FlatNode> flat_nodes_;
    std::set<int> selection_;
    int anchor_ = -1;
    int cursor_ = -1;
    int hovered_ = -1;
    bool alternating_ = false;
    bool multi_select_ = false;
};

} // namespace svision3
