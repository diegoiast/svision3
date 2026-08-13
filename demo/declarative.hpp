// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// Declarative UI API

#include <svision3/button.hpp>
#include <svision3/checkbox.hpp>
#include <svision3/combobox.hpp>
#include <svision3/dock_area.hpp>
#include <svision3/file_browser_widget.hpp>
#include <svision3/html_view.hpp>
#include <svision3/icon_grid.hpp>
#include <svision3/image_widget.hpp>
#include <svision3/label.hpp>
#include <svision3/layout.hpp>
#include <svision3/line_input.hpp>
#include <svision3/list_view.hpp>
#include <svision3/menu.hpp>
#include <svision3/menubar.hpp>
#include <svision3/progress_bar.hpp>
#include <svision3/radio_button.hpp>
#include <svision3/rich_label.hpp>
#include <svision3/scroll_area.hpp>
#include <svision3/slider.hpp>
#include <svision3/spin_box.hpp>
#include <svision3/splitter.hpp>
#include <svision3/status_bar.hpp>
#include <svision3/tab_widget.hpp>
#include <svision3/table_view.hpp>
#include <svision3/text_edit.hpp>
#include <svision3/toast_widget.hpp>
#include <svision3/toolbar.hpp>
#include <svision3/tree_view.hpp>

namespace ui {

// Forward declarations
struct CommandElement;
struct MenuElement;

// Element: fluent builder wrapper around a shared_ptr<T>. Shared, because that's
// what every toolkit container now takes ownership as -- see svision3/layout.hpp.
template <typename T> struct Element {
    std::shared_ptr<T> w;
    svision3::Command *last_cmd_ = nullptr;
    bool expand_ = false;
    Element(std::shared_ptr<T> widget) : w(std::move(widget)) {}
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
        if constexpr (std::is_same_v<T, svision3::Toolbar>) {
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
        if constexpr (std::is_same_v<T, svision3::Checkbox> || std::is_same_v<T, svision3::Button>) {
            w->set_checked(c);
        } else {
            static_assert(std::is_same_v<T, void>, "checked only works on Checkbox or Button");
        }
        return std::move(*this);
    }
    Element checkable(bool c = true) {
        if constexpr (std::is_same_v<T, svision3::Button>) {
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
        static_assert(std::is_same_v<T, svision3::RadioButton>,
                      "selected(bool) only works on RadioButton");
        w->set_selected(s);
        return std::move(*this);
    }
    Element selected(int i) {
        static_assert(std::is_same_v<T, svision3::Combobox>, "selected(int) only works on Combobox");
        w->set_selected(i);
        return std::move(*this);
    }
    Element current(int i) {
        if constexpr (std::is_same_v<T, svision3::TabWidget>) {
            w->set_current(i);
        } else {
            static_assert(std::is_same_v<T, void>, "current only works on TabWidget");
        }
        return std::move(*this);
    }
    Element highlight_current_line(bool h = true) {
        if constexpr (std::is_same_v<T, svision3::TextEdit>) {
            w->set_highlight_current_line(h);
        } else {
            static_assert(std::is_same_v<T, void>, "highlight_current_line only works on TextEdit");
        }
        return std::move(*this);
    }

    Element spacing(float s) {
        w->set_spacing(s);
        return std::move(*this);
    }
    Element margins(svision3::Margins m) {
        w->set_margins(m);
        return std::move(*this);
    }
    Element ratio(int divider, float r) {
        static_assert(std::is_same_v<T, svision3::Splitter>,
                      "ratio(divider, r) only works on Splitter");
        w->set_ratio(divider, r);
        return std::move(*this);
    }
    Element stretch(int child, float factor) {
        static_assert(std::is_same_v<T, svision3::Splitter>,
                      "stretch(child, factor) only works on Splitter");
        w->set_stretch_factor(child, factor);
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
        if constexpr (std::is_same_v<T, svision3::HtmlView>) {
            w->on_link_click = std::move(f);
        } else {
            static_assert(std::is_same_v<T, void>, "on_link_click only works on HtmlView");
        }
        return std::move(*this);
    }

    Element background_color(svision3::Color c) {
        w->set_background_color(c);
        return std::move(*this);
    }
    Element icon(svision3::Icon icon) {
        if constexpr (std::is_same_v<T, svision3::Button>) {
            w->set_icon(std::move(icon));
        } else {
            static_assert(std::is_same_v<T, void>, "icon only works on Button");
        }
        return std::move(*this);
    }
    Element alignment(svision3::Alignment a) {
        w->set_alignment(a);
        return std::move(*this);
    }
    Element buddy(svision3::Widget *b) {
        if constexpr (std::is_same_v<T, svision3::Label>) {
            w->set_buddy(b);
        } else {
            static_assert(std::is_same_v<T, void>, "buddy only works on Label");
        }
        return std::move(*this);
    }
    Element padding(svision3::Margins m) {
        w->set_padding(m);
        return std::move(*this);
    }
    Element focusable(bool f) {
        w->set_focusable(f);
        return std::move(*this);
    }
    Element frame(bool enabled, bool sunken = false) {
        w->set_frame(enabled, sunken);
        return std::move(*this);
    }
    Element flat(bool f) {
        if constexpr (std::is_same_v<T, svision3::Button>) {
            w->set_flat(f);
        } else {
            static_assert(std::is_same_v<T, void>, "flat only works on Button");
        }
        return std::move(*this);
    }
    Element auto_repeat(bool ar, float delay = 0.5f, float interval = 0.1f) {
        if constexpr (std::is_same_v<T, svision3::Button>) {
            w->set_auto_repeat(ar, delay, interval);
        } else {
            static_assert(std::is_same_v<T, void>, "auto_repeat only works on Button");
        }
        return std::move(*this);
    }

    Element icon_size(int size) {
        if constexpr (std::is_same_v<T, svision3::IconGrid>) {
            w->set_icon_size(size);
        } else {
            static_assert(std::is_same_v<T, void>, "icon_size only works on IconGrid");
        }
        return std::move(*this);
    }

    Element scale_icons(bool scale) {
        if constexpr (std::is_same_v<T, svision3::IconGrid>) {
            w->set_scale_icons(scale);
        } else {
            static_assert(std::is_same_v<T, void>, "scale_icons only works on IconGrid");
        }
        return std::move(*this);
    }

    Element on_selection_changed(std::function<void(std::set<size_t> const &)> f) {
        if constexpr (std::is_same_v<T, svision3::IconGrid>) {
            w->on_selection_changed = std::move(f);
        } else {
            static_assert(std::is_same_v<T, void>, "on_selection_changed only works on IconGrid");
        }
        return std::move(*this);
    }

    Element on_item_activated(std::function<void(size_t)> f) {
        if constexpr (std::is_same_v<T, svision3::IconGrid>) {
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
        if constexpr (std::is_same_v<T, svision3::Toolbar>) {
            w->add_widget(std::move(child.w), static_cast<float>(stretch));
        } else {
            w->add_widget(std::move(child.w), stretch, svision3::Alignment::Fill);
        }
        return *this;
    }

    template <typename W> Element add(Element<W> &&child, int stretch = 0) {
        return std::move(add_child(std::move(child), stretch));
    }

    template <typename W> Element add(Element<W> &child, int stretch = 0) {
        return std::move(add_child(std::move(child), stretch));
    }

    template <typename W> Element add(std::shared_ptr<W> child, int stretch = 0) {
        w->add_widget(std::move(child), stretch);
        return std::move(*this);
    }

    Element expand() {
        expand_ = true;
        return std::move(*this);
    }

    template <typename W> Element operator|(Element<W> &&child) {
        int stretch = child.expand_ ? 1 : 0;
        w->add_widget(std::move(child.w), stretch, svision3::Alignment::Fill);
        return std::move(*this);
    }

    template <typename W> Element operator|(Element<W> &child) {
        int stretch = child.expand_ ? 1 : 0;
        w->add_widget(std::move(child.w), stretch, svision3::Alignment::Fill);
        return std::move(*this);
    }

    // MenuBar special functions
    Element add_menu(MenuElement &&m);
    Element add_menu(std::string_view title, MenuElement &&m);

    // Toolbar and Button special functions
    Element command(svision3::Command::Ptr cmd) {
        if constexpr (std::is_same_v<T, svision3::Toolbar>) {
            last_cmd_ = cmd.get();
            w->add_command(std::move(cmd));
        } else if constexpr (std::is_same_v<T, svision3::Button>) {
            w->set_command(std::move(cmd));
        } else {
            static_assert(std::is_same_v<T, svision3::Toolbar> || std::is_same_v<T, svision3::Button>,
                          "command only works on Toolbar or Button");
        }
        return std::move(*this);
    }
    Element menu(MenuElement &&m);
    Element command(std::string name, std::function<void()> action) {
        static_assert(std::is_same_v<T, svision3::Toolbar>, "command only works on Toolbar");
        auto cmd = svision3::Command::create(std::move(name), std::move(action));
        last_cmd_ = cmd.get();
        w->add_command(std::move(cmd));
        return std::move(*this);
    }
    Element command(std::string name, std::string tooltip = {}, std::string icon = {},
                    std::function<void()> action = {}) {
        static_assert(std::is_same_v<T, svision3::Toolbar>, "command only works on Toolbar");
        auto cmd = svision3::Command::create(std::move(name), std::move(action));
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
        static_assert(std::is_same_v<T, svision3::Toolbar>, "separator only works on Toolbar");
        w->add_separator();
        return std::move(*this);
    }

    // TabWidget special functions
    Element orientation(svision3::TabOrientation o) {
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

    template <typename W>
    Element add_tab(std::string_view title, std::string_view tooltip, Element<W> &&child,
                    bool closable = true) {
        auto index = static_cast<int>(w->get_tab_count());
        w->add_tab(std::string(title), std::move(child.w), closable);
        w->set_tab_tooltip(index, std::string(tooltip));
        return std::move(*this);
    }

    template <typename W> Element add_tab(std::string_view title, Element<W> &child) {
        return add_tab(title, std::move(child));
    }

    // Line edit special functions
    Element validation_mode(svision3::LineInput::ValidationMode mode) {
        w->set_validation_mode(mode);
        return std::move(*this);
    }

    Element
    validator(std::function<bool(std::string const &, svision3::LineInput const &input)> validator) {
        w->set_validator(validator);
        return std::move(*this);
    }

    template <typename F> Element on_change(F &&f) {
        if constexpr (std::is_same_v<T, svision3::LineInput>) {
            w->on_change = std::forward<F>(f);
        } else if constexpr (std::is_same_v<T, svision3::Slider>) {
            w->set_on_change([f = std::forward<F>(f)](svision3::Slider &, float v) { f(v); });
        } else if constexpr (std::is_same_v<T, svision3::SpinBox>) {
            w->on_change = std::forward<F>(f);
        } else if constexpr (std::is_same_v<T, svision3::Combobox>) {
            w->on_change = std::forward<F>(f);
        } else {
            static_assert(std::is_same_v<T, void>, "Method not supported");
        }
        return std::move(*this);
    }

    Element alternate_row_colors(bool value) {
        if constexpr (std::is_same_v<T, svision3::TableView>) {
            w->set_alternating_row_colors(value);
        } else if constexpr (std::is_same_v<T, svision3::ListView>) {
            w->set_alternating_row_colors(value);
        } else if constexpr (std::is_same_v<T, svision3::TreeView>) {
            w->set_alternating_row_colors(value);
        } else {
            static_assert(std::is_same_v<T, void>, "Method not supported");
        }
        return std::move(*this);
    }

    Element browser_mode(bool b) {
        if constexpr (std::is_same_v<T, svision3::FileBrowserWidget>) {
            w->set_browser_mode(b);
        } else {
            static_assert(std::is_same_v<T, void>, "browser_mode only works on FileBrowserWidget");
        }
        return std::move(*this);
    }

    Element on_file_activated(std::function<void(std::string const &)> f) {
        if constexpr (std::is_same_v<T, svision3::FileBrowserWidget>) {
            w->on_file_activated = std::move(f);
        } else {
            static_assert(std::is_same_v<T, void>,
                          "on_file_activated only works on FileBrowserWidget");
        }
        return std::move(*this);
    }

    Element on_path_changed(std::function<void(std::string const &)> f) {
        if constexpr (std::is_same_v<T, svision3::FileBrowserWidget>) {
            w->on_path_changed = std::move(f);
        } else {
            static_assert(std::is_same_v<T, void>,
                          "on_path_changed only works on FileBrowserWidget");
        }
        return std::move(*this);
    }

    operator std::shared_ptr<svision3::Widget>() && { return std::move(w); }
    operator std::shared_ptr<svision3::Widget>() & { return w; }
    T *get() const { return w.get(); }

    // A weak_ptr to the widget, valid past the point this Element is moved into
    // a container: the container takes over the shared ownership, so the
    // weak_ptr keeps tracking the same object. Use this rather than get() for
    // anything stored in a long-lived callback.
    std::weak_ptr<T> ref() const { return w; }
};

// CommandElement and MenuElement
struct CommandElement {
    std::shared_ptr<svision3::Command> cmd;
    CommandElement(std::shared_ptr<svision3::Command> c) : cmd(std::move(c)) {}
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
    operator std::shared_ptr<svision3::Command>() const { return cmd; }
};

struct MenuElement {
    std::shared_ptr<svision3::Menu> menu;
    MenuElement(std::shared_ptr<svision3::Menu> m) : menu(std::move(m)) {}
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
    operator std::shared_ptr<svision3::Menu>() const { return menu; }
};

template <typename T> Element<T> Element<T>::add_menu(MenuElement &&m) {
    static_assert(std::is_same_v<T, svision3::MenuBar>, "add_menu only works on MenuBar");
    w->add_menu(m.menu);
    return std::move(*this);
}

template <typename T> Element<T> Element<T>::add_menu(std::string_view title, MenuElement &&m) {
    static_assert(std::is_same_v<T, svision3::MenuBar>, "add_menu only works on MenuBar");
    w->add_menu(std::string(title), m.menu);
    return std::move(*this);
}

template <typename T> Element<T> Element<T>::menu(MenuElement &&m) {
    if constexpr (std::is_same_v<T, svision3::Button>) {
        w->set_menu(m.menu);
    } else {
        static_assert(std::is_same_v<T, void>, "menu only works on Button");
    }
    return std::move(*this);
}

// Factory functions
inline CommandElement command(std::string name, std::function<void()> action = {}) {
    return CommandElement(svision3::Command::create(std::move(name), std::move(action)));
}

inline MenuElement menu(std::string_view title = "") {
    return MenuElement(std::make_shared<svision3::Menu>(std::string(title)));
}

inline Element<svision3::MenuBar> menubar() {
    return Element<svision3::MenuBar>(std::make_unique<svision3::MenuBar>());
}

inline Element<svision3::Label> label(std::string_view text = "") {
    return Element<svision3::Label>(std::make_unique<svision3::Label>(std::string(text)));
}

inline Element<svision3::Button> button(std::string_view text = "") {
    return Element<svision3::Button>(std::make_unique<svision3::Button>(std::string(text)));
}

inline Element<svision3::Checkbox> checkbox(std::string_view text = "") {
    return Element<svision3::Checkbox>(std::make_unique<svision3::Checkbox>(std::string(text)));
}

inline Element<svision3::LineInput> line_input(std::string_view placeholder = "") {
    return Element<svision3::LineInput>(
        std::make_unique<svision3::LineInput>(std::string(placeholder)));
}

inline Element<svision3::ListView> list_view(std::shared_ptr<svision3::ItemModel> model) {
    return Element<svision3::ListView>(std::make_unique<svision3::ListView>(model));
}

inline Element<svision3::TableView> table_view(std::shared_ptr<svision3::ItemModel> model) {
    return Element<svision3::TableView>(std::make_unique<svision3::TableView>(model));
}

inline Element<svision3::TreeView> tree_view(std::shared_ptr<svision3::TreeModel> model) {
    return Element<svision3::TreeView>(std::make_unique<svision3::TreeView>(model));
}

inline Element<svision3::Slider> slider(float value = 0, float min = 0, float max = 100) {
    auto s = std::make_unique<svision3::Slider>();
    s->set_range(min, max);
    s->set_value(value);
    return Element<svision3::Slider>(std::move(s));
}

inline Element<svision3::ProgressBar> progress_bar(float value = 0) {
    auto p = std::make_unique<svision3::ProgressBar>();
    p->set_value(value);
    return Element<svision3::ProgressBar>(std::move(p));
}

inline Element<svision3::SpinBox> spin_box(int value = 0, int min = 0, int max = 100, int step = 1) {
    return Element<svision3::SpinBox>(std::make_unique<svision3::SpinBox>(value, min, max, step));
}

inline Element<svision3::Combobox> combobox(std::vector<std::string> items) {
    return Element<svision3::Combobox>(std::make_unique<svision3::Combobox>(std::move(items)));
}

inline Element<svision3::IconGrid> icon_grid(std::shared_ptr<svision3::ItemModel> model) {
    return Element<svision3::IconGrid>(std::make_unique<svision3::IconGrid>(model));
}

inline Element<svision3::ImageWidget> image_widget() {
    return Element<svision3::ImageWidget>(std::make_unique<svision3::ImageWidget>());
}

inline Element<svision3::Label> spacer() {
    return Element<svision3::Label>(std::make_unique<svision3::Label>(""));
}

// Radio group for radio buttons
struct RadioGroupWrapper {
    std::shared_ptr<svision3::RadioGroup> group;
    RadioGroupWrapper() : group(std::make_shared<svision3::RadioGroup>()) {}
    RadioGroupWrapper &on_change(std::function<void(int index)> f) {
        group->on_change = std::move(f);
        return *this;
    }
    operator svision3::RadioGroup &() const { return *group; }
    operator std::shared_ptr<svision3::RadioGroup>() const { return group; }
};

inline RadioGroupWrapper radio_group() { return RadioGroupWrapper(); }

inline Element<svision3::RadioButton> radio_button(std::string_view text, RadioGroupWrapper &group) {
    return Element<svision3::RadioButton>(
        std::make_unique<svision3::RadioButton>(std::string(text), *group.group));
}

inline Element<svision3::RadioButton> radio_button(std::string_view text,
                                                  std::shared_ptr<svision3::RadioGroup> group) {
    return Element<svision3::RadioButton>(
        std::make_unique<svision3::RadioButton>(std::string(text), *group));
}

inline Element<svision3::RadioButton> radio_button(std::string_view text,
                                                  svision3::RadioGroup &group) {
    return Element<svision3::RadioButton>(
        std::make_unique<svision3::RadioButton>(std::string(text), group));
}

inline Element<svision3::TextEdit> text_edit(std::string text = {}) {
    return Element<svision3::TextEdit>(std::make_unique<svision3::TextEdit>(text));
}

inline Element<svision3::FileBrowserWidget> file_browser() {
    return Element<svision3::FileBrowserWidget>(std::make_unique<svision3::FileBrowserWidget>());
}

// Tab widget - special handling for add_tab
inline Element<svision3::TabWidget> tab_widget() {
    return Element<svision3::TabWidget>(std::make_unique<svision3::TabWidget>());
}

// DockArea - doesn't fit the generic Element<T> template (add_dock/set_center
// take DockPosition, and dock_tab_widget() returns a per-position TabWidget
// rather than a single child), so it gets its own small fluent wrapper.
struct DockElement {
    std::shared_ptr<svision3::DockArea> w;
    DockElement() : w(std::make_shared<svision3::DockArea>()) {}
    DockElement(DockElement &&) = default;
    DockElement &operator=(DockElement &&) = default;
    DockElement(const DockElement &) = delete;
    DockElement &operator=(const DockElement &) = delete;

    template <typename W> DockElement center(Element<W> &&child) {
        w->set_center(std::move(child.w));
        return std::move(*this);
    }

    DockElement add_dock(svision3::DockPosition pos, std::string_view title,
                         std::shared_ptr<svision3::Widget> content) {
        w->add_dock(pos, std::string(title), std::move(content));
        return std::move(*this);
    }

    template <typename W>
    DockElement add_dock(svision3::DockPosition pos, std::string_view title, Element<W> &&child) {
        return add_dock(pos, title, std::shared_ptr<svision3::Widget>(std::move(child.w)));
    }

    DockElement dock_size(svision3::DockPosition pos, float size) {
        w->set_dock_size(pos, size);
        return std::move(*this);
    }

    DockElement dock_orientation(svision3::DockPosition pos, svision3::TabOrientation o) {
        if (auto tab = w->dock_tab_widget(pos).lock()) {
            tab->set_orientation(o);
        }
        return std::move(*this);
    }

    svision3::DockArea *operator->() const { return w.get(); }
    svision3::DockArea *get() const { return w.get(); }
    std::weak_ptr<svision3::DockArea> ref() const { return w; }
    operator std::shared_ptr<svision3::Widget>() && { return std::move(w); }
};

inline DockElement dock_area() { return DockElement{}; }

// Layouts
inline svision3::Margins default_margins() { return {10, 10, 10, 10}; }
inline svision3::Margins default_margins_no_bottom() { return {10, 10, 0, 10}; }
inline svision3::Margins no_margins() { return {0, 0, 0, 0}; }

inline int default_padding() { return 10; }

constexpr int expand = 1;
constexpr int no_stretch = 0;
constexpr int no_spacing = 0;

inline Element<svision3::HtmlView> html_view(std::string_view html = "") {
    auto v = std::make_unique<svision3::HtmlView>();
    if (!html.empty()) {
        v->set_html(std::string(html));
    }
    return Element<svision3::HtmlView>(std::move(v));
}

inline Element<svision3::RichLabel> rich_label(std::string_view text = "") {
    auto v = std::make_unique<svision3::RichLabel>();
    if (!text.empty()) {
        v->set_text(std::string(text));
    }
    return Element<svision3::RichLabel>(std::move(v));
}

inline Element<svision3::RichLabel> rich_label_md(std::string_view markdown) {
    auto v = std::make_unique<svision3::RichLabel>();
    v->set_markdown(std::string(markdown));
    return Element<svision3::RichLabel>(std::move(v));
}

template <typename W> inline Element<svision3::ScrollArea> scroll_area(Element<W> content) {
    auto sa = std::make_unique<svision3::ScrollArea>();
    sa->set_content(std::move(content.w));
    return Element<svision3::ScrollArea>(std::move(sa));
}

// Splitter takes an arbitrary number of children, so these accept any number
// of elements (2 or more). Use .ratio(divider, r) to position a divider, and
// .stretch(child, factor) to control how much of a resize that child
// absorbs (default 1 for every child, so all children grow/shrink equally).
template <typename... Ts> inline Element<svision3::Splitter> hsplit(Element<Ts>... elems) {
    static_assert(sizeof...(Ts) >= 2, "hsplit() needs at least two elements");
    auto sp = std::make_unique<svision3::Splitter>(svision3::Orientation::Horizontal);
    (sp->add_child(std::move(elems.w)), ...);
    return Element<svision3::Splitter>(std::move(sp));
}

template <typename... Ts> inline Element<svision3::Splitter> vsplit(Element<Ts>... elems) {
    static_assert(sizeof...(Ts) >= 2, "vsplit() needs at least two elements");
    auto sp = std::make_unique<svision3::Splitter>(svision3::Orientation::Vertical);
    (sp->add_child(std::move(elems.w)), ...);
    return Element<svision3::Splitter>(std::move(sp));
}

inline Element<svision3::VBoxLayout> vbox() {
    auto layout = std::make_unique<svision3::VBoxLayout>();
    layout->set_spacing(default_padding());
    layout->set_margins(default_margins());
    return Element<svision3::VBoxLayout>(std::move(layout));
}

inline Element<svision3::HBoxLayout> hbox() {
    auto layout = std::make_unique<svision3::HBoxLayout>();
    layout->set_spacing(default_padding());
    layout->set_margins(default_margins());
    return Element<svision3::HBoxLayout>(std::move(layout));
}

inline Element<svision3::Toolbar> toolbar() {
    return Element<svision3::Toolbar>(std::make_unique<svision3::Toolbar>());
}

inline svision3::ToastBuilder toast() { return {}; }

inline Element<svision3::StatusBar> status_bar() {
    return Element<svision3::StatusBar>(std::make_unique<svision3::StatusBar>());
}

} // namespace ui
