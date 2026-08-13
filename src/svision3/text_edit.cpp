// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// FIXME: More documentation on this widget, architecture, cleanups
//        I know writing a text editor component is not trivial. But this
//        looks like a major cleanup, or refactor is needed.
//        Cursor management, drawing scrollbars, gutters, this is all baked in
//        without any way to customize. Its a start, but not something
//        that can be used in production.

// WARNING: here be dragons, this is vibe-coded to the max. I am unsure if this
//          can be properly fixed. I am currently dumping more LLMs calls into this.
//          But I expect for a full re-write eventually.

#include "svision3/text_edit.hpp"
#include "svision3/clipboard.hpp"
#include "svision3/theme.hpp"
#include "svision3/utf8.hpp"
#include "svision3/window.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <nlohmann/json.hpp>

#include <sstream>

namespace svision3 {

static constexpr FontFamily kFont = FontFamily::Monospace;

// Stores only the changed range rather than full-document snapshots.
// Replace: range [pos_, before_end_] (text_before_) → [pos_, after_end_] (text_after_).
// undo: delete [pos_..after_end_], insert text_before_ at pos_.
// redo: delete [pos_..before_end_], insert text_after_ at pos_.
class TextEditCommand : public UndoCommand {
  public:
    TextEditCommand(TextEdit *edit, TextEdit::Position pos, TextEdit::Position before_end,
                    TextEdit::Position after_end, std::string text_before, std::string text_after,
                    TextEdit::Position old_cursor, TextEdit::Position new_cursor, int cmd_id = 0)
        : edit_(edit), pos_(pos), before_end_(before_end), after_end_(after_end),
          text_before_(std::move(text_before)), text_after_(std::move(text_after)),
          old_cursor_(old_cursor), new_cursor_(new_cursor), id_(cmd_id) {}

    void undo() override {
        edit_->delete_range_raw(pos_, after_end_);
        if (!text_before_.empty()) {
            edit_->insert_text_raw(text_before_, pos_);
        }
        edit_->set_cursor_for_undo(old_cursor_);
    }

    void redo() override {
        edit_->delete_range_raw(pos_, before_end_);
        if (!text_after_.empty()) {
            edit_->insert_text_raw(text_after_, pos_);
        }
        edit_->set_cursor_for_undo(new_cursor_);
    }

    bool merge_with(const UndoCommand *other) override {
        if (id_ != 1) {
            return false;
        }
        auto const *o = static_cast<const TextEditCommand *>(other);
        if (o->id_ != 1) {
            return false;
        }
        // Merge sequential single-char inserts (no deletion on either side)
        if (text_before_.empty() && o->text_before_.empty() && after_end_ == o->pos_) {
            text_after_ += o->text_after_;
            after_end_ = o->after_end_;
            new_cursor_ = o->new_cursor_;
            return true;
        }
        return false;
    }

    int id() const override { return id_; }
    std::string text() const override { return "Editing"; }

  private:
    TextEdit *edit_;
    TextEdit::Position pos_;
    TextEdit::Position before_end_;
    TextEdit::Position after_end_;
    std::string text_before_;
    std::string text_after_;
    TextEdit::Position old_cursor_;
    TextEdit::Position new_cursor_;
    int id_;
};

TextEdit::TextEdit(std::string text) {
    set_frame(true, true);
    set_focusable(true);
    cursor_blink_time_ = std::chrono::steady_clock::now();

    select_all_cmd = Command::create("Select All", [this] { select_all(); });
    select_all_cmd->set_shortcut("Std+A");
    add_command(select_all_cmd);

    cut_cmd = Command::create("Cut", [this] { cut(); });
    cut_cmd->set_shortcut("Std+X");
    add_command(cut_cmd);

    copy_cmd = Command::create("Copy", [this] { copy(); });
    copy_cmd->set_shortcut("Std+C");
    add_command(copy_cmd);

    paste_cmd = Command::create("Paste", [this] { paste(); });
    paste_cmd->set_shortcut("Std+V");
    add_command(paste_cmd);

    undo_cmd = Command::create("Undo", [this] { undo(); });
    undo_cmd->set_shortcut("Std+Z");
    add_command(undo_cmd);

    redo_cmd = Command::create("Redo", [this] { redo(); });
    redo_cmd->set_shortcut("Std+Y");
    add_command(redo_cmd);

    set_text(text);
}

nlohmann::json TextEdit::to_json() const {
    auto j = Widget::to_json();
    j["text"] = text();
    return j;
}

void TextEdit::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("text")) {
        set_text(j["text"]);
    }
}

std::string TextEdit::text() const {
    auto total = lines_.size() > 1 ? lines_.size() - 1 : 0;
    for (auto const &ln : lines_) {
        total += ln.size();
    }
    auto result = std::string();
    result.reserve(total);
    for (auto i = 0; i < (int)lines_.size(); i++) {
        if (i > 0) {
            result += '\n';
        }
        result += lines_[i];
    }
    return result;
}

