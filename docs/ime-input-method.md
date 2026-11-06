# IME / Input Method Support

Scoping doc for `todo.md` → Platform → item 5. What cross-platform input
method support actually requires, what the tree has today, and what has to be
built. Design only — no code written yet.

Status markers: `[exists]` already in the tree and reusable as-is, `[gap]`
exists but doesn't do enough, `[new]` not implemented anywhere.

## 1. What the problem actually is

Today a keystroke becomes text through exactly one path: the platform backend
fills `KeyEvent::text` (`include/toolkit/events.hpp:83`) and `LineInput`/
`TextEdit` insert it (`src/toolkit/line_input.cpp:854-880`). That model assumes
**one key press produces zero or one final character**.

Input methods break that assumption in three ways:

1. **Many keys → one character.** Typing `n` `i` `h` `a` `o` in Pinyin produces
   one Chinese character, chosen from a candidate list. The individual key
   presses must never reach the widget.
2. **Uncommitted text exists.** Between the first key press and the final
   choice, there is *preedit* (a.k.a. composition, marked text): text that is
   visible, sits at the caret, but is not yet part of the widget's value. It is
   not in `text_`, is not undoable, and must not fire `on_change`.
3. **The IME needs to know where the caret is on screen**, in screen
   coordinates, so it can park its candidate window next to it rather than in
   the corner of the display.

None of these three are expressible in the current `KeyEvent`-only API. This is
not a "make CJK work slightly better" task — for Japanese, Chinese, Korean,
Vietnamese, and Indic input, **there is no path at all** on most of our
backends.

## 2. Current state, per backend

### 2.1 Windows — `[gap]`, accidentally half-working

`src/toolkit/platform/win32/win32_platform.cpp:553` handles `WM_CHAR` only.
`TranslateMessage` is called (`:745`, `:770`) and everything unhandled falls
through to `DefWindowProcW`, which means **the default IME window is active**:
a Japanese IME does render its own candidate popup and does deliver the final
string as `WM_CHAR`. So Windows is the one platform where CJK input more or
less works today.

What is wrong with it:

- The candidate window is positioned wherever Windows guesses (typically the
  bottom-left of the client area), not at the caret, because we never call
  `ImmSetCompositionWindow`/`ImmSetCandidateWindow`.
- There is no inline preedit — composition is drawn in the IME's own floating
  window, over our content.
- The IME stays enabled over buttons, sliders, and password fields alike.
- **Surrogate pairs are corrupted.** `:554-559` takes a single `wchar_t` from
  `wParam` and runs it through `WideCharToMultiByte` on its own. `WM_CHAR`
  delivers an astral character (emoji, CJK Ext-B) as *two* messages carrying
  the high and low surrogate separately; converting a lone surrogate produces
  garbage. This is a standalone bug, worth fixing regardless of IME work.

### 2.2 macOS — `[gap]`, non-ASCII text is dropped entirely

`src/toolkit/platform/macos/macos_native_platform.mm:458` (and the OpenGL twin
at `macos_opengl_platform.mm:366`) builds a `KeyEvent` from a `keyCode` switch,
then at `:555-559`:

```objc
if (chars.length > 0 && ke.key == toolkit::Key::NoKey) {
    unichar c = [chars characterAtIndex:0];
    if (c >= 32 && c < 127) {
        ke.text = [chars UTF8String];
    }
}
```

That `c < 127` gate means **macOS accepts ASCII and nothing else**. Not just
CJK: accented Latin via dead keys (`é`), Hebrew, Greek, Cyrillic, and the
existing RTL support are all unreachable from the keyboard on macOS today.
`interpretKeyEvents:` is called for exactly one key — `keyCode == 48`, Tab
(`:462-464`) — to pick up `insertTab:`/`insertBacktab:`.

This is the most severe of the current defects and the cheapest to
partially fix.

### 2.3 X11 — `[gap]`, XIM wired up but not driven

An `XIM` is opened once (`src/toolkit/platform/x11/x11_platform.cpp:719`) and an
`XIC` is created per window (`:1145`) with
`XIMPreeditNothing | XIMStatusNothing` — "root window" style, where the IM
server draws preedit in its own window. `Xutf8LookupString` is then used for
lookup (`:471`). The bones are there; the driving is not:

