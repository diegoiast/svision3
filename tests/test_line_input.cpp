#include "toolkit/line_input.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/text_rasterizer.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/theme_factory.hpp"
#ifdef _WIN32
#include "toolkit/painters/win32_painter.hpp"
#else
#include "toolkit/painters/cairo_painter.hpp"
#endif
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <nlohmann/json.hpp>

using namespace toolkit;

// Platform-appropriate BiDi test context.
struct BiDiTestContext {
    std::unique_ptr<TextRasterizer> rasterizer;
    MockPainter painter;

    BiDiTestContext() : rasterizer(create_rasterizer()), painter(rasterizer.get()) {}

  private:
    static auto create_rasterizer() -> std::unique_ptr<TextRasterizer> {
#ifdef _WIN32
        return std::make_unique<Win32TextRasterizer>();
#else
        return std::make_unique<CairoTextRasterizer>();
#endif
    }
};

TEST_CASE("LineInput hit tests with read-only and password", "[lineinput]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    LineInput li("Placeholder");
    li.set_text("some text");
    li.set_password_mode(true);
    li.set_read_only(true);
    li.set_rect({0, 0, 200, 30});

    // In MacOS theme, padding.right is usually 4. sz is ~16.
    // Clear button position would be 200 - 4 - 16 = 180 to 196.
    // Peek button position shifted would be 180 - 16 - 4 = 160 to 176.

    MouseEvent e{};
    e.type = MouseEvent::Type::Move;

    // Test 1: Hit at 190 (where clear would be, but since it's read-only, peek should move there)
    e.position = {190, 15};
    li.handle_mouse(e);
    // This is expected to FAIL if the bug exists (peek stays at 160)
    CHECK(li.cursor() == CursorShape::Hand);

    // Test 2: Hit at 165 (where peek would be if shifted)
    e.position = {165, 15};
    li.handle_mouse(e);
    // This is expected to FAIL if the bug exists (it will still be Hand at 165)
    CHECK(li.cursor() == CursorShape::IBeam);
}

TEST_CASE("LineInput relative coordinates", "[lineinput]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    LineInput li("Placeholder");
    li.set_rect({100, 100, 200, 30});

    MouseEvent e{};
    e.type = MouseEvent::Type::Press;

    // Relative position (10, 10) should succeed
    e.position = {10, 10};
    REQUIRE(li.handle_mouse(e) == true);
}

TEST_CASE("LineInput clear button hover and click", "[lineinput]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    LineInput li("Placeholder");
    li.set_text("Some text");
    li.set_rect({0, 0, 200, 30});

    MouseEvent e{};
    e.type = MouseEvent::Type::Move;
    e.position = {190, 15};

    REQUIRE(li.handle_mouse(e) == true);
    REQUIRE(li.cursor() == CursorShape::Hand);

    e.type = MouseEvent::Type::Press;
    REQUIRE(li.handle_mouse(e) == true);

    e.type = MouseEvent::Type::Release;
    REQUIRE(li.handle_mouse(e) == true);

    REQUIRE(li.text().empty());
}

TEST_CASE("LineInput cursor keys", "[lineinput]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    MockPainter painter(new DummyRasterizer);
    LineInput li("Placeholder");
    li.set_rect({0, 0, 200, 30});

    auto &theme = Theme::current();
    auto P = theme.style.lineInput.padding.left;
    auto W = li.measure_text("w", theme.palette.fonts.size, FontFamily::Monospace).width;
    auto cursor_logical = li.cursor_codepoint();
    auto cursor_physical = li.cursor_physical_x(painter);
    REQUIRE(cursor_logical == 0);
    REQUIRE(cursor_physical >= P + 0.0f);

    li.handle_key({.type = KeyEvent::Type::Press, .text = "a"});
    cursor_logical = li.cursor_codepoint();
    cursor_physical = li.cursor_physical_x(painter);
    REQUIRE(cursor_logical == 1);
    REQUIRE(cursor_physical == P + 1 * W);

    li.handle_key({.type = KeyEvent::Type::Press, .text = "b"});
    cursor_logical = li.cursor_codepoint();
    cursor_physical = li.cursor_physical_x(painter);
    REQUIRE(cursor_logical == 2);
    REQUIRE(cursor_physical == P + 2 * W);

    li.handle_key({.type = KeyEvent::Type::Press, .text = "c"});
    cursor_logical = li.cursor_codepoint();
    cursor_physical = li.cursor_physical_x(painter);
    REQUIRE(cursor_logical == 3);
    REQUIRE(cursor_physical == P + 3 * W);

    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    cursor_logical = li.cursor_codepoint();
    cursor_physical = li.cursor_physical_x(painter);
    REQUIRE(cursor_logical == 4);
    REQUIRE(cursor_physical == P + 4 * W);

    li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Home});
    cursor_logical = li.cursor_codepoint();
    cursor_physical = li.cursor_physical_x(painter);
    REQUIRE(cursor_logical == 0);
    REQUIRE(cursor_physical == P + 0 * W);

    li.handle_key({.type = KeyEvent::Type::Press, .key = Key::End});
    cursor_logical = li.cursor_codepoint();
    cursor_physical = li.cursor_physical_x(painter);
    REQUIRE(cursor_logical == 4);
    REQUIRE(cursor_physical == P + 4 * W);
}

