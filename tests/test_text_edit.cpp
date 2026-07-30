#include "toolkit/text_edit.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace toolkit;

static void init_theme() {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
}

static KeyEvent key_press(Key k, bool shift = false, bool ctrl = false, bool alt = false) {
    KeyEvent e{};
    e.type = KeyEvent::Type::Press;
    e.key = k;
    e.shift = shift;
    e.ctrl = ctrl;
    e.alt = alt;
    return e;
}

static KeyEvent text_press(std::string text) {
    KeyEvent e{};
    e.type = KeyEvent::Type::Press;
    e.key = Key::NoKey;
    e.text = std::move(text);
    return e;
}

// ── specific regression: line 2 col 2 shift+left ─────────────────────────────

TEST_CASE("TextEdit: navigate to 2:2 then shift+left selects one char", "[textedit]") {
    init_theme();
    TextEdit te("aaaa\nbbbb\ncccc");
    te.set_rect({0, 0, 400, 300});

    te.handle_key(key_press(Key::Down));
    te.handle_key(key_press(Key::Down));
    te.handle_key(key_press(Key::Right));
    te.handle_key(key_press(Key::Right));

    // Confirm cursor is at line 2 col 2 by inserting a probe char and undoing.
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "aaaa\nbbbb\nccXcc");
    te.undo_cmd->execute();
    REQUIRE(te.text() == "aaaa\nbbbb\ncccc");

    // Shift+Left should extend selection by exactly one char.
    te.handle_key(key_press(Key::Left, true));

    // Delete the selection — should remove exactly the one 'c' at col 1.
    te.handle_key(key_press(Key::Delete));

    REQUIRE(te.text() == "aaaa\nbbbb\nccc");
}

TEST_CASE("TextEdit: End-key navigation then shift+left selects one char", "[textedit]") {
    init_theme();
    TextEdit te("aaaa\nbbbb\ncccc");
    te.set_rect({0, 0, 400, 300});

    // Navigate to end of line 2 via End key, then back 2 with Left.
    te.handle_key(key_press(Key::Down));
    te.handle_key(key_press(Key::Down));
    te.handle_key(key_press(Key::End));   // col 4 ("cccc" end)
    te.handle_key(key_press(Key::Left));  // col 3
    te.handle_key(key_press(Key::Left));  // col 2

    te.handle_key(key_press(Key::Left, true)); // Shift+Left: select col 1-2

    te.handle_key(key_press(Key::Delete));
    REQUIRE(te.text() == "aaaa\nbbbb\nccc");
}

TEST_CASE("TextEdit: shift+left from col 0 selects the preceding newline", "[textedit]") {
    init_theme();
    TextEdit te("aaaa\nbbbb\ncccc");
    te.set_rect({0, 0, 400, 300});

    // Go to line 2 col 0.  Shift+Left wraps to end of line 1: selection is the
    // newline between "bbbb" and "cccc".  Deleting it merges those two lines.
    te.handle_key(key_press(Key::Down));
    te.handle_key(key_press(Key::Down));
    te.handle_key(key_press(Key::Home));

    te.handle_key(key_press(Key::Left, true));

    te.handle_key(key_press(Key::Delete));
    REQUIRE(te.text() == "aaaa\nbbbbcccc");
}

// ── mouse drag selection ──────────────────────────────────────────────────────
//
// DummyRasterizer: char_width=8, line_height=16, gutter_width=32.
// pos_from_point snapping rules in tests:
//   x <= 32  →  col 0  (inside the gutter, click_x ≤ 0)
//   x >= 100 →  end-of-line  (click_x=68, past all 4×8=32px of text)
//   line     =  int(y / 16)
// Font size: DummyPlatformApplication::system_fonts() returns 14pt,
// Theme::init_fonts scales: floor(14 * 96/72) = 18px, so line_height = 18+2 = 20.
//   x <= 32  →  col 0  (inside the gutter, click_x ≤ 0)
//   x >= 100 →  end-of-line  (click_x=68, past all 4×8=32px of text)
//   line     =  int(y / 20)
//   y=5 → line 0,  y=25 → line 1,  y=45 → line 2

static MouseEvent mouse_event(MouseEvent::Type t, float x, float y, int button = 0,
                              int click_count = 1) {
    MouseEvent e{};
    e.type = t;
    e.position = {x, y};
    e.button = button;
    e.click_count = click_count;
    return e;
}

