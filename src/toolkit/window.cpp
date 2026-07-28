// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/window.hpp"
#include "toolkit/html_view.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/tab_widget.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window_title_bar.hpp"
#include <cctype>
#include <chrono>
#include <nlohmann/json.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

namespace toolkit {

static void draw_size_hint_guides_recursive(Painter &painter, Widget *widget, float font_size);

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
        auto const &m = Theme::current().style.window_decoration;
        size_.width += m.left + m.right;
        size_.height += m.top + m.bottom;
    }
    impl_->platform = detail::current_platform()->create_window(title, size_, this, options);
    // Seed the cached scale so handle_scale_changed() only fires on real changes,
    // even if the window is born on a high-DPI display.
    scale_ = impl_->platform->scale_factor();

    theme_observer_alive_ = std::make_shared<bool>(true);
    Theme::add_theme_observer([this, alive = theme_observer_alive_](const Theme &) {
        if (*alive) {
            on_theme_changed();
        }
    });
}

Window::~Window() {
    // Null out window_ on all owned widgets so any stale external Widget*
    // references see window_ == nullptr rather than a dangling pointer.
    if (root_) {
        root_->set_window(nullptr);
    }
    for (auto &w : widgets_) {
        w->set_window(nullptr);
    }
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
        auto icon = impl_->platform->capture();
        if (!icon) {
            return false;
        }
        auto app = detail::current_platform();
        if (!app) {
            return false;
        }
        auto loader = app->get_image_loader();
        if (!loader) {
            return false;
        }
        return loader->save(*icon, path);
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

void Window::grab_pointer() {
    if (impl_->platform) {
        impl_->platform->grab_pointer();
    }
}

void Window::ungrab_pointer() {
    if (impl_->platform) {
        impl_->platform->ungrab_pointer();
    }
}

auto Window::painter_name() const -> std::string_view { return impl_->platform->painter_name(); }

void Window::show() {
    if (impl_->platform) {
        impl_->platform->show();
        // Sync icon from platform to titlebar
        auto icon = get_icon();
        set_icon(icon);
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
                root_->set_window(this);
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
    if (impl_ && impl_->platform) {
        impl_->platform->stop_timer(id);
    }
}

void Window::set_cursor(CursorShape shape) {
    if (impl_ && impl_->platform) {
        impl_->platform->set_cursor(shape);
    }
}

bool Window::is_resizable() const { return options_.resizable; }
bool Window::is_movable() const { return options_.movable; }
bool Window::is_minimizable() const { return options_.minimizable; }
bool Window::is_maximizable() const { return options_.maximizable; }
bool Window::is_closable() const { return options_.closable; }

void Window::start_system_move(uint32_t serial) {
    if (!is_movable()) {
        return;
    }
    if (impl_ && impl_->platform) {
        impl_->platform->start_system_move(serial);
    }
}

void Window::start_system_resize(WindowEdge edge, uint32_t serial) {
    if (!is_resizable()) {
        return;
    }
    if (impl_ && impl_->platform) {
        impl_->platform->start_system_resize(edge, serial);
    }
}

void Window::minimize() {
    if (!is_minimizable()) {
        return;
    }
    if (impl_ && impl_->platform) {
        impl_->platform->minimize();
    }
}

void Window::maximize() {
    if (!is_maximizable()) {
        return;
    }
    is_maximized_ = true;
    if (impl_ && impl_->platform) {
        impl_->platform->maximize();
    }
}

void Window::restore() {
    is_maximized_ = false;
    if (impl_ && impl_->platform) {
        impl_->platform->restore();
    }
}

void Window::set_title(std::string_view t) {
    title_ = t;
    if (impl_ && impl_->platform) {
        impl_->platform->set_title(t);
    }
}

void Window::set_icon(Icon const &icon) {
    spdlog::log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info,
                "Window::set_icon called, icon valid: {}", (bool)icon);
    if (impl_->platform) {
        impl_->platform->set_icon(icon);
    }
    if (options_.csd && root_) {
        // FIXME: keep a pointer to the title. It should be non null if CSD
        auto *layout = dynamic_cast<VBoxLayout *>(root_.get());
        if (layout && !layout->items().empty()) {
            auto *title_bar = dynamic_cast<WindowTitleBar *>(layout->items().front().widget.get());
            if (title_bar) {
                spdlog::info("WindowTitleBar::set_icon calling (title_bar={:p})",
                             (void *)title_bar);
                title_bar->set_icon(icon);
            }
        }
    }
}

Icon Window::get_icon() const {
    if (impl_->platform) {
        return impl_->platform->get_icon();
    }
    return nullptr;
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
    auto const &s = Theme::current().style;
    auto shadow = (options_.csd && !is_maximized_) ? s.shadow.size : 0.0f;
    auto bw = options_.csd ? s.border_width : 0.0f;
    auto inset = bw + shadow;
    auto toast_x = 10.0f + inset;
    auto toast_y = size_.height - 10.0f - inset;

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

    auto const &style = Theme::current().style;
    auto const &pal = Theme::current().palette;
    auto bg = is_active_ ? pal.window : pal.window_inactive.value_or(pal.window);
    auto repaint_start = std::chrono::steady_clock::now();
    auto content_rect = Rect{0, 0, size_.width, size_.height};

    if (options_.csd && !is_maximized_) {
        auto shadow_size = style.shadow.size;
        content_rect = content_rect.inset(shadow_size);

        // Draw shadow with a softer quintic falloff, using silver color
        for (auto i = 0; i < shadow_size; ++i) {
            auto t = static_cast<float>(i) / shadow_size;
            auto alpha = style.shadow.opacity * std::pow(1.0f - t, 5.0f);
            auto shadow_color = Color{0.25f, 0.25f, 0.25f, alpha};
            painter.draw_rounded_rect(content_rect.inset(-i - 0.5f), shadow_color,
                                      style.corner_radius + i, 1.0f);
        }
    }

    if (style.corner_radius > 0 && !is_maximized_) {
        painter.fill_rounded_rect(content_rect, bg, style.corner_radius);
    } else {
        painter.fill_rect(content_rect, bg);
    }

    if (options_.csd && style.border_width > 0) {
        if (style.corner_radius > 0 && !is_maximized_) {
            painter.draw_rounded_rect(content_rect.inset(style.border_width / 2.0f), pal.border,
                                      style.corner_radius - style.border_width / 2.0f,
                                      style.border_width);
        } else {
            painter.draw_rect(content_rect.inset(style.border_width / 2.0f), pal.border,
                              style.border_width);
        }
    }

    if (root_) {
        if (options_.csd && style.corner_radius > 0 && !is_maximized_) {
            painter.push_clip(content_rect, style.corner_radius);
            root_->draw(painter);
            painter.pop_clip();
        } else {
            root_->draw(painter);
        }
    }
    for (auto &widget : widgets_) {
        widget->draw(painter);
    }
    if (root_) {
        if (options_.csd && style.corner_radius > 0 && !is_maximized_) {
            painter.push_clip(content_rect, style.corner_radius);
            draw_on_top_recursive(painter, root_.get());
            painter.pop_clip();
        } else {
            draw_on_top_recursive(painter, root_.get());
        }
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
    if (Widget::debug_show_inspector) {
        auto const font_size = Theme::current().palette.fonts.size * 0.8f;
        if (root_) {
            draw_size_hint_guides_recursive(painter, root_.get(), font_size);
        }
        for (auto &widget : widgets_) {
            draw_size_hint_guides_recursive(painter, widget.get(), font_size);
        }
        draw_widget_inspector(painter);
    }

    for (auto const &popup : popups_) {
        if (popup.on_paint) {
            painter.push_clip(popup.bounds, style.corner_radius);
            painter.push_translation({popup.bounds.x, popup.bounds.y});
            popup.on_paint(painter);
            painter.pop_translation();
            painter.pop_clip();
        }
    }

    if (impl_->rich_tooltip_view) {
        auto const &theme = Theme::current();
        auto const &pal = theme.palette;
        auto r = impl_->rich_tooltip_view->rect();
        painter.fill_rounded_rect(r, pal.tooltip, theme.style.corner_radius);
        impl_->rich_tooltip_view->draw(painter);
        painter.draw_rounded_rect(r, pal.border, theme.style.corner_radius,
                                  theme.style.border_width);
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

    if (options_.csd && is_resizable()) {
        auto const &s = Theme::current().style;
        auto const &r = event.position;
        auto shadow = is_maximized_ ? 0.0f : s.shadow.size;
        // FIXME: move to style?
        auto corner_area = 25.0f;
        auto edge_area = 5.0f;
        auto edge = WindowEdge::None;

        // Check if mouse is within the "real" window area (excluding shadows)
        if (r.x >= shadow && r.x <= size_.width - shadow && r.y >= shadow &&
            r.y <= size_.height - shadow) {

            auto rx = r.x - shadow;
            auto ry = r.y - shadow;
            auto rw = size_.width - 2 * shadow;
            auto rh = size_.height - 2 * shadow;

            // Bottom corners
            if (ry > rh - corner_area) {
                if (rx < corner_area) {
                    edge = WindowEdge::BottomLeft;
                } else if (rx > rw - corner_area) {
                    edge = WindowEdge::BottomRight;
                }
            }

            if (edge == WindowEdge::None) {
                if (ry < edge_area) {
                    if (rx < edge_area) {
                        edge = WindowEdge::TopLeft;
                    } else if (rx > rw - edge_area) {
                        edge = WindowEdge::TopRight;
                    } else {
                        edge = WindowEdge::Top;
                    }
                } else if (ry > rh - edge_area) {
                    edge = WindowEdge::Bottom;
                } else if (rx < edge_area) {
                    edge = WindowEdge::Left;
                } else if (rx > rw - edge_area) {
                    edge = WindowEdge::Right;
                }
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
        if (captured_widget_ && event.button == 0) {
            platform_window()->grab_pointer();
        }
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
            platform_window()->ungrab_pointer();
            captured_widget_ = nullptr;
        }

        if (event.type == MouseEvent::Type::Move || event.type == MouseEvent::Type::Drag) {
            update_tooltip(captured_widget_, event.position);
        }

        if (needs_redraw) {
            request_redraw("event (captured)");
        }
        return;
    } else if (event.type == MouseEvent::Type::Press || event.type == MouseEvent::Type::Release ||
               event.type == MouseEvent::Type::Scroll) {
        // Press/Release/Scroll are single-target events. Resolve the exact widget under
        // the pointer with widget_at() (bounds-checked at every level — the same resolver
        // used for hover below) and dispatch to it directly, instead of broadcasting the
        // event to every top-level widget/layout and trusting each one to reject it if it
        // isn't actually theirs. That broadcast-and-self-reject model is what let a click
        // land on an unrelated sibling and be misinterpreted by it (e.g. a Scrollbar's
        // "click on track" fallback firing for a click that never touched the scrollbar).
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
        if (under) {
            auto local_ev = event;
            local_ev.position = under->map_from_window(event.position);
            if (under->handle_mouse(local_ev)) {
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

        if (!captured_widget_ || under == captured_widget_) {
            // Only update hovered_widget_ if not dragging, or if under is the captured widget
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

        // This block is only reached when there is no captured widget (the captured-widget
        // branch above returns early), so deliver the Move/Drag itself to whatever is under
        // the pointer — otherwise widgets never see hover-only Move events and can't update
        // their own internal hover state (e.g. Splitter's divider cursor, Scrollbar's
        // button/thumb hover, TabWidget's hovered tab).
        if (under) {
            auto local_ev = event;
            local_ev.position = under->map_from_window(event.position);
            if (under->handle_mouse(local_ev)) {
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
        if (focused_widget_ && focused_widget_->dispatch_key_event(event)) {
            return;
        }
        focus_next(event.shift);
        request_redraw("event");
        return;
    }

    auto mnemonic_mod = event.alt || event.super || event.ctrl;
    if (event.type == KeyEvent::Type::Press && mnemonic_mod && !event.text.empty()) {
        auto key = normalize_mnemonic_key(event.text);
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
            if (w->dispatch_key_event(event)) {
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
    auto handled = false;
    w->for_each_child([&](Widget *child) {
        if (!handled && dispatch_key_event_recursive(child, event)) {
            handled = true;
        }
    });
    if (handled) {
        return true;
    }
    return w->dispatch_key_event(event);
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

void Window::handle_scale_changed(float new_scale) {
    if (new_scale == scale_) {
        return;
    }
    scale_ = new_scale;
    relayout();
    request_redraw("scale changed");
    if (on_scale_changed) {
        on_scale_changed(new_scale);
    }
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
        auto const &s = Theme::current().style;
        auto bw = options_.csd ? s.border_width : 0.0f;
        auto shadow = (options_.csd && !is_maximized_) ? s.shadow.size : 0.0f;
        auto inset = bw + shadow;
        root_->set_rect({inset, inset, size_.width - 2 * inset, size_.height - 2 * inset});
        // Keep the compositor's minimum in sync with the content whenever it
        // is not overridden by an explicit set_min_size() call.
        if (impl_->platform && !(min_size_.width > 0 || min_size_.height > 0)) {
            impl_->platform->set_min_size(content_min_size());
        }
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
    auto const &s = Theme::current().style;
    auto bw = options_.csd ? s.border_width : 0.0f;
    auto shadow = (options_.csd && !is_maximized_) ? s.shadow.size : 0.0f;
    auto inset = bw + shadow;
    root_->set_rect({inset, inset, size_.width - 2 * inset, size_.height - 2 * inset});
    auto hint = root_->size_hint();
    auto changed = false;
    if (hint.width + 2 * inset > size_.width) {
        size_.width = hint.width + 2 * inset;
        changed = true;
    }
    if (hint.height + 2 * inset > size_.height) {
        size_.height = hint.height + 2 * inset;
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
        auto delay = Theme::current().style.tooltip.delay_sec;

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
    auto cw = view->content_width();
    auto w = cw > 0 ? std::min(cw, cap_w) : cap_w;
    auto h = view->size_hint().height;
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
    painter.draw_rect(r, {1.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
    painter.set_line_style(Painter::LineStyle::Solid);
    painter.push_translation({r.x, r.y});
    widget->for_each_child([&](Widget *child) { draw_debug_frames_recursive(painter, child); });
    painter.pop_translation();
}

// Fixed "debug HUD" colors for the widget inspector overlay. These are
// intentionally outside the theme palette (and identical across themes) so
// the overlay stays legible over both light and dark palettes.
static constexpr auto kInspectorBackground = Color{0.05f, 0.05f, 0.05f, 0.72f};
static constexpr auto kInspectorText = Color::rgb(0.15f, 1.0f, 0.45f);
static constexpr auto kInspectorBorder = kInspectorText;
static constexpr auto kInspectorHighlight = kInspectorText;
// Widgets that never overrode class_name() (still reporting the generic
// "Widget" base name) are flagged in red as a hint to add DECLARE_WIDGET.
static constexpr auto kInspectorWarning = Color::rgb(1.0f, 0.15f, 0.15f);

// Bracket guide style for size-hint overlays.
// kGuideArm: how far the arm extends from the widget edge to the bracket bar.
// kGuideTick: half-length of the perpendicular tick at each bracket end.
// kGuideSlack: widgets whose rect exceeds their hint by less than this are
//              considered "at minimum" and drawn in a warning colour.
static constexpr auto kGuideArm = 20.0f;
static constexpr auto kGuideTick = 6.0f;
static constexpr auto kGuideLineWidth = 1.5f;
static constexpr auto kGuideArmWidth = 0.75f;
static constexpr auto kGuideSlack = 4.0f;

// Returns the guide colour for a widget based on how much slack it has
// relative to its size_hint.
//   Red    – rect is BELOW the hint (layout violation, should not happen)
//   Orange – rect is AT the hint (this widget sets the minimum, bottleneck)
//   Blue   – rect exceeds hint (has slack, not the constraining widget)
static auto guide_color_for(Size const &hint, Rect const &r) -> Color {
    auto const slack_w = hint.width > 0.0f ? r.width - hint.width : kGuideSlack + 1.0f;
    auto const slack_h = hint.height > 0.0f ? r.height - hint.height : kGuideSlack + 1.0f;
    auto const slack = std::min(slack_w, slack_h);
    if (slack < 0.0f) {
        return Color{1.0f, 0.15f, 0.15f, 0.9f}; // red – below minimum (layout bug)
    }
    if (slack <= kGuideSlack) {
        return Color{1.0f, 0.65f, 0.1f, 0.9f}; // orange – at minimum, constraining
    }
    return Color{0.2f, 0.55f, 1.0f, 0.55f}; // blue – has slack
}

// Draw |---hint---|  bracket guides for every widget that has a non-zero
// size_hint().  Width guide is drawn above the widget (horizontal bracket);
// height guide is drawn to the left (vertical bracket).  Colour encodes
// how constrained the widget currently is (red = bottleneck).
static void draw_size_hint_guides_recursive(Painter &painter, Widget *widget, float font_size) {
    if (!widget || !widget->is_visible()) {
        return;
    }

    auto const hint = widget->size_hint();
    auto const r = widget->rect();

    if (hint.width > 0.0f || hint.height > 0.0f) {
        auto const col = guide_color_for(hint, r);

        if (hint.width > 0.0f) {
            // Horizontal bracket kGuideArm pixels above the widget top edge.
            auto const bar_y = r.y - kGuideArm;
            auto const x1 = r.x;
            auto const x2 = r.x + hint.width;
            painter.set_line_style(Painter::LineStyle::Dashed);
            painter.draw_line({x1, r.y}, {x1, bar_y}, col, kGuideArmWidth);
            painter.draw_line({x2, r.y}, {x2, bar_y}, col, kGuideArmWidth);
            painter.set_line_style(Painter::LineStyle::Solid);
            painter.draw_line({x1, bar_y}, {x2, bar_y}, col, kGuideLineWidth);
            painter.draw_line({x1, bar_y - kGuideTick}, {x1, bar_y + kGuideTick}, col,
                              kGuideLineWidth);
            painter.draw_line({x2, bar_y - kGuideTick}, {x2, bar_y + kGuideTick}, col,
                              kGuideLineWidth);
            auto const label_w = fmt::format("{:.0f}", hint.width);
            auto const lw = painter.measure_text(label_w, font_size).width;
            painter.draw_text(label_w, {(x1 + x2) / 2.0f - lw / 2.0f, bar_y - kGuideTick - 2.0f},
                              col, font_size);
        }

        if (hint.height > 0.0f) {
            // Vertical bracket kGuideArm pixels to the left of the widget.
            auto const bar_x = r.x - kGuideArm;
            auto const y1 = r.y;
            auto const y2 = r.y + hint.height;
            painter.set_line_style(Painter::LineStyle::Dashed);
            painter.draw_line({r.x, y1}, {bar_x, y1}, col, kGuideArmWidth);
            painter.draw_line({r.x, y2}, {bar_x, y2}, col, kGuideArmWidth);
            painter.set_line_style(Painter::LineStyle::Solid);
            painter.draw_line({bar_x, y1}, {bar_x, y2}, col, kGuideLineWidth);
            painter.draw_line({bar_x - kGuideTick, y1}, {bar_x + kGuideTick, y1}, col,
                              kGuideLineWidth);
            painter.draw_line({bar_x - kGuideTick, y2}, {bar_x + kGuideTick, y2}, col,
                              kGuideLineWidth);
            auto const label_h = fmt::format("{:.0f}", hint.height);
            painter.draw_text(label_h, {bar_x - kGuideTick - 2.0f, y1}, col, font_size);
        }
    }

    painter.push_translation({r.x, r.y});
    widget->for_each_child(
        [&](Widget *child) { draw_size_hint_guides_recursive(painter, child, font_size); });
    painter.pop_translation();
}

static auto json_value_to_string(nlohmann::json const &value) -> std::string {
    if (value.is_string()) {
        return value.get<std::string>();
    }
    auto s = value.dump();
    if (s.size() > 60) {
        s = s.substr(0, 57) + "...";
    }
    return s;
}

void Window::draw_widget_inspector(Painter &painter) {
    auto *widget = hovered_widget_;
    if (!widget) {
        return;
    }
    // hovered_widget_ is only recomputed on mouse move, so it can go stale when
    // a dock is collapsed by keyboard/command while the pointer sits still. A
    // widget's own visible flag stays true even when an ancestor is hidden, so
    // walk the chain and skip the overlay if anything above it is invisible.
    for (auto *w = widget; w; w = w->parent()) {
        if (!w->is_visible()) {
            return;
        }
    }

    using Line = std::pair<std::string, Color>;
    auto lines = std::vector<Line>{};
    auto j = widget->to_json();
    auto const &r = widget->rect();
    auto const hint = widget->size_hint();
    auto const is_generic_widget = widget->class_name() == "Widget";

    auto const header_color = is_generic_widget ? kInspectorWarning : kInspectorText;
    lines.push_back({fmt::format("{}  ({:.0f}x{:.0f} @ {:.0f},{:.0f})", widget->class_name(),
                                 r.width, r.height, r.x, r.y),
                     header_color});

    if (hint.width > 0.0f || hint.height > 0.0f) {
        auto const hint_col = guide_color_for(hint, r);
        lines.push_back({fmt::format("min: {:.0f}x{:.0f}", hint.width, hint.height), hint_col});
    }

    for (auto const &[key, value] : j.items()) {
        if (key == "type" || key == "rect" || value.is_object()) {
            continue;
        }
        lines.push_back({fmt::format("{}: {}", key, json_value_to_string(value)), kInspectorText});
    }

    auto const &theme = Theme::current();
    auto const &pal = theme.palette;
    auto const font_size = pal.fonts.size;
    auto const fm = painter.font_metrics(font_size);
    auto const line_height = fm.height + 2.0f;
    auto const padding = theme.style.tooltip.padding + 2.0f;

    auto max_w = 0.0f;
    for (auto const &[text, _] : lines) {
        max_w = std::max(max_w, painter.measure_text(text, font_size).width);
    }

    auto const box_w = max_w + padding * 2.0f;
    auto const box_h = static_cast<float>(lines.size()) * line_height + padding * 2.0f;
    auto const origin = widget->map_to_window({0, 0});

    auto box_x = origin.x;
    auto box_y = origin.y - box_h - 4.0f;
    if (box_y < 0.0f) {
        box_y = origin.y + r.height + 4.0f;
    }
    box_x = std::clamp(box_x, 0.0f, std::max(0.0f, size_.width - box_w));
    box_y = std::clamp(box_y, 0.0f, std::max(0.0f, size_.height - box_h));

    auto const box = Rect{box_x, box_y, box_w, box_h};
    painter.fill_rounded_rect(box, kInspectorBackground, theme.style.corner_radius);
    painter.draw_rounded_rect(box, kInspectorBorder, theme.style.corner_radius,
                              theme.style.border_width);

    auto text_y = box_y + padding + fm.ascent;
    for (auto const &[text, color] : lines) {
        painter.draw_text(text, {box_x + padding, text_y}, color, font_size);
        text_y += line_height;
    }

    painter.set_line_style(Painter::LineStyle::Dotted);
    painter.draw_rect(Rect{origin.x, origin.y, r.width, r.height}, kInspectorHighlight, 2.0f);
    painter.set_line_style(Painter::LineStyle::Solid);
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
