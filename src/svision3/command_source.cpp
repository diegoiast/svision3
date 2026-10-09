// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/command_source.hpp"
#include "svision3/tab_widget.hpp"
#include "svision3/widget.hpp"
#include "svision3/window.hpp"

namespace svision3 {

TabCommandSource::TabCommandSource(TabWidget &tabs) : tabs_(tabs) {
    tab_subscription_ = tabs_.on_current_changed([this](int) { changed_.emit(); });
}

Widget *TabCommandSource::current_context() const { return tabs_.get_current_widget(); }

Subscription TabCommandSource::on_context_changed(std::function<void()> callback) {
    return changed_.connect(std::move(callback));
}

FocusCommandSource::FocusCommandSource(Window &window) : window_(window) {
    focus_subscription_ =
        window_.on_focus_changed([this](Widget *focused) { focus_moved(focused); });

    // A context is a raw pointer to a widget the window does not own, so it has to
    // be dropped when that widget dies -- the same reason the window nulls its own
    // cached pointers here.
    forget_subscription_ = window_.on_widget_forgotten([this](Widget *removed) {
        if (Window::contains_widget(removed, context_)) {
            context_ = nullptr;
            changed_.emit();
        }
    });

    focus_moved(window_.focused_widget());
}

void FocusCommandSource::focus_moved(Widget *focused) {
    for (auto *w = focused; w; w = w->parent()) {
        if (w->is_command_context()) {
            set_context(w);
            return;
        }
    }
    // Nothing along the chain claims to be a context: the user reached for a
    // toolbar control, or focus went nowhere at all. Leave the menus as they are.
}

void FocusCommandSource::set_context(Widget *widget) {
    if (context_ == widget) {
        return;
    }
    context_ = widget;
    changed_.emit();
}

Subscription FocusCommandSource::on_context_changed(std::function<void()> callback) {
    return changed_.connect(std::move(callback));
}

} // namespace svision3
