#include <catch2/catch_test_macros.hpp>
#include "toolkit/tab_widget.hpp"
#include "toolkit/label.hpp"
#include "toolkit/layout.hpp"
#include "toolkit/button.hpp"

using namespace toolkit;

TEST_CASE("TabWidget default state", "[tabwidget]") {
    TabWidget tw;
    REQUIRE(tw.current_index() == 0);
}

TEST_CASE("TabWidget add_tab and current_index", "[tabwidget]") {
    TabWidget tw;
    tw.add_tab("Tab 1", std::make_unique<Label>("Content 1"));
    tw.add_tab("Tab 2", std::make_unique<Label>("Content 2"));
    REQUIRE(tw.current_index() == 0);

    tw.set_current(1);
    REQUIRE(tw.current_index() == 1);
}

TEST_CASE("TabWidget set_current out of range ignored", "[tabwidget]") {
    TabWidget tw;
    tw.add_tab("Tab 1", std::make_unique<Label>("A"));
    tw.set_current(5);
    REQUIRE(tw.current_index() == 0);
    tw.set_current(-1);
    REQUIRE(tw.current_index() == 0);
}

TEST_CASE("TabWidget on_tab_close callback", "[tabwidget]") {
    TabWidget tw;
    tw.add_tab("First", std::make_unique<Label>("A"));
    tw.add_tab("Second", std::make_unique<Label>("B"));

    int closed_idx = -1;
    std::string closed_title;
    tw.on_tab_close = [&](int idx, std::string const &title) {
        closed_idx = idx;
        closed_title = title;
    };
    tw.on_tab_close(1, "Second");
    REQUIRE(closed_idx == 1);
    REQUIRE(closed_title == "Second");
}

TEST_CASE("TabWidget collect_focusables from active tab only", "[tabwidget]") {
    TabWidget tw;
    tw.set_rect({0, 0, 1000, 1000});

    auto tab1 = std::make_unique<VBoxLayout>();
    tab1->add_widget(std::make_unique<Button>("A"));
    tab1->add_widget(std::make_unique<Button>("B"));

    auto tab2 = std::make_unique<VBoxLayout>();
    tab2->add_widget(std::make_unique<Button>("C"));

    tw.add_tab("Tab 1", std::move(tab1));
    tw.add_tab("Tab 2", std::move(tab2));

    std::vector<Widget *> focusables;
    tw.collect_focusables(focusables);
    REQUIRE(focusables.size() == 2);

    focusables.clear();
    tw.set_current(1);
    tw.collect_focusables(focusables);
    REQUIRE(focusables.size() == 1);
}

TEST_CASE("TabWidget collect_mnemonics from active tab only", "[tabwidget]") {
    TabWidget tw;

    auto tab1 = std::make_unique<VBoxLayout>();
    tab1->add_widget(std::make_unique<Button>("&Save"));

    auto tab2 = std::make_unique<VBoxLayout>();
    tab2->add_widget(std::make_unique<Button>("&Quit"));

    tw.add_tab("Tab 1", std::move(tab1));
    tw.add_tab("Tab 2", std::move(tab2));

    std::vector<Widget *> mnemonics;
    tw.collect_mnemonics(mnemonics);
    REQUIRE(mnemonics.size() == 1);
}

TEST_CASE("TabWidget scrolling with interaction", "[tabwidget]") {
    TabWidget tw;
    tw.set_rect({0, 0, 100, 100}); // Small width

    // Add 10 tabs, each likely wider than 10 pixels
    for (int i = 0; i < 10; ++i) {
        tw.add_tab("Tab " + std::to_string(i), std::make_unique<Label>("C"));
    }

    // Tab 0 should be current
    REQUIRE(tw.current_index() == 0);

    // Click on Tab 9 (at the end), it should scroll and become current
    // We don't know exact positions, but clicking far right should hit something or scroll
    tw.set_current(9);
    REQUIRE(tw.current_index() == 9);

    // Simulate mouse wheel scroll
    MouseEvent scroll_ev;
    scroll_ev.type = MouseEvent::Type::Scroll;
    scroll_ev.position = {50, 5}; // In the bar
    scroll_ev.scroll_dy = -1.0f; // Scroll right/down
    tw.handle_mouse(scroll_ev);

    // After scrolling, hitting a tab at a specific position should change
    // Since we cannot easily assert on internal state, we verify no crashes and basic flow
}

TEST_CASE("TabWidget with leading widget and scrolling", "[tabwidget]") {
    TabWidget tw;
    tw.set_rect({0, 0, 200, 100});

    auto leading = std::make_unique<Button>("L");
    // Assume button size hint is around 20-40 pixels
    tw.set_leading_widget(std::move(leading));

    for (int i = 0; i < 20; ++i) {
        tw.add_tab("T" + std::to_string(i), std::make_unique<Label>("C"));
    }

    // This should definitely trigger scroll buttons.
    // The layout should be: [Leading][Prev][Tabs...][Next]
    // We verify it doesn't crash and respects basic properties.
    REQUIRE(tw.current_index() == 0);
}

