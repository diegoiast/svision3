// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/context_menu.hpp"
#include "toolkit/undo_stack.hpp"
#include "toolkit/widget.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace toolkit {

class TextEditCommand;

class TextEdit : public Widget, public Fluent<TextEdit> {
    DECLARE_WIDGET(Checkbox)
  public:
    struct Pos {
        int line = 0;
        int col = 0;
        bool operator==(Pos const &o) const { return line == o.line && col == o.col; }
        bool operator!=(Pos const &o) const { return !(*this == o); }
        bool operator<(Pos const &o) const {
            return line < o.line || (line == o.line && col < o.col);
        }
        bool operator<=(Pos const &o) const { return *this == o || *this < o; }
    };

    explicit TextEdit(std::string text = "");
    nlohmann::json to_json() const override;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    CursorShape cursor() const override { return CursorShape::IBeam; }
    TextEdit &set_focused(bool focused) override;
    void on_focus() override;
    void on_blur() override;

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

  private:
    friend class TextEditCommand;

    void reset_cursor_blink();
    Pos pos_from_point(Point p) const;
    float line_height() const;
    float gutter_width() const;
    void clamp_scroll();
    void ensure_cursor_visible();
    void move_cursor(Pos p, bool extend_selection);
    void delete_selection();
    void delete_selection_internal();
    void set_cursor_for_undo(Pos p);
    bool has_selection() const { return cursor_ != anchor_; }
    Pos sel_start() const { return cursor_ < anchor_ ? cursor_ : anchor_; }
    Pos sel_end() const { return anchor_ < cursor_ ? cursor_ : anchor_; }
    void insert_text(std::string_view t);
    void move_word_left(bool extend);
    void move_word_right(bool extend);
    void select_all();
    void cut();
    void copy();
    void paste();
    void undo();
    void redo();
    void show_context_menu(Point pos);
    void sync_commands();

    std::vector<std::string> lines_{""};
    Pos cursor_;
    Pos anchor_;
    float scroll_x_ = 0;
    float scroll_y_ = 0;
    bool dragging_ = false;
    std::chrono::steady_clock::time_point cursor_blink_time_;
    int blink_timer_id_ = 0;
    std::unique_ptr<ContextMenu> context_menu_;
    UndoStack undo_stack_;
};

} // namespace toolkit
