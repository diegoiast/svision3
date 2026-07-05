// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <toolkit/button.hpp>
#include <toolkit/image_widget.hpp>
#include <toolkit/widget.hpp>

namespace toolkit {

class Button;
class HBoxLayout;
class Label;
class Window;

// FIXME: we need to update window title bar and maximize button tooltip

/*
        if (window_->is_maximized()) {
            max_btn->set_tooltip("Restore");
        } else {
            max_btn->set_tooltip("Maximize");
        }

        // FIXME: update window label only when the window title changed
        title_label->set_text(std::string(window_->title()));
        // FIXME: update color on blur/active
        title_label->set_color(fg);
*/

class TitlebarButton : public Button {
  public:
    TitlebarButton(DecorationButton type, std::string tooltip, Size size_hint = {20, 20});
    void paint(Painter &painter) override;
    Size size_hint() const override { return custom_size_hint; }

  private:
    DecorationButton type_;
    Size custom_size_hint;
};

class TitleBarIcon : public ImageWidget {
  public:
    TitleBarIcon(Window *w);
    bool handle_mouse(MouseEvent const &event) override;

  private:
    Window *window_;
};

class WindowTitleBar : public Widget {
  public:
    WindowTitleBar(Window *window);

    void paint(Painter &painter) override;
    void set_rect(Rect const &rect) override;
    bool handle_mouse(MouseEvent const &event) override;
    /*    void for_each_child(std::function<void(Widget *)> const &callback) {
            layout->for_each_child(callback);
        }
    */
    Size size_hint() const override;

    // consusmers of this class should derive *this* method
    virtual void initializeTitleBar();
    void set_icon(Icon const &icon);

  protected:
    auto create_btn(DecorationButton type) -> Button *;
    // Re-reads Window::is_minimizable/is_maximizable/is_closable and applies them to whichever
    // of these buttons exist, plus the Maximize/Restore tooltip swap. Call at the top of paint()
    // so a runtime change (e.g. WindowOptions changing) is reflected without redoing layout.
    void sync_button_states();
    ImageWidget *icon_widget = nullptr;
    Label *title_label;
    Button *min_btn = nullptr;
    Button *max_btn = nullptr;
    Button *close_btn = nullptr;
    HBoxLayout *layout;
};

} // namespace toolkit