TEST_CASE("TextEdit drag: press then drag creates selection", "[textedit]") {
    init_theme();
    TextEdit te("aaaa\nbbbb\ncccc");
    te.set_rect({0, 0, 400, 300});

    // Press at line 0, col 0.  Drag to line 2, col 0.
    // anchor={0,0}, cursor={2,0}.  Deleting merges "" + "cccc" → "cccc".
    te.handle_mouse(mouse_event(MouseEvent::Type::Press, 10, 5));
    te.handle_mouse(mouse_event(MouseEvent::Type::Drag,  10, 45));

    te.handle_key(key_press(Key::Delete));
    REQUIRE(te.text() == "cccc");
}

TEST_CASE("TextEdit drag: dragging updates selection on every event", "[textedit]") {
    init_theme();
    TextEdit te("aaaa\nbbbb\ncccc");
    te.set_rect({0, 0, 400, 300});

    te.handle_mouse(mouse_event(MouseEvent::Type::Press, 10, 5));

    // First drag only reaches line 1 col 0 → selection {0,0}→{1,0}.
    te.handle_mouse(mouse_event(MouseEvent::Type::Drag, 10, 25));
    // Second drag extends to line 2 col 0 → selection grows to {0,0}→{2,0}.
    te.handle_mouse(mouse_event(MouseEvent::Type::Drag, 10, 45));

    te.handle_key(key_press(Key::Delete));
    REQUIRE(te.text() == "cccc");
}

TEST_CASE("TextEdit drag: dragging backwards (right-to-left) works", "[textedit]") {
    init_theme();
    TextEdit te("aaaa\nbbbb\ncccc");
    te.set_rect({0, 0, 400, 300});

    // Press at line 2, col 0.  Drag backwards to line 1, col 0.
    // anchor={2,0}, cursor={1,0}.  sel_start={1,0}, sel_end={2,0}.
    // Deleting: merged="" + "cccc"="cccc" replaces lines 1-2 → "aaaa\ncccc".
    te.handle_mouse(mouse_event(MouseEvent::Type::Press, 10, 45));
    te.handle_mouse(mouse_event(MouseEvent::Type::Drag,  10, 25));

    te.handle_key(key_press(Key::Delete));
    REQUIRE(te.text() == "aaaa\ncccc");
}

TEST_CASE("TextEdit drag: release ends drag and preserves selection", "[textedit]") {
    init_theme();
    TextEdit te("aaaa\nbbbb\ncccc");
    te.set_rect({0, 0, 400, 300});

    te.handle_mouse(mouse_event(MouseEvent::Type::Press,   10, 5));
    te.handle_mouse(mouse_event(MouseEvent::Type::Drag,    10, 45));
    te.handle_mouse(mouse_event(MouseEvent::Type::Release, 10, 45));

    // Selection must survive the release.
    te.handle_key(key_press(Key::Delete));
    REQUIRE(te.text() == "cccc");
}

TEST_CASE("TextEdit drag: second click clears previous selection", "[textedit]") {
    init_theme();
    TextEdit te("aaaa\nbbbb\ncccc");
    te.set_rect({0, 0, 400, 300});

    // First drag selects lines 0-2 (col 0 to col 0 of line 2).
    te.handle_mouse(mouse_event(MouseEvent::Type::Press,   10, 5));
    te.handle_mouse(mouse_event(MouseEvent::Type::Drag,    10, 45));
    te.handle_mouse(mouse_event(MouseEvent::Type::Release, 10, 45));

    // Second click at line 1, col 0 — clears selection.
    te.handle_mouse(mouse_event(MouseEvent::Type::Press,   10, 25));
    te.handle_mouse(mouse_event(MouseEvent::Type::Release, 10, 25));

    // Typing inserts at {1,0} with no selection.
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "aaaa\nXbbbb\ncccc");
}

// ── set_text / text() round-trip ─────────────────────────────────────────────

TEST_CASE("TextEdit set_text/text round-trip: plain", "[textedit]") {
    init_theme();
    TextEdit te;
    te.set_text("hello");
    REQUIRE(te.text() == "hello");
}

