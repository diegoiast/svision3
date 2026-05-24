// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace toolkit {

class UndoCommand {
  public:
    virtual ~UndoCommand() = default;

    virtual void undo() = 0;
    virtual void redo() = 0;

    virtual int id() const { return -1; }
    virtual bool merge_with(const UndoCommand *other) {
        (void)other;
        return false;
    }

    virtual std::string text() const { return ""; }
};

class UndoStack {
  public:
    UndoStack(size_t limit = 100);
    ~UndoStack();

    void push(std::unique_ptr<UndoCommand> command);
    bool undo();
    bool redo();

    bool can_undo() const;
    bool can_redo() const;

    void clear();

    size_t limit() const { return limit_; }
    void set_limit(size_t limit);

    size_t count() const { return commands_.size(); }
    int index() const { return index_; }

    std::string undo_text() const;
    std::string redo_text() const;

  private:
    std::vector<std::unique_ptr<UndoCommand>> commands_;
    int index_ = -1;
    size_t limit_;
};

} // namespace toolkit