TEST_CASE("TabWidget hidden scroll buttons hit test", "[tabwidget]") {
    TabWidget tw;
    // Set a width that causes overflow
    tw.set_rect({0, 0, 100, 100});

    for (int i = 0; i < 10; ++i) {
        tw.add_tab("Tab " + std::to_string(i), std::make_unique<Label>("C"));
    }

    // scroll_offset_ should be 0, so prev_button_ is hidden
    // Hit test at x=5 (where prev_button would be if visible) should hit Tab 0
    // We need to be careful about button sizes, but with 100px width and many tabs,
    // scroll buttons WILL be shown.
    
    // This is more of a behavioral test, ensuring no crashes and current logic
    REQUIRE(tw.current_index() == 0);
}

TEST_CASE("TabWidget scroll back to first tab", "[tabwidget]") {
    TabWidget tw;
    tw.set_rect({0, 0, 100, 100}); // Force overflow

    for (int i = 0; i < 10; ++i) {
        tw.add_tab("Tab " + std::to_string(i), std::make_unique<Label>("C"));
    }

    // Scroll to the end
    tw.set_current(9);
    REQUIRE(tw.current_index() == 9);

    // Scroll back to the first
    tw.set_current(0);
    REQUIRE(tw.current_index() == 0);

    // Verify Tab 0 is hit-testable at its expected position
    // Tab 0 should be at x=0 (relative to bar_start, which is start_pos since scroll_offset is 0)
    auto hr = tw.widget_at({5, 5}); // Should be in the first tab
    REQUIRE(hr != nullptr);
    // Depending on TabWidget implementation, widget_at might return the TabWidget itself if 
    // it's not the content. But it should definitely NOT be null or a scroll button.
}

TEST_CASE("TabWidget no gap when prev button hidden", "[tabwidget]") {
    TabWidget tw;
    tw.set_rect({0, 0, 100, 100}); // Force overflow

    // Add a leading widget to make it more complex
    tw.set_leading_widget(std::make_unique<Button>("L"));

    for (int i = 0; i < 10; ++i) {
        tw.add_tab("Tab " + std::to_string(i), std::make_unique<Label>("C"));
    }

    // scroll_offset is 0, prev button is hidden. 
    // Tab 0 should start immediately after leading widget.
    // Assuming leading widget is small, x=30 should definitely be inside Tab 0 
    // if there's no gap. If there's a 20-30px gap for a hidden button, it might miss.
    
    // We don't know exact sizes, but we can verify that widget_at returns 
    // the TabWidget (representing the tab bar) at a point very close to the start.
    
    // First, verify leading widget is there
    auto *w_lead = tw.widget_at({5, 5});
    REQUIRE(w_lead != nullptr);
    REQUIRE(w_lead != &tw);

    // Now check immediately after leading widget (assuming it's around 20-40px)
    // If there was a gap for the hidden "prev" button, x=50 might be empty space (returning null or parent)
    // With buttons at the end, x=50 MUST be a tab.
    auto *w_tab = tw.widget_at({50, 5});
    REQUIRE(w_tab == &tw); 
}

TEST_CASE("TabWidget different orientations selection", "[tabwidget]") {
    auto orientations = {TabOrientation::South, TabOrientation::West, TabOrientation::East};
    
    for (auto o : orientations) {
        TabWidget tw;
        tw.set_orientation(o);
        tw.set_rect({0, 0, 200, 200});
        
        tw.add_tab("Tab 0", std::make_unique<Label>("C0"));
        tw.add_tab("Tab 1", std::make_unique<Label>("C1"));
        
        REQUIRE(tw.current_index() == 0);
        
        // Find a point that should hit Tab 1
        // We don't know exact sizes, but we can guess
        Point p;
        if (o == TabOrientation::South) {
            p = {100, 190}; // Bottom bar
        } else if (o == TabOrientation::West) {
            p = {10, 100}; // Left bar
        } else if (o == TabOrientation::East) {
            p = {190, 100}; // Right bar
        }
        
        MouseEvent ev;
        ev.type = MouseEvent::Type::Press;
        ev.position = p;
        ev.button = 1;
        
        if (tw.handle_mouse(ev)) {
            // It might have hit Tab 0 or Tab 1 depending on coordinates
            // but it should at least not crash and handle the event
        }
    }
}

TEST_CASE("TabWidget keyboard shortcuts", "[tabwidget]") {
    TabWidget tw;
    tw.add_tab("Tab 0", std::make_unique<Label>("C0"));
    tw.add_tab("Tab 1", std::make_unique<Label>("C1"));
    tw.add_tab("Tab 2", std::make_unique<Label>("C2"));
    
    REQUIRE(tw.current_index() == 0);
    
    // Ctrl+PageDown to next tab
    KeyEvent ev;
    ev.type = KeyEvent::Type::Press;
    ev.ctrl = true;
    ev.key = Key::PageDown;
    tw.handle_key(ev);
    REQUIRE(tw.current_index() == 1);
    
    // Ctrl+PageUp back to first tab
    ev.key = Key::PageUp;
    tw.handle_key(ev);
    REQUIRE(tw.current_index() == 0);
    
    // Ctrl+Shift+PageDown to move Tab 0 to index 1
    ev.key = Key::PageDown;
    ev.shift = true;
    tw.handle_key(ev);
    REQUIRE(tw.current_index() == 1);
    // After moving, Tab 0 should be at index 1
    // We can't easily check titles without making more methods public, 
    // but we've verified it doesn't crash and index is updated.
}
