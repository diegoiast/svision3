// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/command.hpp"
#include "toolkit/events.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/types.hpp"
#include <nlohmann/json_fwd.hpp>
#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#define DECLARE_WIDGET(ClassName)                                                                  \
    std::string_view class_name() const override { return #ClassName; }

namespace toolkit {

class Window;

template <typename Derived> struct Fluent {
    Derived &self() { return static_cast<Derived &>(*this); }
};

class Widget {
  public:
    virtual ~Widget() = default;

    // This is a global flag, which will force all widgets to draw a red
    // dashed-border around the perimiters and dotted red border around
    // layouts.
    static bool debug_show_frames;

    void add_command(Command::Ptr cmd) { commands_.push_back(std::move(cmd)); }
    void remove_command(Command::Ptr const &cmd) {
        auto it = std::find(commands_.begin(), commands_.end(), cmd);
        if (it != commands_.end()) {
            commands_.erase(it);
        }
    }
    std::vector<Command::Ptr> const &commands() const { return commands_; }

    void draw(Painter &painter);
    virtual void paint(Painter &painter) = 0;
    virtual bool handle_mouse(MouseEvent const &event) = 0;
    virtual bool handle_key(KeyEvent const &event) { return false; }
    virtual Size size_hint() const { return {0, 0}; }

    virtual void set_rect(Rect const &rect) {
        if (rect_ == rect) {
            return;
        }
        rect_ = rect;
        state.layout_dirty = true;
    }
    Rect const &rect() const { return rect_; }

    // FIXME: is this needed? rect can do this already
    bool hit_test(Point p) const {
        return p.x >= 0 && p.x <= rect_.width && p.y >= 0 && p.y <= rect_.height;
    }

    // Re-layouts widgets, and parent. Then redraws main window.
    void invalidate_layout();

    virtual Widget &set_layout_dirty(bool dirty);
    virtual bool is_layout_dirty() const { return state.layout_dirty; }
    virtual Widget &set_focusable(bool f);
    virtual bool is_focusable() const { return state.focusable; }
    virtual Widget &set_focused(bool focused);
    virtual bool is_focused() const { return state.focused; }
    virtual Widget &set_enabled(bool e);
    virtual bool is_enabled() const { return state.enabled; }
    virtual Widget &set_visible(bool v);
    virtual bool is_visible() const { return state.visible; }

    // FIXME: really? is this a good API?
    bool can_get_non_focus_input() const { return state.non_focus_input; }

    virtual void on_focus() {}
    virtual void on_blur() {}
    virtual void on_theme_changed();

    bool is_effectively_visible() const;
    Widget &show() {
        set_visible(true);
        return *this;
    }
    Widget &hide() {
        set_visible(false);
        return *this;
    }

    void set_min_size(Size s) { min_size_ = s; }
    void set_max_size(Size s) { max_size_ = s; }
    Size min_size() const { return min_size_; }
    Size max_size() const { return max_size_; }

    virtual Widget *find_focusable_at(Point p) {
        if (is_focusable() && state.enabled && state.visible && hit_test(p)) {
            return this;
        }
        return nullptr;
    }

    virtual void collect_focusables(std::vector<Widget *> &out) {
        if (is_focusable() && is_enabled() && is_visible()) {
            out.push_back(this);
        }
    }

    virtual CursorShape cursor() const { return CursorShape::Arrow; }

    virtual Widget *widget_at(Point p) {
        if (state.visible && hit_test(p)) {
            return this;
        }
        return nullptr;
    }

    virtual bool trigger_mnemonic(char /*key*/) { return false; }
    virtual void collect_mnemonics(std::vector<Widget *> &out) { (void)out; }
    virtual void for_each_child(std::function<void(Widget *)> const &callback) { (void)callback; }

    static bool dispatch_mouse_event(Widget *w, MouseEvent const &event);

    virtual void set_window(Window *w) { window_ = w; }
    Window *window() const { return window_; }

    // FIXME: this calls the platform->rasterizer. I wonder how can we improve
    // FIXME: in practice Platform and Painter should have the same rasterizer
    //        - but in practice this may differ. We should choose the rasterizer
    //        from the painter.
    Size measure_text(std::string_view text, float font_size,
                      FontFamily font = FontFamily::System) const;
    Painter::FontMetrics font_metrics(float font_size, FontFamily font = FontFamily::System) const;

    void set_parent(Widget *p) { parent_ = p; }
    Widget *parent() const { return parent_; }

    auto map_to_window(Point p) const -> Point;
    auto map_from_window(Point p) const -> Point;

    void set_tooltip(std::string text);
    virtual std::string const &tooltip() const { return state.tooltip; }

    void set_markdown_tooltip(std::string markdown);
    bool tooltip_is_markdown() const { return state.tooltip_markdown; }

    Widget &set_background_color(std::optional<Color> c);
    std::optional<Color> background_color() const { return background_color_; }

    virtual nlohmann::json to_json() const;
    virtual std::string_view class_name() const { return "Widget"; }

  protected:
    // Let window call this protected method
    friend class Window;

    bool handle_key_impl(KeyEvent const &event);

    struct {
        bool layout_dirty = true;
        bool focused = false;
        bool enabled = true;
        bool visible = true;
        bool focusable = false;
        bool non_focus_input = false;
        std::string tooltip;
        bool tooltip_markdown = false;
    } state;

    Rect rect_;
    Size min_size_;
    Size max_size_;
    Window *window_ = nullptr;
    Widget *parent_ = nullptr;
    std::vector<Command::Ptr> commands_;
    std::optional<Color> background_color_;
};

} // namespace toolkit
