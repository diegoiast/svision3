#include <catch2/catch_test_macros.hpp>
#include "toolkit/combobox.hpp"
#include "toolkit/window.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/theme.hpp"
#include <cstdio>

using namespace toolkit;

class MockPlatformWindow : public PlatformWindow {
public:
    void show() override {}
    void close() override {}
    void set_size(Size) override {}
    void request_redraw() override {}
    void set_min_size(Size) override {}
    void set_max_size(Size) override {}
    int start_timer(float, std::function<void()>, bool) override { return 0; }
    void stop_timer(int) override {}
    void set_cursor(CursorShape) override {}
    void show_tooltip_window(std::string const &, Point) override {}
    void hide_tooltip_window() override {}
    bool save_to_png(std::string const &) override { return true; }
    float scale_factor() const override { return 1.0f; }
};

class MockPlatformApplication : public PlatformApplication {
public:
    std::unique_ptr<PlatformWindow> create_window(std::string_view, Size, Window *) override {
        return std::make_unique<MockPlatformWindow>();
    }
    int run() override { return 0; }
    void quit() override {}
    void post_to_main_thread(std::function<void()> fn) override { fn(); }
    std::string clipboard_get_text() override { return ""; }
    void clipboard_set_text(std::string const &) override {}
    Size measure_text(std::string_view text, float font_size, FontFamily) override {
        return { static_cast<float>(text.size()) * font_size * 0.6f, font_size + 2.0f };
    }
    Painter::FontMetrics measure_font_metrics(float font_size, FontFamily) override {
        return { font_size * 0.8f, font_size * 0.2f, font_size };
    }
    std::string_view name() const override { return "mock"; }
    std::string_view painter_name() const override { return "mock"; }
    float scale_factor() const override { return 1.0f; }
    SystemFonts system_fonts() const override { return { "sans", "mono", 14.0f }; }
};

struct MockPlatformGuard {
    MockPlatformApplication mock;
    MockPlatformGuard() { detail::set_current_platform(&mock); }
    ~MockPlatformGuard() { detail::set_current_platform(nullptr); }
};

TEST_CASE("Combobox default state", "[combobox]") {
    Combobox cb;
    REQUIRE(cb.selected() == -1);
    REQUIRE(cb.selected_text().empty());
    REQUIRE(cb.cursor() == CursorShape::Hand);
}

TEST_CASE("Combobox interaction with virtual window", "[combobox]") {
    MockPlatformGuard guard;
    Theme::set_current(Theme::create(ThemeStyle::MacOS, ColorScheme::Light));

    Window win("Test", {800, 600});
    auto cb_ptr = std::make_unique<Combobox>(std::vector<std::string>{"One", "Two", "Three"});
    auto* cb = cb_ptr.get();
    cb->set_rect({10, 10, 200, 30});
    win.add_widget(std::move(cb_ptr));

    // Initially closed
    REQUIRE(win.has_popup() == false);
    REQUIRE(cb->selected() == -1);

    // Click to open
    MouseEvent me{};
    me.type = MouseEvent::Type::Press;
    me.position = {20, 20}; // Inside CB (10,10,200,30)
    win.handle_mouse(me);
    REQUIRE(win.has_popup() == true);

    // Dropdown is at (10, 40, 200, height)
    // font_size=14, item_padding=4 -> item_h=22
    // One: [40, 62)
    // Two: [62, 84)
    // Three: [84, 106)
    
    me.position = {20, 73}; // Should hit "Two"
    win.handle_mouse(me);
    
    REQUIRE(cb->selected() == 1);
    REQUIRE(cb->selected_text() == "Two");
    REQUIRE(win.has_popup() == false);

    // Open again with keyboard
    win.set_focused_widget(cb);
    KeyEvent ke{};
    ke.type = KeyEvent::Type::Press;
    ke.key = Key::Enter;
    win.handle_key(ke);
    REQUIRE(win.has_popup() == true);

    // Navigate with keys
    ke.key = Key::Down;
    win.handle_key(ke); // Should go to "Three" (idx 2)
    
    // Select with Enter
    ke.key = Key::Enter;
    win.handle_key(ke);
    
    REQUIRE(cb->selected() == 2);
    REQUIRE(cb->selected_text() == "Three");
    REQUIRE(win.has_popup() == false);
}