void TextEdit::set_text(std::string const &text) {
    auto current = this->text();
    if (current == text) {
        return;
    }

    std::istringstream ss(text);
    std::string line;

    lines_.clear();
    while (std::getline(ss, line)) {
        lines_.push_back(std::move(line));
    }
    if (lines_.empty()) {
        lines_.emplace_back();
    }
    // std::getline discards the newline delimiter, so a text that ends with
    // '\n' loses the implicit empty last line.  Restore it here.
    if (!text.empty() && text.back() == '\n') {
        lines_.emplace_back();
    }
    cursor_ = {0, 0};
    anchor_ = cursor_;
    scroll_x_ = scroll_y_ = 0;
    max_line_w_dirty_ = true;
    update_scroll_state();
    sync_commands();
    if (window_) {
        window_->request_redraw("text change");
    }
}

TextEdit &TextEdit::set_highlight_current_line(bool enabled) {
    if (highlight_current_line_ == enabled) {
        return *this;
    }
    highlight_current_line_ = enabled;
    if (window_) {
        window_->request_redraw("property change (highlight_current_line)");
    }
    return *this;
}

TextEdit &TextEdit::set_focused(bool focused) {
    Widget::set_focused(focused);
    if (!focused) {
        anchor_ = cursor_;
    }
    sync_commands();
    return *this;
}

void TextEdit::on_scroll(float /*x*/, float /*y*/) {
    if (window_) {
        window_->request_redraw("text scroll");
    }
}

void TextEdit::set_rect(Rect const &r) {
    Widget::set_rect(r);
    update_scroll_state();
}

void TextEdit::update_scroll_state() {
    auto lh = line_height();
    auto const &palette = Theme::current().palette;
    auto content_h = lh * static_cast<float>(lines_.size());

    if (max_line_w_dirty_) {
        cached_max_line_w_ = 0.0f;
        for (auto const &ln : lines_) {
            float w = measure_text(ln, palette.fonts.size, kFont).width;
            if (w > cached_max_line_w_) {
                cached_max_line_w_ = w;
            }
        }
        max_line_w_dirty_ = false;
    }

    auto gw = gutter_width();
    update_scrollbars({cached_max_line_w_ + gw + 20.0f, content_h});
    last_lines_count_ = lines_.size();
}

void TextEdit::on_focus() {
    paste_available_ = !Clipboard::get_text().empty();
    reset_cursor_blink();
    if (window_ && blink_timer_id_ == 0) {
        // FIXME: cursor blink time should be defined by platform
        blink_timer_id_ = window_->start_timer(
            0.5f,
            [this] {
                if (is_effectively_visible()) {
                    window_->request_redraw("blink");
                }
            },
            true);
    }
}

void TextEdit::on_blur() {
    if (window_ && blink_timer_id_ != 0) {
        window_->stop_timer(blink_timer_id_);
        blink_timer_id_ = 0;
    }
}

void TextEdit::reset_cursor_blink() { cursor_blink_time_ = std::chrono::steady_clock::now(); }

float TextEdit::line_height() const {
    auto const &palette = Theme::current().palette;
    auto fm = font_metrics(palette.fonts.size, kFont);
    return std::max(fm.height, palette.fonts.size) + 2.0f;
}

float TextEdit::gutter_width() const {
    auto const &palette = Theme::current().palette;
    auto digits = 1;
    auto n = static_cast<int>(lines_.size());
    while (n >= 10) {
        digits++;
        n /= 10;
    }
    digits = std::max(digits, 2);
    return measure_text(std::string(digits, '9'), palette.fonts.size, kFont).width + 16.0f;
}

TextEdit::Position TextEdit::pos_from_point(Point p) const {
    auto const &palette = Theme::current().palette;
    auto lh = line_height();
    auto gw = gutter_width();

    if (lh <= 0.0001f) {
        return {0, 0};
    }

    // (p.y + scroll_y_) / lh can be negative for a point above the visible
    // area, so clamp in signed space before converting to the unsigned line
    // index below.
    auto raw_line = static_cast<int>((p.y + scroll_y_) / lh);
    auto click_x = p.x - gw + scroll_x_;
    raw_line = std::clamp(raw_line, 0, static_cast<int>(lines_.size()) - 1);
    auto line = static_cast<size_t>(raw_line);

    if (click_x <= 0) {
        return {line, 0};
    }

    auto const &ln = lines_[line];
    size_t col = 0;
    while (col < ln.size()) {
        auto next_col = Utf8Iterator::next(ln, col);
        auto w = measure_text(ln.substr(0, next_col), palette.fonts.size, kFont).width;

        if (w > click_x) {
            auto prev_w = measure_text(ln.substr(0, col), palette.fonts.size, kFont).width;
            if (click_x - prev_w < w - click_x) {
                return {line, col};
            } else {
                return {line, next_col};
            }
        }
        col = next_col;
    }
    return {line, col};
}

