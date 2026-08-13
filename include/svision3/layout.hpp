// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/types.hpp"
#include "svision3/widget.hpp"
#include <map>
#include <memory>
#include <vector>

namespace svision3 {

// Child widgets are owned by shared_ptr, and every function that takes
// ownership hands back a weak_ptr to what it just stored. Callers that need to
// keep touching a child (a callback, a timer) store that weak_ptr and lock() at
// use, rather than holding a raw pointer that dangles or a shared_ptr that
// keeps the child alive after its parent dropped it.
//
// Storing the *shared_ptr* in something the layout itself owns -- a callback on
// a sibling widget, most often -- is a reference cycle that never frees. That
// is the one thing this ownership model makes possible and the previous
// unique_ptr one did not, so prefer the weak_ptr these functions return.

// Base for all layouts. Shared implementations of paint, handle_mouse,
// handle_key, set_rect, set_window, find_focusable_at, widget_at,
// collect_focusables, collect_mnemonics all delegate to for_each_child().
// Derived classes implement apply_layout(), size_hint(), and for_each_child().
class AbstractLayout : public Widget, public Fluent<AbstractLayout> {
  public:
    AbstractLayout &set_margins(Margins const &m) {
        margins_ = m;
        return *this;
    }
    Margins const &get_margins() const { return margins_; }
    AbstractLayout &set_spacing(float s) {
        spacing_ = s;
        return *this;
    }

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    void set_rect(Rect const &rect) override;
    void set_window(Window *win) override;
    Widget *find_focusable_at(Point p) override;
    Widget *widget_at(Point p) override;
    void collect_focusables(std::vector<Widget *> &out) override;
    void collect_mnemonics(std::vector<Widget *> &out) override;
    void for_each_child(std::function<void(Widget *)> const &callback) override = 0;

  protected:
    virtual void apply_layout() = 0;

    Margins margins_;
    float spacing_ = 8.0f;
};

class VBoxLayout : public AbstractLayout {
    DECLARE_WIDGET(VBoxLayout)
  public:
    VBoxLayout();
    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    struct Item {
        std::shared_ptr<Widget> widget;
        int stretch;
        Alignment h_align;
    };

    std::weak_ptr<Widget> add_widget(std::shared_ptr<Widget> widget, int stretch = 0,
                                     Alignment h_align = Alignment::Fill);
    template <class T> std::weak_ptr<T> add(int stretch = 0, Alignment h_align = Alignment::Fill) {
        auto ptr = std::make_shared<T>();
        auto ref = std::weak_ptr<T>(ptr);
        add_widget(std::move(ptr), stretch, h_align);
        return ref;
    }

    void clear_items() { items_.clear(); }
    auto release_item(int index) -> std::shared_ptr<Widget>;

    Size size_hint() const override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

    std::vector<Item> const &items() const { return items_; }

  protected:
    void apply_layout() override;

  private:
    std::vector<Item> items_;
};

class HBoxLayout : public AbstractLayout {
    DECLARE_WIDGET(HBoxLayout)
  public:
    HBoxLayout();
    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    struct Item {
        std::shared_ptr<Widget> widget;
        int stretch;
        Alignment v_align;
    };

    std::weak_ptr<Widget> add_widget(std::shared_ptr<Widget> widget, int stretch = 0,
                                     Alignment v_align = Alignment::Center);
    std::weak_ptr<Widget> insert_widget(int index, std::shared_ptr<Widget> widget, int stretch = 0,
                                        Alignment v_align = Alignment::Center);
    template <class T> std::weak_ptr<T> add(int stretch = 0, Alignment v_align = Alignment::Fill) {
        auto ptr = std::make_shared<T>();
        auto ref = std::weak_ptr<T>(ptr);
        add_widget(std::move(ptr), stretch, v_align);
        return ref;
    }

    void clear_items() { items_.clear(); }
    auto release_item(int index) -> std::shared_ptr<Widget>;

    Size size_hint() const override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

