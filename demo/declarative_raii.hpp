// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// RAII scope-based UI API.
//
// Hierarchy is expressed with C++ scopes {}.
// Each scope guard adds itself to its parent on destruction.
// Leaf widgets are streamed into a scope with operator<<.
//
// Usage:
//   ui::RootScope root(window);
//   {   ui::TabWidgetScope tabs(root);
//       {   ui::TabScope t(tabs, "Main");
//           t << ui::label("Hello");
//           {   ui::HBoxScope row(t);
//               row << ui::label("Name:");
//               row << ui::line_input("...").expand();
//           }   // row added to t here
//       }   // tab added to tabs here
//   }   // tabs added to root here
//       // root calls window->set_root() here

#pragma once

#include "declarative.hpp"
#include "toolkit/tab_widget.hpp"
#include "toolkit/window.hpp"

namespace ui {

// Base class for anything that can receive child widgets.
// operator<< adds an Element and respects its expand_ flag.
struct ChildReceiver {
    virtual void receive(std::unique_ptr<toolkit::Widget> w, int stretch = 0) = 0;
    virtual ~ChildReceiver() = default;

    template <typename T>
    ChildReceiver &operator<<(Element<T> &&el) {
        receive(std::move(el.w), el.expand_ ? 1 : 0);
        return *this;
    }

    template <typename T>
    ChildReceiver &operator<<(Element<T> &el) {
        return *this << std::move(el);
    }
};

// Scope guard for VBoxLayout / HBoxLayout.
// Destructor adds the completed layout to the parent receiver.
template <typename Layout>
class LayoutScope : public ChildReceiver {
    std::unique_ptr<Layout> w_;
    ChildReceiver *parent_;
    int stretch_;

  public:
    explicit LayoutScope(ChildReceiver &parent, int stretch = 0)
        : w_(std::make_unique<Layout>()), parent_(&parent), stretch_(stretch) {
        w_->set_spacing(default_padding());
        w_->set_margins(default_margins());
    }

    ~LayoutScope() { parent_->receive(std::move(w_), stretch_); }

    void receive(std::unique_ptr<toolkit::Widget> w, int stretch = 0) override {
        w_->add_widget(std::move(w), stretch);
    }

    LayoutScope &margins(toolkit::Margins m) {
        w_->set_margins(m);
        return *this;
    }
    LayoutScope &spacing(float s) {
        w_->set_spacing(s);
        return *this;
    }
};

using VBoxScope = LayoutScope<toolkit::VBoxLayout>;
using HBoxScope = LayoutScope<toolkit::HBoxLayout>;

// Scope guard for TabWidget.
// Does NOT inherit ChildReceiver — tabs are added via TabScope, not operator<<.
class TabWidgetScope {
    std::unique_ptr<toolkit::TabWidget> w_;
    ChildReceiver *parent_;

  public:
    explicit TabWidgetScope(ChildReceiver &parent)
        : w_(std::make_unique<toolkit::TabWidget>()), parent_(&parent) {}

    ~TabWidgetScope() { parent_->receive(std::move(w_)); }

    void add_tab(std::string name, std::unique_ptr<toolkit::Widget> content) {
        w_->add_tab(std::move(name), std::move(content));
    }
};

// Scope guard for a single tab page inside a TabWidgetScope.
// Its content is a VBoxLayout; children are added via operator<<.
// Destructor calls parent.add_tab().
class TabScope : public ChildReceiver {
    std::unique_ptr<toolkit::VBoxLayout> content_;
    TabWidgetScope *parent_;
    std::string name_;

  public:
    TabScope(TabWidgetScope &parent, std::string name)
        : content_(std::make_unique<toolkit::VBoxLayout>())
        , parent_(&parent)
        , name_(std::move(name)) {
        content_->set_spacing(default_padding());
        content_->set_margins(default_margins());
    }

    ~TabScope() { parent_->add_tab(std::move(name_), std::move(content_)); }

    void receive(std::unique_ptr<toolkit::Widget> w, int stretch = 0) override {
        content_->add_widget(std::move(w), stretch);
    }
};

// Root scope: wraps a Window. Its VBoxLayout becomes the window root on destruction.
class RootScope : public ChildReceiver {
    std::unique_ptr<toolkit::VBoxLayout> w_;
    toolkit::Window *window_;

  public:
    explicit RootScope(toolkit::Window *win)
        : w_(std::make_unique<toolkit::VBoxLayout>()), window_(win) {
        w_->set_spacing(no_spacing);
        w_->set_margins(no_margins());
    }

    ~RootScope() { window_->set_root(std::move(w_)); }

    void receive(std::unique_ptr<toolkit::Widget> w, int stretch = 0) override {
        w_->add_widget(std::move(w), stretch);
    }
};

} // namespace ui