void TextEdit::ensure_cursor_visible() {
    auto const &palette = Theme::current().palette;
    auto lh = line_height();
    auto gw = gutter_width();
    auto cy = lh * cursor_.line;
    auto vr = viewport_rect();
    auto cx = 0.0f;

    if (cy + lh > scroll_y_ + vr.height) {
        scroll_y_ = cy + lh - vr.height;
    }
    if (cy < scroll_y_) {
        scroll_y_ = cy;
    }
    if (cursor_.col > 0) {
        cx = measure_text(lines_[cursor_.line].substr(0, cursor_.col), palette.fonts.size, kFont)
                 .width;
    }
    cx += gw;

    // FIXME: what is this 10.0f?
    if (cx > scroll_x_ + vr.width - 10.0f) {
        scroll_x_ = cx - vr.width + 10.0f;
    }
    if (cx < scroll_x_ + gw) {
        scroll_x_ = cx - gw;
    }
    // content_size_ itself is updated lazily in paint() to avoid rescanning
    // all lines on every keystroke/delete, but the scrollbar thumbs still
    // need to reflect the scroll_x_/scroll_y_ set above, so sync them against
    // the last-known content size instead of a full update_scroll_state().
    update_scrollbars(content_size_);
}

void TextEdit::move_cursor(Position p, bool extend_selection) {
    cursor_ = p;
    if (!extend_selection) {
        anchor_ = cursor_;
    }
    sync_commands();
    reset_cursor_blink();
    ensure_cursor_visible();
    if (window_) {
        window_->request_redraw("cursor move");
    }
}

void TextEdit::delete_selection() {
    if (!has_selection()) {
        return;
    }
    auto s = sel_start(), e = sel_end();
    auto deleted = range_text(s, e);
    auto old_cursor = cursor_;
    delete_selection_internal();
    undo_stack_.push(std::make_unique<TextEditCommand>(this, s, e, s, std::move(deleted), "",
                                                       old_cursor, cursor_));
    sync_commands();
    ensure_cursor_visible();
    if (window_) {
        window_->request_redraw("text change");
    }
    if (on_change) {
        on_change();
    }
}

void TextEdit::delete_selection_internal() {
    auto s = sel_start();
    auto e = sel_end();

    if (s.line == e.line) {
        lines_[s.line].erase(s.col, e.col - s.col);
        // Width can only shrink; leave cache conservative (same as single-char delete)
    } else {
        auto const &pal = Theme::current().palette;
        // Check if any deleted line was as wide as the cached max.
        // If so, the true max may have dropped and we need a full rescan.
        // Otherwise the cache is still valid — avoid rescanning the whole document.
        auto need_rescan = false;
        for (auto i = s.line; i <= e.line && !need_rescan; i++) {
            auto w = measure_text(lines_[i], pal.fonts.size, kFont).width;
            if (w >= cached_max_line_w_) {
                need_rescan = true;
            }
        }
        auto merged = lines_[s.line].substr(0, s.col) + lines_[e.line].substr(e.col);
        lines_.erase(lines_.begin() + s.line, lines_.begin() + e.line + 1);
        lines_.insert(lines_.begin() + s.line, std::move(merged));
        // Update cache with the merged line (may only grow, not shrink)
        auto mw = measure_text(lines_[s.line], pal.fonts.size, kFont).width;
        if (mw > cached_max_line_w_) {
            cached_max_line_w_ = mw;
        }
        if (need_rescan) {
            max_line_w_dirty_ = true;
        }
    }
    cursor_ = anchor_ = s;
}

void TextEdit::set_cursor_for_undo(Position p) {
    cursor_ = anchor_ = p;
    sync_commands();
    ensure_cursor_visible();
    if (window_) {
        window_->request_redraw("undo/redo");
    }
}

std::string TextEdit::range_text(Position start, Position end) const {
    if (start == end) {
        return {};
    }
    auto result = std::string{};
    for (auto i = start.line; i <= end.line; i++) {
        auto sc = (i == start.line) ? start.col : size_t{0};
        auto ec = (i == end.line) ? end.col : lines_[i].size();
        result.append(lines_[i], sc, ec - sc);
        if (i < end.line) {
            result += '\n';
        }
    }
    return result;
}

