// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

// FIXME: More documentation on this widget, architecture, cleanups
//        I know writing a text editor component is not trivial. But this
//        looks like a major cleanup, or refactor is needed.
//        Cursor management, drawing scrollbars, gutters, this is all baked in
//        without any way to customize. Its a start, but not something
//        that can be used in production.

#include "toolkit/text_edit.hpp"
#include "toolkit/clipboard.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/utf8.hpp"
#include "toolkit/window.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace toolkit {

TextEdit::TextEdit(std::string text) {
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

    set_text(text);
}

std::string TextEdit::text() const {
    std::string result;
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
    cursor_ = {0, 0};
    anchor_ = cursor_;
    scroll_x_ = scroll_y_ = 0;
    sync_commands();
    if (window_) {
        window_->request_redraw("text change");
    }
}

TextEdit& TextEdit::set_focused(bool focused) {
    Widget::set_focused(focused);
    if (!focused) {
        anchor_ = cursor_;
    }
    sync_commands();
    return *this;
}

void TextEdit::on_focus() {
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

static constexpr FontFamily kFont = FontFamily::Monospace;

float TextEdit::line_height() const {
    auto const &style = Theme::current().text_edit;
    auto const &palette = Theme::current().palette;
    auto fm = Painter::measure_font_metrics(palette.font_size, kFont);
    return std::max(fm.height, palette.font_size) + 2.0f;
}

float TextEdit::gutter_width() const {
    auto const &style = Theme::current().text_edit;
    auto const &palette = Theme::current().palette;
    auto digits = 1;
    auto n = static_cast<int>(lines_.size());
    while (n >= 10) {
        digits++;
        n /= 10;
    }
    digits = std::max(digits, 2);
    return Painter::measure_text(std::string(digits, '9'), palette.font_size, kFont).width + 16.0f;
}

TextEdit::Pos TextEdit::pos_from_point(Point p) const {
    auto const &style = Theme::current().text_edit;
    auto const &palette = Theme::current().palette;
    auto lh = line_height();
    auto gw = gutter_width();

    if (lh <= 0.0001f) {
        return {0, 0};
    }

    auto line = static_cast<int>((p.y + scroll_y_) / lh);
    auto click_x = p.x - gw + scroll_x_;
    line = std::clamp(line, 0, static_cast<int>(lines_.size()) - 1);

    if (click_x <= 0) {
        return {line, 0};
    }

    auto const &ln = lines_[line];
    auto col = 0;
    while (col < (int)ln.size()) {
        auto next_col = Utf8Iterator::next(ln, col);
        auto w = Painter::measure_text(ln.substr(0, next_col), palette.font_size, kFont).width;

        if (w > click_x) {
            auto prev_w = Painter::measure_text(ln.substr(0, col), palette.font_size, kFont).width;
            if (click_x - prev_w < w - click_x) {
                return {line, static_cast<int>(col)};
            } else {
                return {line, static_cast<int>(next_col)};
            }
        }
        col = (int)next_col;
    }
    return {line, static_cast<int>(col)};
}

void TextEdit::clamp_scroll() {
    auto lh = line_height();
    auto const &palette = Theme::current().palette;
    auto content_h = lh * static_cast<float>(lines_.size());
    auto visible_h = rect_.height;
    auto max_line_w = 0.0f;

    scroll_y_ = std::clamp(scroll_y_, 0.0f, std::max(0.0f, content_h - visible_h));
    for (auto const &ln : lines_) {
        float w = Painter::measure_text(ln, palette.font_size, kFont).width;
        if (w > max_line_w) {
            max_line_w = w;
        }
    }
    auto gw = gutter_width();
    auto visible_w = rect_.width - gw;

    // FIXME what is this 20.0f?
    scroll_x_ = std::clamp(scroll_x_, 0.0f, std::max(0.0f, max_line_w + 20.0f - visible_w));
}

void TextEdit::ensure_cursor_visible() {
    auto const &style = Theme::current().text_edit;
    auto const &palette = Theme::current().palette;

    auto lh = line_height();
    auto gw = gutter_width();
    auto cy = lh * cursor_.line;
    auto visible_h = rect_.height;

    if (cy + lh > scroll_y_ + visible_h) {
        scroll_y_ = cy + lh - visible_h;
    }
    if (cy < scroll_y_) {
        scroll_y_ = cy;
    }

    auto cx = 0.0f;
    auto visible_w = rect_.width - gw;

    if (cursor_.col > 0) {
        cx = Painter::measure_text(lines_[cursor_.line].substr(0, cursor_.col), palette.font_size,
                                   kFont)
                 .width;
    }
    // FIXME: what is this 10.0f?
    if (cx - scroll_x_ > visible_w - 10.0f) {
        scroll_x_ = cx - visible_w + 10.0f;
    }
    if (cx - scroll_x_ < 0) {
        scroll_x_ = cx;
    }

    clamp_scroll();
}

void TextEdit::move_cursor(Pos p, bool extend_selection) {
    cursor_ = p;
    if (!extend_selection) {
        anchor_ = cursor_;
    }
    sync_commands();
    reset_cursor_blink();
    ensure_cursor_visible();
}

void TextEdit::delete_selection() {
    if (!has_selection()) {
        return;
    }
    auto s = sel_start();
    auto e = sel_end();

    if (s.line == e.line) {
        lines_[s.line].erase(s.col, e.col - s.col);
    } else {
        auto merged = lines_[s.line].substr(0, s.col) + lines_[e.line].substr(e.col);
        lines_.erase(lines_.begin() + s.line, lines_.begin() + e.line + 1);
        lines_.insert(lines_.begin() + s.line, std::move(merged));
    }
    cursor_ = anchor_ = s;
    sync_commands();
    ensure_cursor_visible();
    if (on_change) {
        on_change();
    }
}

void TextEdit::insert_text(std::string_view t) {
    if (has_selection()) {
        delete_selection();
    }

    for (auto i = 0; i < (int)t.size(); i++) {
        // FIXME: support for unicode UTF/8
        // FIXME: support for unicode char - new paragraph
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
    sync_commands();
    ensure_cursor_visible();
    if (on_change) {
        on_change();
    }
}

void TextEdit::move_word_left(bool extend) {
    auto line = cursor_.line;
    auto col = cursor_.col;

    if (col == 0 && line > 0) {
        line--;
        col = static_cast<int>(lines_[line].size());
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
    auto len = static_cast<int>(ln.size());

    if (col >= len && line + 1 < static_cast<int>(lines_.size())) {
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

void TextEdit::select_all() {
    anchor_ = {0, 0};
    cursor_ = {static_cast<int>(lines_.size()) - 1, static_cast<int>(lines_.back().size())};
    sync_commands();
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
        auto sc = (i == s.line) ? s.col : 0;
        auto ec = (i == e.line) ? e.col : static_cast<int>(lines_[i].size());
        sel += lines_[i].substr(sc, ec - sc);
        if (i < e.line) {
            sel += '\n';
        }
    }
    Clipboard::set_text(sel);
}

void TextEdit::paste() {
    auto clip = Clipboard::get_text();
    if (!clip.empty()) {
        insert_text(clip);
    }
}

void TextEdit::sync_commands() {
    if (!select_all_cmd) {
        return;
    }
    bool has_sel = has_selection();
    bool not_empty = lines_.size() > 1 || !lines_[0].empty();
    select_all_cmd->set_enabled(not_empty);
    cut_cmd->set_enabled(has_sel);
    copy_cmd->set_enabled(has_sel);
    paste_cmd->set_enabled(!Clipboard::get_text().empty());
}

void TextEdit::paint(Painter &painter) {
    auto const &theme = Theme::current();
    auto lh = line_height();
    auto gw = gutter_width();
    auto local_rect = Rect{0, 0, rect_.width, rect_.height};

    clamp_scroll();

    auto first = std::max(0, static_cast<int>(scroll_y_ / lh));

    auto ss = sel_start();
    auto se = sel_end();

    auto sel_start_line = has_selection() ? ss.line : -1;
    auto sel_start_col = has_selection() ? ss.col : -1;
    auto sel_end_line = has_selection() ? se.line : -1;
    auto sel_end_col = has_selection() ? se.col : -1;

    theme.draw_text_edit(painter, local_rect, lines_, cursor_.line, cursor_.col, sel_start_line,
                         sel_start_col, sel_end_line, sel_end_col, first, lh, gw, scroll_x_,
                         scroll_y_, is_focused(), is_enabled(), cursor_blink_time_);
}

// ── Mouse ───────────────────────────────────────────────────────────────────

bool TextEdit::handle_mouse(MouseEvent const &event) {
    auto local_rect = Rect{0, 0, rect_.width, rect_.height};
    if (event.type == MouseEvent::Type::Scroll && local_rect.contains(event.position)) {
        scroll_y_ -= event.scroll_dy;
        scroll_x_ -= event.scroll_dx;
        clamp_scroll();
        return true;
    }

    if (event.type == MouseEvent::Type::Press && local_rect.contains(event.position)) {
        auto lh = line_height();
        if (event.position.y + scroll_y_ > (int)lines_.size() * lh) {
            return false;
        }
        Pos p = pos_from_point(event.position);
        if (event.click_count == 2) {
            // Select word
            auto const &ln = lines_[p.line];
            auto start = p.col, end = p.col;
            auto len = static_cast<int>(ln.size());
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
        } else if (event.click_count >= 3) {
            // Select entire line
            anchor_ = {p.line, 0};
            cursor_ = {p.line, static_cast<int>(lines_[p.line].size())};
            sync_commands();
            reset_cursor_blink();
        } else {
            move_cursor(p, event.shift);
            dragging_ = true;
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Drag && dragging_) {
        cursor_ = pos_from_point(event.position);
        sync_commands();
        reset_cursor_blink();
        return true;
    }

    if (event.type == MouseEvent::Type::Release) {
        dragging_ = false;
        return false;
    }

    return false;
}

// ── Keyboard ────────────────────────────────────────────────────────────────

bool TextEdit::handle_key(KeyEvent const &event) {
    if (Widget::handle_key(event)) {
        return true;
    }
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }
    auto nlines = static_cast<int>(lines_.size());

    reset_cursor_blink();
    ensure_cursor_visible();

    switch (event.key) {
    case Key::Enter:
        // FIXME: add better API for entering new lines
        insert_text("\n");
        return true;

    case Key::Backspace:
        if (has_selection()) {
            delete_selection();
        } else if (event.alt) {
            auto old = cursor_;
            move_word_left(false);
            anchor_ = cursor_;
            cursor_ = old;
            delete_selection();
        } else if (cursor_.col > 0) {
            auto prev = Utf8Iterator::prev(lines_[cursor_.line], cursor_.col);
            lines_[cursor_.line].erase(prev, cursor_.col - prev);
            cursor_.col = static_cast<int>(prev);
            anchor_ = cursor_;
            sync_commands();
            ensure_cursor_visible();
            if (on_change) {
                on_change();
            }
        } else if (cursor_.line > 0) {
            auto prev_len = static_cast<int>(lines_[cursor_.line - 1].size());
            lines_[cursor_.line - 1] += lines_[cursor_.line];
            lines_.erase(lines_.begin() + cursor_.line);
            cursor_ = {cursor_.line - 1, prev_len};
            anchor_ = cursor_;
            sync_commands();
            ensure_cursor_visible();
            if (on_change) {
                on_change();
            }
        }
        return true;

    case Key::Delete:
        if (has_selection()) {
            delete_selection();
        } else if (cursor_.col < static_cast<int>(lines_[cursor_.line].size())) {
            auto next = Utf8Iterator::next(lines_[cursor_.line], cursor_.col);
            lines_[cursor_.line].erase(cursor_.col, next - cursor_.col);
            sync_commands();
            ensure_cursor_visible();
            if (on_change) {
                on_change();
            }
        } else if (cursor_.line + 1 < nlines) {
            lines_[cursor_.line] += lines_[cursor_.line + 1];
            lines_.erase(lines_.begin() + cursor_.line + 1);
            sync_commands();
            ensure_cursor_visible();
            if (on_change) {
                on_change();
            }
        }
        return true;

    case Key::Left:
        if (event.alt) {
            move_word_left(event.shift);
        } else if (!event.shift && has_selection()) {
            move_cursor(sel_start(), false);
        } else if (cursor_.col > 0) {
            move_cursor({cursor_.line,
                         static_cast<int>(Utf8Iterator::prev(lines_[cursor_.line], cursor_.col))},
                        event.shift);
        } else if (cursor_.line > 0) {
            move_cursor({cursor_.line - 1, static_cast<int>(lines_[cursor_.line - 1].size())},
                        event.shift);
        }
        return true;

    case Key::Right:
        if (event.alt) {
            move_word_right(event.shift);
        } else if (!event.shift && has_selection()) {
            move_cursor(sel_end(), false);
        } else if (cursor_.col < static_cast<int>(lines_[cursor_.line].size())) {
            move_cursor({cursor_.line,
                         static_cast<int>(Utf8Iterator::next(lines_[cursor_.line], cursor_.col))},
                        event.shift);
        } else if (cursor_.line + 1 < nlines) {
            move_cursor({cursor_.line + 1, 0}, event.shift);
        }
        return true;

    case Key::Up:
        if (cursor_.line > 0) {
            auto col = std::min(cursor_.col, static_cast<int>(lines_[cursor_.line - 1].size()));
            move_cursor({cursor_.line - 1, col}, event.shift);
        } else {
            move_cursor({0, 0}, event.shift);
        }
        return true;

    case Key::Down:
        if (cursor_.line + 1 < nlines) {
            auto col = std::min(cursor_.col, static_cast<int>(lines_[cursor_.line + 1].size()));
            move_cursor({cursor_.line + 1, col}, event.shift);
        } else {
            move_cursor({cursor_.line, static_cast<int>(lines_[cursor_.line].size())}, event.shift);
        }
        return true;

    case Key::Home:
        if (event.super) {
            move_cursor({0, 0}, event.shift);
        } else {
            move_cursor({cursor_.line, 0}, event.shift);
        }
        return true;

    case Key::End:
        if (event.super) {
            move_cursor({nlines - 1, static_cast<int>(lines_.back().size())}, event.shift);
        } else {
            move_cursor({cursor_.line, static_cast<int>(lines_[cursor_.line].size())}, event.shift);
        }
        return true;

    case Key::Tab:
        insert_text("    ");
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

} // namespace toolkit