- **`XFilterEvent` is never called.** This is the central XIM contract: every
  event must be offered to the input method first, and if it returns `True` the
  event was consumed by the IME and must be dropped. Without it, every
  composition keystroke *also* reaches the focused widget, so typing in an IME
  inserts the raw Latin keys into the text alongside whatever the IME does.
- **`XSetICFocus`/`XUnsetICFocus` are never called**, so the IM server does not
  know which of our windows holds focus.
- **`setlocale(LC_CTYPE, "")` is never called.** `XSetLocaleModifiers("")`
  (`:718`) runs against the default `"C"` locale, which is not what
  `XSupportsLocale()` wants; XIM behaviour here is at best undefined and at
  worst silently degraded.
- `XNSpotLocation` is never set, so over-the-spot positioning is impossible.
- `Xutf8LookupString` is also called on `KeyRelease` (`:517`). Xlib documents
  the `*LookupString` family as valid for `KeyPress` only; behaviour on
  `KeyRelease` is undefined. Should be `XLookupString` there.
- The `XIC` is created for every window unconditionally, including tooltips and
  menus that will never take text.

### 2.4 Wayland — `[new]`, nothing at all

`src/toolkit/platform/wayland/wl_platform.cpp:683-740` goes straight from
`xkb_state_key_get_utf8` to `KeyEvent`. No `zwp_text_input_manager_v3` is bound
in the registry handler (`:417-453`). Under Wayland an input method talks to the
*compositor*, and the compositor forwards to clients over the `text-input`
protocol — a client that does not bind it receives nothing. So on Wayland,
ibus/fcitx5 are completely inert for us. Since GNOME and KDE both default to
Wayland now, this is the backend with the largest real-world gap.

### 2.5 Summary

| | preedit | candidate positioning | IME on/off | non-ASCII typing |
|---|---|---|---|---|
| Win32 | IME's own window | wrong place | always on | broken for astral |
| macOS | none | n/a | always on | **entirely broken** |
| X11 | IM server window, unfiltered | wrong place | always on | works |
| Wayland | none | n/a | n/a | works (no IME) |

## 3. The abstraction

Everything below follows the existing platform-abstraction rule from
`AGENTS.md`: no platform-dependent code outside the backends, virtual interface
in `include/toolkit/platform.hpp`.

The design is a **two-way** contract, which is what makes this different from
every other input we handle. Events flow up (preedit, commit), but state also
has to flow *down* continuously (caret rect, surrounding text, enabled/disabled,
content hints). A one-way event pipe is not sufficient — IMEs will misbehave
without the downward half.

### 3.1 New event types — `include/toolkit/events.hpp` `[new]`

```cpp
// Text the input method is still composing. Visible at the caret, but not
// part of the widget's value: not in text(), not undoable, does not fire
// on_change. Replaced wholesale on each PreeditEvent, dropped on reset.
struct PreeditEvent {
    std::string text;
    // Caret position the IME wants, as a byte offset into `text`.
    size_t cursor = 0;
    // Byte range within `text` the IME wants emphasised -- the clause
    // currently being converted. start == end means no emphasis.
    size_t highlight_start = 0;
    size_t highlight_end = 0;
};

// The input method finished: `text` becomes real widget content, inserted at
// the caret, replacing any active preedit and the current selection.
struct CommitEvent {
    std::string text;
};

// Asks the widget to delete text around the caret before a commit -- used by
// predictive/reconverting IMEs. Byte counts, relative to the caret.
struct DeleteSurroundingEvent {
    size_t before_bytes = 0;
    size_t after_bytes = 0;
};
```

### 3.2 Input hints — `include/toolkit/events.hpp` `[new]`

Every platform has some way to say "this field is a password, do not compose
into it" and "this field is numeric, show a numeric keyboard/mode". One enum
covers all four backends:

```cpp
enum class InputPurpose { Normal, Digits, Number, Phone, Url, Email, Name,
                          Password, Pin, Terminal };

struct InputHints {
    InputPurpose purpose = InputPurpose::Normal;
    bool multiline = false;
    bool spellcheck = true;
    bool autocapitalize = false;
};
```

`InputPurpose::Password`/`Pin` must switch the IME **off**, not merely mask it —
otherwise preedit for a password ends up in the IME's own history/learning
database.

### 3.3 Widget additions — `include/toolkit/widget.hpp` `[gap]`

Alongside the existing `handle_key` (`:62`) and `on_focus`/`on_blur` (`:106`):

```cpp
virtual bool handle_preedit(PreeditEvent const &) { return false; }
virtual bool handle_commit(CommitEvent const &) { return false; }
virtual bool handle_delete_surrounding(DeleteSurroundingEvent const &) { return false; }

// True while an input method composition is in flight. Window checks this
// before routing navigation keys, because arrows/Enter/Escape belong to the
// IME while it is composing, not to the widget.
virtual bool has_preedit() const { return false; }

// Caret rectangle in widget-local coordinates, for anchoring the candidate
// window. nullopt means "this widget takes no text" -- the default, so only
// LineInput/TextEdit override it.
virtual std::optional<Rect> caret_rect() const { return std::nullopt; }

// What kind of text this widget wants. Only consulted when caret_rect()
// returns a value.
virtual InputHints input_hints() const { return {}; }

// Text around the caret plus the selection range within it, for IMEs that
// reconvert or predict from context. Optional: backends that don't need it
// never call this, and widgets may return nothing.
virtual std::optional<SurroundingText> surrounding_text() const { return std::nullopt; }
```

### 3.4 Platform additions — `include/toolkit/platform.hpp` `[gap]`

On `PlatformWindow`, all defaulted to no-ops so backends can land one at a
time (same pattern as `can_set_position()` at `:63`):

```cpp
// Turn the input method on for this window, with the hints of whatever
// widget just took focus. Called with false when focus leaves a text widget,
// when the window deactivates, and before showing a modal.
virtual void set_input_method_enabled(bool, InputHints const &) {}

// Caret rect in window-local LOGICAL coordinates (backends scale). Must be
// pushed whenever the caret moves, the widget scrolls, or the window moves --
// on macOS and Wayland a stale rect leaves the candidate window stranded.
virtual void set_input_method_cursor_rect(Rect const &) {}

virtual void set_input_method_surrounding_text(SurroundingText const &) {}

// Abandon any composition in flight without committing it. Focus loss,
// Escape, programmatic set_text(), and widget destruction.
virtual void reset_input_method() {}

// Whether this backend can actually drive an input method. Lets the widget
// layer skip the whole downward-state dance where it would be wasted.
virtual bool has_input_method() const { return false; }
```

### 3.5 Window plumbing — `include/toolkit/window.hpp` `[gap]`

Mirrors the existing `handle_key` (`:83`) routing:

```cpp
void handle_preedit(PreeditEvent const &event);
void handle_commit(CommitEvent const &event);
void handle_delete_surrounding(DeleteSurroundingEvent const &event);

// Called by text widgets after any caret move / scroll / text change so the
// platform's candidate window follows. Cheap and deduplicated -- widgets
// should not have to be careful about calling it too often.
void notify_caret_moved();
```

`Window::set_focused_widget` (`:142`) becomes the single choke point: on every
focus change, query the new widget's `caret_rect()`/`input_hints()` and call
`set_input_method_enabled` accordingly, and `reset_input_method()` on the
outgoing one. `handle_activate` (`:85`) must disable on deactivation.

Popups need care: `open_popup` already saves and clears focus (`window.cpp:604`)
and `close_popup` restores it (`:618`), so IME enable/disable follows for free
*if* it hangs off `set_focused_widget` and nowhere else.

## 4. Per-backend implementation

### 4.1 Win32 — IMM32

Link `imm32`. Handle in the window proc alongside the existing `WM_CHAR` case:

- `WM_IME_SETCONTEXT` — clear `ISC_SHOWUICOMPOSITIONWINDOW` from `lParam`
  before `DefWindowProc`, to suppress the default composition window once we
  draw preedit inline.
