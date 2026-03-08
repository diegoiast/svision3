#include <catch2/catch_test_macros.hpp>
#include "toolkit/command.hpp"
#include "toolkit/context_menu.hpp"

using namespace toolkit;

TEST_CASE("Command execute fires callback", "[command]") {
    int count = 0;
    Command cmd("Test", [&] { count++; });
    cmd.execute();
    REQUIRE(count == 1);
}

TEST_CASE("Command is_enabled defaults to true", "[command]") {
    Command cmd("Test", [] {});
    REQUIRE(cmd.is_enabled());
}

TEST_CASE("Command is_enabled respects predicate", "[command]") {
    bool flag = false;
    Command cmd("Test", [] {}, [&] { return flag; });
    REQUIRE(!cmd.is_enabled());
    flag = true;
    REQUIRE(cmd.is_enabled());
}

TEST_CASE("Command does not execute when disabled", "[command]") {
    int count = 0;
    Command cmd("Test", [&] { count++; }, [] { return false; });
    cmd.execute();
    REQUIRE(count == 0);
}

TEST_CASE("Command name", "[command]") {
    Command cmd("My Action", [] {});
    REQUIRE(cmd.name() == "My Action");
}

TEST_CASE("MenuItem::action creates item with command", "[command]") {
    auto item = MenuItem::action("Do it", [] {});
    REQUIRE(!item.separator);
    REQUIRE(item.command != nullptr);
    REQUIRE(item.command->name() == "Do it");
}

TEST_CASE("MenuItem::sep creates separator", "[command]") {
    auto item = MenuItem::sep();
    REQUIRE(item.separator);
    REQUIRE(item.command == nullptr);
}

TEST_CASE("ContextMenu can be constructed with items", "[command]") {
    std::vector<MenuItem> items;
    items.push_back(MenuItem::action("Cut", [] {}));
    items.push_back(MenuItem::sep());
    items.push_back(MenuItem::action("Paste", [] {}));
    ContextMenu menu(std::move(items));
}

TEST_CASE("Shortcut parsing and matching", "[command]") {
    auto cmd = Command::create("Save", [] {});
    cmd->set_shortcut("Ctrl+S");

    KeyEvent ev{};
    ev.type = KeyEvent::Type::Press;
    ev.text = "s";
    ev.ctrl = true;
    REQUIRE(cmd->matches_key_event(ev));

    ev.ctrl = false;
    REQUIRE(!cmd->matches_key_event(ev));

    ev.ctrl = true;
    ev.text = "x";
    REQUIRE(!cmd->matches_key_event(ev));
}

TEST_CASE("Shortcut parsing with special keys", "[command]") {
    auto cmd = Command::create("Return", [] {});
    cmd->set_shortcut("Enter");

    KeyEvent ev{};
    ev.type = KeyEvent::Type::Press;
    ev.key = Key::Enter;
    REQUIRE(cmd->matches_key_event(ev));

    cmd->set_shortcut("Ctrl+Shift+Delete");
    ev.key = Key::Delete;
    ev.ctrl = true;
    ev.shift = true;
    REQUIRE(cmd->matches_key_event(ev));

    ev.shift = false;
    REQUIRE(!cmd->matches_key_event(ev));
}

TEST_CASE("Shortcut case insensitivity", "[command]") {
    auto cmd = Command::create("Test", [] {});
    cmd->set_shortcut("ctrl+a");

    KeyEvent ev{};
    ev.type = KeyEvent::Type::Press;
    ev.text = "A";
    ev.ctrl = true;
    REQUIRE(cmd->matches_key_event(ev));
}