TEST_CASE("Serialize/de-serialize", "[lineinput]") {
    Theme::set_current(ThemeFactory::create(ThemeStyle::MacOS, ColorScheme::Light));
    LineInput li("placeholder");
    li.set_text("Some text");

    {
        li.set_password_mode(true);
        li.set_read_only(false);
        auto j = li.to_json();
        REQUIRE(j["text"] == "Some text");
        REQUIRE(j["placeholder"] == "placeholder");
        REQUIRE(j["read_only"] == false);
        REQUIRE(j["is_password"] == true);
    }
    {
        li.set_password_mode(false);
        li.set_read_only(true);
        auto j = li.to_json();
        REQUIRE(j["read_only"] == true);
        REQUIRE(j["is_password"] == false);
    }
}

TEST_CASE("BiDi cursor positions", "[lineinput][bidi]") {
    BiDiTestContext ctx;
    auto &painter = ctx.painter;
    LineInput li("Placeholder");
    li.set_rect({0, 0, 200, 30});
    li.set_font_family(FontFamily::Monospace);

    auto P = Theme::current().style.lineInput.padding.left;

    li.handle_key({.type = KeyEvent::Type::Press, .text = "a"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "b"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "c"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});

    REQUIRE(li.text() == "abc ");
    auto phys_before = li.cursor_physical_x(painter);
    REQUIRE(phys_before > P);

    // Typing RTL after LTR: cursor stays at the same physical position
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x90"});
    REQUIRE(li.cursor_codepoint() == 5);
    auto phys_after = li.cursor_physical_x(painter);
    REQUIRE(phys_after == phys_before);
}

TEST_CASE("BiDi cursor right at end", "[lineinput][bidi]") {
    BiDiTestContext ctx;
    auto &painter = ctx.painter;
    LineInput li("");
    li.set_rect({0, 0, 200, 30});
    li.set_font_family(FontFamily::Monospace);

    li.handle_key({.type = KeyEvent::Type::Press, .text = "a"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "b"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "c"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x90"}); // aleph
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x91"}); // bet
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x92"}); // gimel

    REQUIRE(li.text() == "abc אבג");
    REQUIRE(li.cursor_codepoint() == 7);

    li.handle_key({.type = KeyEvent::Type::Press, .key = Key::End});
    REQUIRE_FALSE(li.text().empty());
    REQUIRE(li.cursor_codepoint() == 7);

    li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Right});
    REQUIRE_FALSE(li.text().empty());
    INFO("text after Right at end: '" << li.text() << "'");
    REQUIRE(li.cursor_codepoint() == 7);

    auto palette = Theme::current().palette;
    auto l = painter.text_cursor_positions(li.text(), palette.fonts.size, li.font_family());

    // Indices below are UTF-8 codepoint boundaries for "abc אבג":
    // a=0 b=1 c=2 sp=3 א=4-5 ב=6-7 ג=8-9, end=10.
    // Boundaries: 0,1,2,3,4,6,8,10 (5,7,9 are mid-codepoint, not valid here).
    //
    // The RTL run is mirrored within its own [run_start_x, run_end_x] span:
    // the first logical RTL char (א) renders at the run's right edge, the
    // last (ג) at its left edge. So crossing into the run jumps the x
    // position up to the right edge, then it decreases through the run,
    // ending back at the run's start x (since the run is at the end of the
    // text, nothing follows it).

    // |a bc אבג -> a| bc אבג
    auto la = l[1];
    auto lb = l[0];
    REQUIRE(la > lb);

    // a|b c אבג -> ab|c אבג
    la = l[2];
    lb = l[1];
    REQUIRE(la > lb);

    // ab|c אבג -> abc| אבג
    la = l[3];
    lb = l[2];
    REQUIRE(la > lb);

    // abc| אבג -> abc |אבג
    la = l[4];
    lb = l[3];
    REQUIRE(la > lb);

    // abc |אבג -> abc א|בג (entering the run: jumps to the run's right edge)
    la = l[6];
    lb = l[4];
    REQUIRE(la > lb);

    // abc א|בג -> abc אב|ג (still inside the run: decreasing)
    la = l[8];
    lb = l[6];
    REQUIRE(la < lb);

    // abc אב|ג -> abc אבג| (leaving the run: decreasing, back to the run's start x)
    la = l[10];
    lb = l[8];
    REQUIRE(la < lb);
}

