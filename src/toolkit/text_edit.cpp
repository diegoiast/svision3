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
    focusable_ = true;
    set_text(text);
    cursor_blink_time_ = std::chrono::steady_clock::now();
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
}

void TextEdit::set_focused(bool focused) {
    focused_ = focused;
    if (focused) {
        reset_cursor_blink();
    } else {
        anchor_ = cursor_;
    }
}

void TextEdit::reset_cursor_blink() { cursor_blink_time_ = std::chrono::steady_clock::now(); }

static constexpr FontFamily kFont = FontFamily::Monospace;

float TextEdit::line_height() const {
    auto const &style = Theme::current().text_edit;
    auto fm = Painter::measure_font_metrics(style.font_size, kFont);
    return std::max(fm.height, style.font_size) + 2.0f;
}

float TextEdit::gutter_width() const {
    auto const &style = Theme::current().text_edit;
    auto digits = 1;
    auto n = static_cast<int>(lines_.size());
    while (n >= 10) {
        digits++;
        n /= 10;
    }
    digits = std::max(digits, 2);
    return Painter::measure_text(std::string(digits, '9'), style.font_size, kFont).width + 16.0f;
}

TextEdit::Pos TextEdit::pos_from_point(Point p) const {
    auto const &style = Theme::current().text_edit;
    auto lh = line_height();
    auto gw = gutter_width();
    auto line = static_cast<int>((p.y + scroll_y_) / lh);
    auto click_x = p.x - gw + scroll_x_;

    if (click_x <= 0) {
        return {line, 0};
    }

    line = std::clamp(line, 0, static_cast<int>(lines_.size()) - 1);
    auto const &ln = lines_[line];
    auto col = 0;
    while (col < (int)ln.size()) {
        auto next_col = Utf8Iterator::next(ln, col);
        auto w = Painter::measure_text(ln.substr(0, next_col), style.font_size, kFont).width;

        if (w > click_x) {
            // Check if closer to current or next character
            auto prev_w = Painter::measure_text(ln.substr(0, col), style.font_size, kFont).width;
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
    auto content_h = lh * static_cast<float>(lines_.size());
    auto visible_h = rect_.height;
    auto max_line_w = 0.0f;
    auto const &style = Theme::current().text_edit;

    scroll_y_ = std::clamp(scroll_y_, 0.0f, std::max(0.0f, content_h - visible_h));
    for (auto const &ln : lines_) {
        float w = Painter::measure_text(ln, style.font_size, kFont).width;
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
        cx = Painter::measure_text(lines_[cursor_.line].substr(0, cursor_.col), style.font_size,
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

void TextEdit::paint(Painter &painter) {
    auto const &style = Theme::current().text_edit;
    auto lh = line_height();
    auto gw = gutter_width();
    auto fm = painter.font_metrics(style.font_size, kFont);
    auto bg = focused_ ? style.background_focused : style.background;
    auto border = focused_ ? style.border_focused : style.border;
    auto local_rect = Rect{0, 0, rect_.width, rect_.height};

    painter.draw_frame(local_rect, bg, border, style, true);
    painter.push_clip(local_rect);
    clamp_scroll();

    auto first = std::max(0, static_cast<int>(scroll_y_ / lh));
    auto last = std::min(static_cast<int>(lines_.size()) - 1,
                         static_cast<int>((scroll_y_ + rect_.height) / lh));

    // Gutter background
    // FIXME: use colors from theme
    auto gutter_rect = Rect{0, 0, gw, rect_.height};
    auto gutter_bg = style.background.darken(0.03f);
    painter.fill_rect(gutter_rect, gutter_bg);

    for (auto i = first; i <= last; i++) {
        auto y = lh * i - scroll_y_;
        auto baseline = y + (lh - fm.height) / 2.0f + fm.ascent;
        auto num = std::to_string(i + 1);
        auto nw = Painter::measure_text(num, style.font_size, kFont).width;

        painter.draw_text(num, {gw - nw - 8.0f, baseline}, style.placeholder, style.font_size,
                          kFont);
    }

    // Text area clipping
    auto text_area = Rect{gw, 0, rect_.width - gw, rect_.height};
    auto tx0 = gw - scroll_x_;
    auto ss = sel_start();
    auto se = sel_end();
    auto sel_bg = Color::rgba(0.26f, 0.52f, 0.96f, 0.35f);

    painter.push_clip(text_area);
    for (auto i = first; i <= last; i++) {
        auto y = lh * i - scroll_y_;
        auto baseline = y + (lh - fm.height) / 2.0f + fm.ascent;

        // Selection highlight
        // FIXME: do we need static cast? This is barely readable
        if (has_selection() && Pos{i, 0} <= se &&
            ss < Pos{i, static_cast<int>(lines_[i].size()) + 1}) {
            auto sc = (i == ss.line) ? ss.col : 0;
            auto ec = (i == se.line) ? se.col : static_cast<int>(lines_[i].size());
            auto sx =
                tx0 +
                (sc > 0
                     ? Painter::measure_text(lines_[i].substr(0, sc), style.font_size, kFont).width
                     : 0.0f);
            auto ex =
                tx0 + Painter::measure_text(lines_[i].substr(0, ec), style.font_size, kFont).width;
            if (i != se.line) {
                ex += style.font_size * 0.4f;
            }
            painter.fill_rect({sx, y, ex - sx, lh}, sel_bg);
        }

        painter.draw_text(lines_[i], {tx0, baseline}, style.text, style.font_size, kFont);
    }

    // Cursor
    if (focused_) {
        auto elapsed = std::chrono::steady_clock::now() - cursor_blink_time_;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        if ((ms / 500) % 2 == 0) {
            auto cy = lh * cursor_.line - scroll_y_;
            auto cx = tx0;
            if (cursor_.col > 0) {
                cx += Painter::measure_text(lines_[cursor_.line].substr(0, cursor_.col),
                                            style.font_size, kFont)
                          .width;
            }
            painter.draw_line({cx, cy}, {cx, cy + lh}, style.cursor, 1.5f);
        }
    }

    painter.pop_clip(); // text area

    // ── Scrollbars ──────────────────────────────────────────────────────────
    auto content_h = lh * static_cast<float>(lines_.size());
    auto visible_h = rect_.height;
    if (content_h > visible_h) {
        auto bar_h = std::max(20.0f, visible_h * (visible_h / content_h));
        auto bar_y = (scroll_y_ / content_h) * visible_h;
        auto sb = Rect{rect_.width - 6.0f, bar_y, 4.0f, bar_h};

        painter.fill_rounded_rect(sb, Color::rgba(style.text.r, style.text.g, style.text.b, 0.25f),
                                  2.0f);
    }

    auto max_line_w = 0.0f;
    for (int i = first; i <= last; i++) {
        float w = Painter::measure_text(lines_[i], style.font_size, kFont).width;
        if (w > max_line_w) {
            max_line_w = w;
        }
    }
    auto content_w = max_line_w + 20.0f;
    auto visible_w = rect_.width - gw;
    if (content_w > visible_w) {
        auto bar_w = std::max(20.0f, rect_.width * (visible_w / content_w));
        auto bar_x = (scroll_x_ / content_w) * rect_.width;
        Rect sb{bar_x, rect_.height - 6.0f, bar_w, 4.0f};
        painter.fill_rounded_rect(sb, Color::rgba(style.text.r, style.text.g, style.text.b, 0.25f),
                                  2.0f);
    }

    painter.pop_clip(); // outer

    if (focused_) {
        painter.draw_focus_ring(local_rect, style.corner_radius);
    }
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
            reset_cursor_blink();
        } else if (event.click_count >= 3) {
            // Select entire line
            anchor_ = {p.line, 0};
            cursor_ = {p.line, static_cast<int>(lines_[p.line].size())};
            reset_cursor_blink();
        } else {
            move_cursor(p, event.shift);
            dragging_ = true;
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Drag && dragging_) {
        cursor_ = pos_from_point(event.position);
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

    // FIXME handle commands using proper actions, not by handling direct keyboard presses
    if (event.super && !event.text.empty()) {
        auto ch = event.text[0];
        if (ch == 'a') {
            anchor_ = {0, 0};
            cursor_ = {nlines - 1, static_cast<int>(lines_.back().size())};
            return true;
        }
        if (ch == 'x') {
            if (!has_selection()) {
                return true;
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
            delete_selection();
            return true;
        }
        if (ch == 'c') {
            if (!has_selection()) {
                return true;
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
            return true;
        }
        if (ch == 'v') {
            auto clip = Clipboard::get_text();
            if (!clip.empty()) {
                insert_text(clip);
            }
            return true;
        }
    }

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
            ensure_cursor_visible();
            if (on_change) {
                on_change();
            }
        } else if (cursor_.line + 1 < nlines) {
            lines_[cursor_.line] += lines_[cursor_.line + 1];
            lines_.erase(lines_.begin() + cursor_.line + 1);
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
