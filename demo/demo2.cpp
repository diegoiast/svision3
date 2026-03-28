// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Declarative UI API demo - exploring what the API could look like.

#include "toolkit/application.hpp"
#include "toolkit/button.hpp"
#include "toolkit/checkbox.hpp"
#include "toolkit/combobox.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/line_input.hpp"
#include "toolkit/progress_bar.hpp"
#include "toolkit/radio_button.hpp"
#include "toolkit/slider.hpp"
#include "toolkit/spin_box.hpp"
#include "toolkit/tab_widget.hpp"
#include "toolkit/window.hpp"

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include <spdlog/fmt/fmt.h>

namespace ui {

// Widget wrapper with fluent API
template <typename T> struct Widget {
    std::unique_ptr<T> w;

    Widget(std::unique_ptr<T> widget) : w(std::move(widget)) {}

    Widget(Widget &&) = default;
    Widget &operator=(Widget &&) = default;
    Widget(const Widget &) = delete;
    Widget &operator=(const Widget &) = delete;

    Widget text(std::string_view t) {
        w->set_text(std::string(t));
        return std::move(*this);
    }
    Widget tooltip(std::string_view t) {
        w->set_tooltip(std::string(t));
        return std::move(*this);
    }
    Widget enabled(bool e = true) {
        w->set_enabled(e);
        return std::move(*this);
    }
    Widget visible(bool v = true) {
        w->set_visible(v);
        return std::move(*this);
    }
    Widget checked(bool c = true) {
        w->set_checked(c);
        return std::move(*this);
    }
    Widget selected(int i) {
        w->set_selected(i);
        return std::move(*this);
    }
    Widget spacing(float s) {
        w->set_spacing(s);
        return std::move(*this);
    }
    Widget margins(toolkit::Margins m) {
        w->set_margins(m);
        return std::move(*this);
    }

    // Widget-specific setters
    Widget value(float v) {
        w->set_value(v);
        return std::move(*this);
    }
    Widget range(float min, float max) {
        w->set_range(min, max);
        return std::move(*this);
    }
    Widget shrinkable(bool s) {
        w->set_shrinkable(s);
        return std::move(*this);
    }
    Widget stretch(int s) {
        w->set_stretch(s);
        return std::move(*this);
    }
    Widget password_mode(bool p) {
        w->set_password_mode(p);
        return std::move(*this);
    }
    Widget read_only(bool r) {
        w->set_read_only(r);
        return std::move(*this);
    }

    template <typename W> Widget add(Widget<W> child, int stretch = 0) {
        w->add_widget(std::move(child.w), stretch);
        return std::move(*this);
    }

    template <typename W> Widget add(std::unique_ptr<W> child, int stretch = 0) {
        w->add_widget(std::move(child), stretch);
        return std::move(*this);
    }

    // Special handling for TabWidget
    template <typename W> Widget add_tab(std::string_view title, Widget<W> child) {
        if constexpr (std::is_same_v<T, toolkit::TabWidget>) {
            w->add_tab(std::string(title), std::move(child.w));
        } else {
            static_assert(sizeof(T) == 0, "add_tab only works on TabWidget");
        }
        return std::move(*this);
    }

    Widget on_click(std::function<void()> f) {
        w->on_click = std::move(f);
        return std::move(*this);
    }
    Widget on_toggle(std::function<void(bool)> f) {
        w->on_toggle = std::move(f);
        return std::move(*this);
    }
    Widget on_change(std::function<void(std::string const &)> f) {
        w->on_change = std::move(f);
        return std::move(*this);
    }
    Widget on_change(std::function<void(int)> f) {
        w->on_change = std::move(f);
        return std::move(*this);
    }
    Widget on_change(std::function<void(float)> f) {
        w->set_on_change([f = std::move(f)](T &, float v) { f(v); });
        return std::move(*this);
    }

    operator std::unique_ptr<toolkit::Widget>() && { return std::move(w); }
    T *get() const { return w.get(); }
};

// Factory functions
inline Widget<toolkit::Label> label(std::string_view text = "") {
    return Widget<toolkit::Label>(std::make_unique<toolkit::Label>(std::string(text)));
}

inline Widget<toolkit::Button> button(std::string_view text = "") {
    return Widget<toolkit::Button>(std::make_unique<toolkit::Button>(std::string(text)));
}

inline Widget<toolkit::Checkbox> checkbox(std::string_view text = "") {
    return Widget<toolkit::Checkbox>(std::make_unique<toolkit::Checkbox>(std::string(text)));
}

inline Widget<toolkit::LineInput> line_input(std::string_view placeholder = "") {
    return Widget<toolkit::LineInput>(
        std::make_unique<toolkit::LineInput>(std::string(placeholder)));
}

inline Widget<toolkit::Slider> slider(float value = 0, float min = 0, float max = 100) {
    auto s = std::make_unique<toolkit::Slider>();
    s->set_range(min, max);
    s->set_value(value);
    return Widget<toolkit::Slider>(std::move(s));
}

inline Widget<toolkit::ProgressBar> progress_bar(float value = 0) {
    auto p = std::make_unique<toolkit::ProgressBar>();
    p->set_value(value);
    return Widget<toolkit::ProgressBar>(std::move(p));
}

inline Widget<toolkit::SpinBox> spin_box(int value = 0, int min = 0, int max = 100, int step = 1) {
    return Widget<toolkit::SpinBox>(std::make_unique<toolkit::SpinBox>(value, min, max, step));
}

inline Widget<toolkit::Combobox> combobox(std::vector<std::string> items) {
    return Widget<toolkit::Combobox>(std::make_unique<toolkit::Combobox>(std::move(items)));
}

inline Widget<toolkit::Label> spacer() {
    return Widget<toolkit::Label>(std::make_unique<toolkit::Label>(""));
}

// Radio group for radio buttons
inline std::shared_ptr<toolkit::RadioGroup> radio_group() {
    return std::make_shared<toolkit::RadioGroup>();
}

inline Widget<toolkit::RadioButton> radio_button(std::string_view text,
                                                 toolkit::RadioGroup &group) {
    return Widget<toolkit::RadioButton>(
        std::make_unique<toolkit::RadioButton>(std::string(text), group));
}

// Tab widget - special handling for add_tab
inline Widget<toolkit::TabWidget> tab_widget() {
    return Widget<toolkit::TabWidget>(std::make_unique<toolkit::TabWidget>());
}

// Layouts
inline toolkit::Margins default_margins() { return {10, 10, 10, 10}; }

inline int default_padding() { return 10; }

inline Widget<toolkit::VBoxLayout> vbox() {
    auto layout = std::make_unique<toolkit::VBoxLayout>();
    layout->set_spacing(default_padding());
    layout->set_margins(default_margins());
    return Widget<toolkit::VBoxLayout>(std::move(layout));
}

inline Widget<toolkit::HBoxLayout> hbox() {
    auto layout = std::make_unique<toolkit::HBoxLayout>();
    layout->set_spacing(default_padding());
    layout->set_margins(default_margins());

    return Widget<toolkit::HBoxLayout>(std::move(layout));
}

} // namespace ui