TEST_CASE("TextEdit set_text/text round-trip: multiline", "[textedit]") {
    init_theme();
    TextEdit te;
    te.set_text("line1\nline2\nline3");
    REQUIRE(te.text() == "line1\nline2\nline3");
}

TEST_CASE("TextEdit set_text/text round-trip: trailing newline", "[textedit]") {
    init_theme();
    TextEdit te;
    te.set_text("hello\n");
    REQUIRE(te.text() == "hello\n");
}

TEST_CASE("TextEdit set_text/text round-trip: two trailing newlines", "[textedit]") {
    init_theme();
    TextEdit te;
    te.set_text("hello\n\n");
    REQUIRE(te.text() == "hello\n\n");
}

TEST_CASE("TextEdit set_text/text round-trip: only newline", "[textedit]") {
    init_theme();
    TextEdit te;
    te.set_text("\n");
    REQUIRE(te.text() == "\n");
}

TEST_CASE("TextEdit set_text/text round-trip: empty string", "[textedit]") {
    init_theme();
    TextEdit te;
    te.set_text("");
    REQUIRE(te.text() == "");
}

TEST_CASE("TextEdit set_text idempotent: same text is no-op", "[textedit]") {
    init_theme();
    TextEdit te("hello");
    int changes = 0;
    te.on_change = [&] { changes++; };
    te.set_text("hello");
    REQUIRE(changes == 0);
}

// ── insert_text ──────────────────────────────────────────────────────────────

TEST_CASE("TextEdit insert_text appends to empty", "[textedit]") {
    init_theme();
    TextEdit te;
    te.set_rect({0, 0, 400, 300});
    te.handle_key(text_press("a"));
    REQUIRE(te.text() == "a");
}

TEST_CASE("TextEdit insert_text: typing multiple chars", "[textedit]") {
    init_theme();
    TextEdit te;
    te.set_rect({0, 0, 400, 300});
    te.handle_key(text_press("h"));
    te.handle_key(text_press("i"));
    REQUIRE(te.text() == "hi");
}

TEST_CASE("TextEdit insert_text: newline splits line", "[textedit]") {
    init_theme();
    TextEdit te("hello");
    te.set_rect({0, 0, 400, 300});
    // Move to col 3
    for (int i = 0; i < 3; i++) {
        te.handle_key(key_press(Key::Right));
    }
    te.handle_key(key_press(Key::Enter));
    REQUIRE(te.text() == "hel\nlo");
}

TEST_CASE("TextEdit insert_text replaces selection", "[textedit]") {
    init_theme();
    TextEdit te("hello world");
    te.set_rect({0, 0, 400, 300});
    // Select all then type
    te.handle_key(key_press(Key::Home));
    for (int i = 0; i < 5; i++) {
        te.handle_key(key_press(Key::Right, true));
    }
    te.handle_key(text_press("bye"));
    REQUIRE(te.text() == "bye world");
}

// ── cursor movement ───────────────────────────────────────────────────────────

TEST_CASE("TextEdit cursor Right moves forward", "[textedit]") {
    init_theme();
    TextEdit te("abc");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::Right));
    // Type at new position — should insert after 'a'
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "aXbc");
}

TEST_CASE("TextEdit cursor Left at col 0 moves to end of previous line", "[textedit]") {
    init_theme();
    TextEdit te("ab\ncd");
    te.set_rect({0, 0, 400, 300});
    // Move to line 1 col 0
    te.handle_key(key_press(Key::Down));
    te.handle_key(key_press(Key::Home));
    // Left should go to end of line 0
    te.handle_key(key_press(Key::Left));
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "abX\ncd");
}

TEST_CASE("TextEdit cursor Right at end of line moves to next line", "[textedit]") {
    init_theme();
    TextEdit te("ab\ncd");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::End));
    te.handle_key(key_press(Key::Right));
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "ab\nXcd");
}

TEST_CASE("TextEdit Home moves to start of line", "[textedit]") {
    init_theme();
    TextEdit te("hello");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::End));
    te.handle_key(key_press(Key::Home));
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "Xhello");
}

TEST_CASE("TextEdit End moves to end of line", "[textedit]") {
    init_theme();
    TextEdit te("hello");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::End));
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "helloX");
}

