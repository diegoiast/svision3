// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/line_input.hpp"
#include "toolkit/undo_stack.hpp"

namespace toolkit {

class TextCommand : public UndoCommand {
  public:
    TextCommand(std::string *text, std::string old_val, std::string new_val, size_t pos)
        : text_(text), old_val_(std::move(old_val)), new_val_(std::move(new_val)), pos_(pos) {}

    void undo() override { text_->replace(pos_, new_val_.size(), old_val_); }

    void redo() override { text_->replace(pos_, old_val_.size(), new_val_); }

    int id() const override { return 1; }

    std::string text() const override { return old_val_.empty() ? "Typing" : "Deletion"; }

    bool merge_with(const UndoCommand *other) override {
        auto const *o = static_cast<const TextCommand *>(other);
        if (o->pos_ == pos_ + new_val_.size()) {
            new_val_ += o->new_val_;
            return true;
        }
        return false;
    }

  private:
    std::string *text_;
    std::string old_val_;
    std::string new_val_;
    size_t pos_;
};

} // namespace toolkit
