// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/window.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/tab_widget.hpp"
#include "toolkit/theme.hpp"
#include <cctype>
#include <chrono>
#include <iomanip>
#include <spdlog/spdlog.h>

namespace toolkit {

struct Window::Impl {
    std::unique_ptr<PlatformWindow> platform;
    Window::Statistics stats;
    std::chrono::steady_clock::time_point last_log_time = std::chrono::steady_clock::now();
    uint64_t draws_since_last_log = 0;
    double draw_time_sum_ms = 0;
    double repaint_time_sum_ms = 0;
    bool logging_enabled = false;
    int stats_timer_id = 0;
};

Window::Window(std::string_view title, Size size)
    : title_(title), size_(size), impl_(std::make_unique<Impl>()) {
    impl_->platform = detail::current_platform()->create_window(title, size, this);
    spdlog::info("Window '{}' created ({}x{})", title_, size.width, size.height);
}

Window::~Window() {
    if (impl_->platform) {
        impl_->platform->hide_tooltip_window();
    }
}

Window::Statistics const &Window::statistics() const { return impl_->stats; }

void Window::reset_statistics() {
    impl_->stats = Statistics{};
    impl_->draws_since_last_log = 0;
    impl_->draw_time_sum_ms = 0;
    impl_->repaint_time_sum_ms = 0;
    impl_->last_log_time = std::chrono::steady_clock::now();
    spdlog::debug("Window '{}' statistics reset", title_);
}

void Window::set_statistics_logging_enabled(bool enabled) {
    if (impl_->logging_enabled == enabled) {
        return;
    }
    impl_->logging_enabled = enabled;

    if (enabled) {
        if (impl_->stats_timer_id == 0) {
            impl_->last_log_time = std::chrono::steady_clock::now();
            impl_->draws_since_last_log = 0;
            impl_->draw_time_sum_ms = 0;
            impl_->repaint_time_sum_ms = 0;

            impl_->stats_timer_id = start_timer(
                2.0f,
                [this] {
                    auto now = std::chrono::steady_clock::now();
                    auto time_since_log =
                        std::chrono::duration<double>(now - impl_->last_log_time).count();

                    if (time_since_log > 0) {
                        impl_->stats.avg_fps =
                            static_cast<double>(impl_->draws_since_last_log) / time_since_log;
                        if (impl_->draws_since_last_log > 0) {
                            impl_->stats.avg_draw_time_ms =
                                impl_->draw_time_sum_ms /
                                static_cast<double>(impl_->draws_since_last_log);
                            impl_->stats.avg_repaint_time_ms =
                                impl_->repaint_time_sum_ms /
                                static_cast<double>(impl_->draws_since_last_log);
                        } else {
                            impl_->stats.avg_draw_time_ms = 0;
                            impl_->stats.avg_repaint_time_ms = 0;
                        }

                        spdlog::info("Window '{}' Stats: draws={}, fps={:.1f}, draw_time={:.2f}ms, "
                                     "repaint_time={:.2f}ms",
                                     title_, impl_->stats.total_draws, impl_->stats.avg_fps,
                                     impl_->stats.avg_draw_time_ms,
                                     impl_->stats.avg_repaint_time_ms);
                    }

                    impl_->last_log_time = now;
                    impl_->draws_since_last_log = 0;
                    impl_->draw_time_sum_ms = 0;
                    impl_->repaint_time_sum_ms = 0;
                },
                true);
        }
    } else {
        if (impl_->stats_timer_id != 0) {
            stop_timer(impl_->stats_timer_id);
            impl_->stats_timer_id = 0;
        }
    }
}

bool Window::is_statistics_logging_enabled() const { return impl_->logging_enabled; }

auto Window::save_to_png(std::string const &path) -> bool {
    if (impl_->platform) {
        return impl_->platform->save_to_png(path);
    }
    return false;
}

auto Window::scale_factor() const -> float {
    if (impl_->platform) {
        return impl_->platform->scale_factor();
    }
    return 1.0f;
}

auto Window::platform_window() const -> PlatformWindow * { return impl_->platform.get(); }

void Window::show() {
    if (impl_->platform) {
        impl_->platform->show();
    }
}

static void on_theme_changed_recursive(Widget *w) {
    if (!w) {
        return;
    }
    w->on_theme_changed();
    w->for_each_child([](Widget *child) { on_theme_changed_recursive(child); });
}

void Window::on_theme_changed() {
    if (root_) {
        on_theme_changed_recursive(root_.get());
    }
    for (auto &w : widgets_) {
        on_theme_changed_recursive(w.get());
    }
}

void Window::close() {
    if (impl_->platform) {
        impl_->platform->close();
    }
}

void Window::request_redraw(std::string_view reason) {
    spdlog::trace("Window '{}' redraw requested (reason={})", title_, reason);
    if (impl_->platform) {
        impl_->platform->request_redraw();
    }
}

void Window::set_min_size(Size s) {
    min_size_ = s;
    if (impl_->platform) {
        impl_->platform->set_min_size(s);
    }
}

void Window::set_max_size(Size s) {
    max_size_ = s;
    if (impl_->platform) {
        impl_->platform->set_max_size(s);
    }
}

auto Window::start_timer(float interval_sec, std::function<void()> callback, bool repeat) -> int {
    if (impl_->platform) {
        return impl_->platform->start_timer(interval_sec, std::move(callback), repeat);
    }
    return 0;
}

void Window::stop_timer(int id) {
    if (impl_->platform) {
        impl_->platform->stop_timer(id);
    }
}

void Window::set_cursor(CursorShape shape) {
    if (impl_->platform) {
        impl_->platform->set_cursor(shape);
    }
}

void Window::show_tooltip_window(std::string const &text, Point pos) {
    if (impl_->platform) {
        impl_->platform->show_tooltip_window(text, pos);
    }
}

void Window::hide_tooltip_window() {
    if (impl_->platform) {
        impl_->platform->hide_tooltip_window();
    }
}

void Window::set_root(std::unique_ptr<Widget> root) {
    root_ = std::move(root);
    if (root_) {
        root_->set_window(this);
        root_->set_rect({0, 0, size_.width, size_.height});
        if (impl_->platform) {
            impl_->platform->set_min_size(min_size());
        }
    }
}

void Window::add_widget(std::unique_ptr<Widget> widget) {
    widget->set_window(this);
    widgets_.push_back(std::move(widget));
}

void Window::open_popup(Popup popup) {
    if (popups_.empty()) {
        saved_focus_ = focused_widget_;
        set_focused_widget(nullptr);
    }
    popups_.push_back(std::move(popup));
    request_redraw("popup open");
}

void Window::close_popup() {
    if (!popups_.empty()) {
        auto popup = std::move(popups_.back());
        popups_.pop_back();
        if (popup.on_close) {
            popup.on_close();
        }
        if (popups_.empty()) {
            set_focused_widget(saved_focus_);
            saved_focus_ = nullptr;
        }
    }
    request_redraw("popup close");
}

void Window::close_all_popups() {
    while (!popups_.empty()) {
        close_popup();
    }
}

void Window::set_focused_widget(Widget *w) {
    if (focused_widget_ == w) {
        return;
    }
    if (focused_widget_) {
        focused_widget_->set_focused(false);
        focused_widget_->on_blur();
    }
    focused_widget_ = w;
    if (focused_widget_) {
        focused_widget_->set_focused(true);
        focused_widget_->on_focus();
    }
}

void Window::handle_paint(Painter &painter) {
    auto draw_start = std::chrono::steady_clock::now();
    impl_->stats.total_draws++;
    impl_->draws_since_last_log++;

    auto const &style = Theme::current();
    painter.fill_rect({0, 0, size_.width, size_.height}, style.window.background);

    auto repaint_start = std::chrono::steady_clock::now();

    if (root_) {
        root_->draw(painter);
    }
    for (auto &widget : widgets_) {
        widget->draw(painter);
    }

    if (focused_widget_ && focused_widget_->is_focused() && focused_widget_->is_focusable()) {
        Theme::current().draw_focus_ring_for_widget(painter, focused_widget_);
    }

    auto repaint_end = std::chrono::steady_clock::now();
    if (Widget::debug_show_frames) {
        if (root_) {
            draw_debug_frames_recursive(painter, root_.get());
        }
        for (auto &widget : widgets_) {
            draw_debug_frames_recursive(painter, widget.get());
        }
    }

    for (auto const &popup : popups_) {
        if (popup.on_paint) {
            painter.push_translation({popup.bounds.x, popup.bounds.y});
            popup.on_paint(painter);
            painter.pop_translation();
        }
    }

    if (tooltip_visible_ && tooltip_widget_) {
        auto const &current = tooltip_widget_->tooltip();
        if (current != tooltip_text_) {
            tooltip_text_ = current;
            if (!tooltip_text_.empty()) {
                show_tooltip_window(tooltip_text_, tooltip_mouse_pos_);
            } else {
                hide_tooltip();
            }
        }
    }

    auto draw_end = std::chrono::steady_clock::now();
    auto draw_duration = std::chrono::duration<double, std::milli>(draw_end - draw_start).count();
    auto repaint_duration =
        std::chrono::duration<double, std::milli>(repaint_end - repaint_start).count();

    impl_->draw_time_sum_ms += draw_duration;
    impl_->repaint_time_sum_ms += repaint_duration;
}

void Window::focus_next(bool reverse) {
    auto focusables = std::vector<Widget *>{};
    auto idx = -1;
    auto next = 0;

    if (root_) {
        root_->collect_focusables(focusables);
    }
    for (auto &w : widgets_) {
        w->collect_focusables(focusables);
    }

    if (focusables.empty()) {
        return;
    }

    for (auto i = 0; i < static_cast<int>(focusables.size()); i++) {
        if (focusables[i] == focused_widget_) {
            idx = i;
            break;
        }
    }

    if (reverse) {
        next = idx <= 0 ? static_cast<int>(focusables.size()) - 1 : idx - 1;
    } else {
        next = (idx + 1) % static_cast<int>(focusables.size());
    }

    set_focused_widget(focusables[next]);
}

void Window::handle_mouse(MouseEvent const &event) {
    auto needs_redraw = false;

    if (!popups_.empty()) {
        for (int i = static_cast<int>(popups_.size()) - 1; i >= 0; --i) {
            auto &popup = popups_[i];
            if (popup.on_mouse) {
                auto size_before = popups_.size();
                auto local_event = event;
                local_event.position.x -= popup.bounds.x;
                local_event.position.y -= popup.bounds.y;
                if (popup.on_mouse(local_event)) {
                    auto size_after = popups_.size();
                    // If a "parent" popup handled the event, close all "child" popups
                    // But ONLY if we didn't just open a new child.
                    if (size_after < size_before) {
                        while (static_cast<int>(popups_.size()) - 1 > i) {
                            close_popup();
                        }
                    } else if (size_after > size_before) {
                        // We opened a new child. Close any other children that were
                        // already there (siblings of the new child).
                        while (static_cast<int>(popups_.size()) - 1 > (i + 1)) {
                            // Close the one that WAS at i+1, but now is at i+1 after popping?
                            // No, if we just pushed to 'size_after', then 'size_after-1'
                            // is the NEW child. Everything between 'i' and 'size_after-1'
                            // should be closed.
                            auto new_child = std::move(popups_.back());
                            popups_.pop_back();
                            while (static_cast<int>(popups_.size()) - 1 > i) {
                                close_popup();
                            }
                            popups_.push_back(std::move(new_child));
                        }
                    } else {
                        // size_after == size_before.
                        // For Press events, we always want to close children of the clicked popup.
                        if (event.type == MouseEvent::Type::Press) {
                            while (static_cast<int>(popups_.size()) - 1 > i) {
                                close_popup();
                            }
                        }
                        // For Move events, we DON'T close children here.
                        // The individual popups (like Menu) are responsible for
                        // opening new submenus (which triggers the size_after > size_before case)
                        // or closing them if they want to.
                    }
                    request_redraw("event");
                    return;
                }
            }
        }

        if (event.type == MouseEvent::Type::Press) {
            bool click_inside_any_popup = false;
            for (auto const &p : popups_) {
                if (p.bounds.contains(event.position)) {
                    click_inside_any_popup = true;
                    break;
                }
            }
            if (!click_inside_any_popup) {
                close_all_popups();
                needs_redraw = true;
            }
        }
    }

    if (event.type == MouseEvent::Type::Press) {
        auto new_focus = static_cast<Widget *>(nullptr);
        if (root_) {
            auto p = event.position;
            p.x -= root_->rect().x;
            p.y -= root_->rect().y;
            new_focus = root_->find_focusable_at(p);
        }
        if (!new_focus) {
            for (auto &w : widgets_) {
                auto p = event.position;
                p.x -= w->rect().x;
                p.y -= w->rect().y;
                new_focus = w->find_focusable_at(p);
                if (new_focus) {
                    break;
                }
            }
        }
        set_focused_widget(new_focus);
        needs_redraw = true;
    }

    if (root_) {
        if (Widget::dispatch_mouse_event(root_.get(), event)) {
            needs_redraw = true;
            if (event.type != MouseEvent::Type::Move && event.type != MouseEvent::Type::Drag) {
                request_redraw("event");
                return;
            }
        }
    }
    for (auto &widget : widgets_) {
        if (Widget::dispatch_mouse_event(widget.get(), event)) {
            needs_redraw = true;
            if (event.type != MouseEvent::Type::Move && event.type != MouseEvent::Type::Drag) {
                request_redraw("event");
                return;
            }
        }
    }

    if (event.type == MouseEvent::Type::Move) {
        auto under = static_cast<Widget *>(nullptr);
        if (root_) {
            auto p = event.position;
            p.x -= root_->rect().x;
            p.y -= root_->rect().y;
            under = root_->widget_at(p);
        }
        if (!under) {
            for (auto &w : widgets_) {
                auto p = event.position;
                p.x -= w->rect().x;
                p.y -= w->rect().y;
                under = w->widget_at(p);
                if (under) {
                    break;
                }
            }
        }
        if (under != hovered_widget_) {
            if (hovered_widget_) {
                auto leave_ev = event;
                leave_ev.type = MouseEvent::Type::Leave;
                Widget::dispatch_mouse_event(hovered_widget_, leave_ev);
            }
            hovered_widget_ = under;
            needs_redraw = true;
        }
        update_tooltip(under, event.position);
    }

    if (event.type == MouseEvent::Type::Press) {
        hide_tooltip();
    }

    if (hovered_widget_) {
        set_cursor(hovered_widget_->cursor());
    } else {
        set_cursor(CursorShape::Arrow);
    }

    if (needs_redraw) {
        request_redraw("event");
    }
}

void Window::handle_key(KeyEvent const &event) {
    if (!popups_.empty()) {
        auto &popup = popups_.back();
        if (popup.on_key && popup.on_key(event)) {
            request_redraw("event");
            return;
        }
    }

    if (event.type == KeyEvent::Type::Press && event.key == Key::Tab) {
        focus_next(event.shift);
        request_redraw("event");
        return;
    }

    auto mnemonic_mod = event.alt || event.super || event.ctrl;
    if (event.type == KeyEvent::Type::Press && mnemonic_mod && !event.text.empty()) {
        auto key = static_cast<char>(std::tolower(static_cast<unsigned char>(event.text[0])));
        auto targets = std::vector<Widget *>{};

        if (root_) {
            root_->collect_mnemonics(targets);
        }
        for (auto &w : widgets_) {
            w->collect_mnemonics(targets);
        }
        for (auto *w : targets) {
            if (w->trigger_mnemonic(key)) {
                request_redraw("event");
                return;
            }
        }
    }

    for (auto const &cmd : global_commands_) {
        if (cmd->matches_key_event(event)) {
            cmd->execute();
            request_redraw("event");
            return;
        }
    }

    if (focused_widget_) {
        auto w = focused_widget_;
        while (w) {
            if (w->handle_key_impl(event)) {
                return;
            }
            w = w->parent();
        }
    }

    // If focused widget didn't handle it, or nothing is focused,
    // try a recursive search from the root for global shortcuts attached to widgets.
    if (root_ && dispatch_key_event_recursive(root_.get(), event)) {
        request_redraw("event");
        return;
    }

    for (auto &widget : widgets_) {
        if (dispatch_key_event_recursive(widget.get(), event)) {
            request_redraw("event");
            return;
        }
    }
}

bool Window::dispatch_key_event_recursive(Widget *w, KeyEvent const &event) {
    if (!w->is_visible()) {
        return false;
    }

    // Try children first (depth first)
    bool handled = false;
    w->for_each_child([&](Widget *child) {
        if (!handled && dispatch_key_event_recursive(child, event)) {
            handled = true;
        }
    });

    if (handled) {
        return true;
    }

    // Try the widget itself
    return w->handle_key_impl(event);
}

auto Window::content_min_size() const -> Size {
    if (root_) {
        return root_->size_hint();
    }
    return {};
}

auto Window::min_size() const -> Size {
    if (min_size_.width > 0 || min_size_.height > 0) {
        return min_size_;
    }
    return content_min_size();
}

void Window::handle_resize(Size new_size) {
    size_ = new_size;
    if (has_popup()) {
        close_all_popups();
    }
    if (root_) {
        root_->set_rect({0, 0, size_.width, size_.height});
    }
}

void Window::resize_to_fit() {
    if (!root_) {
        return;
    }
    auto hint = root_->size_hint();
    Size new_size = size_;
    bool changed = false;
    if (hint.width > size_.width) {
        new_size.width = hint.width;
        changed = true;
    }
    if (hint.height > size_.height) {
        new_size.height = hint.height;
        changed = true;
    }
    if (changed) {
        size_ = new_size;
        if (impl_->platform) {
            impl_->platform->set_size(size_);
        }
    }
}

void Window::update_tooltip(Widget *under, Point mouse_pos) {
    if (under == tooltip_widget_) {
        if (tooltip_visible_ && under && under->tooltip() != tooltip_text_) {
            tooltip_text_ = under->tooltip();
            if (!tooltip_text_.empty()) {
                show_tooltip_window(tooltip_text_, tooltip_mouse_pos_);
            } else {
                hide_tooltip();
            }
        }
        return;
    }

    hide_tooltip();
    tooltip_widget_ = under;

    if (under && !under->tooltip().empty()) {
        auto delay = Theme::current().tooltip.delay_sec;

        tooltip_mouse_pos_ = mouse_pos;
        tooltip_text_ = under->tooltip();
        tooltip_timer_id_ = start_timer(delay, [this] { show_tooltip(); }, false);
    }
}

void Window::show_tooltip() {
    tooltip_visible_ = true;
    tooltip_timer_id_ = 0;
    show_tooltip_window(tooltip_text_, tooltip_mouse_pos_);
}

void Window::hide_tooltip() {
    if (tooltip_timer_id_) {
        stop_timer(tooltip_timer_id_);
        tooltip_timer_id_ = 0;
    }
    if (tooltip_visible_) {
        tooltip_visible_ = false;
        hide_tooltip_window();
    }
    tooltip_widget_ = nullptr;
    tooltip_text_.clear();
}

void Window::draw_debug_frames_recursive(Painter &painter, Widget *widget) {
    if (!widget || !widget->is_visible()) {
        return;
    }

    // Draw for the widget itself
    auto const is_layout = dynamic_cast<VBoxLayout *>(widget) != nullptr ||
                           dynamic_cast<HBoxLayout *>(widget) != nullptr ||
                           dynamic_cast<TabWidget *>(widget) != nullptr;
    auto const r = widget->rect();

    if (is_layout) {
        painter.set_line_style(Painter::LineStyle::Dotted);
    } else {
        painter.set_line_style(Painter::LineStyle::Dashed);
    }

    // ALWAYS draw at 'r' because the painter is currently at the parent's origin
    painter.draw_rect(r, {1.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
    painter.set_line_style(Painter::LineStyle::Solid);

    // Recurse into children
    painter.push_translation({r.x, r.y});
    widget->for_each_child([&](Widget *child) { draw_debug_frames_recursive(painter, child); });
    painter.pop_translation();
}

} // namespace toolkit
