// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <svision3/button.hpp>
#include <svision3/image_widget.hpp>
#include <svision3/widget.hpp>

namespace svision3 {

class Button;
class HBoxLayout;
class Label;
class Window;

class TitlebarButton : public Button {
  public:
    TitlebarButton(DecorationButton type, std::string tooltip, Size size_hint = {20, 20});
    void paint(Painter &painter) override;
    Size size_hint() const override { return custom_size_hint; }

    // The maximize/restore button is the one titlebar button whose glyph must change with window
    // state (see WindowTitleBar::sync_button_states()) -- everything else keeps the icon it was
    // constructed with for its whole lifetime.
    void set_type(DecorationButton type) { type_ = type; }

  private:
    DecorationButton type_;
    Size custom_size_hint;
};

class TitleBarIcon : public Button {
  public:
    TitleBarIcon(Window *w);
    bool handle_mouse(MouseEvent const &event) override;
    Size size_hint() const override { return custom_size_hint_; }
    void paint(Painter &painter) override;

    // Named set_image() so the themes' existing icon_widget->set_image(...) calls keep working.
    void set_image(Icon const &icon);

  private:
    Window *window_;
    Icon icon_image_;
    Size custom_size_hint_{16, 16};
};

class WindowTitleBar : public Widget {
  public:
    WindowTitleBar(Window *window);

    void paint(Painter &painter) override;
    void set_rect(Rect const &rect) override;
    bool handle_mouse(MouseEvent const &event) override;
    // Without this, hover resolution stops at the title bar and never reaches the buttons inside
    // its layout, so Window::hovered_widget_ is the bar itself -- which has no tooltip. That is
    // why the minimise/maximise/close buttons never showed one despite setting it.
    Widget *widget_at(Point p) override;
    Size size_hint() const override;

    // consusmers of this class should derive *this* method
    virtual void initializeTitleBar();
    void set_icon(Icon const &icon);

  protected:
    // Every theme override's initializeTitleBar() starts by building its own `layout`. Route that
    // through here instead of a bare `new HBoxLayout()` so the parent link is set immediately at
    // construction -- the same moment Layout::add_widget() parents its own children -- rather than
    // leaving `layout` parentless until something else patches it up later.
    auto create_title_layout() -> HBoxLayout *;
    auto create_btn(DecorationButton type) -> Button *;
    // Re-reads Window::is_minimizable/is_maximizable/is_closable and applies them to whichever
    // of these buttons exist, plus the Maximize/Restore tooltip swap. Call at the top of paint()
    // so a runtime change (e.g. WindowOptions changing) is reflected without redoing layout.
    void sync_button_states();
    TitleBarIcon *icon_widget = nullptr;
    Label *title_label;
    Button *min_btn = nullptr;
    Button *max_btn = nullptr;
    Button *close_btn = nullptr;
    HBoxLayout *layout;

  private:
    // Restores the window and, where the platform allows a client to place its own windows,
    // moves it under the pointer so the drag that follows feels continuous.
    void pull_loose_from_maximized(MouseEvent const &event);

    // A press on a maximized window cannot start a system move straight away: the window has to
    // be restored first, and only a real drag should do that -- a plain click on the bar must
    // leave the window maximized. So the press is remembered here until the pointer has actually
    // travelled far enough (see handle_mouse()).
    bool pending_move = false;
    Point press_position{};
    uint32_t press_serial = 0;
};

} // namespace svision3
