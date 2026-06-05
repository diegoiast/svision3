// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/window.hpp"
#include "toolkit/html_view.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/tab_widget.hpp"
#include "toolkit/theme.hpp"
#include <cctype>
#include <chrono>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace toolkit {

static auto is_descendant_of(Widget *descendant, Widget *ancestor) -> bool {
    while (descendant) {
        if (descendant == ancestor) {
            return true;
        }
        descendant = descendant->parent();
    }
    return false;
}

static void draw_on_top_recursive(Painter &painter, Widget *parent) {
    parent->for_each_child([&](Widget *child) {
        if (!child->is_visible()) {
            return;
        }
        if (child->is_on_top()) {
            child->draw(painter);
        }
        draw_on_top_recursive(painter, child);
    });
}

static auto find_focusable_on_top(Widget *parent, Point window_pos) -> Widget * {
    auto result = static_cast<Widget *>(nullptr);
    parent->for_each_child([&](Widget *child) {
        if (result || !child->is_visible()) {
            return;
        }
        auto local = window_pos;
        local.x -= child->rect().x;
        local.y -= child->rect().y;
        if (child->is_on_top()) {
            result = child->find_focusable_at(local);
        }
        if (!result) {
            result = find_focusable_on_top(child, local);
        }
    });
    return result;
}

static auto widget_at_on_top(Widget *parent, Point window_pos) -> Widget * {
    auto result = static_cast<Widget *>(nullptr);
    parent->for_each_child([&](Widget *child) {
        if (result || !child->is_visible()) {
            return;
        }
        auto local = window_pos;
        local.x -= child->rect().x;
        local.y -= child->rect().y;
        if (child->is_on_top()) {
            result = child->widget_at(local);
        }
        if (!result) {
            result = widget_at_on_top(child, local);
        }
    });
    return result;
}

static bool dispatch_to_on_top(Widget *parent, MouseEvent const &event) {
    auto handled = false;
    parent->for_each_child([&](Widget *child) {
        if (handled || !child->is_visible()) {
            return;
        }
        if (child->is_on_top()) {
            if (Widget::dispatch_mouse_event(child, event)) {
                handled = true;
            }
        }
        if (!handled) {
            auto child_event = event;
            child_event.position.x -= child->rect().x;
            child_event.position.y -= child->rect().y;
            handled = dispatch_to_on_top(child, child_event);
        }
    });
    return handled;
}

struct Window::Impl {
    std::unique_ptr<PlatformWindow> platform;
    Window::Statistics stats;
    std::chrono::steady_clock::time_point last_log_time = std::chrono::steady_clock::now();

    // FIXME: review these parts.
    uint64_t draws_since_last_log = 0;
    double draw_time_sum_ms = 0;
    double repaint_time_sum_ms = 0;

    bool logging_enabled = false;
    int stats_timer_id = 0;

    std::unique_ptr<HtmlView> rich_tooltip_view;
    Point rich_tooltip_pos;
};

Window::Window(std::string_view title, Size size, WindowOptions options)
    : title_(title), size_(size), options_(options), impl_(std::make_unique<Impl>()) {
    if (options_.csd) {
        auto const &m = Theme::current().palette.window_decoration;
        size_.width += m.left + m.right;
        size_.height += m.top + m.bottom;
    }
    impl_->platform = detail::current_platform()->create_window(title, size_, this, options);

    theme_observer_alive_ = std::make_shared<bool>(true);
    Theme::add_theme_observer([this, alive = theme_observer_alive_](const Theme &) {
        if (*alive) {
            on_theme_changed();
        }
    });
}

