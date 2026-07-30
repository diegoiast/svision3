// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/context_menu.hpp"
#include "toolkit/scrollable_widget.hpp"
#include "toolkit/undo_stack.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace toolkit {

class TextEditCommand;

class TextEdit : public ScrollableWidget {
    DECLARE_WIDGET(TextEdit)
  public:
    struct Position {
        size_t line = 0;
        size_t col = 0;
        bool operator==(Position const &o) const { return line == o.line && col == o.col; }
        bool operator!=(Position const &o) const { return !(*this == o); }
        bool operator<(Position const &o) const {
            return line < o.line || (line == o.line && col < o.col);
        }
        bool operator<=(Position const &o) const { return *this == o || *this < o; }
    };

    explicit TextEdit(std::string text = "");
    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    void paint_background(Painter &painter) override;
    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    CursorShape cursor() const override { return CursorShape::IBeam; }
    TextEdit &set_focused(bool focused) override;
    void on_focus() override;
    void on_blur() override;
    void set_rect(Rect const &rect) override;

    std::string text() const;
    // FIXME add API to read directly from stream
    // FIXME add API to read using different encodings
    void set_text(std::string const &text);

    Command::Ptr select_all_cmd;
    Command::Ptr cut_cmd;
    Command::Ptr copy_cmd;
    Command::Ptr paste_cmd;
    Command::Ptr undo_cmd;
    Command::Ptr redo_cmd;

    std::function<void()> on_change;

  protected:
    void on_scroll(float x, float y) override;

  private:
    friend class TextEditCommand;

    void reset_cursor_blink();
    Position pos_from_point(Point p) const;
    float line_height() const;
    float gutter_width() const;
    void update_scroll_state();
    void ensure_cursor_visible();
    void move_cursor(Position p, bool extend_selection);
    void delete_selection();
    void delete_selection_internal();
    void set_cursor_for_undo(Position p);
    bool has_selection() const { return cursor_ != anchor_; }
    Position sel_start() const { return cursor_ < anchor_ ? cursor_ : anchor_; }
    Position sel_end() const { return anchor_ < cursor_ ? cursor_ : anchor_; }
    void insert_text(std::string_view t);
    void move_word_left(bool extend);
    void move_word_right(bool extend);
    void move_document_start(bool extend);
    void move_document_end(bool extend);
    void select_all();
    void cut();
    void copy();
    void paste();
    void undo();
    void redo();
    void show_context_menu(Point pos);
    void sync_commands();
    void insert_newline();
    void delete_char_backward(bool word = false);
    void delete_char_forward();
    void indent_selection(bool unindent, int spaces = 4);
    std::string range_text(Position start, Position end) const;
    void insert_text_raw(std::string_view t, Position at);
    void delete_range_raw(Position start, Position end);

    std::vector<std::string> lines_{""};
    Position cursor_;
    Position anchor_;
    bool dragging_ = false;
    std::chrono::steady_clock::time_point cursor_blink_time_;
    int blink_timer_id_ = 0;
    std::unique_ptr<ContextMenu> context_menu_;
    UndoStack undo_stack_;
    mutable float cached_max_line_w_ = 0.0f;
    mutable bool max_line_w_dirty_ = true;
    mutable size_t last_lines_count_ = 0;
    bool paste_available_ = false;
};

} // namespace toolkit
