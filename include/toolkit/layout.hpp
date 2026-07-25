// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/types.hpp"
#include "toolkit/widget.hpp"
#include <map>
#include <memory>
#include <vector>

namespace toolkit {

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
        std::unique_ptr<Widget> widget;
        int stretch;
        Alignment h_align;
    };

    void add_widget(std::unique_ptr<Widget> widget, int stretch = 0,
                    Alignment h_align = Alignment::Fill);
    template <class T> T &add(int stretch = 0, Alignment h_align = Alignment::Fill) {
        auto ptr = std::make_unique<T>();
        T &ref = *ptr;
        add_widget(std::move(ptr), stretch, h_align);
        return ref;
    }

    void clear_items() { items_.clear(); }
    auto release_item(int index) -> std::unique_ptr<Widget>;

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
        std::unique_ptr<Widget> widget;
        int stretch;
        Alignment v_align;
    };

    void add_widget(std::unique_ptr<Widget> widget, int stretch = 0,
                    Alignment v_align = Alignment::Center);
    void insert_widget(int index, std::unique_ptr<Widget> widget, int stretch = 0,
                       Alignment v_align = Alignment::Center);
    template <class T> T &add(int stretch = 0, Alignment v_align = Alignment::Fill) {
        auto ptr = std::make_unique<T>();
        T &ref = *ptr;
        add_widget(std::move(ptr), stretch, v_align);
        return ref;
    }

    void clear_items() { items_.clear(); }
    auto release_item(int index) -> std::unique_ptr<Widget>;

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
        std::unique_ptr<Widget> widget;
        int row;
        int col;
        int rowspan;
        int colspan;
        Alignment h_align;
        Alignment v_align;
    };

    void add_widget(std::unique_ptr<Widget> widget, int row, int col, int rowspan = 1,
                    int colspan = 1, Alignment h_align = Alignment::Fill,
                    Alignment v_align = Alignment::Fill);

    template <class T>
    T &add(int row, int col, int rowspan = 1, int colspan = 1, Alignment h_align = Alignment::Fill,
           Alignment v_align = Alignment::Fill) {
        auto ptr = std::make_unique<T>();
        T &ref = *ptr;
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

    void add_widget(std::unique_ptr<Widget> widget);
    template <class T> T &add() {
        auto ptr = std::make_unique<T>();
        T &ref = *ptr;
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

    std::vector<std::unique_ptr<Widget>> const &items() const { return items_; }

  protected:
    void apply_layout() override;

  private:
    std::vector<std::unique_ptr<Widget>> items_;
    int current_ = -1;
};

class FormLayout : public AbstractLayout {
    DECLARE_WIDGET(FormLayout)
  public:
    FormLayout();
    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    struct Row {
        std::unique_ptr<Widget> label;
        std::unique_ptr<Widget> field;
    };

    void add_row(std::unique_ptr<Widget> label, std::unique_ptr<Widget> field);
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

} // namespace toolkit