TEST_CASE("TextEdit Ctrl+Home moves to start of document", "[textedit]") {
    init_theme();
    TextEdit te("aaa\nbbb\nccc");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::Down));
    te.handle_key(key_press(Key::Down));
    te.handle_key(key_press(Key::End));
    te.handle_key(key_press(Key::Home, false, true)); // Ctrl+Home
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "Xaaa\nbbb\nccc");
}

TEST_CASE("TextEdit Ctrl+End moves to end of document", "[textedit]") {
    init_theme();
    TextEdit te("aaa\nbbb\nccc");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::End, false, true)); // Ctrl+End
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "aaa\nbbb\ncccX");
}

TEST_CASE("TextEdit Ctrl+Shift+End selects from cursor to end of document", "[textedit]") {
    init_theme();
    TextEdit te("aaa\nbbb\nccc");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::End, true, true)); // Ctrl+Shift+End
    te.handle_key(key_press(Key::Delete));
    REQUIRE(te.text() == "");
}

TEST_CASE("TextEdit Ctrl+Shift+Home selects from cursor back to start of document",
          "[textedit]") {
    init_theme();
    TextEdit te("aaa\nbbb\nccc");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::End, false, true));  // Ctrl+End: cursor at doc end, no selection
    te.handle_key(key_press(Key::Home, true, true));  // Ctrl+Shift+Home: select back to doc start
    te.handle_key(key_press(Key::Delete));
    REQUIRE(te.text() == "");
}

TEST_CASE("TextEdit Up clamps col to shorter line", "[textedit]") {
    init_theme();
    TextEdit te("hi\nhello world");
    te.set_rect({0, 0, 400, 300});
    // Move to line 1, end
    te.handle_key(key_press(Key::Down));
    te.handle_key(key_press(Key::End));
    // Up should land at end of "hi" (col 2, not 11)
    te.handle_key(key_press(Key::Up));
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "hiX\nhello world");
}

// Position's line/col are size_t; PageUp computes cursor_.line - page_lines,
// which must clamp to 0 via a guarded subtraction rather than going negative,
// or it would underflow to a huge index and crash on lines_[new_line].
TEST_CASE("TextEdit PageUp near the top of the document clamps to line 0", "[textedit]") {
    init_theme();
    TextEdit te("aaa\nbbb\nccc");
    te.set_rect({0, 0, 400, 3000}); // tall viewport => page_lines exceeds line count
    te.handle_key(key_press(Key::Down)); // cursor at line 1
    te.handle_key(key_press(Key::PageUp));
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "Xaaa\nbbb\nccc");
}

TEST_CASE("TextEdit Down clamps col to shorter line", "[textedit]") {
    init_theme();
    TextEdit te("hello world\nhi");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::End));
    te.handle_key(key_press(Key::Down));
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "hello world\nhiX");
}

// ── Backspace / Delete ────────────────────────────────────────────────────────

TEST_CASE("TextEdit Backspace deletes previous char", "[textedit]") {
    init_theme();
    TextEdit te("hello");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::End));
    te.handle_key(key_press(Key::Backspace));
    REQUIRE(te.text() == "hell");
}

TEST_CASE("TextEdit Backspace at col 0 merges with previous line", "[textedit]") {
    init_theme();
    TextEdit te("ab\ncd");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::Down));
    te.handle_key(key_press(Key::Home));
    te.handle_key(key_press(Key::Backspace));
    REQUIRE(te.text() == "abcd");
}

TEST_CASE("TextEdit Backspace at start of doc is no-op", "[textedit]") {
    init_theme();
    TextEdit te("a");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::Home));
    te.handle_key(key_press(Key::Backspace));
    REQUIRE(te.text() == "a");
}

TEST_CASE("TextEdit Delete deletes current char", "[textedit]") {
    init_theme();
    TextEdit te("hello");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::Delete));
    REQUIRE(te.text() == "ello");
}

TEST_CASE("TextEdit Delete at end of line merges next line", "[textedit]") {
    init_theme();
    TextEdit te("ab\ncd");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::End));
    te.handle_key(key_press(Key::Delete));
    REQUIRE(te.text() == "abcd");
}

TEST_CASE("TextEdit Delete at end of doc is no-op", "[textedit]") {
    init_theme();
    TextEdit te("a");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::End));
    te.handle_key(key_press(Key::Delete));
    REQUIRE(te.text() == "a");
}