int main(int argc, char *argv[]) {
    toolkit::Application app;
    auto *window = app.create_window("Declarative Demo", {600, 500});

    auto volume_label = std::make_unique<toolkit::Label>("50%");
    auto *volume_label_ptr = volume_label.get();

    auto volume_slider = std::make_unique<toolkit::Slider>();
    volume_slider->set_range(0, 100);
    volume_slider->set_value(50);
    volume_slider->set_on_change([volume_label_ptr](toolkit::Slider &, float v) {
        volume_label_ptr->set_text(fmt::format("{:.0f}%", v));
    });

    auto group = ui::radio_group();
    ui::radio_button("Option A", *group);
    ui::radio_button("Option B", *group);
    group->select(nullptr);

    auto name_input = std::make_unique<toolkit::LineInput>("Type here...");
    auto password_input = std::make_unique<toolkit::LineInput>("Password");
    password_input->set_password_mode(true);
    auto email_input = std::make_unique<toolkit::LineInput>("email@example.com");

    auto inputs_tab =
        ui::vbox()
            .add(ui::label("Form Inputs Demo").shrinkable(true))
            .add(ui::checkbox("Enable feature").checked(true).tooltip("Toggle something"))
            .add(ui::hbox().add(ui::label("Name:")).add(std::move(name_input)))
            .add(ui::hbox().add(ui::label("Password:")).add(std::move(password_input)))
            .add(ui::hbox().add(ui::label("Email:")).add(std::move(email_input)))
            .add(ui::label(""))
            .add(ui::hbox()
                     .add(ui::spin_box(5, 0, 100, 5))
                     .add(ui::combobox({"Option 1", "Option 2", "Option 3"}).selected(0)));

    auto sliders_tab = ui::vbox()
                           .add(ui::label("Sliders Demo").shrinkable(true))
                           .add(ui::label("Volume:"))
                           .add(std::move(volume_slider))
                           .add(std::move(volume_label))
                           .add(ui::progress_bar(0.5f))
                           .add(ui::label(""))
                           .add(ui::label("Radio Buttons:"))
                           .add(ui::hbox()
                                    .add(ui::radio_button("Option A", *group))
                                    .add(ui::radio_button("Option B", *group)));

    auto buttons_tab =
        ui::vbox()
            .add(ui::label("Buttons Demo").shrinkable(true))
            .add(ui::button("Regular Button").on_click([window] { window->close(); }))
            .add(ui::button("Disabled Button").enabled(false))
            .add(ui::button("Tooltip Button").tooltip("This is a tooltip"))
            .add(ui::checkbox("Checkbox in tab"))
            .add(ui::checkbox("Another checkbox").checked(true));

    auto actions_tab = ui::vbox()
                           .add(ui::label("Actions Demo").shrinkable(true))
                           .add(ui::label(""))
                           .add(ui::hbox()
                                    .add(ui::button("OK").on_click([window] { window->close(); }))
                                    .add(ui::button("Cancel").enabled(false))
                                    .add(ui::button("Apply")))
                           .add(ui::label(""));

    auto tw = ui::tab_widget();
    tw.w->add_tab("Inputs", std::move(inputs_tab.w));
    tw.w->add_tab("Sliders", std::move(sliders_tab.w));
    tw.w->add_tab("Buttons", std::move(buttons_tab.w));
    tw.w->add_tab("Actions", std::move(actions_tab.w));

    window->set_root(ui::vbox()
                         .margins({0,0,0,0})
                         .add(std::move(tw))
                         .add(ui::spacer(), 1)
                         .add(ui::hbox()
                                  .add(ui::button("&Quit").on_click([window] { window->close(); }))
                                  .add(ui::button("About").enabled(false))));

    window->show();
    return app.run();
}
