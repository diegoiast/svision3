// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Declarative UI API

#include "toolkit/button.hpp"
#include "toolkit/checkbox.hpp"
#include "toolkit/combobox.hpp"
#include "toolkit/image_widget.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/line_input.hpp"
#include "toolkit/progress_bar.hpp"
#include "toolkit/radio_button.hpp"
#include "toolkit/slider.hpp"
#include "toolkit/spin_box.hpp"
#include "toolkit/tab_widget.hpp"

#include <toolkit/html_view.hpp>
#include <toolkit/icon_grid.hpp>
#include <toolkit/list_view.hpp>
#include <toolkit/menu.hpp>
#include <toolkit/menubar.hpp>
#include <toolkit/rich_label.hpp>
#include <toolkit/scroll_area.hpp>
#include <toolkit/splitter.hpp>
#include <toolkit/status_bar.hpp>
#include <toolkit/table_view.hpp>
#include <toolkit/text_edit.hpp>
#include <toolkit/toast_widget.hpp>
#include <toolkit/toolbar.hpp>
#include <toolkit/tree_view.hpp>

namespace ui {

// Forward declarations
struct CommandElement;
struct MenuElement;

// Element: fluent builder wrapper around a unique_ptr<T>
template <typename T> struct Element {
    std::unique_ptr<T> w;
    toolkit::Command *last_cmd_ = nullptr;
    bool expand_ = false;
    Element(std::unique_ptr<T> widget) : w(std::move(widget)) {}
    Element(Element &&) = default;
    Element &operator=(Element &&) = default;
    Element(const Element &) = delete;
    Element &operator=(const Element &) = delete;

    Element text(std::string_view t) {
        w->set_text(std::string(t));
        return std::move(*this);
    }
    Element markdown(std::string_view t) {
        w->set_markdown(std::string(t));
        return std::move(*this);
    }
    Element tooltip(std::string_view t) {
        w->set_tooltip(std::string(t));
        return std::move(*this);
    }
    Element markdownToolTip(std::string_view t) {
        w->set_markdown_tooltip(std::string(t));
        return std::move(*this);
    }
    Element enabled(bool e = true) {
        w->set_enabled(e);
        return std::move(*this);
    }
    Element disable() {
        if constexpr (std::is_same_v<T, toolkit::Toolbar>) {
            if (last_cmd_) {
                last_cmd_->set_enabled(false);
            }
        } else {
            w->set_enabled(false);
        }
        return std::move(*this);
    }
    Element visible(bool v = true) {
        w->set_visible(v);
        return std::move(*this);
    }
    Element checked(bool c = true) {
        if constexpr (std::is_same_v<T, toolkit::Checkbox> || std::is_same_v<T, toolkit::Button>) {
            w->set_checked(c);
        } else {
            static_assert(std::is_same_v<T, void>, "checked only works on Checkbox or Button");
        }
        return std::move(*this);
    }
    Element checkable(bool c = true) {
        if constexpr (std::is_same_v<T, toolkit::Button>) {
            w->set_checkable(c);
        } else {
            static_assert(std::is_same_v<T, void>, "checkable only works on Button");
        }
        return std::move(*this);
    }
    Element tri_state(bool b) {
        w->set_tri_state(b);
        return std::move(*this);
    }
    Element selected(bool s) {
        static_assert(std::is_same_v<T, toolkit::RadioButton>,
                      "selected(bool) only works on RadioButton");
        w->set_selected(s);
        return std::move(*this);
    }
    Element selected(int i) {
        static_assert(std::is_same_v<T, toolkit::Combobox>, "selected(int) only works on Combobox");
        w->set_selected(i);
        return std::move(*this);
    }
    Element current(int i) {
        if constexpr (std::is_same_v<T, toolkit::TabWidget>) {
            w->set_current(i);
        } else {
            static_assert(std::is_same_v<T, void>, "current only works on TabWidget");
        }
        return std::move(*this);
    }

    Element spacing(float s) {
        w->set_spacing(s);
        return std::move(*this);
    }
    Element margins(toolkit::Margins m) {
        w->set_margins(m);
        return std::move(*this);
    }

    Element on_click(std::function<void()> f) {
        w->on_click = std::move(f);
        return std::move(*this);
    }

    Element on_toggle(std::function<void(bool)> f) {
        w->on_toggle = std::move(f);
        return std::move(*this);
    }

    Element on_link_click(std::function<void(std::string const &)> f) {
        if constexpr (std::is_same_v<T, toolkit::HtmlView>) {
            w->on_link_click = std::move(f);
        } else {
            static_assert(std::is_same_v<T, void>, "on_link_click only works on HtmlView");
        }
        return std::move(*this);
    }

    Element background_color(toolkit::Color c) {
        w->set_background_color(c);
        return std::move(*this);
    }
    Element icon(toolkit::Icon icon) {
        if constexpr (std::is_same_v<T, toolkit::Button>) {
            w->set_icon(std::move(icon));
        } else {
            static_assert(std::is_same_v<T, void>, "icon only works on Button");
        }
        return std::move(*this);
    }
    Element alignment(toolkit::Alignment a) {
        w->set_alignment(a);
        return std::move(*this);
    }
    Element padding(toolkit::Margins m) {
        w->set_padding(m);
        return std::move(*this);
    }
    Element focusable(bool f) {
        w->set_focusable(f);
        return std::move(*this);
    }
    Element flat(bool f) {
        if constexpr (std::is_same_v<T, toolkit::Button>) {
            w->set_flat(f);
        } else {
            static_assert(std::is_same_v<T, void>, "flat only works on Button");
        }
        return std::move(*this);
    }
    Element auto_repeat(bool ar, float delay = 0.5f, float interval = 0.1f) {
        if constexpr (std::is_same_v<T, toolkit::Button>) {
            w->set_auto_repeat(ar, delay, interval);
        } else {
            static_assert(std::is_same_v<T, void>, "auto_repeat only works on Button");
        }
        return std::move(*this);
    }

    Element icon_size(int size) {
        if constexpr (std::is_same_v<T, toolkit::IconGrid>) {
            w->set_icon_size(size);
        } else {
            static_assert(std::is_same_v<T, void>, "icon_size only works on IconGrid");
        }
        return std::move(*this);
    }

    Element scale_icons(bool scale) {
        if constexpr (std::is_same_v<T, toolkit::IconGrid>) {
            w->set_scale_icons(scale);
        } else {
            static_assert(std::is_same_v<T, void>, "scale_icons only works on IconGrid");
        }
        return std::move(*this);
    }

    Element on_selection_changed(std::function<void(std::set<size_t> const &)> f) {
        if constexpr (std::is_same_v<T, toolkit::IconGrid>) {
            w->on_selection_changed = std::move(f);
        } else {
            static_assert(std::is_same_v<T, void>, "on_selection_changed only works on IconGrid");
        }
        return std::move(*this);
    }

    Element on_item_activated(std::function<void(size_t)> f) {
        if constexpr (std::is_same_v<T, toolkit::IconGrid>) {
            w->on_item_activated = std::move(f);
        } else {
            static_assert(std::is_same_v<T, void>, "on_item_activated only works on IconGrid");
        }
        return std::move(*this);
    }

    // Element-specific setters
    Element value(float v) {
        w->set_value(v);
        return std::move(*this);
    }
    Element range(float min, float max) {
        w->set_range(min, max);
        return std::move(*this);
    }
    Element shrinkable(bool s) {
        w->set_shrinkable(s);
        return std::move(*this);
    }
    Element stretch(int s) {
        w->set_stretch(s);
        return std::move(*this);
    }
    Element password_mode(bool p) {
        w->set_password_mode(p);
        return std::move(*this);
    }
    Element read_only(bool r) {
        w->set_read_only(r);
        return std::move(*this);
    }

    T *operator->() const { return w.get(); }
    operator T *() const { return w.get(); }

    template <typename W> Element &add_child(Element<W> &&child, int stretch = 0) {
        if constexpr (std::is_same_v<T, toolkit::Toolbar>) {
            w->add_widget(std::move(child.w), static_cast<float>(stretch));
        } else {
            w->add_widget(std::move(child.w), stretch, toolkit::Alignment::Fill);
        }
        return *this;
    }

    template <typename W> Element add(Element<W> &&child, int stretch = 0) {
        return std::move(add_child(std::move(child), stretch));
    }

    template <typename W> Element add(Element<W> &child, int stretch = 0) {
        return std::move(add_child(std::move(child), stretch));
    }

    template <typename W> Element add(std::unique_ptr<W> child, int stretch = 0) {
        w->add_widget(std::move(child), stretch);
        return std::move(*this);
    }

    Element expand() {
        expand_ = true;
        return std::move(*this);
    }

    template <typename W> Element operator|(Element<W> &&child) {
        int stretch = child.expand_ ? 1 : 0;
        w->add_widget(std::move(child.w), stretch, toolkit::Alignment::Fill);
        return std::move(*this);
    }

    template <typename W> Element operator|(Element<W> &child) {
        int stretch = child.expand_ ? 1 : 0;
        w->add_widget(std::move(child.w), stretch, toolkit::Alignment::Fill);
        return std::move(*this);
    }

    // MenuBar special functions
    Element add_menu(MenuElement &&m);
    Element add_menu(std::string_view title, MenuElement &&m);

    // Toolbar and Button special functions
    Element command(toolkit::Command::Ptr cmd) {
        if constexpr (std::is_same_v<T, toolkit::Toolbar>) {
            last_cmd_ = cmd.get();
            w->add_command(std::move(cmd));
        } else if constexpr (std::is_same_v<T, toolkit::Button>) {
            w->set_command(std::move(cmd));
        } else {
            static_assert(std::is_same_v<T, toolkit::Toolbar> || std::is_same_v<T, toolkit::Button>,
                          "command only works on Toolbar or Button");
        }
        return std::move(*this);
    }
    Element menu(MenuElement &&m);
    Element command(std::string name, std::function<void()> action) {
        static_assert(std::is_same_v<T, toolkit::Toolbar>, "command only works on Toolbar");
        auto cmd = toolkit::Command::create(std::move(name), std::move(action));
        last_cmd_ = cmd.get();
        w->add_command(std::move(cmd));
        return std::move(*this);
    }
    Element command(std::string name, std::string tooltip = {}, std::string icon = {},
                    std::function<void()> action = {}) {
        static_assert(std::is_same_v<T, toolkit::Toolbar>, "command only works on Toolbar");
        auto cmd = toolkit::Command::create(std::move(name), std::move(action));
        last_cmd_ = cmd.get();
        if (!tooltip.empty()) {
            cmd->set_tooltip(std::move(tooltip));
        }
        if (!icon.empty()) {
            cmd->set_icon(std::move(icon));
        }
        w->add_command(std::move(cmd));
        return std::move(*this);
    }
    Element separator() {
        static_assert(std::is_same_v<T, toolkit::Toolbar>, "separator only works on Toolbar");
        w->add_separator();
        return std::move(*this);
    }

    // TabWidget special functions
    Element orientation(toolkit::TabOrientation o) {
        w->set_orientation(o);
        return std::move(*this);
    }

    template <typename W> Element leading_widget(Element<W> &&child) {
        w->set_leading_widget(std::move(child.w));
        return std::move(*this);
    }

    template <typename W> Element trailing_widget(Element<W> &&child) {
        w->set_trailing_widget(std::move(child.w));
        return std::move(*this);
    }

    Element tabs_closable(bool closable) {
        w->set_tabs_closable(closable);
        return std::move(*this);
    }

    Element min_tab_width(float width) {
        w->set_min_tab_width(width);
        return std::move(*this);
    }

    Element tabs_movable(bool movable) {
        w->set_tabs_movable(movable);
        return std::move(*this);
    }

    template <typename W>
    Element add_tab(std::string_view title, Element<W> &&child, bool closable = true) {
        w->add_tab(std::string(title), std::move(child.w), closable);
        return std::move(*this);
    }

    template <typename W> Element add_tab(std::string_view title, Element<W> &child) {
        return add_tab(title, std::move(child));
    }

    // Line edit special functions
    Element validation_mode(toolkit::LineInput::ValidationMode mode) {
        w->set_validation_mode(mode);
        return std::move(*this);
    }

    Element
    validator(std::function<bool(std::string const &, toolkit::LineInput const &input)> validator) {
        w->set_validator(validator);
        return std::move(*this);
    }

    template <typename F> Element on_change(F &&f) {
        if constexpr (std::is_same_v<T, toolkit::LineInput>) {
            w->on_change = std::forward<F>(f);
        } else if constexpr (std::is_same_v<T, toolkit::Slider>) {
            w->set_on_change([f = std::forward<F>(f)](toolkit::Slider &, float v) { f(v); });
        } else if constexpr (std::is_same_v<T, toolkit::SpinBox>) {
            w->on_change = std::forward<F>(f);
        } else if constexpr (std::is_same_v<T, toolkit::Combobox>) {
            w->on_change = std::forward<F>(f);
        } else {
            static_assert(std::is_same_v<T, void>, "Method not supported");
        }
        return std::move(*this);
    }

    Element alternate_row_colors(bool value) {
        if constexpr (std::is_same_v<T, toolkit::TableView>) {
            w->set_alternating_row_colors(value);
        } else if constexpr (std::is_same_v<T, toolkit::ListView>) {
            w->set_alternating_row_colors(value);
        } else if constexpr (std::is_same_v<T, toolkit::TreeView>) {
            w->set_alternating_row_colors(value);
        } else {
            static_assert(std::is_same_v<T, void>, "Method not supported");
        }
        return std::move(*this);
    }

    operator std::unique_ptr<toolkit::Widget>() && { return std::move(w); }
    operator std::unique_ptr<toolkit::Widget>() & { return std::move(w); }
    T *get() const { return w.get(); }
};

// CommandElement and MenuElement
struct CommandElement {
    std::shared_ptr<toolkit::Command> cmd;
    CommandElement(std::shared_ptr<toolkit::Command> c) : cmd(std::move(c)) {}
    CommandElement shortcut(std::string s) {
        cmd->set_shortcut(std::move(s));
        return std::move(*this);
    }
    CommandElement tooltip(std::string t) {
        cmd->set_tooltip(std::move(t));
        return std::move(*this);
    }
    CommandElement icon(std::string i) {
        cmd->set_icon(std::move(i));
        return std::move(*this);
    }
    operator std::shared_ptr<toolkit::Command>() const { return cmd; }
};

struct MenuElement {
    std::shared_ptr<toolkit::Menu> menu;
    MenuElement(std::shared_ptr<toolkit::Menu> m) : menu(std::move(m)) {}
    MenuElement action(CommandElement ce) {
        menu->add_action(ce.cmd);
        return std::move(*this);
    }
    MenuElement action(std::string name, std::function<void()> action) {
        menu->add_action(std::move(name), std::move(action));
        return std::move(*this);
    }
    MenuElement separator() {
        menu->add_separator();
        return std::move(*this);
    }
    MenuElement submenu(std::string name, MenuElement me) {
        menu->add_submenu(std::move(name), me.menu);
        return std::move(*this);
    }
    operator std::shared_ptr<toolkit::Menu>() const { return menu; }
};

template <typename T> Element<T> Element<T>::add_menu(MenuElement &&m) {
    static_assert(std::is_same_v<T, toolkit::MenuBar>, "add_menu only works on MenuBar");
    w->add_menu(m.menu);
    return std::move(*this);
}

template <typename T> Element<T> Element<T>::add_menu(std::string_view title, MenuElement &&m) {
    static_assert(std::is_same_v<T, toolkit::MenuBar>, "add_menu only works on MenuBar");
    w->add_menu(std::string(title), m.menu);
    return std::move(*this);
}

template <typename T> Element<T> Element<T>::menu(MenuElement &&m) {
    if constexpr (std::is_same_v<T, toolkit::Button>) {
        w->set_menu(m.menu);
    } else {
        static_assert(std::is_same_v<T, void>, "menu only works on Button");
    }
    return std::move(*this);
}

// Factory functions
inline CommandElement command(std::string name, std::function<void()> action = {}) {
    return CommandElement(toolkit::Command::create(std::move(name), std::move(action)));
}

inline MenuElement menu(std::string_view title = "") {
    return MenuElement(std::make_shared<toolkit::Menu>(std::string(title)));
}

inline Element<toolkit::MenuBar> menubar() {
    return Element<toolkit::MenuBar>(std::make_unique<toolkit::MenuBar>());
}

inline Element<toolkit::Label> label(std::string_view text = "") {
    return Element<toolkit::Label>(std::make_unique<toolkit::Label>(std::string(text)));
}

inline Element<toolkit::Button> button(std::string_view text = "") {
    return Element<toolkit::Button>(std::make_unique<toolkit::Button>(std::string(text)));
}

inline Element<toolkit::Checkbox> checkbox(std::string_view text = "") {
    return Element<toolkit::Checkbox>(std::make_unique<toolkit::Checkbox>(std::string(text)));
}

inline Element<toolkit::LineInput> line_input(std::string_view placeholder = "") {
    return Element<toolkit::LineInput>(
        std::make_unique<toolkit::LineInput>(std::string(placeholder)));
}

inline Element<toolkit::ListView> list_view(std::shared_ptr<toolkit::ItemModel> model) {
    return Element<toolkit::ListView>(std::make_unique<toolkit::ListView>(model));
}

inline Element<toolkit::TableView> table_view(std::shared_ptr<toolkit::ItemModel> model) {
    return Element<toolkit::TableView>(std::make_unique<toolkit::TableView>(model));
}

inline Element<toolkit::TreeView> tree_view(std::shared_ptr<toolkit::TreeModel> model) {
    return Element<toolkit::TreeView>(std::make_unique<toolkit::TreeView>(model));
}

inline Element<toolkit::Slider> slider(float value = 0, float min = 0, float max = 100) {
    auto s = std::make_unique<toolkit::Slider>();
    s->set_range(min, max);
    s->set_value(value);
    return Element<toolkit::Slider>(std::move(s));
}

inline Element<toolkit::ProgressBar> progress_bar(float value = 0) {
    auto p = std::make_unique<toolkit::ProgressBar>();
    p->set_value(value);
    return Element<toolkit::ProgressBar>(std::move(p));
}

inline Element<toolkit::SpinBox> spin_box(int value = 0, int min = 0, int max = 100, int step = 1) {
    return Element<toolkit::SpinBox>(std::make_unique<toolkit::SpinBox>(value, min, max, step));
}

inline Element<toolkit::Combobox> combobox(std::vector<std::string> items) {
    return Element<toolkit::Combobox>(std::make_unique<toolkit::Combobox>(std::move(items)));
}

inline Element<toolkit::IconGrid> icon_grid(std::shared_ptr<toolkit::ItemModel> model) {
    return Element<toolkit::IconGrid>(std::make_unique<toolkit::IconGrid>(model));
}

inline Element<toolkit::ImageWidget> image_widget() {
    return Element<toolkit::ImageWidget>(std::make_unique<toolkit::ImageWidget>());
}

inline Element<toolkit::Label> spacer() {
    return Element<toolkit::Label>(std::make_unique<toolkit::Label>(""));
}

// Radio group for radio buttons
struct RadioGroupWrapper {
    std::shared_ptr<toolkit::RadioGroup> group;
    RadioGroupWrapper() : group(std::make_shared<toolkit::RadioGroup>()) {}
    RadioGroupWrapper &on_change(std::function<void(int index)> f) {
        group->on_change = std::move(f);
        return *this;
    }
    operator toolkit::RadioGroup &() const { return *group; }
    operator std::shared_ptr<toolkit::RadioGroup>() const { return group; }
};

inline RadioGroupWrapper radio_group() { return RadioGroupWrapper(); }

inline Element<toolkit::RadioButton> radio_button(std::string_view text, RadioGroupWrapper &group) {
    return Element<toolkit::RadioButton>(
        std::make_unique<toolkit::RadioButton>(std::string(text), *group.group));
}

inline Element<toolkit::RadioButton> radio_button(std::string_view text,
                                                  std::shared_ptr<toolkit::RadioGroup> group) {
    return Element<toolkit::RadioButton>(
        std::make_unique<toolkit::RadioButton>(std::string(text), *group));
}

inline Element<toolkit::RadioButton> radio_button(std::string_view text,
                                                  toolkit::RadioGroup &group) {
    return Element<toolkit::RadioButton>(
        std::make_unique<toolkit::RadioButton>(std::string(text), group));
}

inline Element<toolkit::TextEdit> text_edit(std::string text = {}) {
    return Element<toolkit::TextEdit>(std::make_unique<toolkit::TextEdit>(text));
}

// Tab widget - special handling for add_tab
inline Element<toolkit::TabWidget> tab_widget() {
    return Element<toolkit::TabWidget>(std::make_unique<toolkit::TabWidget>());
}

// Layouts
inline toolkit::Margins default_margins() { return {10, 10, 10, 10}; }
inline toolkit::Margins no_margins() { return {0, 0, 0, 0}; }

inline int default_padding() { return 10; }

constexpr int expand = 1;
constexpr int no_stretch = 0;
constexpr int no_spacing = 0;

inline Element<toolkit::HtmlView> html_view(std::string_view html = "") {
    auto v = std::make_unique<toolkit::HtmlView>();
    if (!html.empty()) {
        v->set_html(std::string(html));
    }
    return Element<toolkit::HtmlView>(std::move(v));
}

inline Element<toolkit::RichLabel> rich_label(std::string_view text = "") {
    auto v = std::make_unique<toolkit::RichLabel>();
    if (!text.empty()) {
        v->set_text(std::string(text));
    }
    return Element<toolkit::RichLabel>(std::move(v));
}

inline Element<toolkit::RichLabel> rich_label_md(std::string_view markdown) {
    auto v = std::make_unique<toolkit::RichLabel>();
    v->set_markdown(std::string(markdown));
    return Element<toolkit::RichLabel>(std::move(v));
}

template <typename W> inline Element<toolkit::ScrollArea> scroll_area(Element<W> content) {
    auto sa = std::make_unique<toolkit::ScrollArea>();
    sa->set_content(std::move(content.w));
    return Element<toolkit::ScrollArea>(std::move(sa));
}

template <typename A, typename B>
inline Element<toolkit::Splitter> hsplit(Element<A> first, Element<B> second, float ratio = 0.5f) {
    auto sp = std::make_unique<toolkit::Splitter>(toolkit::Orientation::Horizontal);
    sp->set_first(std::move(first.w));
    sp->set_second(std::move(second.w));
    sp->set_ratio(ratio);
    return Element<toolkit::Splitter>(std::move(sp));
}

template <typename A, typename B>
inline Element<toolkit::Splitter> vsplit(Element<A> first, Element<B> second, float ratio = 0.5f) {
    auto sp = std::make_unique<toolkit::Splitter>(toolkit::Orientation::Vertical);
    sp->set_first(std::move(first.w));
    sp->set_second(std::move(second.w));
    sp->set_ratio(ratio);
    return Element<toolkit::Splitter>(std::move(sp));
}

inline Element<toolkit::VBoxLayout> vbox() {
    auto layout = std::make_unique<toolkit::VBoxLayout>();
    layout->set_spacing(default_padding());
    layout->set_margins(default_margins());
    return Element<toolkit::VBoxLayout>(std::move(layout));
}

inline Element<toolkit::HBoxLayout> hbox() {
    auto layout = std::make_unique<toolkit::HBoxLayout>();
    layout->set_spacing(default_padding());
    layout->set_margins(default_margins());
    return Element<toolkit::HBoxLayout>(std::move(layout));
}

inline Element<toolkit::Toolbar> toolbar() {
    return Element<toolkit::Toolbar>(std::make_unique<toolkit::Toolbar>());
}

inline toolkit::ToastBuilder toast() { return {}; }

inline Element<toolkit::StatusBar> status_bar() {
    return Element<toolkit::StatusBar>(std::make_unique<toolkit::StatusBar>());
}

} // namespace ui