- `WM_IME_STARTCOMPOSITION` — return 0 (do not let `DefWindowProc` open its
  window).
- `WM_IME_COMPOSITION` — `ImmGetContext`, then `ImmGetCompositionStringW` with
  `GCS_COMPSTR` (preedit), `GCS_COMPATTR` (per-byte attributes → highlight
  range), `GCS_CURSORPOS` (caret) → `PreeditEvent`; `GCS_RESULTSTR` →
  `CommitEvent`. `ImmReleaseContext` after.
- `WM_IME_ENDCOMPOSITION` — empty `PreeditEvent`.
- `WM_IME_CHAR` — swallow; the result string already arrived via `GCS_RESULTSTR`.
- `set_input_method_cursor_rect` → `ImmSetCompositionWindow(CFS_POINT)` +
  `ImmSetCandidateWindow(CFS_EXCLUDE)` with the caret rect so the candidate
  list never covers the text being typed.
- `set_input_method_enabled(false)` → `ImmAssociateContextEx(hwnd, nullptr, 0)`;
  re-enable with `IACE_DEFAULT`. Cache the `HIMC` rather than destroying it.
- `WM_INPUTLANGCHANGE` — the layout changed; re-push hints and caret rect.

Fix the surrogate handling in `WM_CHAR` at the same time: buffer a high
surrogate and convert the pair together.

**TSF vs IMM32.** Text Services Framework is the modern API and is what
Windows' own emoji panel, handwriting, and voice input expect. IMM32 is a
compatibility layer, but it is still fully functional on Windows 11 and covers
every mainstream IME. Recommendation: **IMM32 now**, TSF only if handwriting or
reconversion is ever asked for — TSF is roughly an order of magnitude more code
(COM interfaces `ITfContextOwnerCompositionSink`, `ITfTextEditSink`,
`ITfDisplayAttributeProvider`, a document manager per window).

### 4.2 macOS — `NSTextInputClient`

The view already receives `keyDown:`. The change is to route it through
`interpretKeyEvents:` *always* and implement `NSTextInputClient` on the view:

| Method | Maps to |
|---|---|
| `insertText:replacementRange:` | `CommitEvent` |
| `setMarkedText:selectedRange:replacementRange:` | `PreeditEvent` (attributed string → highlight range) |
| `unmarkText` | empty `PreeditEvent` |
| `hasMarkedText` / `markedRange` | focused widget's `has_preedit()` |
| `selectedRange` / `attributedSubstringForProposedRange:` | `surrounding_text()` |
| `firstRectForCharacterRange:actualRange:` | `caret_rect()`, **converted to screen coordinates** (`convertRect:toView:nil` then `convertRectToScreen:`) |
| `doCommandBySelector:` | navigation/editing keys (`moveLeft:`, `deleteBackward:`, `insertNewline:`, `cancelOperation:`, …) → `KeyEvent` |

This is a real restructure of `keyDown:`: the `keyCode` switch at `:473-552`
gets largely replaced by `doCommandBySelector:`, because while a composition is
active the arrow keys belong to the IME (they move through candidates) and Cocoa
decides that, not us. A lower-risk staging: keep the `keyCode` switch for the
navigation keys, but call `interpretKeyEvents:` whenever
`hasMarkedText` is true or the key is not a recognised navigation key/shortcut.

Both `macos_native_platform.mm` and `macos_opengl_platform.mm` carry duplicate
copies of this view. Factoring the shared `NSTextInputClient` implementation
into one place (a category, or the existing `macos_application_base`) before
writing it twice is worth the detour.

**Independently of IME**, deleting the `c < 127` gate at `:557` and using
`[chars UTF8String]` unconditionally makes non-ASCII keyboards work on macOS.
That is a two-line fix with disproportionate value.

### 4.3 X11 — XIM/XIC, on-the-spot

Order matters here:

1. `setlocale(LC_CTYPE, "")` then `XSupportsLocale()` then
   `XSetLocaleModifiers("")`, all *before* `XOpenIM` (`:718-723`).
2. Call `XFilterEvent(&xev, None)` immediately after `XNextEvent` in the event
   loop and `continue` when it returns `True`. Non-negotiable.
