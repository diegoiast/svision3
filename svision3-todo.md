# svision3 — issues found while porting Scintilla (scintilla-svision3)

Bugs/gaps found in svision3 itself while building `scintilla-svision3`
(a Scintilla text-editing widget on top of this toolkit), where the fix
has to happen here — not something the widget can work around on its own
side. See that repo's `AGENTS.md`-equivalent convention: "do not modify
platform code from widget code, prompt the developer instead."

Legend: [x] done [ ] pending

1. [ ] `toolkit::MouseEvent` (`include/toolkit/events.hpp`) has `shift`,
   `ctrl`, and `super` modifier flags, but no `alt` field at all.

   `toolkit::KeyEvent` already has `alt` — only `MouseEvent` is missing it.

   **Impact**: any widget that needs to distinguish Alt-held mouse
   gestures from plain ones cannot, no matter what it does on its own
   side — the information never arrives on the event at all. Concretely
   for `scintilla-svision3`: Scintilla's own mouse handling
   (`Editor::ButtonDownWithModifiers`/`ButtonMoveWithModifiers`, core,
   unmodified) checks `KeyMod::Alt` to start/extend a rectangular (block)
   selection on Alt+drag — the same gesture every mainstream text editor
   (VS Code, Sublime, Notepad++, ...) uses. `ScintillaSVision3::
   HandleMouseEvent`'s modifiers construction already forwards
   `shift`/`ctrl`/`super` correctly; Alt is the only one it structurally
   cannot forward, since `MouseEvent` doesn't carry it. Found live: a user
   tried Alt+drag expecting rectangular selection and it silently did
   nothing.

   (Keyboard-driven rectangular selection, Alt+Shift+Arrow, already works
   with zero changes needed — `KeyEvent::alt` already exists and core
   Scintilla's own default keymap already handles the rest. Only the
   mouse gesture is blocked.)

   **Suggested fix**: add `bool alt = false;` to `toolkit::MouseEvent`,
   populated the same way `shift`/`ctrl`/`super` already are on each
   backend (X11/Wayland/Win32/Cocoa) wherever those get set.