// ── selection ────────────────────────────────────────────────────────────────

TEST_CASE("TextEdit Shift+Right creates selection", "[textedit]") {
    init_theme();
    TextEdit te("aaa bbb ccc");
    te.set_rect({0, 0, 400, 300});
    for (int i = 0; i < 4; i++) {
        te.handle_key(key_press(Key::Right, true));
    }
    te.handle_key(key_press(Key::Delete));
    REQUIRE(te.text() == "bbb ccc");
}

TEST_CASE("TextEdit selection and deletion", "[textedit]") {
    init_theme();
    TextEdit te("aaa bbb ccc");
    te.set_rect({0, 0, 400, 300});
    for (int i = 0; i < 3; i++) {
        te.handle_key(key_press(Key::Right));
    }
    for (int i = 0; i < 4; i++) {
        te.handle_key(key_press(Key::Right, true));
    }
    te.handle_key(key_press(Key::Delete));
    REQUIRE(te.text() == "aaa ccc");
}

TEST_CASE("TextEdit Shift+Down extends selection across lines", "[textedit]") {
    init_theme();
    TextEdit te("line1\nline2\nline3");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::Down, true));
    te.handle_key(key_press(Key::Delete));
    REQUIRE(te.text() == "line2\nline3");
}

TEST_CASE("TextEdit deselect on non-shift Left keeps cursor at sel_start", "[textedit]") {
    init_theme();
    TextEdit te("abcde");
    te.set_rect({0, 0, 400, 300});
    for (int i = 0; i < 3; i++) {
        te.handle_key(key_press(Key::Right, true));
    }
    // Press Left without shift — cursor collapses to selection start (col 0)
    te.handle_key(key_press(Key::Left));
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "Xabcde");
}

TEST_CASE("TextEdit deselect on non-shift Right keeps cursor at sel_end", "[textedit]") {
    init_theme();
    TextEdit te("abcde");
    te.set_rect({0, 0, 400, 300});
    for (int i = 0; i < 3; i++) {
        te.handle_key(key_press(Key::Right, true));
    }
    // Press Right without shift — cursor collapses to selection end (col 3)
    te.handle_key(key_press(Key::Right));
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "abcXde");
}

// ── word movement ─────────────────────────────────────────────────────────────

TEST_CASE("TextEdit Alt+Right moves to start of next word", "[textedit]") {
    init_theme();
    TextEdit te("hello world foo");
    te.set_rect({0, 0, 400, 300});
    // move_word_right skips current word then trailing spaces, landing at start of next word
    te.handle_key(key_press(Key::Right, false, false, true));
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "hello Xworld foo");
}

TEST_CASE("TextEdit Alt+Left moves to previous word", "[textedit]") {
    init_theme();
    TextEdit te("hello world foo");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::End));
    te.handle_key(key_press(Key::Left, false, false, true));
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "hello world Xfoo");
}

TEST_CASE("TextEdit Alt+Left at start of line wraps to previous line", "[textedit]") {
    init_theme();
    TextEdit te("first\nsecond");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::Down));
    te.handle_key(key_press(Key::Home));
    // move_word_left wraps to end of previous line then skips the whole word,
    // landing at the start of that word (col 0 of line 0)
    te.handle_key(key_press(Key::Left, false, false, true));
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "Xfirst\nsecond");
}

// ── undo / redo ──────────────────────────────────────────────────────────────

TEST_CASE("TextEdit undo single insert", "[textedit]") {
    init_theme();
    TextEdit te("hi");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::End));
    te.handle_key(text_press("!"));
    REQUIRE(te.text() == "hi!");
    te.handle_key(key_press(Key::Left, false, false, false)); // just move
    // Undo via Std+Z — simulate directly
    te.undo_cmd->execute();
    REQUIRE(te.text() == "hi");
}

TEST_CASE("TextEdit redo after undo", "[textedit]") {
    init_theme();
    TextEdit te("hi");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::End));
    te.handle_key(text_press("!"));
    te.undo_cmd->execute();
    REQUIRE(te.text() == "hi");
    te.redo_cmd->execute();
    REQUIRE(te.text() == "hi!");
}