TEST_CASE("BiDi leading RTL right from end", "[lineinput][bidi]") {
    BiDiTestContext ctx;
    auto &painter = ctx.painter;
    LineInput li("");
    li.set_rect({0, 0, 200, 30});
    li.set_font_family(FontFamily::Monospace);

    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x90"}); // aleph
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x91"}); // bet
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x92"}); // gimel
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "t"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "e"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "s"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "t"});

    REQUIRE(li.text() == "אבג test");
    REQUIRE(li.cursor_codepoint() == 8);

    // Press End (stays at end), then Left moves backward in logical order.
    // The trailing LTR section "test" starts at the far LEFT of the text,
    // so pressing Left within it moves LEFTWARD (cx decreases).
    li.handle_key({.type = KeyEvent::Type::Press, .key = Key::End});
    REQUIRE(li.cursor_codepoint() == 8);

    // During the LTR section: Left in LTR key mode moves backward (lower byte).
    // With pure RTL flow positions, lower byte = less negative = x increases (rightward).
    auto x_prev = li.cursor_physical_x(painter);
    auto prev_cp = li.cursor_codepoint();
    for (auto i = 0; i < 3; i++) {
        li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Left});
        REQUIRE_FALSE(li.text().empty());
        INFO("i=" << i << " cursor_codepoint=" << li.cursor_codepoint());
        REQUIRE(li.cursor_codepoint() < prev_cp);
        auto x = li.cursor_physical_x(painter);
        REQUIRE(x > x_prev); // rightward in LTR section (going backward in RTL flow)
        x_prev = x;
        prev_cp = li.cursor_codepoint();
    }
}

TEST_CASE("BiDi RTL with digits stays in RTL run", "[lineinput][bidi]") {
    // Digits (0-9) are weak characters and should stay in the RTL run,
    // not trigger an LTR section. Cursor at end appears at the left
    // edge (past all characters), not at the RTL/LTR boundary.
    BiDiTestContext ctx;
    auto &painter = ctx.painter;
    LineInput li("");
    li.set_rect({0, 0, 500, 30});
    li.set_font_family(FontFamily::Monospace);

    // Type "אבג 123" (Hebrew, space, then digits)
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x90"}); // א
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x91"}); // ב
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x92"}); // ג
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "1"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "2"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "3"});
    REQUIRE(li.text() == "אבג 123");

    auto positions = painter.text_cursor_positions(li.text(), 14.0f, FontFamily::Monospace);
    REQUIRE(positions.size() == 11);

    // All positions should be decreasing (pure RTL, no LTR section)
    REQUIRE(positions[0] > positions[2]);
    REQUIRE(positions[6] > positions[8]); // space → digit

    // End position should be at the leftmost edge (past all chars)
    auto x_end_val = positions[10];
    auto x_3_val = positions[9];  // after '3'
    REQUIRE(x_end_val < x_3_val); // end is further left than '3'

    // Right-march from Home through all chars to end
    li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Home});
    REQUIRE(li.cursor_position() == 0);

    for (auto i = 0; i < 7; i++) {
        auto prev = li.cursor_physical_x(painter);
        li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Right});
        REQUIRE(li.cursor_position() > 0);
        // In pure RTL, each Right press moves leftward (decreases cx)
        auto cur = li.cursor_physical_x(painter);
        REQUIRE(cur < prev);
    }
    // Now at end (byte 10)
    REQUIRE(li.cursor_position() == 10);

    // Cursor at end should be at leftmost — past all characters.
    auto x_end_phys = li.cursor_physical_x(painter);
    // Already verified each step decreases cx via cur < prev above;
    // just log the value for diagnostics.
    INFO("end_phys=" << x_end_phys);
    // Cursor at end of RTL text is left of the content area (negative).
    REQUIRE(x_end_phys < 0);
}

