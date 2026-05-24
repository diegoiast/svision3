// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/undo_stack.hpp"
#include <string>
#include <vector>

namespace toolkit {

class TextEditCommand : public UndoCommand {
public:
    // This structure holds the state of the text editor before and after an operation.
    // It's flexible enough for both single-line (LineInput) and multi-line (TextEdit) by
    // representing text as a vector of strings.
    TextEditCommand(std::vector<std::string> *lines, 
                    std::vector<std::string> old_lines, 
                    std::vector<std::string> new_lines,
                    std::function<void()> on_change)
        : lines_(lines), old_lines_(std::move(old_lines)), new_lines_(std::move(new_lines)), on_change_(std::move(on_change)) {}

    void undo() override {
        *lines_ = old_lines_;
        if (on_change_) on_change_();
    }

    void redo() override {
        *lines_ = new_lines_;
        if (on_change_) on_change_();
    }

    int id() const override { return 1; }

private:
    std::vector<std::string> *lines_;
    std::vector<std::string> old_lines_;
    std::vector<std::string> new_lines_;
    std::function<void()> on_change_;
};

} // namespace toolkit
