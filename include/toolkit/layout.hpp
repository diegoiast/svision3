// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/types.hpp"
#include "toolkit/widget.hpp"
#include <memory>
#include <vector>

namespace toolkit {

class VBoxLayout : public Widget, public Fluent<VBoxLayout> {
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

    void set_margins(Margins const &m) { margins_ = m; }
    void set_spacing(float s) { spacing_ = s; }
    void clear_items() { items_.clear(); }
    auto release_item(int index) -> std::unique_ptr<Widget>;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    void set_rect(Rect const &rect) override;
    void set_window(Window *w) override;
    Widget *find_focusable_at(Point p) override;
    Widget *widget_at(Point p) override;
    void collect_focusables(std::vector<Widget *> &out) override;
    void collect_mnemonics(std::vector<Widget *> &out) override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

    std::vector<Item> const &items() const { return items_; }

  private:
    void apply_layout();

    std::vector<Item> items_;
    Margins margins_;
    float spacing_ = 8.0f;
};

class HBoxLayout : public Widget {
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
    template <class T> T &add(int stretch = 0, Alignment h_align = Alignment::Fill) {
        auto ptr = std::make_unique<T>();
        T &ref = *ptr;
        add_widget(std::move(ptr), stretch, h_align);
        return ref;
    }

    void set_margins(Margins const &m) { margins_ = m; }
    const Margins &get_margins() const { return margins_; }
    void set_spacing(float s) { spacing_ = s; }
    void clear_items() { items_.clear(); }
    auto release_item(int index) -> std::unique_ptr<Widget>;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    void set_rect(Rect const &rect) override;
    void set_window(Window *w) override;
    Widget *find_focusable_at(Point p) override;
    Widget *widget_at(Point p) override;
    void collect_focusables(std::vector<Widget *> &out) override;
    void collect_mnemonics(std::vector<Widget *> &out) override;
    void for_each_child(std::function<void(Widget *)> const &callback) override;

    std::vector<Item> const &items() const { return items_; }

  private:
    void apply_layout();

    std::vector<Item> items_;
    Margins margins_;
    float spacing_ = 8.0f;
};

} // namespace toolkit