TEST_CASE("BiDi RTL leading with LTR suffix positioning", "[lineinput][bidi]") {
    BiDiTestContext ctx;
    auto &painter = ctx.painter;
    LineInput li("");
    li.set_rect({0, 0, 500, 30});
    li.set_font_family(FontFamily::Monospace);

    // Type "שלום ABCD" (Hebrew then space then Latin)
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\xa9"}); // ש
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x9c"}); // ל
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x95"}); // ו
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x9d"}); // ם
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "A"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "B"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "C"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "D"});
    REQUIRE(li.text() == "שלום ABCD");

    // Positions indexed by byte offset
    auto positions = painter.text_cursor_positions(li.text(), 14.0f, FontFamily::Monospace);
    REQUIRE(positions.size() == 14);

    // RTL run (bytes 0-8): decreasing positions (right-to-left)
    REQUIRE(positions[0] > positions[2]);

    // LTR run (bytes 10-13): decreasing positions (monotonic RTL flow)
    REQUIRE(positions[10] > positions[11]); // after 'A' → after 'B': more negative
    REQUIRE(positions[11] > positions[12]); // after 'B' → after 'C'
    REQUIRE(positions[12] > positions[13]); // after 'C' → after 'D'

    // RTL end → LTR start: both in monotonic RTL flow
    REQUIRE(positions[9] > positions[10]);

    // Home → Right 5 times lands at 'A'. Physical x should be
    // to the LEFT of RTL end (space), not overlapping 'D'.
    li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Home});
    REQUIRE(li.cursor_position() == 0);

    for (auto i = 0; i < 5; i++) {
        li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Right});
    }
    // After 5 Rights we should be at the space (byte 9)
    REQUIRE(li.cursor_position() == 9);

    // One more Right → at 'A' (byte 10)
    li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Right});
    REQUIRE(li.cursor_position() == 10);

    // Physical x of 'A' should be less than physical x of space
    // (LTR starts LEFT of the RTL section)
    auto x_a = li.cursor_physical_x(painter);

    // Right to 'B' → goes forward in RTL flow (more negative = leftward)
    li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Right});
    auto x_b = li.cursor_physical_x(painter);
    REQUIRE(x_b < x_a);
}

TEST_CASE("BiDi RTL insert space and char after mixed text", "[lineinput][bidi]") {
    BiDiTestContext ctx;
    auto &painter = ctx.painter;
    LineInput li("");
    li.set_rect({0, 0, 500, 30});
    li.set_font_family(FontFamily::Monospace);

    // Type "אבג ABC" (RTL + space + LTR)
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x90"}); // א
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x91"}); // ב
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x92"}); // ג
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "A"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "B"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "C"});
    REQUIRE(li.text() == "אבג ABC");
    REQUIRE(li.cursor_position() == 10); // byte after C

    // Add space → cursor advances to byte 11
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    REQUIRE(li.text() == "אבג ABC ");
    REQUIRE(li.cursor_position() == 11);

    // Add D → cursor advances to byte 12; should be right of D
    li.handle_key({.type = KeyEvent::Type::Press, .text = "D"});
    REQUIRE(li.text() == "אבג ABC D");
    REQUIRE(li.cursor_position() == 12);

    auto positions = painter.text_cursor_positions(li.text(), 14.0f, FontFamily::Monospace);
    // D is at wchar index 8, end is at index 9
    // In RTL flow all positions decrease monotonically:
    // after D (byte 11) → end (byte 12) should be more negative
    REQUIRE(positions[11] > positions[12]);

    // Physical x decreases from D to end (RTL flow: end is leftmost)
    li.handle_key({.type = KeyEvent::Type::Press, .key = Key::End});
    REQUIRE(li.cursor_position() == 12);
    auto x_end = li.cursor_physical_x(painter);

    li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Left}); // to D (byte 11)
    auto x_d = li.cursor_physical_x(painter);
    REQUIRE(x_d > x_end);
}

