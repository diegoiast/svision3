#include "svision3/undo_stack.hpp"
#include <catch2/catch_test_macros.hpp>
#include <iostream>

using namespace svision3;

class MockCommand : public UndoCommand {
  public:
    MockCommand(int *value, int delta, int id = -1) : value_(value), delta_(delta), id_(id) {}
    void undo() override { *value_ -= delta_; }
    void redo() override { *value_ += delta_; }
    int id() const override { return id_; }
    bool merge_with(const UndoCommand *other) override {
        if (other->id() == id_ && id_ != -1) {
            delta_ += static_cast<const MockCommand *>(other)->delta_;
            return true;
        }
        return false;
    }

    int delta() const { return delta_; }

  private:
    int *value_;
    int delta_;
    int id_;
};

TEST_CASE("UndoStack basic operations", "[undo]") {
    UndoStack stack;
    int value = 0;

    REQUIRE_FALSE(stack.can_undo());
    REQUIRE_FALSE(stack.can_redo());

    stack.push(std::make_unique<MockCommand>(&value, 10, 1));
    value += 10;
    REQUIRE(stack.can_undo());
    REQUIRE_FALSE(stack.can_redo());
    REQUIRE(value == 10);

    stack.undo();
    REQUIRE(value == 0);
    REQUIRE_FALSE(stack.can_undo());
    REQUIRE(stack.can_redo());

    stack.redo();
    REQUIRE(value == 10);
    REQUIRE(stack.can_undo());
    REQUIRE_FALSE(stack.can_redo());
}

TEST_CASE("UndoStack redo clearing", "[undo]") {
    UndoStack stack;
    int value = 0;

    stack.push(std::make_unique<MockCommand>(&value, 10, 123));
    value += 10;
    stack.undo();
    REQUIRE(stack.can_redo());

    stack.push(std::make_unique<MockCommand>(&value, 5, 456));
    value += 5;
    REQUIRE_FALSE(stack.can_redo());
    REQUIRE(stack.count() == 1); // count should be 1
}

TEST_CASE("UndoStack merging", "[undo]") {
    UndoStack stack;
    int value = 0;

    stack.push(std::make_unique<MockCommand>(&value, 1, 100));
    value += 1;
    stack.push(std::make_unique<MockCommand>(&value, 2, 100));
    value += 2;

    REQUIRE(stack.count() == 1);
    REQUIRE(value == 3);

    stack.undo();
    REQUIRE(value == 0);
}

TEST_CASE("UndoStack limit", "[undo]") {
    UndoStack stack(2);
    int value = 0;

    stack.push(std::make_unique<MockCommand>(&value, 1, 1));
    stack.push(std::make_unique<MockCommand>(&value, 2, 2));
    stack.push(std::make_unique<MockCommand>(&value, 3, 3));

    REQUIRE(stack.count() == 2);
    REQUIRE(stack.can_undo());
    
    stack.undo(); // value -= 3
    stack.undo(); // value -= 2
    REQUIRE_FALSE(stack.can_undo());
}