void TextEdit::insert_text_raw(std::string_view t, Position at) {
    cursor_ = anchor_ = at;
    for (auto i = 0; i < (int)t.size(); i++) {
        if (t[i] == '\n' || t[i] == '\r') {
            if (t[i] == '\r' && i + 1 < (int)t.size() && t[i + 1] == '\n') {
                i++;
            }
            auto rest = lines_[cursor_.line].substr(cursor_.col);
            lines_[cursor_.line].erase(cursor_.col);
            lines_.insert(lines_.begin() + cursor_.line + 1, std::move(rest));
            cursor_.line++;
            cursor_.col = 0;
        } else {
            lines_[cursor_.line].insert(lines_[cursor_.line].begin() + cursor_.col, t[i]);
            cursor_.col++;
        }
    }
    anchor_ = cursor_;
}

void TextEdit::delete_range_raw(Position start, Position end) {
    if (start == end) {
        return;
    }
    cursor_ = end;
    anchor_ = start;
    delete_selection_internal();
}

void TextEdit::insert_text(std::string_view t) {
    auto old_cursor = cursor_;
    auto insert_pos = has_selection() ? sel_start() : cursor_;
    auto sel_e = has_selection() ? sel_end() : cursor_;

    auto deleted_text = std::string{};
    if (has_selection()) {
        deleted_text = range_text(insert_pos, sel_e);
        delete_selection_internal();
    }

    // FIXME: support for unicode UTF/8
    // FIXME: support for unicode char - new paragraph
    auto insert_start_line = cursor_.line;
    for (auto i = 0; i < (int)t.size(); i++) {
        if (t[i] == '\n' || t[i] == '\r') {
            if (t[i] == '\r' && i + 1 < (int)t.size() && t[i + 1] == '\n') {
                i++;
            }
            std::string rest = lines_[cursor_.line].substr(cursor_.col);
            lines_[cursor_.line].erase(cursor_.col);
            lines_.insert(lines_.begin() + cursor_.line + 1, std::move(rest));
            cursor_.line++;
            cursor_.col = 0;
        } else {
            lines_[cursor_.line].insert(lines_[cursor_.line].begin() + cursor_.col, t[i]);
            cursor_.col++;
        }
    }
    anchor_ = cursor_;

    // id=1 enables merging for single-char inserts with no replaced selection
    auto cmd_id = (deleted_text.empty() && t.size() == 1 && t[0] != '\n' && t[0] != '\r') ? 1 : 0;
    undo_stack_.push(std::make_unique<TextEditCommand>(this, insert_pos, sel_e, cursor_,
                                                       std::move(deleted_text), std::string(t),
                                                       old_cursor, cursor_, cmd_id));

    // Insertion can only increase line widths — check only the affected lines
    // rather than doing a full rescan. If a selection was deleted first,
    // max_line_w_dirty_ is already true and the rescan will cover everything.
    if (!max_line_w_dirty_) {
        auto const &palette = Theme::current().palette;
        for (auto i = insert_start_line; i <= cursor_.line && i < lines_.size(); i++) {
            auto w = measure_text(lines_[i], palette.fonts.size, kFont).width;
            if (w > cached_max_line_w_) {
                cached_max_line_w_ = w;
            }
        }
    }
    // Widening a single line (e.g. pasting) doesn't set max_line_w_dirty_ or
    // change lines_.size(), so paint()'s update_scroll_state() gate would
    // otherwise miss it and the horizontal scrollbar would stay hidden.
    update_scroll_state();

    sync_commands();
    ensure_cursor_visible();
    if (window_) {
        window_->request_redraw("text change");
    }
    if (on_change) {
        on_change();
    }
}

void TextEdit::move_word_left(bool extend) {
    auto line = cursor_.line;
    auto col = cursor_.col;

    if (col == 0 && line > 0) {
        line--;
        col = lines_[line].size();
    }

    auto const &ln = lines_[line];
    while (col > 0 && std::isspace(static_cast<unsigned char>(ln[col - 1]))) {
        col--;
    }
    while (col > 0 && !std::isspace(static_cast<unsigned char>(ln[col - 1]))) {
        col--;
    }
    move_cursor({line, col}, extend);
}

void TextEdit::move_word_right(bool extend) {
    auto line = cursor_.line;
    auto col = cursor_.col;
    auto const &ln = lines_[line];
    auto len = ln.size();

    if (col >= len && line + 1 < lines_.size()) {
        line++;
        col = 0;
    } else {
        while (col < len && !std::isspace(static_cast<unsigned char>(ln[col]))) {
            col++;
        }
        while (col < len && std::isspace(static_cast<unsigned char>(ln[col]))) {
            col++;
        }
    }
    move_cursor({line, col}, extend);
}

void TextEdit::move_document_start(bool extend) { move_cursor({0, 0}, extend); }

void TextEdit::move_document_end(bool extend) {
    move_cursor({lines_.size() - 1, lines_.back().size()}, extend);
}

