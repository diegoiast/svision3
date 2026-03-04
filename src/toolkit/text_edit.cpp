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
    for (size_t i = 0; i < lines_.size(); i++) {
        if (i > 0) result += '\n';
        result += lines_[i];
    }
    return result;
}

void TextEdit::set_text(std::string const &text) {
    lines_.clear();
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line))
        lines_.push_back(std::move(line));
    if (lines_.empty()) lines_.emplace_back();
    cursor_ = {0, 0};
    anchor_ = cursor_;
    scroll_x_ = scroll_y_ = 0;
}

void TextEdit::set_focused(bool focused) {
    focused_ = focused;
    if (focused)
        reset_cursor_blink();
    else
        anchor_ = cursor_;
}

void TextEdit::reset_cursor_blink() {
    cursor_blink_time_ = std::chrono::steady_clock::now();
}

static constexpr FontFamily kFont = FontFamily::Monospace;

float TextEdit::line_height() const {
    auto const &style = Theme::current().text_edit;
    auto fm = Painter::measure_font_metrics(style.font_size, kFont);
    return fm.height + 2.0f;
}

float TextEdit::gutter_width() const {
    auto const &style = Theme::current().text_edit;
    int digits = 1;
    int n = static_cast<int>(lines_.size());
    while (n >= 10) { digits++; n /= 10; }
    digits = std::max(digits, 2);
    return Painter::measure_text(std::string(digits, '9'), style.font_size, kFont).width + 16.0f;
}

TextEdit::Pos TextEdit::pos_from_point(Point p) const {
    auto const &style = Theme::current().text_edit;
    float lh = line_height();
    float gw = gutter_width();

    int line = static_cast<int>((p.y - rect_.y + scroll_y_) / lh);
    line = std::clamp(line, 0, static_cast<int>(lines_.size()) - 1);

    float click_x = p.x - rect_.x - gw + scroll_x_;
    if (click_x <= 0) return {line, 0};

    auto const &ln = lines_[line];
    size_t col = 0;
    while (col < ln.size()) {
        size_t next_col = Utf8Iterator::next(ln, col);
        float w = Painter::measure_text(ln.substr(0, next_col), style.font_size, kFont).width;
        if (w > click_x) {
            // Check if closer to current or next character
            float prev_w = Painter::measure_text(ln.substr(0, col), style.font_size, kFont).width;
            if (click_x - prev_w < w - click_x) {
                return {line, static_cast<int>(col)};
            } else {
                return {line, static_cast<int>(next_col)};
            }
        }
        col = next_col;
    }
    return {line, static_cast<int>(col)};
}

void TextEdit::clamp_scroll() {
    float lh = line_height();
    float content_h = lh * static_cast<float>(lines_.size());
    float visible_h = rect_.height;
    scroll_y_ = std::clamp(scroll_y_, 0.0f, std::max(0.0f, content_h - visible_h));

    float max_line_w = 0;
    auto const &style = Theme::current().text_edit;
    for (auto const &ln : lines_) {
        float w = Painter::measure_text(ln, style.font_size, kFont).width;
        if (w > max_line_w) max_line_w = w;
    }
    float gw = gutter_width();
    float visible_w = rect_.width - gw;
    scroll_x_ = std::clamp(scroll_x_, 0.0f, std::max(0.0f, max_line_w + 20.0f - visible_w));
}

void TextEdit::ensure_cursor_visible() {
    auto const &style = Theme::current().text_edit;
    float lh = line_height();
    float gw = gutter_width();

    float cy = lh * cursor_.line;
    float visible_h = rect_.height;
    if (cy + lh > scroll_y_ + visible_h)
        scroll_y_ = cy + lh - visible_h;
    if (cy < scroll_y_)
        scroll_y_ = cy;

    float cx = 0;
    if (cursor_.col > 0)
        cx = Painter::measure_text(lines_[cursor_.line].substr(0, cursor_.col),
                                   style.font_size, kFont)
                 .width;
    float visible_w = rect_.width - gw;
    if (cx - scroll_x_ > visible_w - 10.0f)
        scroll_x_ = cx - visible_w + 10.0f;
    if (cx - scroll_x_ < 0)
        scroll_x_ = cx;

    clamp_scroll();
}

void TextEdit::move_cursor(Pos p, bool extend_selection) {
    cursor_ = p;
    if (!extend_selection)
        anchor_ = cursor_;
    reset_cursor_blink();
    ensure_cursor_visible();
}

