# Toolkit

A C++20 GUI toolkit with Cairo-based rendering and native platform backends for
macOS, Windows, and Linux (X11 and Wayland).

## Features

- Immediate-style widget painting via Cairo
- Layouts: `HBoxLayout`, `VBoxLayout` with margins, spacing, and stretch factors
- Widgets: `Button`, `Label`, `Checkbox`, `RadioButton`, `Combobox`, `LineInput`,
  `SpinBox`, `ProgressBar`, `TabWidget`, `ListView`, `ContextMenu`
- Keyboard navigation (Tab/Shift-Tab), mnemonics (`&ok` renders as underlined `o`),
  focus management
- Clipboard (copy/paste), timers, tooltips, cursor shapes
- Multiple theme styles (macOS, Windows, Linux) and color schemes (Light, Dark, Pink)
- Filterable list adapter with async progress reporting
- Thread-safe `Application::post_to_main_thread()` for background work
- Runtime backend selection on Linux via environment variables

## Platform & Rendering Matrix

| Platform | Windowing System | Rendering Backend | Selection Environment |
|----------|------------------|-------------------|-----------------------|
| **macOS**| Cocoa/AppKit     | CoreGraphics      | `SVISION_PAINT=native` (Default) |
| **macOS**| Cocoa/AppKit     | Cairo             | `SVISION_PAINT=cairo` |
| **macOS**| Cocoa/AppKit     | OpenGL 2.1        | `SVISION_PAINT=opengl` |
| **Windows**| Win32          | Cairo             | Default |
| **Linux**| X11              | Cairo             | `SVISION_PAINT=cairo` (Default) |
| **Linux**| X11              | OpenGL 2.1        | `SVISION_PAINT=opengl` |
| **Linux**| Wayland          | Cairo             | `SVISION_PAINT=cairo` (Default) |
| **Linux**| Wayland          | OpenGL 2.1        | `SVISION_PAINT=opengl` |

### Backend Selection

On Linux, the windowing system is selected automatically based on the environment, but can be forced:
1. `SVISION_BACKEND` environment variable (`x11` or `wayland`)
2. Presence of `WAYLAND_DISPLAY` (prefers Wayland)
3. Presence of `DISPLAY` (falls back to X11)

The rendering backend (Painter) is selected via the `SVISION_PAINT` environment variable on supported platforms.

## Dependencies

Managed via [Conan 2](https://conan.io/):

- **cairo** 1.18.0 -- 2D rendering
- **spdlog** 1.14.1 -- logging
- **Catch2** 3.7.1 -- testing

### System dependencies (Linux only)

For X11:

```
sudo apt install libx11-dev
```

For Wayland:

```
sudo apt install libwayland-dev wayland-protocols libxkbcommon-dev
```

Both can be installed side-by-side. CMake detects what's available and compiles
the corresponding backends.

## Building

### Install Conan dependencies

```bash
conan install . --build=missing
```

### Configure and build

```bash
cmake --preset conan-release
cmake --build build/Release
```

For a debug build:

```bash
conan install . -s build_type=Debug --build=missing
cmake --preset conan-debug
cmake --build build/Debug
```

### Run

```bash
./build/Release/demo
```

### Run tests

```bash
./build/Release/tests
```

## Project structure

```
include/toolkit/         Public headers
  application.hpp        Application lifecycle, window creation
  window.hpp             Window: widget tree, events, timers, tooltips
  widget.hpp             Base widget class
  platform.hpp           PlatformApplication / PlatformWindow interfaces
  painter.hpp            Cairo-based painting API
  theme.hpp              Theming system
  clipboard.hpp          Clipboard access
  events.hpp             MouseEvent, KeyEvent types
  types.hpp              Point, Size, Rect, Color, CursorShape, Key
  layout.hpp             HBoxLayout, VBoxLayout
  button.hpp             Button widget
  label.hpp              Label widget (multi-line, shrinkable)
  checkbox.hpp           Checkbox widget
  radio_button.hpp       RadioButton + RadioGroup
  combobox.hpp           Dropdown combobox
  line_input.hpp         Single-line text input with selection
  spin_box.hpp           Numeric spin box
  progress_bar.hpp       Progress bar
  tab_widget.hpp         Tabbed container
  list_view.hpp          Scrollable list with adapters
  context_menu.hpp       Popup context menu
  command.hpp            Keyboard shortcut commands
  stopwatch.hpp          High-resolution timer utility

src/toolkit/             Implementation
  platform_factory.cpp   Backend selection + Application/Clipboard impl
  window.cpp             Shared window logic (delegates to platform)
  platform/
    macos/               macOS backend (Cocoa, CoreGraphics)
    x11/                 X11 backend (Xlib, cairo-xlib)
    win32/               Win32 backend (user32, gdi32)
    wayland/             Wayland backend (libwayland, xdg-shell, xkbcommon)

demo/main.cpp            Demo application
tests/                   Catch2 test suite
```

## Architecture

`Application` and `Window` use the PIMPL pattern to delegate platform-specific
work to virtual interfaces (`PlatformApplication`, `PlatformWindow`). Each
backend provides concrete implementations:

```
Application  -->  PlatformApplication  -->  MacOSCairoPlatformApplication
                                            MacOSNativePlatformApplication
                                            MacOSOpenGLPlatformApplication
                                            X11PlatformApplication
                                            Win32PlatformApplication
                                            WaylandPlatformApplication

Window       -->  PlatformWindow       -->  MacOSCairoPlatformWindow
                                            MacOSNativePlatformWindow
                                            MacOSOpenGLPlatformWindow
                                            X11PlatformWindow
                                            Win32PlatformWindow
                                            WaylandPlatformWindow
```

The factory function `create_platform_application()` in `platform_factory.cpp`
instantiates the correct backend. On Linux, this happens at runtime based on
environment detection. On macOS and Windows, it's a compile-time decision.

All rendering goes through the abstract `Painter` interface. The toolkit provides two main implementations:
- `CairoPainter`: Uses the Cairo graphics library for high-quality 2D vector graphics.
- `GLPainter`: A hardware-accelerated OpenGL 2.1 implementation.

For text rendering in OpenGL, the toolkit utilizes a `TextRasterizer` interface with platform-specific implementations (CoreText on macOS, Cairo on Linux).

## Usage example

```cpp
#include "toolkit/application.hpp"
#include "toolkit/button.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/window.hpp"

int main() {
    toolkit::Application app;
    auto *window = app.create_window("Hello", {400, 200});

    auto root = std::make_unique<toolkit::VBoxLayout>();
    root->set_margins({20, 20, 20, 20});
    root->set_spacing(12);

    root->add_widget(std::make_unique<toolkit::Label>("Hello, world!"));

    auto btn = std::make_unique<toolkit::Button>("&Quit");
    btn->on_click = [window] { window->close(); };
    root->add_widget(std::move(btn));

    window->set_root(std::move(root));
    window->show();
    return app.run();
}
```