TEST_CASE("BiDi LTR insert space and char after mixed text", "[lineinput][bidi]") {
    BiDiTestContext ctx;
    auto &painter = ctx.painter;
    LineInput li("");
    li.set_rect({0, 0, 500, 30});
    li.set_font_family(FontFamily::Monospace);
    li.set_text_direction(Painter::TextDirection::LTR);

    // Type "ABC 123" (all LTR-neutral, no RTL chars)
    li.handle_key({.type = KeyEvent::Type::Press, .text = "A"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "B"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "C"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "1"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "2"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "3"});
    REQUIRE(li.text() == "ABC 123");
    REQUIRE(li.cursor_position() == 7); // byte after '3'

    // Add space → cursor advances
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    REQUIRE(li.text() == "ABC 123 ");
    REQUIRE(li.cursor_position() == 8);

    // Add D → cursor advances; should be right of D
    li.handle_key({.type = KeyEvent::Type::Press, .text = "D"});
    REQUIRE(li.text() == "ABC 123 D");
    REQUIRE(li.cursor_position() == 9);

    // In LTR mode, physical x increases rightward monotonically
    auto x_end = li.cursor_physical_x(painter);
    li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Left}); // to D (byte 8)
    auto x_d = li.cursor_physical_x(painter);
    REQUIRE(x_d < x_end);
}
TEST_CASE("BiDi RTL digits with LTR suffix insert spaces", "[lineinput][bidi]") {
    BiDiTestContext ctx;
    auto &painter = ctx.painter;
    LineInput li("");
    li.set_rect({0, 0, 500, 30});
    li.set_font_family(FontFamily::Monospace);

    // Type "אבג 111 ccc" (RTL + digits + space + LTR)
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x90"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x91"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x92"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "1"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "1"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "1"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "c"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "c"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "c"});
    REQUIRE(li.text() == "אבג 111 ccc");
    REQUIRE(li.cursor_position() == 14);

    // X before adding space
    auto x_before = li.cursor_physical_x(painter);

    // Add space at end
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    REQUIRE(li.text() == "אבג 111 ccc ");
    REQUIRE(li.cursor_position() == 15);

    // X after space: in RTL mode, cursor should move LEFT (smaller x)
    auto x_after = li.cursor_physical_x(painter);
    REQUIRE(x_after < x_before);
}

TEST_CASE("BiDi LTR digits with LTR suffix insert spaces", "[lineinput][bidi]") {
    BiDiTestContext ctx;
    auto &painter = ctx.painter;
    LineInput li("");
    li.set_rect({0, 0, 500, 30});
    li.set_font_family(FontFamily::Monospace);
    li.set_text_direction(Painter::TextDirection::LTR);

    // Type "אבג 111 ccc" (RTL + digits + space + LTR) with LTR forced
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x90"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x91"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x92"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "1"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "1"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "1"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "c"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "c"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "c"});
    REQUIRE(li.text() == "אבג 111 ccc");
    REQUIRE(li.cursor_position() == 14);

    // X before adding space
    auto x_before = li.cursor_physical_x(painter);

    // Add space at end
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    REQUIRE(li.text() == "אבג 111 ccc ");
    REQUIRE(li.cursor_position() == 15);

    // X after space: in LTR mode, cursor should move RIGHT (larger x)
    auto x_after = li.cursor_physical_x(painter);
    REQUIRE(x_after > x_before);
}

TEST_CASE("BiDi Auto RTL-leading with digits insert spaces", "[lineinput][bidi]") {
    BiDiTestContext ctx;
    auto &painter = ctx.painter;
    LineInput li("");
    li.set_rect({0, 0, 500, 30});
    li.set_font_family(FontFamily::Monospace);

    // Type "אבג 111 ccc" (RTL + digits + space + LTR) - Auto mode
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x90"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x91"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x92"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "1"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "1"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "1"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "c"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "c"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "c"});
    REQUIRE(li.text() == "אבג 111 ccc");
    REQUIRE(li.cursor_position() == 14);

    // Auto mode: positions[0] > positions[1] (RTL) → is_rtl
    // cursor_physical_x = content_w + positions[cursor_pos_]
    // Adding space at end should move cursor LEFT (text grows leftward)
    auto x_before = li.cursor_physical_x(painter);

    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    REQUIRE(li.text() == "אבג 111 ccc ");
    REQUIRE(li.cursor_position() == 15);

    auto x_after = li.cursor_physical_x(painter);
    REQUIRE(x_after < x_before);
}