void TextEdit::select_all() {
    anchor_ = {0, 0};
    cursor_ = {lines_.size() - 1, lines_.back().size()};
    sync_commands();
    reset_cursor_blink();
    ensure_cursor_visible();
    if (window_) {
        window_->request_redraw("text change");
    }
}

void TextEdit::cut() {
    if (!has_selection()) {
        return;
    }
    copy();
    delete_selection();
}

void TextEdit::copy() {
    if (!has_selection()) {
        return;
    }
    auto s = sel_start(), e = sel_end();
    auto sel = std::string{};
    for (auto i = s.line; i <= e.line; i++) {
        auto sc = (i == s.line) ? s.col : size_t{0};
        auto ec = (i == e.line) ? e.col : lines_[i].size();
        sel += lines_[i].substr(sc, ec - sc);
        if (i < e.line) {
            sel += '\n';
        }
    }
    Clipboard::set_text(sel);
    paste_available_ = true;
}

void TextEdit::paste() {
    auto clip = Clipboard::get_text();
    if (!clip.empty()) {
        insert_text(clip);
    }
}

void TextEdit::undo() {
    if (undo_stack_.undo()) {
        max_line_w_dirty_ = true;
        sync_commands();
        if (window_) {
            window_->request_redraw("undo");
        }
    }
}

void TextEdit::redo() {
    if (undo_stack_.redo()) {
        max_line_w_dirty_ = true;
        sync_commands();
        if (window_) {
            window_->request_redraw("redo");
        }
    }
}

void TextEdit::show_context_menu(Point pos) {
    if (!window()) {
        return;
    }

    std::vector<MenuItem> items;
    items.push_back(MenuItem::action(undo_cmd));
    items.push_back(MenuItem::action(redo_cmd));
    items.push_back(MenuItem::sep());
    items.push_back(MenuItem::action(cut_cmd));
    items.push_back(MenuItem::action(copy_cmd));
    items.push_back(MenuItem::action(paste_cmd));
    items.push_back(MenuItem::sep());
    items.push_back(MenuItem::action(select_all_cmd));

    auto delete_cmd = Command::create("Delete", [this] { delete_selection(); }, has_selection());
    delete_cmd->set_shortcut("Delete");
    items.push_back(MenuItem::action(delete_cmd));

    context_menu_ = std::make_unique<ContextMenu>(std::move(items));
    context_menu_->show(window(), map_to_window(pos));
}

void TextEdit::sync_commands() {
    if (!select_all_cmd) {
        return;
    }
    auto has_sel = has_selection();
    auto not_empty = lines_.size() > 1 || !lines_[0].empty();
    select_all_cmd->set_enabled(not_empty);
    cut_cmd->set_enabled(has_sel);
    copy_cmd->set_enabled(has_sel);
    paste_cmd->set_enabled(paste_available_);

    undo_cmd->set_enabled(undo_stack_.can_undo());
    if (undo_stack_.can_undo()) {
        undo_cmd->set_tooltip("Undo " + undo_stack_.undo_text());
    } else {
        undo_cmd->set_tooltip("Undo");
    }

    redo_cmd->set_enabled(undo_stack_.can_redo());
    if (undo_stack_.can_redo()) {
        redo_cmd->set_tooltip("Redo " + undo_stack_.redo_text());
    } else {
        redo_cmd->set_tooltip("Redo");
    }
}

void TextEdit::paint_background(Painter &painter) {
    auto const &pal = Theme::current().palette;
    auto local = Rect{0, 0, rect_.width, rect_.height};
    if (has_frame()) {
        auto border = state.focused ? pal.accent : pal.border;
        painter.draw_filled_frame(local, pal.base, border, pal, true);
    } else {
        painter.fill_rect(local, pal.base);
    }
}

void TextEdit::paint(Painter &painter) {
    auto const &theme = Theme::current();
    auto lh = line_height();
    auto gw = gutter_width();
    auto local_rect = Rect{0, 0, rect_.width, rect_.height};

    // Update scrollbars when width is dirty OR the line count changed.
    // The expensive width rescan only runs when max_line_w_dirty_ is set.
    // Drag and pure cursor movement skip both checks and go straight to drawing.
    if (max_line_w_dirty_ || lines_.size() != last_lines_count_) {
        update_scroll_state();
    }

    auto first = std::max(0, static_cast<int>(scroll_y_ / lh));
    auto ss = sel_start();
    auto se = sel_end();
    // draw_text_edit()'s selection params are int with -1 meaning "no
    // selection"; Position's fields are size_t, so the -1 sentinel has to be
    // applied here rather than folded into the (unsigned) ternary above.
    auto has_sel = has_selection();
    auto sel_start_line = has_sel ? static_cast<int>(ss.line) : -1;
    auto sel_start_col = has_sel ? static_cast<int>(ss.col) : -1;
    auto sel_end_line = has_sel ? static_cast<int>(se.line) : -1;
    auto sel_end_col = has_sel ? static_cast<int>(se.col) : -1;

    auto wstate = WidgetState{
        .interaction = ButtonState::Normal,
        .focused = is_focused(),
        .enabled = is_enabled(),
        .window_active = window_ ? window_->is_active() : true,
    };
    theme.draw_text_edit(painter, local_rect, lines_, static_cast<int>(cursor_.line),
                         static_cast<int>(cursor_.col), sel_start_line, sel_start_col, sel_end_line,
                         sel_end_col, first, lh, gw, scroll_x_, scroll_y_, wstate,
                         cursor_blink_time_, highlight_current_line_);

    draw_scrollbars(painter);
}

