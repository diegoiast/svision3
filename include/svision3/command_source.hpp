// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/subscription.hpp"
#include <functional>

namespace svision3 {

class TabWidget;
class Widget;
class Window;

// "Which widget's commands should the menus and toolbars be showing right now?"
//
// CommandAnchor reads its run of commands from whatever this points at, and
// refreshes whenever it changes. Splitting it out of CommandAnchor is what lets
// the same anchors be driven by the active tab in one application and by the
// focused widget in another, without either knowing about the other.
class CommandSource {
  public:
    virtual ~CommandSource() = default;

    // The widget currently supplying commands, or null when there is none.
    virtual Widget *current_context() const = 0;

    // Fires when current_context() changes. The handle must outlive the desired
    // listening period.
    [[nodiscard]] virtual Subscription on_context_changed(std::function<void()> callback) = 0;
};

// The context is the active tab's widget. Menus follow the document the user
// switched to, and nothing else in the window can disturb them.
class TabCommandSource : public CommandSource {
  public:
    explicit TabCommandSource(TabWidget &tabs);

    Widget *current_context() const override;
    Subscription on_context_changed(std::function<void()> callback) override;

  private:
    TabWidget &tabs_;
    Signal<> changed_;
    Subscription tab_subscription_;
};

// The context follows keyboard focus, so any part of the window can supply
// commands -- a build-output pane in a dock is as much a context as a centre tab.
//
// Two rules make that usable rather than chaotic:
//
//  - On a focus change, the walk goes *up* from the focused widget to the nearest
//    ancestor with is_command_context(). Focus usually lands on a child of the
//    document (TabWidget focuses the first focusable inside the tab's content),
//    so the focused widget itself is rarely the answer.
//
//  - If that walk finds nothing, the context is left alone. Focusing a toolbar
//    combobox, or a button, or losing focus entirely as a window deactivates,
//    must not swap the menus out from under the document being edited.
class FocusCommandSource : public CommandSource {
  public:
    explicit FocusCommandSource(Window &window);

    Widget *current_context() const override { return context_; }
    Subscription on_context_changed(std::function<void()> callback) override;

    // Adopts `widget` as the context without waiting for focus, for the initial
    // context a window starts without. A null argument clears it.
    void set_context(Widget *widget);

  private:
    void focus_moved(Widget *focused);

    Window &window_;
    Widget *context_ = nullptr;
    Signal<> changed_;
    Subscription focus_subscription_;
    Subscription forget_subscription_;
};

} // namespace svision3
