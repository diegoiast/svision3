# GUI Toolkit – Task List

Legend: [x] done [ ] pending

Implemented: 111
Total: 208
Progress: 111 / 208 ≈ 53.3%

This document is not final (note that the menus and toolbars support is done in
another document). The idea is to reach version 0.9 with all these items marked.

Version 0.9.x will be polished until version 1.0.0 is marked as "good enough".

## Core Architecture [19/39]

1. [x] Cross-platform GUI toolkit in C++20
2. [x] macOS backend (Cocoa/CoreGraphics)
3. [x] Linux backend (X11/Wayland)
4. [x] Windows backend (Win32)
5. [x] PIMPL for Window and Application
6. [x] PIMPL for platform headers (X11, Win32 text rasterizer)
7. [x] CMake build system
8. [x] Conan package manager (cairo, spdlog)
9. [x] Cairo 2D drawing abstraction (Painter)
10. [x] High DPI / fractional scaling (all platforms)
11. [ ] OpenGL rendering backend on Linux/Windows blurry fonts.
12. [ ] Cairo has blurry fonts.
13. [ ] OpenGL uses cairo for painting text. Should use stb_ttf or semething.
14. [x] Command interfaces for menus/toolbars and app shortcuts.
15. [ ] Backends have open/cairo/whatevre backed in. We need to separate them.
16. [x] Paintings inside widgets should start at (0,0) not position at window.
17. [ ] Scroll size - detect at runtime.
18. [ ] Natural scrolling - detect at runtime.
19. [ ] Blinking API - we have a timer, I am unsure what can be done to make it public.
20. [ ] Widget has 2 naked pointers `parent` and `window`. Understand when this will fail.
21. [ ] Shortcuts per platform. Functions to read shortcuts per platform on runtime.
22. [ ] Some way to connect to Appium for GUI testing. Maybe emulate the flutter observatory?
23. [x] Generic timer system (start_timer / stop_timer)
24. [x] Popup/overlay mechanism (used by Combobox)
25. [x] Painter helpers (draw_frame, draw_focus_ring, fill_circle, etc.)
26. [x] Font metrics for consistent text positioning
27. [x] Clipping (push_clip / pop_clip).
28. [x] Screenshot / save_to_png.
29. [ ] Resource / asset management.
30. [ ] Logging levels configurable at runtime.
31. [x] Clipboard (copy/paste, all platforms).
32. [ ] Drag and drop.
33. [ ] Font selection / custom fonts.
34. [ ] Image loading / rendering.
35. [ ] Animation framework (transitions, easing).
36. [ ] Undo/redo framework.
37. [ ] Signals/slots (beyond single std::function) - or alternative.
38. [ ] Serialization (save/restore widget state).
39. [ ] Declarative UI support.

## Platform – Not Yet Implemented

