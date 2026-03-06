#include <catch2/catch_test_macros.hpp>
#include "toolkit/layout.hpp"
#include "toolkit/button.hpp"
#include "toolkit/window.hpp"
#include "toolkit/platform.hpp"
#include "toolkit/theme.hpp"
#include <cstdio>

using namespace toolkit;

class MockPlatformWindow : public PlatformWindow {
public:
    void show() override {}
    void close() override {}
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
    Size measure_text(std::string_view, float font_size, FontFamily) override {
        return { 100, font_size + 2.0f };
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

TEST_CASE("Layout mouse interaction relative", "[layout]") {
    MockPlatformGuard guard;
    Theme::set_current(Theme::create(ThemeStyle::MacOS, ColorScheme::Light));

    Window win("Test", {800, 600});
    
    auto root = std::make_unique<VBoxLayout>();
    root->set_margins({50, 0, 0, 50}); // top=50, left=50
    root->set_spacing(0);
    
    auto inner = std::make_unique<VBoxLayout>();
    inner->set_margins({100, 0, 0, 100}); // top=100, left=100 relative to inner
    
    auto btn_ptr = std::make_unique<Button>("Click me");
    auto* btn = btn_ptr.get();
    bool clicked = false;
    btn->on_click = [&]() { clicked = true; };
    
    auto* inner_ptr = inner.get();
    inner->add_widget(std::move(btn_ptr));
    root->add_widget(std::move(inner));
    win.set_root(std::move(root));
    
    // root is at (0,0).
    // inner is at (50, 50) relative to root.
    // Button is at (100, 100) relative to inner.
    // Physical button position: (50+100, 50+100) = (150, 150).
    
    // Click at absolute (155, 155). This should hit the button.
    MouseEvent me{};
    me.type = MouseEvent::Type::Press;
    me.position = {155, 155};
    win.handle_mouse(me);
    
    // Check if layout was applied correctly
    REQUIRE(inner_ptr->rect().x == 50);
    REQUIRE(btn->rect().x == 100);

    me.type = MouseEvent::Type::Release;
    win.handle_mouse(me);
    
    REQUIRE(clicked == true);
    
    // Click at absolute (105, 105). This should NOT hit the button.
    clicked = false;
    me.type = MouseEvent::Type::Press;
    me.position = {105, 105};
    win.handle_mouse(me);
    
    me.type = MouseEvent::Type::Release;
    win.handle_mouse(me);
    
    REQUIRE(clicked == false);
}
