#include "toolkit/window.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/theme.hpp"
#include <cctype>
#include <spdlog/spdlog.h>

namespace toolkit {

struct Window::Impl {
    std::unique_ptr<PlatformWindow> platform;
};

Window::Window(std::string_view title, Size size)
    : title_(title), size_(size), impl_(std::make_unique<Impl>()) {
    impl_->platform =
        detail::current_platform()->create_window(title, size, this);
    spdlog::info("Window '{}' created ({}x{})", title_, size.width,
                 size.height);
}

Window::~Window() {
    hide_tooltip_window();
}

void Window::show() {
    impl_->platform->show();
    spdlog::info("Window '{}' shown", title_);
}

void Window::close() {
    hide_tooltip();
    impl_->platform->close();
}

void Window::request_redraw() { impl_->platform->request_redraw(); }

void Window::set_min_size(Size s) {
    min_size_ = s;
    impl_->platform->set_min_size(s);
}

void Window::set_max_size(Size s) {
    max_size_ = s;
    impl_->platform->set_max_size(s);
}

int Window::start_timer(float interval_sec, std::function<void()> callback,
                        bool repeats) {
    return impl_->platform->start_timer(interval_sec, std::move(callback),
                                        repeats);
}

void Window::stop_timer(int timer_id) { impl_->platform->stop_timer(timer_id); }

void Window::set_cursor(CursorShape shape) {
    if (shape == current_cursor_) return;
    current_cursor_ = shape;
    impl_->platform->set_cursor(shape);
}

void Window::show_tooltip_window(std::string const &text, Point pos) {
    impl_->platform->show_tooltip_window(text, pos);
}

void Window::hide_tooltip_window() {
    if (impl_->platform) impl_->platform->hide_tooltip_window();
}

bool Window::save_to_png(std::string const &path) {
    return impl_->platform->save_to_png(path);
}

float Window::scale_factor() const {
    return impl_->platform->scale_factor();
}

PlatformWindow *Window::platform_window() const {
    return impl_->platform.get();
}

void Window::add_widget(std::unique_ptr<Widget> widget) {
    widget->set_window(this);
    widgets_.push_back(std::move(widget));
}

void Window::set_root(std::unique_ptr<Widget> root) {
    root_ = std::move(root);
    root_->set_window(this);
    root_->set_rect({0, 0, size_.width, size_.height});
}

void Window::open_popup(Popup popup) {
    popup_ = std::move(popup);
    request_redraw();
}

void Window::close_popup() {
    popup_.reset();
    request_redraw();
}

void Window::handle_paint(Painter &painter) {
    painter.fill_rect({0, 0, size_.width, size_.height}, Theme::current().window.background);

    if (root_) {
        painter.push_clip(root_->rect());
        root_->paint(painter);
        painter.pop_clip();
    }
    for (auto &widget : widgets_) {
        painter.push_clip(widget->rect());
        widget->paint(painter);
        painter.pop_clip();
    }

    if (popup_ && popup_->on_paint) {
        popup_->on_paint(painter);
    }

    if (tooltip_visible_ && tooltip_widget_) {
        auto const &current = tooltip_widget_->tooltip();
        if (current != tooltip_text_) {
            tooltip_text_ = current;
            if (!tooltip_text_.empty())
                show_tooltip_window(tooltip_text_, tooltip_mouse_pos_);
            else
                hide_tooltip();
        }
    }
}

void Window::set_focused_widget(Widget *w) {
    if (w == focused_widget_) return;
    if (focused_widget_) focused_widget_->set_focused(false);
    focused_widget_ = w;
    if (focused_widget_) {
        focused_widget_->set_focused(true);
        if (!blink_timer_id_) {
            blink_timer_id_ = start_timer(0.5f, [this] { request_redraw(); });
        }
    } else {
        if (blink_timer_id_) {
            stop_timer(blink_timer_id_);
            blink_timer_id_ = 0;
        }
    }
}

void Window::focus_next(bool reverse) {
    std::vector<Widget *> focusables;
    if (root_) root_->collect_focusables(focusables);
    for (auto &w : widgets_) w->collect_focusables(focusables);
    if (focusables.empty()) return;

    int idx = -1;
    for (int i = 0; i < static_cast<int>(focusables.size()); i++) {
        if (focusables[i] == focused_widget_) { idx = i; break; }
    }

    int next;
    if (reverse) {
        next = idx <= 0 ? static_cast<int>(focusables.size()) - 1 : idx - 1;
    } else {
        next = (idx + 1) % static_cast<int>(focusables.size());
    }

    set_focused_widget(focusables[next]);
}

void Window::handle_mouse(MouseEvent const &event) {
    bool needs_redraw = false;

    if (popup_ && popup_->on_mouse) {
        if (popup_->on_mouse(event)) {
            request_redraw();
            return;
        }
        if (event.type == MouseEvent::Type::Press) {
            close_popup();
            needs_redraw = true;
        }
    }

    if (event.type == MouseEvent::Type::Press) {
        Widget *new_focus = nullptr;
        if (root_) new_focus = root_->find_focusable_at(event.position);
        if (!new_focus) {
            for (auto &w : widgets_) {
                new_focus = w->find_focusable_at(event.position);
                if (new_focus) break;
            }
        }
        set_focused_widget(new_focus);
        needs_redraw = true;
    }

    if (root_ && root_->handle_mouse(event)) {
        needs_redraw = true;
    }
    for (auto &widget : widgets_) {
        if (widget->handle_mouse(event)) {
            needs_redraw = true;
        }
    }

    if (event.type == MouseEvent::Type::Move) {
        Widget *under = nullptr;
        if (root_) under = root_->widget_at(event.position);
        if (!under) {
            for (auto &w : widgets_) {
                under = w->widget_at(event.position);
                if (under) break;
            }
        }
        hovered_widget_ = under;
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
        request_redraw();
    }
}

void Window::handle_key(KeyEvent const &event) {
    if (popup_ && popup_->on_key) {
        if (popup_->on_key(event)) {
            request_redraw();
            return;
        }
    }

    if (event.type == KeyEvent::Type::Press && event.key == Key::Tab) {
        focus_next(event.shift);
        request_redraw();
        return;
    }

    bool mnemonic_mod = event.alt || event.super || event.ctrl;
    if (event.type == KeyEvent::Type::Press && mnemonic_mod && !event.text.empty()) {
        char key = static_cast<char>(
            std::tolower(static_cast<unsigned char>(event.text[0])));
        std::vector<Widget *> targets;
        if (root_) root_->collect_mnemonics(targets);
        for (auto &w : widgets_) w->collect_mnemonics(targets);
        for (auto *w : targets) {
            if (w->trigger_mnemonic(key)) {
                request_redraw();
                return;
            }
        }
    }

    if (focused_widget_ && focused_widget_->handle_key(event)) {
        request_redraw();
    }
}

Size Window::content_min_size() const {
    if (root_) return root_->size_hint();
    return {};
}

Size Window::min_size() const {
    if (min_size_.width > 0 || min_size_.height > 0)
        return min_size_;
    return content_min_size();
}

void Window::handle_resize(Size new_size) {
    size_ = new_size;
    if (popup_)
        close_popup();
    if (root_) {
        root_->set_rect({0, 0, size_.width, size_.height});
    }
}

void Window::update_tooltip(Widget *under, Point mouse_pos) {
    if (under == tooltip_widget_) {
        if (tooltip_visible_ && under && under->tooltip() != tooltip_text_) {
            tooltip_text_ = under->tooltip();
            if (!tooltip_text_.empty())
                show_tooltip_window(tooltip_text_, tooltip_mouse_pos_);
            else
                hide_tooltip();
        }
        return;
    }

    hide_tooltip();
    tooltip_widget_ = under;

    if (under && !under->tooltip().empty()) {
        tooltip_mouse_pos_ = mouse_pos;
        tooltip_text_ = under->tooltip();
        float delay = Theme::current().tooltip.delay_sec;
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

} // namespace toolkit