void TextEdit::delete_selection() {
    if (!has_selection()) return;
    Pos s = sel_start();
    Pos e = sel_end();

    if (s.line == e.line) {
        lines_[s.line].erase(s.col, e.col - s.col);
    } else {
        std::string merged = lines_[s.line].substr(0, s.col) +
                             lines_[e.line].substr(e.col);
        lines_.erase(lines_.begin() + s.line, lines_.begin() + e.line + 1);
        lines_.insert(lines_.begin() + s.line, std::move(merged));
    }
    cursor_ = anchor_ = s;
    ensure_cursor_visible();
    if (on_change) on_change();
}

void TextEdit::insert_text(std::string_view t) {
    if (has_selection()) delete_selection();

    for (size_t i = 0; i < t.size(); i++) {
        if (t[i] == '\n' || t[i] == '\r') {
            if (t[i] == '\r' && i + 1 < t.size() && t[i + 1] == '\n')
                i++;
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
    if (on_change) on_change();
}

void TextEdit::move_word_left(bool extend) {
    int line = cursor_.line;
    int col = cursor_.col;
    if (col == 0 && line > 0) {
        line--;
        col = static_cast<int>(lines_[line].size());
    }
    auto const &ln = lines_[line];
    while (col > 0 && std::isspace(static_cast<unsigned char>(ln[col - 1])))
        col--;
    while (col > 0 && !std::isspace(static_cast<unsigned char>(ln[col - 1])))
        col--;
    move_cursor({line, col}, extend);
}

void TextEdit::move_word_right(bool extend) {
    int line = cursor_.line;
    int col = cursor_.col;
    auto const &ln = lines_[line];
    int len = static_cast<int>(ln.size());
    if (col >= len && line + 1 < static_cast<int>(lines_.size())) {
        line++;
        col = 0;
    } else {
        while (col < len && !std::isspace(static_cast<unsigned char>(ln[col])))
            col++;
        while (col < len && std::isspace(static_cast<unsigned char>(ln[col])))
            col++;
    }
    move_cursor({line, col}, extend);
}

// ── Paint ───────────────────────────────────────────────────────────────────

void TextEdit::paint(Painter &painter) {
    auto const &style = Theme::current().text_edit;
    float lh = line_height();
    float gw = gutter_width();
    auto fm = painter.font_metrics(style.font_size, kFont);

    Color bg = focused_ ? style.background_focused : style.background;
    Color border = focused_ ? style.border_focused : style.border;
    painter.draw_frame(rect_, bg, border, style, true);
    painter.push_clip(rect_);

    clamp_scroll();

    int first = std::max(0, static_cast<int>(scroll_y_ / lh));
    int last = std::min(static_cast<int>(lines_.size()) - 1,
                        static_cast<int>((scroll_y_ + rect_.height) / lh));

    // Gutter background
    Rect gutter_rect{rect_.x, rect_.y, gw, rect_.height};
    Color gutter_bg = style.background.darken(0.03f);
    painter.fill_rect(gutter_rect, gutter_bg);

    for (int i = first; i <= last; i++) {
        float y = rect_.y + lh * i - scroll_y_;
        float baseline = y + (lh - fm.height) / 2.0f + fm.ascent;

        // Line number
        std::string num = std::to_string(i + 1);
        float nw = Painter::measure_text(num, style.font_size, kFont).width;
        painter.draw_text(num, {rect_.x + gw - nw - 8.0f, baseline},
                          style.placeholder, style.font_size, kFont);
    }

    // Text area clipping
    Rect text_area{rect_.x + gw, rect_.y, rect_.width - gw, rect_.height};
    painter.push_clip(text_area);

    float tx0 = rect_.x + gw - scroll_x_;

    Pos ss = sel_start();
    Pos se = sel_end();
    Color sel_bg = Color::rgba(0.26f, 0.52f, 0.96f, 0.35f);

    for (int i = first; i <= last; i++) {
        float y = rect_.y + lh * i - scroll_y_;
        float baseline = y + (lh - fm.height) / 2.0f + fm.ascent;

        // Selection highlight
        if (has_selection() && Pos{i, 0} <= se &&
            ss < Pos{i, static_cast<int>(lines_[i].size()) + 1}) {
            int sc = (i == ss.line) ? ss.col : 0;
            int ec = (i == se.line) ? se.col : static_cast<int>(lines_[i].size());
            float sx = tx0 + (sc > 0 ? Painter::measure_text(
                                            lines_[i].substr(0, sc), style.font_size, kFont)
                                            .width
                                      : 0.0f);
            float ex =
                tx0 +
                Painter::measure_text(lines_[i].substr(0, ec), style.font_size, kFont)
                    .width;
            if (i != se.line)
                ex += style.font_size * 0.4f;
            painter.fill_rect({sx, y, ex - sx, lh}, sel_bg);
        }

        painter.draw_text(lines_[i], {tx0, baseline}, style.text,
                          style.font_size, kFont);
    }

    // Cursor
    if (focused_) {
        auto elapsed = std::chrono::steady_clock::now() - cursor_blink_time_;
        auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                .count();
        if ((ms / 500) % 2 == 0) {
            float cy = rect_.y + lh * cursor_.line - scroll_y_;
            float cx = tx0;
            if (cursor_.col > 0)
                cx += Painter::measure_text(
                          lines_[cursor_.line].substr(0, cursor_.col),
                          style.font_size, kFont)
                          .width;
            painter.draw_line({cx, cy}, {cx, cy + lh}, style.cursor, 1.5f);
        }
    }

    painter.pop_clip(); // text area

    // ── Scrollbars ──────────────────────────────────────────────────────────
    float content_h = lh * static_cast<float>(lines_.size());
    float visible_h = rect_.height;
    if (content_h > visible_h) {
        float bar_h = std::max(20.0f, visible_h * (visible_h / content_h));
        float bar_y = rect_.y + (scroll_y_ / content_h) * visible_h;
        Rect sb{rect_.x + rect_.width - 6.0f, bar_y, 4.0f, bar_h};
        painter.fill_rounded_rect(
            sb,
            Color::rgba(style.text.r, style.text.g, style.text.b, 0.25f),
            2.0f);
    }

    float max_line_w = 0;
    for (int i = first; i <= last; i++) {
        float w = Painter::measure_text(lines_[i], style.font_size, kFont).width;
        if (w > max_line_w) max_line_w = w;
    }
    float content_w = max_line_w + 20.0f;
    float visible_w = rect_.width - gw;
    if (content_w > visible_w) {
        float bar_w = std::max(20.0f, rect_.width * (visible_w / content_w));
        float bar_x = rect_.x + (scroll_x_ / content_w) * rect_.width;
        Rect sb{bar_x, rect_.y + rect_.height - 6.0f, bar_w, 4.0f};
        painter.fill_rounded_rect(
            sb,
            Color::rgba(style.text.r, style.text.g, style.text.b, 0.25f),
            2.0f);
    }

    painter.pop_clip(); // outer

    if (focused_)
        painter.draw_focus_ring(rect_, style.corner_radius);
}

// ── Mouse ───────────────────────────────────────────────────────────────────

bool TextEdit::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Scroll && hit_test(event.position)) {
        scroll_y_ -= event.scroll_dy;
        scroll_x_ -= event.scroll_dx;
        clamp_scroll();
        return true;
    }

    if (event.type == MouseEvent::Type::Press && hit_test(event.position)) {
        Pos p = pos_from_point(event.position);
        if (event.click_count == 2) {
            // Select word
            auto const &ln = lines_[p.line];
            int start = p.col, end = p.col;
            int len = static_cast<int>(ln.size());
            if (p.col < len &&
                !std::isspace(static_cast<unsigned char>(ln[p.col]))) {
                while (start > 0 &&
                       !std::isspace(
                           static_cast<unsigned char>(ln[start - 1])))
                    start--;
                while (end < len &&
                       !std::isspace(static_cast<unsigned char>(ln[end])))
                    end++;
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
    if (event.type != KeyEvent::Type::Press) return false;
    reset_cursor_blink();
    ensure_cursor_visible();

    int nlines = static_cast<int>(lines_.size());

    if (event.super && !event.text.empty()) {
        char ch = event.text[0];
        if (ch == 'a') {
            anchor_ = {0, 0};
            cursor_ = {nlines - 1,
                       static_cast<int>(lines_.back().size())};
            return true;
        }
        if (ch == 'x') {
            if (!has_selection()) return true;
            Pos s = sel_start(), e = sel_end();
            std::string sel;
            for (int i = s.line; i <= e.line; i++) {
                int sc = (i == s.line) ? s.col : 0;
                int ec = (i == e.line) ? e.col
                                       : static_cast<int>(lines_[i].size());
                sel += lines_[i].substr(sc, ec - sc);
                if (i < e.line) sel += '\n';
            }
            Clipboard::set_text(sel);
            delete_selection();
            return true;
        }
        if (ch == 'c') {
            if (!has_selection()) return true;
            Pos s = sel_start(), e = sel_end();
            std::string sel;
            for (int i = s.line; i <= e.line; i++) {
                int sc = (i == s.line) ? s.col : 0;
                int ec = (i == e.line) ? e.col
                                       : static_cast<int>(lines_[i].size());
                sel += lines_[i].substr(sc, ec - sc);
                if (i < e.line) sel += '\n';
            }
            Clipboard::set_text(sel);
            return true;
        }
        if (ch == 'v') {
            auto clip = Clipboard::get_text();
            if (!clip.empty()) insert_text(clip);
            return true;
        }
    }

    switch (event.key) {
    case Key::Enter:
        insert_text("\n");
        return true;

    case Key::Backspace:
        if (has_selection()) {
            delete_selection();
        } else if (event.alt) {
            Pos old = cursor_;
            move_word_left(false);
            anchor_ = cursor_;
            cursor_ = old;
            delete_selection();
        } else if (cursor_.col > 0) {
            size_t prev = Utf8Iterator::prev(lines_[cursor_.line], cursor_.col);
            lines_[cursor_.line].erase(prev, cursor_.col - prev);
            cursor_.col = static_cast<int>(prev);
            anchor_ = cursor_;
            ensure_cursor_visible();
            if (on_change) on_change();
        } else if (cursor_.line > 0) {
            int prev_len = static_cast<int>(lines_[cursor_.line - 1].size());
            lines_[cursor_.line - 1] += lines_[cursor_.line];
            lines_.erase(lines_.begin() + cursor_.line);
            cursor_ = {cursor_.line - 1, prev_len};
            anchor_ = cursor_;
            ensure_cursor_visible();
            if (on_change) on_change();
        }
        return true;

    case Key::Delete:
        if (has_selection()) {
            delete_selection();
        } else if (cursor_.col < static_cast<int>(lines_[cursor_.line].size())) {
            size_t next = Utf8Iterator::next(lines_[cursor_.line], cursor_.col);
            lines_[cursor_.line].erase(cursor_.col, next - cursor_.col);
            ensure_cursor_visible();
            if (on_change) on_change();
        } else if (cursor_.line + 1 < nlines) {
            lines_[cursor_.line] += lines_[cursor_.line + 1];
            lines_.erase(lines_.begin() + cursor_.line + 1);
            ensure_cursor_visible();
            if (on_change) on_change();
        }
        return true;

    case Key::Left:
        if (event.alt) {
            move_word_left(event.shift);
        } else if (!event.shift && has_selection()) {
            move_cursor(sel_start(), false);
        } else if (cursor_.col > 0) {
            move_cursor({cursor_.line, static_cast<int>(Utf8Iterator::prev(lines_[cursor_.line], cursor_.col))}, event.shift);
        } else if (cursor_.line > 0) {
            move_cursor(
                {cursor_.line - 1,
                 static_cast<int>(lines_[cursor_.line - 1].size())},
                event.shift);
        }
        return true;

    case Key::Right:
        if (event.alt) {
            move_word_right(event.shift);
        } else if (!event.shift && has_selection()) {
            move_cursor(sel_end(), false);
        } else if (cursor_.col <
                   static_cast<int>(lines_[cursor_.line].size())) {
            move_cursor({cursor_.line, static_cast<int>(Utf8Iterator::next(lines_[cursor_.line], cursor_.col))}, event.shift);
        } else if (cursor_.line + 1 < nlines) {
            move_cursor({cursor_.line + 1, 0}, event.shift);
        }
        return true;

    case Key::Up:
        if (cursor_.line > 0) {
            int col = std::min(
                cursor_.col,
                static_cast<int>(lines_[cursor_.line - 1].size()));
            move_cursor({cursor_.line - 1, col}, event.shift);
        } else {
            move_cursor({0, 0}, event.shift);
        }
        return true;

    case Key::Down:
        if (cursor_.line + 1 < nlines) {
            int col = std::min(
                cursor_.col,
                static_cast<int>(lines_[cursor_.line + 1].size()));
            move_cursor({cursor_.line + 1, col}, event.shift);
        } else {
            move_cursor(
                {cursor_.line,
                 static_cast<int>(lines_[cursor_.line].size())},
                event.shift);
        }
        return true;

    case Key::Home:
        if (event.super)
            move_cursor({0, 0}, event.shift);
        else
            move_cursor({cursor_.line, 0}, event.shift);
        return true;

    case Key::End:
        if (event.super)
            move_cursor(
                {nlines - 1, static_cast<int>(lines_.back().size())},
                event.shift);
        else
            move_cursor(
                {cursor_.line,
                 static_cast<int>(lines_[cursor_.line].size())},
                event.shift);
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

Size TextEdit::size_hint() const {
    return {0, 200};
}

} // namespace toolkit