1. [x] File dialogs (open/save) - implemented via [Native File Dialogs](https://github.com/mlabbe/nativefiledialog).
1. [ ] Non-native file dialogs (open/save) - re-implement them in this toolkit.
1. [ ] Message boxes / alerts.
1. [ ] Multi-window.
1. [ ] IME / input method support.
1. [ ] RTL / bidirectional text.
1. [ ] Accessibility (screen readers).
1. [ ] System tray / notifications.
1. [ ] Native file drag-and-drop.
1. [ ] Wayland clipboard (cross-client).
1. [x] Wayland tooltips (xdg_popup)

## Widgets [16/37]

1. [x] Button (click callback, hover/pressed states).
2. [x] Label (text display, shrinkable/clippable)
3. [x] LineInput (text editing, cursor, blinking, placeholder).
4. [x] Checkbox (toggle, on_toggle callback).
5. [x] Checkbox - tristate.
6. [ ] Checkbox - paint using the theme, not hardcoded drawing.
7. [x] RadioButton + RadioGroup (mutual exclusivity).
8. [x] Combobox (dropdown, keyboard nav, selection).
9. [x] ProgressBar (themed, value 0-1).
10. [x] TabWidget (tab bar, close button, drag reorder).
11. [x] ListView: MVC (ListAdapter / StringListAdapter / FilterAdapter).
12. [ ] ListView: display widgets instead of simple text.
13. [x] SpinBox (int value, min/max/step, arrow buttons).
14. [ ] SpinBox: auto repeat (use regular buttons?)
15. [x] Tooltip.
16. [x] Slider (horizontal, vertical, range).
17. [ ] ScrollBar / ScrollArea.
18. [ ] ScrollBar: API for theme/app to tell widgets "have an inline scrollbar" or external.
19. [x] TextArea (multi-line input).
20. [ ] TreeView.
21. [x] TableView / DataGrid.
22. [ ] MenuBar / Menu / ContextMenu. - see [menus-toolbars-commands.md](separate document).
23. [x] Toolbar.
24. [ ] Toolbar + images.
25. [ ] Toolbar + custom widgets.
26. [ ] Dialog (modal/modeles).
27. [ ] StatusBar.
28. [ ] GroupBox / Frame.
29. [ ] Splitter.
30. [ ] Image widget.
31. [ ] Markdown label using litehtml+(markdown processor for C++).
32. [ ] Markdown tooltip.
33. [ ] Undo/redo system (text area + linuedit).
34. [ ] Client side decorations with theme.
35. [ ] Proper filter API for listview and table view.
36. [ ] MainWindow with docking widgets on the sides.
37. [ ] Text cursor drawing: input, text and and spinbox use use it. Maybe use a shared class?

## Layout System [6/12]

1. [x] VBoxLayout (vertical stacking, stretch factors, alignment).
2. [x] HBoxLayout (horizontal stacking, stretch factors, alignment).
3. [x] Margins and spacing.
4. [x] Buttons don't stretch vertically.
5. [x] Layouts skip hidden widgets.
6. [x] Min/max size constraints respected in layout.
7. [ ] GridLayout.
8. [ ] FormLayout (label + field pairs).
9. [ ] StackedLayout (one visible child at a time).
10. [ ] RTL layout direction switch (right-to-left mirroring).
11. [ ] Method to center point inside rect.
12. [ ] Paddings/margins should be defined by the theme (unless overriden in the widget).

## Theming & Styling [9/24]

1. [x] Theme/style abstraction (WidgetStyle base struct).
2. [x] Palette-based theme derivation (Theme::from_palette).
3. [ ] Applications can load custom palette.
4. [x] Color schemes: Light, Dark, Pink.
5. [x] ThemeStyle + ColorScheme enums for structured selection.
6. [x] Custom palette override support.
7. [x] Theme simplification (reduced duplication, dynamic hover/pressed).
8. [x] Win95 progress bar: taller, chunked blocks.
9. [x] Native theme/font support.
10. [ ] Tint support.
11. [ ] Scrollbar theming (overlay vs classic per theme).
12. [ ] Focus ring styling per theme.
13. [ ] XDG Icon theme support.
14. [ ] Simplify and minimize the theme structs.
15. [ ] Theme should be in application, then window then widget, in all but app - optional.
16. [ ] Load them from config file (which theme?).
17. [ ] Draw premetives from theme
    1. [ ] Borders
    2. [ ] Background
    3. [ ] Tabs
    4. [ ] Menus
    5. [ ] Checkbox
    6. [ ] Radio buttons
    7. [ ] Ring focus

### Windows 11 Theme

1. Fix

### Plasma theme

1. Button
    1. [ ] Non-disabled button should have double bottom line

### Gnome theme

1. Fix

### Material

1. Fix

### macOS

1. Fix

## Windows 95 theme

1. Fix

## Event Handling [7/12]

1. [x] Mouse events (Press, Release, Move, Drag, Scroll).
2. [x] Keyboard events (key codes, modifiers, text input).
3. [x] Scroll wheel support (macOS scrollWheel:).
4. [x] Focus management (Tab / Shift+Tab navigation).
5. [x] Mnemonic shortcuts (& prefix, Alt/Cmd/Ctrl activation - button.
6. [ ] Better handle of mnemonic: I don't like current implementation.
7. [ ] Mnemonic shortcuts for readio buttons
8. [ ] Mnemonic shortcuts for checkbox
9. [ ] Mnemonic shortcuts selecting a label's buddy
10. [x] Cmd+Q / Ctrl+Q triggers Quit mnemonic.
11. [ ] Global keyboard shortcuts / accelerators.
12. [ ] Mouse enter/leave events on widgets.

## Button features [3/7]

1. [x] Background color defintion.
2. [ ] Attach a menu (long click, or sub-button).
3. [x] Flat display (borders on hover only).
4. [ ] Toggle buttton - on/off.
5. [x] Auto repeat.
6. [ ] Icon support (image + text)
7. [ ] Button group (mutually exclusive toggles)

## LineInput Features [DONE]

1. [x] Text cursor with blinking (NSTimer + steady_clock).
2. [x] Cursor navigation (arrows, Home, End).
3. [x] Word-by-word navigation (Alt+Left/Right).
4. [x] Space key advances cursor.
5. [x] Text selection (Shift+Arrow, Shift+Home/End, Shift+Alt+Arrow).
6. [x] Cmd+A select all.
7. [x] Mouse click positioning, double-click word select, triple-click.
8. [x] Mouse drag selection.
9. [x] Typing/delete replaces selection.
10. [x] Text scrolling when content exceeds frame.
11. [x] Clipping (text doesn't draw over frame).
12. [x] Password mode (masked characters).
13. [x] Read-only mode.
14. [x] Input validation / formatting.

## Widget State & Interaction [DONE]

1. [x] Widget enable/disable (grayed rendering, non-interactive)
2. [x] Widget show/hide (hidden widgets skipped in layout/events)
3. [x] Custom mouse cursors (IBeam for LineInput, Hand for Combobox, NotAllowed for disabled)
4. [x] Min/max size constraints on widgets
5. [x] Min/max size constraints on window
6. [x] Window min size auto-computed from root content

## TabWidget / TabHeader Features [9/11]

1. [x] Clickable tab bar with active/inactive/hover states.
2. [x] Close button on tabs (logs, doesn't close).
3. [x] on_tab_close callback.
4. [x] Drag-to-reorder tabs.
5. [x] Dragged tab renders on top of siblings.
6. [ ] Separate TabHeader from TabWidget (standalone tab bar widget).
7. [x] TabHeader placement: North, South, East, West.
8. [x] Support for south/west east.
9. [x] Support for trailing/leading button.
10. [x] Tab bar overflow (scroll when too many tabs).
11. [ ] Tabs can have several icons at the title

## ListView Features [14/17]

1. [x] ListAdapter abstract interface (count, text_at)
2. [x] StringListAdapter (`vector<string>`, append, remove, set_items)
3. [x] FilterAdapter (async filtering with progress callback)
4. [x] on_data_changed notification from adapter to view
5. [x] Mouse click to select, hover highlight
6. [x] Scroll wheel support
7. [x] Keyboard navigation (Up/Down/Home/End) with auto-scroll
8. [x] Overlay scrollbar
9. [x] Focus ring
10. [x] on_selection_changed callback
11. [x] Virtual rendering (only visible items drawn)
12. [x] Alternating row colors (boolean toggle)
13. [x] Multi-selection (Shift+Click range, Shift+Arrow, Cmd+A)
14. [x] Selection API (selection(), set_selection(), select_all(), clear_selection())
15. [ ] Column headers: TableView [done].
16. [ ] Drag to reorder items.
17. [ ] Inline editing.

## Label Features [1/5]

1. [x] Text alignment (left / center / right)
2. [ ] Word wrap / multi-line
3. [ ] Selectable textfor checkbox
4. [ ] Rich text / markup
5. [ ] Buddy support - when clicking a label, mark the buddy active.

## Combobox Features [0/2]

1. [ ] Editable / searchable mode
1. [ ] Grouped items / separators

## Testing [0/3]

1. [ ] Visual regression tests (screenshot comparison)
1. [ ] Widget interaction tests (simulated input)
1. [ ] Appium test - using a dummy platform

## Demo - WIP

1. [x] Tab 1 – Main: label, line input, checkbox, combobox (theme styles), radio buttons (color schemes)
1. [x] Tab 2 – Progress: progress bar updating every 1 second via timer
1. [x] Tab 3 – Songs: ListView with Beatles songs, alternating rows, multi-select, filter + async
1. [x] Tab 4: Button bar: ok, Disabled (grayed), spacer, Quit (right-aligned)
1. [x] --screenshot= CLI flag for headless PNG export
1. [ ] Tab 5 – Kitchen Sink (slider, spinbox, group box, all widgets)

## Sqlite_viewer - WIP

1. [ ] Pretty ++
1. [ ] Synatx highlighter for the query