    std::vector<Item> const &items() const { return items_; }

  protected:
    void apply_layout() override;

  private:
    std::vector<Item> items_;
};

class GridLayout : public AbstractLayout {
    DECLARE_WIDGET(GridLayout)
  public:
    GridLayout();
    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    struct Item {
        std::shared_ptr<Widget> widget;
        int row;
        int col;
        int rowspan;
        int colspan;
        Alignment h_align;
        Alignment v_align;
    };

    std::weak_ptr<Widget> add_widget(std::shared_ptr<Widget> widget, int row, int col,
                                     int rowspan = 1, int colspan = 1,
                                     Alignment h_align = Alignment::Fill,
                                     Alignment v_align = Alignment::Fill);

    template <class T>
    std::weak_ptr<T> add(int row, int col, int rowspan = 1, int colspan = 1,
                         Alignment h_align = Alignment::Fill,
                         Alignment v_align = Alignment::Fill) {
        auto ptr = std::make_shared<T>();
        auto ref = std::weak_ptr<T>(ptr);
        add_widget(std::move(ptr), row, col, rowspan, colspan, h_align, v_align);
        return ref;
    }

    GridLayout &set_column_stretch(int col, int stretch) {
        col_stretch_[col] = stretch;
        return *this;
    }
    GridLayout &set_row_stretch(int row, int stretch) {
        row_stretch_[row] = stretch;
        return *this;
    }
    GridLayout &set_spacing(float s) {
        col_spacing_ = row_spacing_ = s;
        return *this;
    }
    GridLayout &set_column_spacing(float s) {
        col_spacing_ = s;
        return *this;
    }
    GridLayout &set_row_spacing(float s) {
        row_spacing_ = s;
        return *this;
    }

    Size size_hint() const override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

    std::vector<Item> const &items() const { return items_; }

  protected:
    void apply_layout() override;

  private:
    std::vector<Item> items_;
    std::map<int, int> col_stretch_;
    std::map<int, int> row_stretch_;
    float col_spacing_ = 8.0f;
    float row_spacing_ = 8.0f;
};

class StackedLayout : public AbstractLayout {
    DECLARE_WIDGET(StackedLayout)
  public:
    StackedLayout();
    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    std::weak_ptr<Widget> add_widget(std::shared_ptr<Widget> widget);
    template <class T> std::weak_ptr<T> add() {
        auto ptr = std::make_shared<T>();
        auto ref = std::weak_ptr<T>(ptr);
        add_widget(std::move(ptr));
        return ref;
    }
    void remove_widget(int index);
    void swap_widgets(int a, int b);

    StackedLayout &set_current(int index);
    int current() const { return current_; }
    int count() const { return static_cast<int>(items_.size()); }

    Size size_hint() const override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

    std::vector<std::shared_ptr<Widget>> const &items() const { return items_; }

  protected:
    void apply_layout() override;

  private:
    std::vector<std::shared_ptr<Widget>> items_;
    int current_ = -1;
};

class FormLayout : public AbstractLayout {
    DECLARE_WIDGET(FormLayout)
  public:
    FormLayout();
    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    struct Row {
        std::shared_ptr<Widget> label;
        std::shared_ptr<Widget> field;
    };

    // A row is two widgets, so this hands back both weak_ptrs rather than the
    // single one the other add functions return. Either may be empty, matching
    // a null label or field going in.
    struct RowRef {
        std::weak_ptr<Widget> label;
        std::weak_ptr<Widget> field;
    };

    RowRef add_row(std::shared_ptr<Widget> label, std::shared_ptr<Widget> field);
    FormLayout &set_label_spacing(float s) {
        label_spacing_ = s;
        return *this;
    }

    Size size_hint() const override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

    std::vector<Row> const &rows() const { return rows_; }

  protected:
    void apply_layout() override;

  private:
    std::vector<Row> rows_;
    float label_spacing_ = 8.0f;
};

} // namespace svision3