3. `XSetICFocus`/`XUnsetICFocus` from `FocusIn`/`FocusOut`, and from
   `set_input_method_enabled`.
4. Create the `XIC` **lazily**, on first `set_input_method_enabled(true)`,
   instead of unconditionally at window creation (`:1145`).
5. Style: `XIMPreeditCallbacks | XIMStatusNothing` ("on-the-spot") to get inline
   preedit — requires `PreeditStartCallback`, `PreeditDrawCallback`,
   `PreeditCaretCallback`, `PreeditDoneCallback` in `XNPreeditAttributes`, with
   `PreeditDrawCallback` translating `XIMPreeditDrawCallbackStruct` into a
   `PreeditEvent`. Negotiate down: query `XNQueryInputStyle` and fall back to
   `XIMPreeditPosition` (over-the-spot: IM draws, we supply `XNSpotLocation`,
   needs an `XNFontSet`) and then to today's `XIMPreeditNothing` if the IM
   server offers nothing better.
6. `set_input_method_cursor_rect` → `XNSpotLocation` via `XVaCreateNestedList`
   + `XSetICValues`, scaled by the window scale factor.
7. Fix `KeyRelease` to use `XLookupString` (`:517`).

### 4.4 Wayland — `zwp_text_input_v3`

New protocol binding, following the existing wayland-scanner pattern in
`CMakeLists.txt:180-197`:

```cmake
set(TEXT_INPUT_XML ${WAYLAND_PROTOCOLS_DIR}/unstable/text-input/text-input-unstable-v3.xml)
```

Bind `zwp_text_input_manager_v3` in the registry handler (`wl_platform.cpp:417`),
then `zwp_text_input_manager_v3_get_text_input(mgr, seat)` once per seat.

Requests we send: `enable`/`disable`, `set_surrounding_text`,
`set_text_change_cause`, `set_content_type` (from `InputHints`),
`set_cursor_rectangle`, `commit`.

Events we receive: `enter`/`leave` (surface focus), `preedit_string`,
`commit_string`, `delete_surrounding_text`, `done`.

**The serial protocol is the part that gets implementations wrong.** Every
`commit` request bumps a client-side serial. Events arriving before a `done`
are a batch that only takes effect at `done`, and `done` carries the serial of
the `commit` it is answering — if that serial is not the latest one we sent, the
batch is stale and must be discarded. Apply order within a batch is fixed:
`delete_surrounding_text`, then `commit_string`, then `preedit_string`.

Version note: v3 is what mutter+ibus and kwin+fcitx5 implement. v1 and v2 are
legacy and not worth supporting. Compositors that expose no
`text_input_manager_v3` simply leave `has_input_method()` false and behave
exactly as today.

## 5. Widget side

### 5.1 `LineInput` — `[gap]`

New state: `preedit_`, `preedit_cursor_`, `preedit_highlight_{start,end}_`.

The critical detail is **`build_layout()` (`line_input.hpp:81`) must lay out
`text_` with `preedit_` spliced in at `cursor_pos_`, as one string.** Preedit
cannot be drawn as a separate pass appended after the text: it participates in
the same bidi resolution and the same shaping run (`text::TextLayout`,
`include/toolkit/text/text_layout.hpp`). Composing Hebrew or Arabic next to
Latin, or an IME preedit inside RTL text, produces wrong visual order otherwise.
This also means `caret_x()`, `index_from_x()`, and `ensure_cursor_visible()`
keep working unmodified — they operate on the composed string.

Everything else is bookkeeping:

- `handle_commit` routes into the **existing** insert path
  (`line_input.cpp:854-880`) so validation, `on_change`, and `UndoStack` all
  behave. One undo entry per commit — never per preedit update.
- `handle_preedit` only repaints; it must not touch `text_`, `on_change`, the
  validator, or the undo stack.
- `has_preedit()` gates `handle_key` — while composing, keys must not reach the
  widget's own handling.
- `set_text()`, `on_blur()`, and Escape all call the window's
  `reset_input_method()`.