TEST_CASE("BiDi Auto RTL-leading with digits multiple spaces", "[lineinput][bidi]") {
    BiDiTestContext ctx;
    auto &painter = ctx.painter;
    LineInput li("");
    li.set_rect({0, 0, 500, 30});
    li.set_font_family(FontFamily::Monospace);

    // Type "אבג 111 ccc" (RTL + digits + space + LTR) - Auto mode
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x90"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x91"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x92"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "1"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "1"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "1"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "c"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "c"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "c"});
    REQUIRE(li.text() == "אבג 111 ccc");
    REQUIRE(li.cursor_position() == 14);

    // Check that each successive space advances cursor and moves it
    auto prev_x = li.cursor_physical_x(painter);
    auto prev_pos = li.cursor_position();

    for (auto i = 0; i < 5; i++) {
        li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
        REQUIRE(li.cursor_position() == prev_pos + 1);
        auto cur_x = li.cursor_physical_x(painter);
        // In RTL mode, each space should move cursor left
        REQUIRE(cur_x < prev_x);
        prev_x = cur_x;
        prev_pos = li.cursor_position();
    }

    // Final text should be "אבג 111 ccc     "
    REQUIRE(li.cursor_position() == 19);
}

TEST_CASE("BiDi neutrals at start follow RTL direction", "[lineinput][bidi]") {
    // Text "123 אבג" starts with digits (weak), first strong is RTL → paragraph is RTL.
    // Neutrals/digits before the first RTL should follow RTL flow (positions go left).
    BiDiTestContext ctx;
    auto &painter = ctx.painter;
    LineInput li("");
    li.set_rect({0, 0, 500, 30});
    li.set_font_family(FontFamily::Monospace);

    // Type "123 אבג" (digits + space + RTL)
    li.handle_key({.type = KeyEvent::Type::Press, .text = "1"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "2"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "3"});
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x90"}); // א
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x91"}); // ב
    li.handle_key({.type = KeyEvent::Type::Press, .text = "\xd7\x92"}); // ג
    REQUIRE(li.text() == "123 אבג");
    REQUIRE(li.cursor_codepoint() == 7);

    auto positions = painter.text_cursor_positions(li.text(), 14.0f, FontFamily::Monospace);

    // All positions should be decreasing (RTL flow) — including digits at start
    REQUIRE(positions[0] > positions[1]); // digit 1 → digit 2: leftward
    REQUIRE(positions[2] > positions[4]); // digit 3 → space: leftward
    REQUIRE(positions[5] > positions[7]); // space → 'ב': leftward (through RTL)

    // Each successive codepoint position is more negative
    auto prev = positions[0];
    for (auto cp = 1; cp <= 7; cp++) {
        // Find the byte position for this codepoint
        auto byte_pos = Utf8Iterator::find_char(li.text(), cp);
        REQUIRE(positions[byte_pos] < prev);
        prev = positions[byte_pos];
    }

    // Home → Right marches forward, each step moves LEFT (decreasing x)
    li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Home});
    for (auto i = 0; i < 7; i++) {
        auto prev_x = li.cursor_physical_x(painter);
        li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Right});
        auto cur_x = li.cursor_physical_x(painter);
        REQUIRE(cur_x < prev_x);
    }

    // Adding space at end: cursor should move LEFT (text grows leftward)
    auto x_before = li.cursor_physical_x(painter);
    li.handle_key({.type = KeyEvent::Type::Press, .text = " "});
    REQUIRE(li.text() == "123 אבג ");
    auto x_after = li.cursor_physical_x(painter);
    REQUIRE(x_after < x_before);
}

// Known-bug regression tests: typing/navigating "שלום test 123" (RTL word,
// then an embedded LTR run "test 123") currently moves the cursor visually
// the same direction throughout (continuing the RTL decrease even inside
// the embedded LTR run), instead of moving forward within the LTR run like
// ordinary LTR typing/navigation. These document the bug and are expected
// to FAIL until the embedded-run handling is fixed.