TEST_CASE("TextEdit undo delete selection", "[textedit]") {
    init_theme();
    TextEdit te("hello world");
    te.set_rect({0, 0, 400, 300});
    for (int i = 0; i < 5; i++) {
        te.handle_key(key_press(Key::Right, true));
    }
    te.handle_key(key_press(Key::Delete));
    REQUIRE(te.text() == " world");
    te.undo_cmd->execute();
    REQUIRE(te.text() == "hello world");
}

TEST_CASE("TextEdit undo backspace", "[textedit]") {
    init_theme();
    TextEdit te("hello");
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::End));
    te.handle_key(key_press(Key::Backspace));
    REQUIRE(te.text() == "hell");
    te.undo_cmd->execute();
    REQUIRE(te.text() == "hello");
}

// ── Tab ──────────────────────────────────────────────────────────────────────

TEST_CASE("TextEdit Tab inserts four spaces", "[textedit]") {
    init_theme();
    TextEdit te;
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::Tab));
    REQUIRE(te.text() == "    ");
}

// col is size_t; unindent removes more leading spaces than the cursor's own
// column when the cursor sits inside the indent being stripped, so the
// col -= removed subtraction must clamp to 0 via a guard rather than
// underflowing.
TEST_CASE("TextEdit Shift+Tab unindent clamps cursor col instead of underflowing",
          "[textedit]") {
    init_theme();
    TextEdit te("  hi"); // two leading spaces
    te.set_rect({0, 0, 400, 300});
    te.handle_key(key_press(Key::Right)); // cursor at col 1, inside the indent
    te.handle_key(key_press(Key::Tab, true)); // Shift+Tab: unindent (default width 4)
    te.handle_key(text_press("X"));
    REQUIRE(te.text() == "Xhi");
}

// ── on_change callback ───────────────────────────────────────────────────────

TEST_CASE("TextEdit on_change fires on insert", "[textedit]") {
    init_theme();
    TextEdit te;
    te.set_rect({0, 0, 400, 300});
    int calls = 0;
    te.on_change = [&] { calls++; };
    te.handle_key(text_press("a"));
    REQUIRE(calls == 1);
}

TEST_CASE("TextEdit on_change fires on backspace", "[textedit]") {
    init_theme();
    TextEdit te("a");
    te.set_rect({0, 0, 400, 300});
    int calls = 0;
    te.on_change = [&] { calls++; };
    te.handle_key(key_press(Key::End));
    te.handle_key(key_press(Key::Backspace));
    REQUIRE(calls == 1);
}

TEST_CASE("TextEdit on_change fires on delete", "[textedit]") {
    init_theme();
    TextEdit te("a");
    te.set_rect({0, 0, 400, 300});
    int calls = 0;
    te.on_change = [&] { calls++; };
    te.handle_key(key_press(Key::Delete));
    REQUIRE(calls == 1);
}

TEST_CASE("TextEdit on_change does not fire on cursor movement", "[textedit]") {
    init_theme();
    TextEdit te("hello");
    te.set_rect({0, 0, 400, 300});
    int calls = 0;
    te.on_change = [&] { calls++; };
    te.handle_key(key_press(Key::Right));
    te.handle_key(key_press(Key::Left));
    te.handle_key(key_press(Key::Home));
    te.handle_key(key_press(Key::End));
    REQUIRE(calls == 0);
}

// ── mouse: relative coordinates ──────────────────────────────────────────────

TEST_CASE("TextEdit relative coordinates", "[textedit]") {
    init_theme();
    TextEdit te("Line 1\nLine 2");
    te.set_rect({100, 100, 200, 200});

    MouseEvent e{};
    e.type = MouseEvent::Type::Press;

    e.position = {10, 10};
    REQUIRE(te.handle_mouse(e) == true);

    e.position = {10, 190};
    REQUIRE(te.handle_mouse(e) == false);

    e.position = {110, 110};
    REQUIRE(te.handle_mouse(e) == false);
}

// ── serialization ─────────────────────────────────────────────────────────────

TEST_CASE("TextEdit to_json / from_json round-trip", "[textedit]") {
    init_theme();
    TextEdit te("hello\nworld");
    auto j = te.to_json();
    REQUIRE(j["text"] == "hello\nworld");

    TextEdit te2;
    te2.from_json(j);
    REQUIRE(te2.text() == "hello\nworld");
}

