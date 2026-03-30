// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Declarative UI API 

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

#include <toolkit/list_view.hpp>
#include <toolkit/table_view.hpp>
#include <toolkit/text_edit.hpp>

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
        if constexpr (std::is_same_v<T, toolkit::Checkbox> ||
                      std::is_same_v<T, toolkit::RadioButton>) {
            w->set_checked(c);
        } else {
            static_assert(sizeof(T) == 0, "checked only works on Checkbox or RadioButton");
        }
        return std::move(*this);
    }
    Widget tri_state(bool b) {
        if constexpr (std::is_same_v<T, toolkit::Checkbox>) {
            w->set_tri_state(b);
        } else {
            static_assert(sizeof(T) == 0, "checked only works on Checkbox");
        }
        return std::move(*this);
    }
    Widget selected(int i) {
        if constexpr (std::is_same_v<T, toolkit::Combobox>) {
            w->set_selected(i);
        } else {
            static_assert(sizeof(T) == 0, "selected only works on Combobox");
        }
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
    
    T *operator->() const { return w.get(); }
    operator T *() const { return w.get(); }
    
    template <typename W> Widget &add_child(Widget<W> &&child, int stretch = 0) {
        w->add_widget(std::move(child.w), stretch);
        return *this;
    }
    
    template <typename W> Widget add(Widget<W> &&child, int stretch = 0) {
        return std::move(add_child(std::move(child), stretch));
    }
    
    template <typename W> Widget add(Widget<W> &child, int stretch = 0) {
        return std::move(add_child(std::move(child), stretch));
    }
    
    template <typename W> Widget add(std::unique_ptr<W> child, int stretch = 0) {
        w->add_widget(std::move(child), stretch);
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

    // TabWidget special functions
    template <typename W> Widget add_tab(std::string_view title, Widget<W> &&child) {
        if constexpr (std::is_same_v<T, toolkit::TabWidget>) {
            w->add_tab(std::string(title), std::move(child.w));
        } else {
            static_assert(sizeof(T) == 0, "add_tab only works on TabWidget");
        }
        return std::move(*this);
    }

    template <typename W> Widget add_tab(std::string_view title, Widget<W> &child) {
        return add_tab(title, std::move(child));
    }

    // Line edit special functions
    Widget validation_mode(toolkit::LineInput::ValidationMode mode) {
        w->set_validation_mode(mode);
        return std::move(*this);
    }

    Widget
    validator(std::function<bool(std::string const &, toolkit::LineInput const &input)> validator) {
        w->set_validator(validator);
        return std::move(*this);
    }

    template <typename F> Widget on_change(F &&f) {
        if constexpr (std::is_same_v<T, toolkit::LineInput>) {
            w->on_change = std::forward<F>(f);
        } else if constexpr (std::is_same_v<T, toolkit::Slider>) {
            w->set_on_change([f = std::forward<F>(f)](toolkit::Slider &, float v) { f(v); });
        } else if constexpr (std::is_same_v<T, toolkit::SpinBox>) {
            w->on_change = std::forward<F>(f);
        } else if constexpr (std::is_same_v<T, toolkit::Combobox>) {
            w->on_change = std::forward<F>(f);
        }
        return std::move(*this);
    }

    Widget alternate_row_colors(bool value) {
        if constexpr (std::is_same_v<T, toolkit::TableView>) {
            w->set_alternating_row_colors(value);
        } else if constexpr (std::is_same_v<T, toolkit::ListView>) {
            w->set_alternating_row_colors(value);
        }
        return std::move(*this);
    }

    operator std::unique_ptr<toolkit::Widget>() && { return std::move(w); }
    operator std::unique_ptr<toolkit::Widget>() & { return std::move(w); }
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

inline Widget<toolkit::ListView> list_view(std::shared_ptr<toolkit::ListAdapter> model) {
    return Widget<toolkit::ListView>(std::make_unique<toolkit::ListView>(model));
}

inline Widget<toolkit::TableView> table_view(std::shared_ptr<toolkit::TableModel> model) {
    return Widget<toolkit::TableView>(std::make_unique<toolkit::TableView>(model));
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
                                                 std::shared_ptr<toolkit::RadioGroup> group) {
    return Widget<toolkit::RadioButton>(
        std::make_unique<toolkit::RadioButton>(std::string(text), *group));
}

inline Widget<toolkit::RadioButton> radio_button(std::string_view text,
                                                 toolkit::RadioGroup &group) {
    return Widget<toolkit::RadioButton>(
        std::make_unique<toolkit::RadioButton>(std::string(text), group));
}

inline Widget<toolkit::TextEdit> text_edit(std::string text = {}) {
    return Widget<toolkit::TextEdit>(std::make_unique<toolkit::TextEdit>(text));
}

// Tab widget - special handling for add_tab
inline Widget<toolkit::TabWidget> tab_widget() {
    return Widget<toolkit::TabWidget>(std::make_unique<toolkit::TabWidget>());
}

// Layouts
inline toolkit::Margins default_margins() { return {10, 10, 10, 10}; }
inline toolkit::Margins no_margins() { return {0, 0, 0, 0}; }

inline int default_padding() { return 10; }

constexpr int expand = 1;
constexpr int no_stretch = 0;

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