// ── Mouse ───────────────────────────────────────────────────────────────────

bool TextEdit::handle_mouse(MouseEvent const &event) {
    // Clear drag state before handle_scrollbar_mouse so the flag is not left
    // stuck when the scrollbar consumes a Release that follows a text drag.
    if (event.type == MouseEvent::Type::Release && dragging_) {
        dragging_ = false;
        reset_cursor_blink();
        sync_commands();
        if (window_) {
            window_->request_redraw("selection");
        }
        return true;
    }

    if (handle_scrollbar_mouse(event)) {
        return true;
    }

    auto local_rect = Rect{0, 0, rect_.width, rect_.height};

    if (event.type == MouseEvent::Type::Press && local_rect.contains(event.position)) {
        if (event.button == 1) {
            show_context_menu(event.position);
            return true;
        }
        auto lh = line_height();
        if (event.position.y + scroll_y_ > (int)lines_.size() * lh) {
            return false;
        }
        Position p = pos_from_point(event.position);
        if (event.click_count == 2) {
            // Select word
            auto const &ln = lines_[p.line];
            auto start = p.col, end = p.col;
            auto len = ln.size();
            if (p.col < len && !std::isspace(static_cast<unsigned char>(ln[p.col]))) {
                while (start > 0 && !std::isspace(static_cast<unsigned char>(ln[start - 1]))) {
                    start--;
                }
                while (end < len && !std::isspace(static_cast<unsigned char>(ln[end]))) {
                    end++;
                }
            }
            anchor_ = {p.line, start};
            cursor_ = {p.line, end};
            sync_commands();
            reset_cursor_blink();
            if (window_) {
                window_->request_redraw("selection");
            }
        } else if (event.click_count >= 3) {
            // Select entire line
            anchor_ = {p.line, 0};
            cursor_ = {p.line, lines_[p.line].size()};
            sync_commands();
            reset_cursor_blink();
            if (window_) {
                window_->request_redraw("selection");
            }
        } else {
            move_cursor(p, event.shift);
            dragging_ = true;
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Drag && dragging_) {
        auto prev = cursor_;
        cursor_ = pos_from_point(event.position);
        if (cursor_ == prev) {
            return true;
        }
        // Skip clipboard check — Clipboard::get_text() blocks on X11 for up to 1s
        if (cut_cmd) {
            cut_cmd->set_enabled(has_selection());
            copy_cmd->set_enabled(has_selection());
        }
        reset_cursor_blink();
        ensure_cursor_visible();
        if (window_) {
            window_->request_redraw("selection");
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Release) {
        return false;
    }

    return false;
}

// ── Keyboard ────────────────────────────────────────────────────────────────

void TextEdit::insert_newline() {
    auto const &line = lines_[cursor_.line];
    auto indent_end = 0;
    while (indent_end < (int)line.size() && (line[indent_end] == ' ' || line[indent_end] == '\t')) {
        indent_end++;
    }
    insert_text("\n" + line.substr(0, indent_end));
}

void TextEdit::delete_char_backward(bool word) {
    std::unique_ptr<TextEditCommand> cmd;
    if (word) {
        auto end_pos = cursor_;
        move_word_left(false);
        auto start_pos = cursor_;
        anchor_ = start_pos;
        cursor_ = end_pos;
        auto deleted = range_text(start_pos, end_pos);
        delete_selection_internal();
        max_line_w_dirty_ = true;
        cmd = std::make_unique<TextEditCommand>(this, start_pos, end_pos, start_pos,
                                                std::move(deleted), "", end_pos, cursor_);
    } else if (cursor_.col > 0) {
        auto prev = Utf8Iterator::prev(lines_[cursor_.line], cursor_.col);
        auto del_start = Position{cursor_.line, prev};
        auto del_end = cursor_;
        auto old_cursor = cursor_;
        auto deleted_char = lines_[cursor_.line].substr(prev, cursor_.col - prev);
        lines_[cursor_.line].erase(prev, cursor_.col - prev);
        cursor_.col = prev;
        anchor_ = cursor_;
        cmd = std::make_unique<TextEditCommand>(this, del_start, del_end, del_start,
                                                std::move(deleted_char), "", old_cursor, cursor_);
    } else if (cursor_.line > 0) {
        auto prev_len = lines_[cursor_.line - 1].size();
        auto old_cursor = cursor_;
        auto del_start = Position{cursor_.line - 1, prev_len};
        auto del_end = Position{cursor_.line, 0};
        lines_[cursor_.line - 1] += lines_[cursor_.line];
        lines_.erase(lines_.begin() + cursor_.line);
        cursor_ = {cursor_.line - 1, prev_len};
        anchor_ = cursor_;
        auto const &p = Theme::current().palette;
        auto w = measure_text(lines_[cursor_.line], p.fonts.size, kFont).width;
        if (w > cached_max_line_w_) {
            cached_max_line_w_ = w;
        }
        cmd = std::make_unique<TextEditCommand>(this, del_start, del_end, del_start, "\n", "",
                                                old_cursor, cursor_);
    }
    if (cmd) {
        undo_stack_.push(std::move(cmd));
        sync_commands();
        ensure_cursor_visible();
        if (window_) {
            window_->request_redraw("text change");
        }
        if (on_change) {
            on_change();
        }
    }
}

void TextEdit::delete_char_forward() {
    auto nlines = lines_.size();
    std::unique_ptr<TextEditCommand> cmd;
    if (cursor_.col < lines_[cursor_.line].size()) {
        auto next = Utf8Iterator::next(lines_[cursor_.line], cursor_.col);
        auto del_start = cursor_;
        auto del_end = Position{cursor_.line, next};
        auto deleted_char = lines_[cursor_.line].substr(cursor_.col, next - cursor_.col);
        lines_[cursor_.line].erase(cursor_.col, next - cursor_.col);
        cmd = std::make_unique<TextEditCommand>(this, del_start, del_end, del_start,
                                                std::move(deleted_char), "", cursor_, cursor_);
    } else if (cursor_.line + 1 < nlines) {
        auto old_cursor = cursor_;
        auto del_start = Position{cursor_.line, lines_[cursor_.line].size()};
        auto del_end = Position{cursor_.line + 1, 0};
        lines_[cursor_.line] += lines_[cursor_.line + 1];
        lines_.erase(lines_.begin() + cursor_.line + 1);
        auto const &p = Theme::current().palette;
        auto w = measure_text(lines_[cursor_.line], p.fonts.size, kFont).width;
        if (w > cached_max_line_w_) {
            cached_max_line_w_ = w;
        }
        cmd = std::make_unique<TextEditCommand>(this, del_start, del_end, del_start, "\n", "",
                                                old_cursor, cursor_);
    }
    if (cmd) {
        undo_stack_.push(std::move(cmd));
        sync_commands();
        ensure_cursor_visible();
        if (window_) {
            window_->request_redraw("text change");
        }
        if (on_change) {
            on_change();
        }
    }
}

void TextEdit::indent_selection(bool unindent, int spaces) {
    auto first_line = has_selection() ? sel_start().line : cursor_.line;
    auto last_sel = has_selection() ? sel_end() : cursor_;
    // A selection ending at col 0 doesn't visually include that line
    auto last_line =
        (last_sel.col == 0 && last_sel.line > first_line) ? last_sel.line - 1 : last_sel.line;
    auto multi_line = last_line > first_line;

    if (!unindent && !multi_line) {
        insert_text(std::string(spaces, ' '));
        return;
    }

    auto old_cursor = cursor_;
    auto changed = false;
    auto range_start = Position{first_line, 0};
    auto range_end_before = Position{last_line, lines_[last_line].size()};
    auto text_before = range_text(range_start, range_end_before);
    auto indent = std::string(spaces, ' ');

    if (unindent) {
        for (auto i = first_line; i <= last_line; i++) {
            size_t sp = 0;
            while (sp < static_cast<size_t>(spaces) && sp < lines_[i].size() &&
                  lines_[i][sp] == ' ') {
                sp++;
            }
            if (sp > 0) {
                lines_[i].erase(0, sp);
                // sp counted leading spaces actually present on line i, but
                // the cursor/anchor column on that line can still be less
                // than sp -- guard the subtraction instead of letting it
                // underflow (col is unsigned).
                if (cursor_.line == i) {
                    cursor_.col = (sp > cursor_.col) ? 0 : cursor_.col - sp;
                }
                if (anchor_.line == i) {
                    anchor_.col = (sp > anchor_.col) ? 0 : anchor_.col - sp;
                }
                changed = true;
            }
        }
    } else {
        for (auto i = first_line; i <= last_line; i++) {
            lines_[i].insert(0, indent);
        }
        if (cursor_.line >= first_line && cursor_.line <= last_line) {
            cursor_.col += spaces;
        }
        if (anchor_.line >= first_line && anchor_.line <= last_line) {
            anchor_.col += spaces;
        }
        changed = true;
    }

    if (changed) {
        auto range_end_after = Position{last_line, lines_[last_line].size()};
        auto text_after = range_text(range_start, range_end_after);
        max_line_w_dirty_ = true;
        undo_stack_.push(std::make_unique<TextEditCommand>(
            this, range_start, range_end_before, range_end_after, std::move(text_before),
            std::move(text_after), old_cursor, cursor_));
        sync_commands();
        ensure_cursor_visible();
        if (window_) {
            window_->request_redraw("indent");
        }
        if (on_change) {
            on_change();
        }
    }
}

bool TextEdit::handle_key(KeyEvent const &event) {
    if (Widget::handle_key(event)) {
        return true;
    }
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }
    auto nlines = lines_.size();

    reset_cursor_blink();
    ensure_cursor_visible();

    switch (event.key) {
    case Key::Enter:
        insert_newline();
        return true;

    case Key::Backspace:
        if (has_selection()) {
            delete_selection();
        } else {
            delete_char_backward(event.alt);
        }
        return true;

    case Key::Delete:
        if (has_selection()) {
            delete_selection();
        } else {
            delete_char_forward();
        }
        return true;

    case Key::Left:
        if (event.alt || event.ctrl) {
            move_word_left(event.shift);
        } else if (!event.shift && has_selection()) {
            move_cursor(sel_start(), false);
        } else if (cursor_.col > 0) {
            move_cursor({cursor_.line, Utf8Iterator::prev(lines_[cursor_.line], cursor_.col)},
                        event.shift);
        } else if (cursor_.line > 0) {
            move_cursor({cursor_.line - 1, lines_[cursor_.line - 1].size()}, event.shift);
        }
        return true;

    case Key::Right:
        if (event.alt || event.ctrl) {
            move_word_right(event.shift);
        } else if (!event.shift && has_selection()) {
            move_cursor(sel_end(), false);
        } else if (cursor_.col < lines_[cursor_.line].size()) {
            move_cursor({cursor_.line, Utf8Iterator::next(lines_[cursor_.line], cursor_.col)},
                        event.shift);
        } else if (cursor_.line + 1 < nlines) {
            move_cursor({cursor_.line + 1, 0}, event.shift);
        }
        return true;

    case Key::Up:
        if (cursor_.line > 0) {
            auto col = std::min(cursor_.col, lines_[cursor_.line - 1].size());
            move_cursor({cursor_.line - 1, col}, event.shift);
        } else {
            move_cursor({0, 0}, event.shift);
        }
        return true;

    case Key::Down:
        if (cursor_.line + 1 < nlines) {
            auto col = std::min(cursor_.col, lines_[cursor_.line + 1].size());
            move_cursor({cursor_.line + 1, col}, event.shift);
        } else {
            move_cursor({cursor_.line, lines_[cursor_.line].size()}, event.shift);
        }
        return true;

    case Key::PageUp: {
        auto lh = line_height();
        auto vr = viewport_rect();
        auto page_lines =
            static_cast<size_t>(std::max(1, static_cast<int>(vr.height / lh) - 1));
        auto new_line = (page_lines > cursor_.line) ? size_t{0} : cursor_.line - page_lines;
        auto col = std::min(cursor_.col, lines_[new_line].size());
        move_cursor({new_line, col}, event.shift);
        return true;
    }

    case Key::PageDown: {
        auto lh = line_height();
        auto vr = viewport_rect();
        auto page_lines =
            static_cast<size_t>(std::max(1, static_cast<int>(vr.height / lh) - 1));
        auto new_line = std::min(nlines - 1, cursor_.line + page_lines);
        auto col = std::min(cursor_.col, lines_[new_line].size());
        move_cursor({new_line, col}, event.shift);
        return true;
    }

    case Key::Home:
        if (event.ctrl || event.super) {
            move_document_start(event.shift);
        } else {
            move_cursor({cursor_.line, 0}, event.shift);
        }
        return true;

    case Key::End:
        if (event.ctrl || event.super) {
            move_document_end(event.shift);
        } else {
            move_cursor({cursor_.line, lines_[cursor_.line].size()}, event.shift);
        }
        return true;

    case Key::Tab:
        if (event.alt || event.ctrl || event.super) {
            return false;
        }
        indent_selection(event.shift);
        return true;

    default:
        break;
    }

    if (!event.text.empty() && !event.super && !event.ctrl) {
        insert_text(event.text);
        return true;
    }

    return false;
}

Size TextEdit::size_hint() const { return {0, 200}; }

} // namespace svision3
