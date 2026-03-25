# Theme Refactoring Plan

This document outlines the refactoring of the theming system in the `svision3` toolkit. The goal is to move from a monolithic struct with style-specific `if` statements to an inheritable class hierarchy with primitive drawing methods.

## Goals

1.  **Inheritable Themes:** Replace the `ThemeStyle` enum and large `Theme` struct with a `Theme` base class and platform/style-specific subclasses (e.g., `MacOSTheme`, `Win11Theme`, `Win95Theme`).
2.  **Primitive Drawing Methods:** Move widget-specific painting logic from the widgets' `paint()` methods into virtual methods in the `Theme` class.
3.  **Simplified Palette:** Use a simple `Palette` struct for core theme colors, and let themes derive their specific sub-styles from it.

## Proposed Architecture

### 1. Simplified Palette (Colors)

The current theme has many redundant colors spread across `WidgetStyle` subclasses. We will move to a core `Palette` that defines the basic identity of the color scheme (Light, Dark, High Contrast, etc.).

```cpp
struct Palette {
    Color window;      // Main background for windows
    Color base;        // Background for input widgets/lists
    Color alternate;   // Alternate background for lists
    Color text;        // Normal text color
    Color placeholder; // Placeholder/de-emphasized text
    Color highlight;   // Background for selected items
    Color highlighted_text; // Text color for selected items
    Color border;      // Border color for widgets
    Color accent;      // Primary brand/action color
    Color link;        // Color for links/actions
    
    // Semantic colors
    Color success;
    Color warning;
    Color error;

    // Platform-specific defaults
    SystemFonts fonts;
};
```

### 2. The `Theme` Base Class

The `Theme` class will provide virtual methods for drawing various UI primitives. It will also store the specific style structs (which may be moved into the subclasses or kept as defaults).

```cpp
class Theme {
public:
    virtual ~Theme() = default;

    // Factory methods
    static std::unique_ptr<Theme> create(ThemeStyle style, ColorScheme scheme = ColorScheme::Light);
    static const Theme& current();
    static void set_current(std::unique_ptr<Theme> theme);

    // Primitive Drawing Methods
    virtual void draw_button(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon, bool hovered, bool pressed, bool focused, bool enabled, bool flat) const = 0;
    virtual void draw_checkbox(Painter &painter, Rect const &rect, std::string_view text, CheckState state, bool hovered, bool pressed, bool focused, bool enabled) const = 0;
    virtual void draw_radio_button(Painter &painter, Rect const &rect, std::string_view text, bool checked, bool hovered, bool pressed, bool focused, bool enabled) const = 0;
    virtual void draw_line_input(Painter &painter, Rect const &rect, std::string_view text, std::string_view placeholder, int cursor_pos, int selection_start, int selection_end, bool focused, bool enabled) const = 0;
    virtual void draw_menubar_item(Painter &painter, Rect const &rect, std::string_view title, bool hovered, bool active, bool show_mnemonics, int mnemonic_index) const = 0;
    virtual void draw_menubar_background(Painter &painter, Rect const &rect) const = 0;
    virtual void draw_menu_background(Painter &painter, Rect const &rect) const = 0;
    virtual void draw_menu_item(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon, std::string_view shortcut, bool hovered, bool enabled, bool checkable, bool checked) const = 0;
    virtual void draw_menu_separator(Painter &painter, Rect const &rect) const = 0;
    virtual void draw_progress_bar(Painter &painter, Rect const &rect, float progress, bool enabled) const = 0;
    virtual void draw_slider(Painter &painter, Rect const &rect, float value, bool hovered, bool pressed, bool focused, bool enabled) const = 0;
    virtual void draw_tab_bar_background(Painter &painter, Rect const &rect) const = 0;
    virtual void draw_tab(Painter &painter, Rect const &rect, std::string_view text, bool active, bool hovered, bool enabled) const = 0;
    virtual void draw_list_item(Painter &painter, Rect const &rect, std::string_view text, Icon const &icon, bool selected, bool hovered, bool alternate) const = 0;
    virtual void draw_tooltip(Painter &painter, Rect const &rect, std::string_view text) const = 0;

    // Metrics and Styles
    virtual Size measure_button(std::string_view text, Icon const &icon) const = 0;
    virtual Size measure_menubar_item(std::string_view text) const = 0;
    virtual Size measure_menu_item(std::string_view text, Icon const &icon, std::string_view shortcut) const = 0;
    virtual float menu_separator_height() const = 0;
    virtual Size measure_tab(std::string_view text) const = 0;
    virtual float list_item_height() const = 0;
    virtual Size measure_tooltip(std::string_view text) const = 0;

    virtual Margins button_padding() const = 0;
    virtual Margins line_input_padding() const = 0;
};
```

## Themes to Implement

The following themes should be implemented as subclasses of `Theme` (or a `BaseTheme`):

- **BaseTheme:** Common logic and fallback implementations.
- **MacOSTheme:** Mimics the look and feel of macOS (rounded, subtle gradients, CoreGraphics style).
- **Win11Theme:** Modern Windows look (rounded corners, specific accent colors).
- **Win95Theme:** Classic 3D beveled look.
- **Plasma6Theme:** KDE Plasma look.
- **GNOMETheme:** Adwaita/GTK-like look.

## Implementation Phases

### Phase 1: Define Interfaces
- Update `include/toolkit/theme.hpp` with the new `Palette` and `Theme` class hierarchy.
- Define the set of virtual methods (primitives) required for each widget.

### Phase 2: Implement Base and Default Themes
- Create a `DefaultTheme` or `BaseTheme` in `src/toolkit/theme.cpp` that implements the generic look.
- Implement style-specific subclasses (`MacOSTheme`, `Win11Theme`, etc.).
- Port existing logic from `Theme::create` and `Theme::draw_menubar_item` into these subclasses.

### Phase 3: Update Widgets
- Modify widget `paint()` methods (e.g., `Button::paint`, `Checkbox::paint`) to delegate drawing to `Theme::current().draw_xxx(...)`.
- Update `size_hint()` methods to use `Theme::current().measure_xxx(...)`.

### Phase 4: Cleanup
- Remove the old `Theme` struct and sub-style structs if they are no longer needed.
- Simplify `Painter::draw_frame` and `Painter::draw_focus_ring` if they are better handled by themes.

## Verification & Testing
- Use `test_theme.cpp` to verify color derivation.
- Run `demo` and switch between themes at runtime (if supported) to ensure visual consistency.
- Ensure that the "Pink" and "Dark" schemes still work by passing different `Palette`s to the theme constructors.