Window::~Window() {
    *theme_observer_alive_ = false;
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

auto Window::painter_name() const -> std::string_view { return impl_->platform->painter_name(); }

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
    if (options_.csd) {
        if (auto *layout = dynamic_cast<VBoxLayout *>(root_.get())) {
            if (layout->items().size() >= 2) {
                auto content = layout->release_item(1);
                layout->clear_items();
                auto new_layout = std::make_unique<VBoxLayout>();
                new_layout->set_spacing(0);
                new_layout->set_margins({0, 0, 0, 0});
                auto *title_bar = Theme::current().create_title_bar(this).release();
                new_layout->add_widget(std::unique_ptr<Widget>(title_bar));
                title_bar->set_window(this);
                new_layout->add_widget(std::move(content), 1);
                root_ = std::move(new_layout);
            }
        }
    }
    if (root_) {
        on_theme_changed_recursive(root_.get());
    }
    for (auto &w : widgets_) {
        on_theme_changed_recursive(w.get());
    }
    relayout();
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

void Window::start_system_move(uint32_t serial) {
    if (impl_->platform) {
        impl_->platform->start_system_move(serial);
    }
}

void Window::start_system_resize(WindowEdge edge, uint32_t serial) {
    if (impl_->platform) {
        impl_->platform->start_system_resize(edge, serial);
    }
}

void Window::minimize() {
    if (impl_->platform) {
        impl_->platform->minimize();
    }
}

void Window::maximize() {
    is_maximized_ = true;
    if (impl_->platform) {
        impl_->platform->maximize();
    }
}

void Window::restore() {
    is_maximized_ = false;
    if (impl_->platform) {
        impl_->platform->restore();
    }
}

void Window::set_title(std::string_view t) {
    title_ = t;
    if (impl_->platform) {
        impl_->platform->set_title(t);
    }
}

void Window::handle_maximized(bool maximized) {
    if (is_maximized_ == maximized) {
        return;
    }
    is_maximized_ = maximized;
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

Window &Window::set_root(std::unique_ptr<Widget> root) {
    if (options_.csd) {
        auto layout = std::make_unique<VBoxLayout>();
        layout->set_spacing(0);
        layout->set_margins({0, 0, 0, 0});
        auto *title_bar = Theme::current().create_title_bar(this).release();
        layout->add_widget(std::unique_ptr<Widget>(title_bar));
        title_bar->set_window(this);
        layout->add_widget(std::move(root), 1);
        root_ = std::move(layout);
    } else {
        root_ = std::move(root);
    }
    if (root_) {
        root_->set_window(this);
    }

    relayout();
    return *this;
}

void Window::add_widget(std::unique_ptr<Widget> widget) {
    widget->set_window(this);
    widgets_.push_back(std::move(widget));
}

void Window::relayout_toasts() {
    auto toast_x = 10.0f;
    auto toast_y = size_.height - 10.0f;

    for (auto &widget : widgets_) {
        if (auto toast = dynamic_cast<ToastWidget *>(widget.get())) {
            auto hint = toast->size_hint();
            toast_y -= hint.height;
            toast->set_rect({toast_x, toast_y, hint.width, hint.height});
            toast_y -= 10.0f;
        }
    }
}

void Window::show_toast(std::string text, std::string title, std::string icon_path, float timeout) {
    auto toast = std::make_unique<ToastWidget>(text, title, icon_path, timeout);
    auto toast_ptr = toast.get();
    toast->set_on_close([this, toast_ptr] { this->close_toast(toast_ptr); });
    add_widget(std::move(toast));
    relayout_toasts();

    start_toast_timer();
    request_redraw("toast added");
}

void Window::show_toast(ToastBuilder const &builder) {
    auto toast = builder.build();
    auto toast_ptr = toast.get();
    toast->set_on_close([this, toast_ptr] { this->close_toast(toast_ptr); });
    add_widget(std::move(toast));
    relayout_toasts();

    start_toast_timer();
    request_redraw("toast added");
}

void Window::close_toast(ToastWidget *toast) {
    toast->expire();
    relayout_toasts();
    request_redraw("toast closed");
}

void Window::start_toast_timer() {
    if (toast_timer_id_ != 0) {
        return;
    }
    toast_timer_id_ = start_timer(
        0.1f,
        [this] {
            auto needs_redraw = false;
            auto needs_relayout = false;
            for (auto it = widgets_.begin(); it != widgets_.end();) {
                if (auto toast = dynamic_cast<ToastWidget *>(it->get())) {
                    toast->update_remaining_time(0.1f);
                    needs_redraw = true;
                    if (toast->is_expired()) {
                        if (hovered_widget_ && is_descendant_of(hovered_widget_, toast)) {
                            hovered_widget_ = nullptr;
                        }
                        if (captured_widget_ && is_descendant_of(captured_widget_, toast)) {
                            captured_widget_ = nullptr;
                        }
                        if (focused_widget_ && is_descendant_of(focused_widget_, toast)) {
                            set_focused_widget(nullptr);
                        }
                        it = widgets_.erase(it);
                        needs_relayout = true;
                        continue;
                    }
                }
                ++it;
            }
            if (needs_redraw) {
                request_redraw("toast tick");
            }
            if (needs_relayout) {
                relayout_toasts();
            }
            auto any_toasts = std::any_of(widgets_.begin(), widgets_.end(), [](auto const &w) {
                return dynamic_cast<ToastWidget *>(w.get()) != nullptr;
            });
            if (!any_toasts) {
                stop_timer(toast_timer_id_);
                toast_timer_id_ = 0;
            }
        },
        true);
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

    auto const &pal = Theme::current().palette;
    auto bg = is_active_ ? pal.window : pal.window_inactive.value_or(pal.window);
    auto repaint_start = std::chrono::steady_clock::now();
    painter.fill_rect({0, 0, size_.width, size_.height}, bg);

    if (options_.csd && pal.border_width > 0) {
        painter.draw_rect(Rect{0, 0, size_.width, size_.height}.inset(pal.border_width / 2.0f),
                          pal.border, pal.border_width);
    }

    if (root_) {
        root_->draw(painter);
    }
    for (auto &widget : widgets_) {
        widget->draw(painter);
    }
    if (root_) {
        draw_on_top_recursive(painter, root_.get());
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

    if (impl_->rich_tooltip_view) {
        auto const &theme = Theme::current();
        auto const &pal = theme.palette;
        auto r = impl_->rich_tooltip_view->rect();
        painter.fill_rounded_rect(r, pal.tooltip, pal.corner_radius);
        impl_->rich_tooltip_view->draw(painter);
        painter.draw_rounded_rect(r, pal.border, pal.corner_radius, pal.border_width);
    }

    if (tooltip_visible_ && tooltip_widget_) {
        auto const &current = tooltip_widget_->tooltip();
        if (current != tooltip_text_) {
            tooltip_text_ = current;
            if (!tooltip_text_.empty() && !tooltip_widget_->tooltip_is_markdown()) {
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

    if (event.type == MouseEvent::Type::Press) {
        last_serial_ = event.serial;
    }

    if (options_.csd) {
        auto const &m = Theme::current().palette.window_decoration;
        auto const &r = event.position;
        float corner_area = 25.0f;
        float edge_area = 5.0f;
        WindowEdge edge = WindowEdge::None;

        // Bottom corners: 25px threshold
        if (r.y > size_.height - corner_area) {
            if (r.x < corner_area) {
                edge = WindowEdge::BottomLeft;
            } else if (r.x > size_.width - corner_area) {
                edge = WindowEdge::BottomRight;
            }
        }

        // Everything else: 5px threshold
        if (edge == WindowEdge::None) {
            if (r.y < edge_area) {
                if (r.x < edge_area) {
                    edge = WindowEdge::TopLeft;
                } else if (r.x > size_.width - edge_area) {
                    edge = WindowEdge::TopRight;
                } else {
                    edge = WindowEdge::Top;
                }
            } else if (r.y > size_.height - edge_area) {
                edge = WindowEdge::Bottom;
            } else if (r.x < edge_area) {
                edge = WindowEdge::Left;
            } else if (r.x > size_.width - edge_area) {
                edge = WindowEdge::Right;
            }
        }

        if (edge != WindowEdge::None) {
            if (event.type == MouseEvent::Type::Press) {
                start_system_resize(edge, event.serial);
                return;
            } else if (event.type == MouseEvent::Type::Move ||
                       event.type == MouseEvent::Type::Drag) {
                if (edge == WindowEdge::Left || edge == WindowEdge::Right) {
                    set_cursor(CursorShape::ResizeEW);
                } else if (edge == WindowEdge::Top || edge == WindowEdge::Bottom) {
                    set_cursor(CursorShape::ResizeNS);
                } else if (edge == WindowEdge::TopLeft || edge == WindowEdge::BottomRight) {
                    set_cursor(CursorShape::ResizeNW);
                } else {
                    set_cursor(CursorShape::ResizeNESW);
                }
                return;
            }
        }
    }

    if (event.type == MouseEvent::Type::Press) {
        hide_tooltip();
    }

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
                    if (size_after < size_before) {
                        while (static_cast<int>(popups_.size()) - 1 > i) {
                            close_popup();
                        }
                    } else if (size_after > size_before) {
                        while (static_cast<int>(popups_.size()) - 1 > (i + 1)) {
                            auto new_child = std::move(popups_.back());
                            popups_.pop_back();
                            while (static_cast<int>(popups_.size()) - 1 > i) {
                                close_popup();
                            }
                            popups_.push_back(std::move(new_child));
                        }
                    } else {
                        if (event.type == MouseEvent::Type::Press) {
                            while (static_cast<int>(popups_.size()) - 1 > i) {
                                close_popup();
                            }
                        }
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
        auto under = static_cast<Widget *>(nullptr);
        for (auto &w : widgets_) {
            auto p = event.position;
            p.x -= w->rect().x;
            p.y -= w->rect().y;
            under = w->find_focusable_at(p);
            if (under) {
                break;
            }
        }
        if (!under && root_) {
            under = find_focusable_on_top(root_.get(), event.position);
        }
        if (!under && root_) {
            auto p = event.position;
            p.x -= root_->rect().x;
            p.y -= root_->rect().y;
            under = root_->find_focusable_at(p);
        }
        set_focused_widget(under);
        captured_widget_ = under;
        needs_redraw = true;
    }

    if (captured_widget_) {
        set_cursor(captured_widget_->cursor());
        // Dispatch to captured widget
        auto captured_ev = event;
        captured_ev.position = captured_widget_->map_from_window(event.position);
        if (captured_widget_->handle_mouse(captured_ev)) {
            needs_redraw = true;
        }

        if (event.type == MouseEvent::Type::Release) {
            captured_widget_ = nullptr;
        }

        if (event.type == MouseEvent::Type::Move || event.type == MouseEvent::Type::Drag) {
            update_tooltip(captured_widget_, event.position);
        }

        if (needs_redraw) {
            request_redraw("event (captured)");
        }
        return;
    } else {
        // Normal dispatch — overlay widgets (toasts) first
        for (auto &widget : widgets_) {
            if (Widget::dispatch_mouse_event(widget.get(), event)) {
                needs_redraw = true;
            }
        }
        if (root_) {
            // FIXME: what is the difference between these 2? This triggers clicking 2 times
            if (dispatch_to_on_top(root_.get(), event)) {
                needs_redraw = true;
            }
            if (Widget::dispatch_mouse_event(root_.get(), event)) {
                needs_redraw = true;
            }
        }
    }

    if (event.type == MouseEvent::Type::Move || event.type == MouseEvent::Type::Drag) {
        auto under = static_cast<Widget *>(nullptr);
        for (auto &w : widgets_) {
            auto p = event.position;
            p.x -= w->rect().x;
            p.y -= w->rect().y;
            under = w->widget_at(p);
            if (under) {
                break;
            }
        }
        if (!under && root_) {
            under = widget_at_on_top(root_.get(), event.position);
        }
        if (!under && root_) {
            auto p = event.position;
            p.x -= root_->rect().x;
            p.y -= root_->rect().y;
            under = root_->widget_at(p);
        }

        // Only update hovered_widget_ if not dragging, or if under is the captured widget
        if (!captured_widget_ || under == captured_widget_) {
            if (under != hovered_widget_) {
                if (hovered_widget_) {
                    auto leave_ev = event;
                    leave_ev.type = MouseEvent::Type::Leave;
                    Widget::dispatch_mouse_event(hovered_widget_, leave_ev);
                }
                hovered_widget_ = under;
                needs_redraw = true;
            }
        } else {
            // Dragging but over another widget - send leave to current hovered if any
            if (hovered_widget_) {
                auto leave_ev = event;
                leave_ev.type = MouseEvent::Type::Leave;
                Widget::dispatch_mouse_event(hovered_widget_, leave_ev);
                hovered_widget_ = nullptr;
                needs_redraw = true;
            }
        }
        update_tooltip(under, event.position);
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
    if (on_key && on_key(event)) {
        return;
    }
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

    // Check focused widget and its parents FIRST
    if (focused_widget_) {
        auto w = focused_widget_;
        while (w) {
            if (w->handle_key_impl(event)) {
                return;
            }
            w = w->parent();
        }
    }

    // Try global commands LAST
    for (auto const &cmd : global_commands_) {
        if (cmd->matches_key_event(event)) {
            cmd->execute();
            request_redraw("event");
            return;
        }
    }

    // If focused widget didn't handle it, or nothing is focused,
    // try a recursive search from the root for shortcuts attached to widgets.
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
    relayout();
}

void Window::handle_activate(bool active) {
    if (is_active_ == active) {
        return;
    }
    is_active_ = active;
    request_redraw("window activate");
}

void Window::relayout() {
    if (root_) {
        auto bw = options_.csd ? Theme::current().palette.border_width : 0.0f;
        root_->set_rect({bw, bw, size_.width - 2 * bw, size_.height - 2 * bw});
    }
    request_redraw();
}

Window &Window::resize_to_fit() {
    if (!root_) {
        return *this;
    }
    // First pass: layout at the current width so height-dependent widgets
    // (e.g. RichLabel/HtmlView) render at their actual allocated width
    // before we query their size_hint for the final height.
    auto bw = options_.csd ? Theme::current().palette.border_width : 0.0f;
    root_->set_rect({bw, bw, size_.width - 2 * bw, size_.height - 2 * bw});
    auto hint = root_->size_hint();
    auto changed = false;
    if (hint.width > size_.width) {
        size_.width = hint.width;
        changed = true;
    }
    if (hint.height > size_.height) {
        size_.height = hint.height;
        changed = true;
    }
    if (changed) {
        if (impl_->platform) {
            impl_->platform->set_size(size_);
        }
    }
    relayout();
    return *this;
}

void Window::update_tooltip(Widget *under, Point mouse_pos) {
    if (under == tooltip_widget_) {
        tooltip_mouse_pos_ = mouse_pos;
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

void Window::show_rich_tooltip() {
    auto cap_w = std::min(size_.width, 280.0f);
    auto view = std::make_unique<HtmlView>();
    view->set_window(this);
    view->set_background_color(Color::rgba(0, 0, 0, 0));
    view->set_draw_frame(false);
    view->set_content_margin(2);
    view->set_content_max_width(static_cast<int>(cap_w));
    view->set_rect({0, 0, cap_w, 1000});
    view->set_markdown(tooltip_text_);
    auto hint = view->size_hint();
    auto w = std::min(hint.width, cap_w);
    auto h = hint.height;
    auto x = tooltip_mouse_pos_.x;
    auto y = tooltip_mouse_pos_.y - h - 8;
    if (y < 0) {
        y = tooltip_mouse_pos_.y + 20;
    }
    if (x + w > size_.width) {
        x = size_.width - w;
    }
    x = std::max(x, 0.0f);
    view->set_rect({x, y, w, h});
    impl_->rich_tooltip_view = std::move(view);
    impl_->rich_tooltip_pos = {x, y};
    request_redraw("rich tooltip show");
}

void Window::show_tooltip() {
    tooltip_visible_ = true;
    tooltip_timer_id_ = 0;
    if (tooltip_widget_ && tooltip_widget_->tooltip_is_markdown()) {
        show_rich_tooltip();
    } else {
        show_tooltip_window(tooltip_text_, tooltip_mouse_pos_);
    }
}

void Window::hide_tooltip() {
    if (tooltip_timer_id_) {
        stop_timer(tooltip_timer_id_);
        tooltip_timer_id_ = 0;
    }
    if (tooltip_visible_) {
        tooltip_visible_ = false;
        if (impl_->rich_tooltip_view) {
            impl_->rich_tooltip_view.reset();
            request_redraw("rich tooltip hide");
        } else {
            hide_tooltip_window();
        }
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

nlohmann::json Window::to_json() const {
    nlohmann::json j;
    j["title"] = title_;
    j["size"] = {{"width", size_.width}, {"height", size_.height}};
    if (root_) {
        j["root"] = root_->to_json();
    }
    if (!widgets_.empty()) {
        auto widgets = nlohmann::json::array();
        for (auto const &w : widgets_) {
            widgets.push_back(w->to_json());
        }
        j["widgets"] = widgets;
    }
    return j;
}
} // namespace toolkit
