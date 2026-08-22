#include <catch2/catch_test_macros.hpp>
#include "svision3/command.hpp"
#include "svision3/context_menu.hpp"
#include <nlohmann/json.hpp>

using namespace svision3;

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

TEST_CASE("Command is_enabled respects set_enabled", "[command]") {
    Command cmd("Test", [] {});
    cmd.set_enabled(false);
    REQUIRE(!cmd.is_enabled());
    cmd.set_enabled(true);
    REQUIRE(cmd.is_enabled());
}

TEST_CASE("Command does not execute when disabled", "[command]") {
    int count = 0;
    Command cmd("Test", [&] { count++; });
    cmd.set_enabled(false);
    cmd.execute();
    REQUIRE(count == 0);
}

TEST_CASE("Command name", "[command]") {
    Command cmd("My Action", [] {});
    REQUIRE(cmd.name() == "My Action");
}

TEST_CASE("MenuItem::action creates item with command", "[command]") {
    auto item = MenuItem::action("Do it", [] {});
    REQUIRE(!item.is_separator());
    REQUIRE(item.command != nullptr);
    REQUIRE(item.command->name() == "Do it");
}

TEST_CASE("MenuItem::sep creates separator", "[command]") {
    auto item = MenuItem::sep();
    REQUIRE(item.is_separator());
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

TEST_CASE("Shortcut virtual modifier 'std'", "[command]") {
    auto cmd = Command::create("Test", [] {});
    cmd->set_shortcut("Std+C");

    KeyEvent ev{};
    ev.type = KeyEvent::Type::Press;
    ev.text = "c";

#ifdef APPLE
    ev.super = true;
    ev.ctrl = false;
#else
    ev.ctrl = true;
    ev.super = false;
#endif

    REQUIRE(cmd->matches_key_event(ev));
}

// Following QAction: only a checkable command can be checked, and asking to check
// one that is not is ignored rather than quietly making it checkable. Reserving
// the check mark's column is a layout decision that belongs where the command is
// declared -- a stray set_checked() must not indent a whole menu's labels.
TEST_CASE("Only a checkable command can be checked", "[command]") {
    auto cmd = Command::create("Toggle", [] {});
    REQUIRE_FALSE(cmd->is_checkable());

    cmd->set_checked(true);
    REQUIRE_FALSE(cmd->is_checked());
    REQUIRE_FALSE(cmd->is_checkable());

    cmd->set_checkable(true);
    REQUIRE_FALSE(cmd->is_checked());
    cmd->set_checked(true);
    REQUIRE(cmd->is_checked());

    cmd->set_checked(false);
    REQUIRE_FALSE(cmd->is_checked());
    REQUIRE(cmd->is_checkable()); // stays checkable, so the label does not shift
}

TEST_CASE("checkable and checked both survive a round trip", "[command][serialization]") {
    auto checkable_only = Command::create("Toggle", [] {});
    checkable_only->set_checkable(true);

    auto restored = Command::create("", nullptr);
    restored->from_json(checkable_only->to_json());
    REQUIRE(restored->is_checkable());
    REQUIRE_FALSE(restored->is_checked());
    // And so it can actually be checked, which is the point of carrying the flag.
    restored->set_checked(true);
    REQUIRE(restored->is_checked());

    // A document written before "checkable" existed still restores its state.
    auto legacy = nlohmann::json::object();
    legacy["name"] = "Toggle";
    legacy["checked"] = true;
    auto from_legacy = Command::create("", nullptr);
    from_legacy->from_json(legacy);
    REQUIRE(from_legacy->is_checkable());
    REQUIRE(from_legacy->is_checked());
}
