# Menus, Toolbars, and Commands Plan

## 1. Command Interface (Abstraction) [WIP 4/6]

1. Properties: name, tooltip, shortcut, icon (deferred), enabled, checked. [done]
2. Signal: on_triggered. [done]
3. Purpose: Decouple the logic from the UI (Menu, Toolbar, Button). [done]
4. Text Fallback: If both name and icon are missing, "???" will be used. [done]
5. Icon support.
6. Support for High-DPI (@2x) image assets.

## 2. Widget Integration [DONE]

1. Widgets can own local Commands. [done]
2. Window owns global Commands. [done]
3. Event Handling: KeyEvents will be matched against Shortcut strings. [done]
4. Shortcut Priority: Shortcuts should be checked before focused widget input but after basic navigation keys. [done]

## 3. Toolbar Widget [WIP 4/8]

1. Container (HBoxLayout) for ToolButtons and arbitrary Widgets. [done]
2. Command Actions: ToolButtons are automatically created from a Command; sync with enabled/checked state. [done]
3. Embedded Widgets: Support adding any Widget (ComboBox, SpinBox, Label, etc.) to the toolbar.
4. Layout Management:
  1. Vertical or Horizontal orientation.
  2. Support for "Stretching" widgets to fill available space (e.g., search boxes).
  3. Vertical/Horizontal Dividers for grouping.[WIP]
5. Visuals: Text-only for now; use "???" if no text or icon is provided. [done]
6. Visuals: use icons from command.

## 4. Menu System [WIP 6/12]

1. MenuBar: Top-level horizontal container for Menus (File, Edit, etc.). [done]
2. Menu: A specialized Popup that contains a list of MenuItems. [done]
3. MenuItem:
     1. Action Item: Triggers a Command. Displays "Name" and "Shortcut" (e.g., "Open... Ctrl+O"). [done]
     2. Separator: A horizontal line to group related items.[done]
     3. Left/Right keyboard should move to the next/prev menu. [done]
     4. Submenu: A MenuItem that opens another nested Menu. [done]
     5. Global Coordination:
        1. The MenuBar (or Window) maintains a registry of all active Shortcuts. [WIP - verify, make unit test]
        2. Mnemonics: Supports "Alt+F" style access to MenuBar items. [done]
     6. "F10" to open menu (menu keyboard shortcut). [done]
     7. ContextMenu vs Menu classes are those the same?
     8. macOS Specifics & "PC Mode":
        1. Native Mode (Default): MenuBar maps to the global NSMenu bar.
        2. Embedded Mode ("PC Style"): MenuBar is rendered as a standard widget inside the window's layout, even on macOS.
     9. Search inside menus like macOS.

## 5. Popup and Overlay System

1. Move from a single `std::optional<Popup>` to a `std::vector<Popup>` (a stack) to support nested submenus. [WIP]
2. Implement "Recursive Closing" logic: Closing a parent popup automatically closes all its children in the stack.
3. Formalize `ContextMenu` to use the new `Command` system and stack.
4. Ensure `ComboBox` dropdowns align with the formalized popup logic.
5. Coordinate popup lifecycle (auto-closing on outside clicks/Escape) with the stack.

## 6. Icon & Image System

1. Platform Image Loading API:
    1. Abstract interface for loading pixel data from files or memory.
    2. Windows: Native implementation using WIC (Windows Imaging Component) or GDI+.
    3. Linux/macOS: Implementation using `stb_image` or similar lightweight library.
2. Cross-Platform Icon Engine:
    1. The lookup API must be available on all platforms (Windows, macOS, Linux).
    2. Configurable Pixmap Root: Developers define a root directory for assets; the engine recursively searches for matching icon names.
    3. XDG Spec Integration (Linux): On Linux, default to standard system icon themes; on other platforms, use the custom Pixmap Root as the primary source.
    4. Theme-aware lookup: resolve generic names (e.g., "document-open") to specific files.
    5. Size-based selection: automatically choose the best-matching resolution for the requested size.
3. Icon Registry & Cache:
    1. Centralized cache to manage loaded image resources and avoid redundant I/O.
4. Painter Integration:
    1. Add `draw_image(Rect destination, const Image& img)` to the `Painter` interface.
    2. Ensure all backends (Cairo, OpenGL, Win32, Cocoa) implement efficient image scaling and drawing.