TEST_CASE("BiDi typing within embedded LTR run after RTL word moves cursor forward",
         "[lineinput][bidi]") {
    BiDiTestContext ctx;
    auto &painter = ctx.painter;
    LineInput li("");
    li.set_rect({0, 0, 500, 30});
    li.set_font_family(FontFamily::Monospace);

    auto type_and_x = [&](char const *ch) {
        li.handle_key({.type = KeyEvent::Type::Press, .text = ch});
        return li.cursor_physical_x(painter);
    };

    // "שלום": pure RTL run, each new char moves the cursor LEFT (decreasing
    // x). This part already works correctly today.
    auto x_shin = type_and_x("\xd7\xa9");  // ש
    auto x_lamed = type_and_x("\xd7\x9c"); // ל
    REQUIRE(x_lamed < x_shin);
    auto x_vav = type_and_x("\xd7\x95"); // ו
    REQUIRE(x_vav < x_lamed);
    auto x_mem = type_and_x("\xd7\x9d"); // ם
    REQUIRE(x_mem < x_vav);
    type_and_x(" ");

    // "test 123": an embedded LTR run. The boundary crossing INTO the run
    // (from the trailing space) is not asserted here -- it is legitimately
    // ambiguous which "side" a run-boundary cursor position belongs to.
    // But movement WITHIN the run must move forward (increasing x), the
    // same as ordinary LTR typing -- not continue the RTL decrease.
    auto x_t1 = type_and_x("t");
    auto x_e = type_and_x("e");
    REQUIRE(x_e > x_t1);
    auto x_s = type_and_x("s");
    REQUIRE(x_s > x_e);
    auto x_t2 = type_and_x("t");
    REQUIRE(x_t2 > x_s);
    auto x_sp2 = type_and_x(" ");
    REQUIRE(x_sp2 > x_t2);
    auto x_1 = type_and_x("1");
    REQUIRE(x_1 > x_sp2);
    auto x_2 = type_and_x("2");
    REQUIRE(x_2 > x_1);
    auto x_3 = type_and_x("3");
    REQUIRE(x_3 > x_2);

    REQUIRE(li.text() == "\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d test 123");
}

TEST_CASE("BiDi moving left out of an embedded LTR run causes a visual jump", "[lineinput][bidi]") {
    BiDiTestContext ctx;
    auto &painter = ctx.painter;
    LineInput li("");
    li.set_rect({0, 0, 500, 30});
    li.set_font_family(FontFamily::Monospace);
    li.set_text("\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d test 123"); // שלום test 123
    REQUIRE(li.cursor_position() == 17);

    li.handle_key({.type = KeyEvent::Type::Press, .key = Key::End});
    REQUIRE(li.cursor_position() == 17);

    // March Left through "3 2 1 <space> t s e t" -- 8 presses, all within
    // the embedded LTR run, ending right before the run's first character
    // ('t'), i.e. cursor_position() == 9 (just after the space following
    // "שלום").
    std::vector<double> deltas;
    auto prev_x = li.cursor_physical_x(painter);
    for (auto i = 0; i < 8; i++) {
        li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Left});
        auto x = li.cursor_physical_x(painter);
        deltas.push_back(std::abs(x - prev_x));
        prev_x = x;
    }
    REQUIRE(li.cursor_position() == 9);

    // This press crosses the run boundary, back into the RTL word "שלום".
    // Visually this must JUMP -- it must not continue the smooth
    // per-character movement seen above.
    li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Left});
    REQUIRE(li.cursor_position() == 8);
    auto x_after_jump = li.cursor_physical_x(painter);
    auto jump_delta = std::abs(x_after_jump - prev_x);

    auto typical_delta = deltas[0];
    for (auto d : deltas) {
        REQUIRE_THAT(d, Catch::Matchers::WithinAbs(typical_delta, 0.5));
    }
    REQUIRE(jump_delta > typical_delta * 2);

    // Resume marching Left through the RTL word; movement should again be
    // smooth (consistent per-character magnitude), now increasing x since
    // moving backward through RTL text moves the cursor visually rightward.
    prev_x = x_after_jump;
    for (auto i = 0; i < 4; i++) {
        li.handle_key({.type = KeyEvent::Type::Press, .key = Key::Left});
        auto x = li.cursor_physical_x(painter);
        REQUIRE(x > prev_x);
        prev_x = x;
    }
    REQUIRE(li.cursor_position() == 0);
}