- Password mode (`:42`) reports `InputPurpose::Password`, which disables the IME
  entirely.
- Every existing `cursor_pos_` mutation should end at `notify_caret_moved()`.

### 5.2 `TextEdit` — `[gap]`

Same shape, applied to the current line, plus `caret_rect()` has to account for
the widget's scroll offset (`ScrollableWidget`) — a caret scrolled out of view
should report clamped/absent rather than an off-widget rect.

### 5.3 Theme — `[gap]`

Preedit is conventionally drawn with an underline: thin for the whole
composition, thick (or a filled highlight) for the segment being converted.
`Theme::draw_line_input` (`theme.hpp:250`) and `draw_text_edit` (`:315`) would
need the preedit ranges. Per `AGENTS.md` ("painting new widgets should be done
by drawing widget primitives"), the lighter option is one new theme primitive —
`draw_preedit_underline(painter, rect, emphasised)` — that the text widgets call
themselves, so the four existing themes get a working default from
`BaseTheme` and only override it if they want to.

## 6. Testing

Real IMEs cannot be driven from a unit test, so split it:

**Synthetic.** Expose preedit/commit injection at the `Window` level (the
`handle_preedit`/`handle_commit` entry points are already the right seam) and
test widget behaviour headlessly: preedit does not fire `on_change`; commit
produces exactly one undo entry; preedit inside RTL text lays out in the right
visual order; Escape during composition restores the pre-composition state;
focus change mid-composition does not commit; password fields never receive
preedit. This covers the majority of the bugs that this feature will generate.

**Manual matrix.** Unavoidable for the backends themselves:

| Platform | IME |
|---|---|
| Windows 11 | MS-IME Japanese, MS Pinyin, emoji panel (Win+.) |
| macOS | Kotoeri Japanese, Pinyin, dead keys (`option`+`e`, `e`), Hebrew |
| Linux/X11 | ibus + mozc, fcitx5 |
| Linux/Wayland | GNOME/mutter + ibus, KDE/kwin + fcitx5 |

Cheap smoke test that catches most regressions without any IME installed: type
`é` with a dead key and a non-Latin layout on each platform.

## 7. Phasing

**Phase 0 — standalone bug fixes.** Independently valuable, no new API:
drop the macOS `c < 127` filter (§4.2); fix Win32 surrogate pairs (§4.1); add
`XFilterEvent` + `XSetICFocus` + `setlocale` on X11 (§4.3); fix `KeyRelease`
lookup. Small, and turns "non-ASCII typing is broken on macOS" into "works".

**Phase 1 — the abstraction.** `PreeditEvent`/`CommitEvent`/`InputHints`,
`Widget` virtuals, `PlatformWindow` virtuals, `Window` routing and the
`set_focused_widget` choke point. All backends still no-op, nothing regresses.

**Phase 2 — `LineInput` + `TextEdit` + theme.** Preedit inside `TextLayout`,
undo/validation semantics, underline primitive. Testable synthetically with zero
backend work.

**Phase 3 — Win32 IMM32 and macOS `NSTextInputClient`.** The two backends where
the OS does the most work for us, and between them most users.

**Phase 4 — X11 on-the-spot and Wayland `text-input-v3`.** The most protocol
work. Wayland is arguably higher priority than X11 given both major desktops
default to it, and X11 at least half-functions today.

**Phase 5 — optional.** Surrounding text and reconversion; TSF on Windows.

Phases 1 and 2 are the design commitment; 0 is free; 3–5 are independent and can
land per backend without blocking each other, which is the main reason to keep
every new `PlatformWindow` method defaulted to a no-op.

## 8. Explicitly out of scope

- On-screen/virtual keyboards (`zwp_input_method_v2`, Windows touch keyboard) —
  a different protocol and a different problem.
- Handwriting and voice input (would force TSF on Windows).
- Being an input method *host* — we are only ever a client.
- Grapheme clustering. `bidi::BidiLine` and `text::TextLayout` currently treat
  one Unicode scalar as one cluster (`text_layout.hpp:14-21`); preedit does not
  make that worse, but combining-mark-heavy scripts will still mis-measure until
  that documented limitation is lifted.
