// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/undo_stack.hpp"
#include <algorithm>
#include <cstdio>

namespace toolkit {

UndoStack::UndoStack(size_t limit) : limit_(limit) {}

UndoStack::~UndoStack() = default;

void UndoStack::push(std::unique_ptr<UndoCommand> command) {
    if (!command) {
        return;
    }

    // Clear redo history
    while (static_cast<int>(commands_.size()) > index_ + 1) {
        commands_.pop_back();
    }

    // Try to merge with top
    if (index_ >= 0 && commands_[index_]->id() != -1 && commands_[index_]->id() == command->id()) {
        if (commands_[index_]->merge_with(command.get())) {
            return;
        }
    }

    // Push new command
    commands_.push_back(std::move(command));
    index_ = static_cast<int>(commands_.size()) - 1;

    // Enforce limit
    if (limit_ > 0 && commands_.size() > limit_) {
        size_t to_remove = commands_.size() - limit_;
        commands_.erase(commands_.begin(), commands_.begin() + static_cast<int>(to_remove));
        index_ -= static_cast<int>(to_remove);
    }
}

bool UndoStack::undo() {
    if (!can_undo()) {
        return false;
    }
    commands_[index_]->undo();
    index_--;
    return true;
}

bool UndoStack::redo() {
    if (!can_redo()) {
        return false;
    }
    index_++;
    commands_[index_]->redo();
    return true;
}

bool UndoStack::can_undo() const { return index_ >= 0; }

bool UndoStack::can_redo() const { return index_ < static_cast<int>(commands_.size()) - 1; }

void UndoStack::clear() {
    commands_.clear();
    index_ = -1;
}

void UndoStack::set_limit(size_t limit) {
    limit_ = limit;
    if (limit_ > 0 && commands_.size() > limit_) {
        // This is a bit tricky: if we are in the middle of history,
        // we might lose our current position or redo steps.
        // Simplest is to keep the most recent 'limit' commands.
        size_t to_remove = commands_.size() - limit_;
        commands_.erase(commands_.begin(), commands_.begin() + to_remove);
        index_ = std::max(-1, index_ - static_cast<int>(to_remove));
    }
}

std::string UndoStack::undo_text() const {
    if (!can_undo()) {
        return "";
    }
    return commands_[index_]->text();
}

std::string UndoStack::redo_text() const {
    if (!can_redo()) {
        return "";
    }
    return commands_[index_ + 1]->text();
}

} // namespace toolkit
