#include <catch2/catch_test_macros.hpp>
#include "toolkit/context_menu.hpp"
#include "toolkit/window.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/theme.hpp"

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

TEST_CASE("ContextMenu interaction", "[contextmenu]") {
    MockPlatformGuard guard;
    Theme::set_current(Theme::create(ThemeStyle::MacOS, ColorScheme::Light));

    Window win("Test", {800, 600});
    
    bool action1_called = false;
    bool action2_called = false;
    
    std::vector<MenuItem> items;
    items.push_back(MenuItem::action("Action 1", [&]() { action1_called = true; }));
    items.push_back(MenuItem::sep());
    items.push_back(MenuItem::action("Action 2", [&]() { action2_called = true; }));
    
    ContextMenu menu(std::move(items));
    
    // Show at (100, 100)
    menu.show(&win, {100, 100});
    REQUIRE(win.has_popup() == true);
    
    // Test mouse selection
    // item_h = 14 + 4*2 + 4 = 26
    // Action 1: y=[0, 26) relative to popup origin
    // Sep: y=[26, 33) (sep_h = 7)
    // Action 2: y=[33, 59)
    
    MouseEvent me{};
    me.type = MouseEvent::Type::Press;
    
    // Click Action 2
    // Popup is at (100, 100). We need to click at (100+some_x, 100+40)
    me.position = {150, 145}; 
    win.handle_mouse(me);
    
    REQUIRE(action2_called == true);
    REQUIRE(action1_called == false);
    REQUIRE(win.has_popup() == false); // Closed after click
    
    // Show again and test keyboard
    menu.show(&win, {100, 100});
    REQUIRE(win.has_popup() == true);
    
    KeyEvent ke{};
    ke.type = KeyEvent::Type::Press;
    
    // Select first item (it's not hovered by default usually, but handle_key next_enabled might set it)
    ke.key = Key::Down;
    win.handle_key(ke); 
    
    ke.key = Key::Enter;
    win.handle_key(ke);
    
    REQUIRE(action1_called == true);
    REQUIRE(win.has_popup() == false);
}
