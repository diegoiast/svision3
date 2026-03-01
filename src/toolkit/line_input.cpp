// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/line_input.hpp"
#include "toolkit/clipboard.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>
#include <cctype>

namespace toolkit {

LineInput::LineInput(std::string placeholder)
    : placeholder_(std::move(placeholder)), cursor_blink_time_(std::chrono::steady_clock::now()) {}

void LineInput::set_text(std::string const &text) {
    text_ = text;
    cursor_pos_ = text_.size();
    sel_anchor_ = cursor_pos_;
}

void LineInput::set_focused(bool focused) {
    focused_ = focused;
    if (focused) {
        reset_cursor_blink();
    } else {
        sel_anchor_ = cursor_pos_;
    }
}

void LineInput::reset_cursor_blink() { cursor_blink_time_ = std::chrono::steady_clock::now(); }

void LineInput::delete_selection() {
    if (!has_selection()) {
        return;
    }
    size_t s = sel_start();
    size_t e = sel_end();
    text_.erase(s, e - s);
    cursor_pos_ = s;
    sel_anchor_ = s;
    if (on_change) {
        on_change(text_);
    }
}

void LineInput::move_cursor(size_t pos, bool extend_selection) {
    cursor_pos_ = pos;
    if (!extend_selection) {
        sel_anchor_ = cursor_pos_;
    }
    reset_cursor_blink();
}

void LineInput::move_word_left(bool extend_selection) {
    size_t p = cursor_pos_;
    while (p > 0 && std::isspace(static_cast<unsigned char>(text_[p - 1]))) {
        p--;
    }
    while (p > 0 && !std::isspace(static_cast<unsigned char>(text_[p - 1]))) {
        p--;
    }
    move_cursor(p, extend_selection);
}

void LineInput::move_word_right(bool extend_selection) {
    auto p = cursor_pos_;
    while (p < text_.size() && !std::isspace(static_cast<unsigned char>(text_[p]))) {
        p++;
    }
    while (p < text_.size() && std::isspace(static_cast<unsigned char>(text_[p]))) {
        p++;
    }
    move_cursor(p, extend_selection);
}

void LineInput::select_word_at(size_t pos) {
    auto start = pos;
    auto end = pos;
    if (pos < text_.size() && !std::isspace(static_cast<unsigned char>(text_[pos]))) {
        while (start > 0 && !std::isspace(static_cast<unsigned char>(text_[start - 1]))) {
            start--;
        }
        while (end < text_.size() && !std::isspace(static_cast<unsigned char>(text_[end]))) {
            end++;
        }
    }
    sel_anchor_ = start;
    cursor_pos_ = end;
    reset_cursor_blink();
}

float LineInput::clear_btn_size() const {
    auto const &style = Theme::current().line_input;
    return style.font_size + 2.0f;
}

float LineInput::content_right_inset() const {
    auto const &style = Theme::current().line_input;
    if (text_.empty()) {
        return style.padding.right;
    }
    return style.padding.right + clear_btn_size() + 4.0f;
}

bool LineInput::hit_clear_btn(Point pos) const {
    if (text_.empty()) {
        return false;
    }
    auto sz = clear_btn_size();
    auto const &style = Theme::current().line_input;
    auto bx = rect_.x + rect_.width - style.padding.right - sz;
    auto by = rect_.y + (rect_.height - sz) / 2.0f;
    return pos.x >= bx && pos.x <= bx + sz && pos.y >= by && pos.y <= by + sz;
}

size_t LineInput::pos_from_x(float x) const {
    auto const &style = Theme::current().line_input;
    auto click_x = x - (rect_.x + style.padding.left) + scroll_offset_;
    auto pos = 0;

    if (click_x <= 0) {
        return 0;
    }

    for (size_t i = 1; i <= text_.size(); i++) {
        auto sz = Painter::measure_text(text_.substr(0, i), style.font_size);
        if (sz.width > click_x) {
            break;
        }
        pos = i;
    }
    return pos;
}

void LineInput::paint(Painter &painter) {
    auto const &style = Theme::current().line_input;
    auto bg = focused_ ? style.background_focused : style.background;
    auto border = focused_ ? style.border_focused : style.border;
    auto fm = painter.font_metrics(style.font_size);
    auto baseline_y = rect_.y + (rect_.height - fm.height) / 2.0f + fm.ascent;
    auto content_x = rect_.x + style.padding.left;
    auto content_w = rect_.width - style.padding.left - content_right_inset();
    auto clip_rect = Rect{content_x, rect_.y, content_w, rect_.height};
    auto tx = content_x - scroll_offset_;

    painter.draw_frame(rect_, bg, border, style, true);
    painter.push_clip(clip_rect);
    ensure_cursor_visible(painter);

    if (text_.empty() && !focused_) {
        painter.draw_text(placeholder_, {tx, baseline_y}, style.placeholder, style.font_size);
    } else {
        if (has_selection()) {
            // FIXME: short undescriptive variable names
            auto s = sel_start();
            auto e = sel_end();
            auto ex = tx + painter.text_size(text_.substr(0, e), style.font_size).width;
            auto hy = rect_.y + (rect_.height - fm.height) / 2.0f - 1.0f;
            auto sel_bg = Color::rgba(0.26f, 0.52f, 0.96f, 0.35f);
            auto sx =
                tx + (s > 0 ? painter.text_size(text_.substr(0, s), style.font_size).width : 0.0f);
            painter.fill_rect({sx, hy, ex - sx, fm.height + 2.0f}, sel_bg);
        }

        if (!text_.empty()) {
            painter.draw_text(text_, {tx, baseline_y}, style.text, style.font_size);
        }

        if (focused_) {
            auto elapsed = std::chrono::steady_clock::now() - cursor_blink_time_;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            bool cursor_on = (ms / 500) % 2 == 0;

            if (cursor_on) {
                auto before = text_.substr(0, cursor_pos_);
                auto cx = tx;
                auto cy_top = rect_.y + (rect_.height - fm.height) / 2.0f - 1.0f;
                auto cy_bot = cy_top + fm.height + 2.0f;

                if (!before.empty()) {
                    cx += painter.text_size(before, style.font_size).width;
                }
                painter.draw_line({cx, cy_top}, {cx, cy_bot}, style.cursor, 1.5f);
            }
        }
    }

    painter.pop_clip();

    if (!text_.empty()) {
        auto sz = clear_btn_size();
        auto bx = rect_.x + rect_.width - style.padding.right - sz;
        auto by = rect_.y + (rect_.height - sz) / 2.0f;
        auto cx = bx + sz / 2.0f;
        auto cy = by + sz / 2.0f;
        auto r = sz / 2.0f;

        if (clear_hovered_ || clear_pressed_) {
            Color circle_bg = style.text;
            circle_bg.a = clear_pressed_ ? 0.22f : 0.12f;
            painter.fill_rounded_rect({bx, by, sz, sz}, circle_bg, r);
        }

        auto x_col = style.text;
        auto half = sz * 0.22f;

        // FIXME: what are these constants?
        x_col.a = clear_hovered_ ? 0.8f : 0.45f;
        painter.draw_line({cx - half, cy - half}, {cx + half, cy + half}, x_col, 1.5f);
        painter.draw_line({cx - half, cy + half}, {cx + half, cy - half}, x_col, 1.5f);
    }
}

bool LineInput::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Move) {
        bool over = hit_clear_btn(event.position);
        if (over != clear_hovered_) {
            clear_hovered_ = over;
        }
        return false;
    }

    if (event.type == MouseEvent::Type::Press) {
        if (!hit_test(event.position)) {
            return false;
        }

        if (event.button == 1) {
            show_context_menu(event.position);
            return true;
        }

        if (hit_clear_btn(event.position)) {
            clear_pressed_ = true;
            return true;
        }

        auto pos = pos_from_x(event.position.x);

        if (event.click_count == 2) {
            select_word_at(pos);
        } else if (event.click_count >= 3) {
            sel_anchor_ = 0;
            cursor_pos_ = text_.size();
            reset_cursor_blink();
        } else {
            move_cursor(pos, event.shift);
            dragging_ = true;
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Release) {
        if (clear_pressed_) {
            clear_pressed_ = false;
            if (hit_clear_btn(event.position)) {
                text_.clear();
                cursor_pos_ = 0;
                sel_anchor_ = 0;
                scroll_offset_ = 0;
                if (on_change) {
                    on_change(text_);
                }
            }
            return true;
        }
        dragging_ = false;
        return false;
    }

    if (event.type == MouseEvent::Type::Drag && dragging_) {
        cursor_pos_ = pos_from_x(event.position.x);
        reset_cursor_blink();
        return true;
    }

    return false;
}

void LineInput::ensure_cursor_visible(Painter &painter) {
    auto const &style = Theme::current().line_input;
    auto content_w = rect_.width - style.padding.left - style.padding.right;
    auto before = text_.substr(0, cursor_pos_);
    auto cursor_x = before.empty() ? 0.0f : painter.text_size(before, style.font_size).width;

    if (cursor_x - scroll_offset_ > content_w) {
        scroll_offset_ = cursor_x - content_w;
    } else if (cursor_x - scroll_offset_ < 0) {
        scroll_offset_ = cursor_x;
    }

    if (scroll_offset_ < 0) {
        scroll_offset_ = 0;
    }
}

bool LineInput::handle_key(KeyEvent const &event) {
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }
    reset_cursor_blink();

    if (event.super && !event.text.empty()) {
        char ch = event.text[0];
        if (ch == 'a') {
            sel_anchor_ = 0;
            cursor_pos_ = text_.size();
            return true;
        }
        if (ch == 'x') {
            cut();
            return true;
        }
        if (ch == 'c') {
            copy();
            return true;
        }
        if (ch == 'v') {
            paste();
            return true;
        }
    }

    switch (event.key) {
    case Key::Backspace:
        if (has_selection()) {
            delete_selection();
        } else if (event.alt) {
            size_t old = cursor_pos_;
            move_word_left(false);
            text_.erase(cursor_pos_, old - cursor_pos_);
            sel_anchor_ = cursor_pos_;
            if (on_change) {
                on_change(text_);
            }
        } else if (cursor_pos_ > 0) {
            text_.erase(cursor_pos_ - 1, 1);
            cursor_pos_--;
            sel_anchor_ = cursor_pos_;
            if (on_change) {
                on_change(text_);
            }
        }
        return true;
    case Key::Delete:
        if (has_selection()) {
            delete_selection();
        } else if (cursor_pos_ < text_.size()) {
            text_.erase(cursor_pos_, 1);
            if (on_change) {
                on_change(text_);
            }
        }
        return true;
    case Key::Left:
        if (event.alt) {
            move_word_left(event.shift);
        } else if (!event.shift && has_selection()) {
            move_cursor(sel_start(), false);
        } else if (cursor_pos_ > 0) {
            move_cursor(cursor_pos_ - 1, event.shift);
        }
        return true;
    case Key::Right:
        if (event.alt) {
            move_word_right(event.shift);
        } else if (!event.shift && has_selection()) {
            move_cursor(sel_end(), false);
        } else if (cursor_pos_ < text_.size()) {
            move_cursor(cursor_pos_ + 1, event.shift);
        }
        return true;
    case Key::Home:
        move_cursor(0, event.shift);
        return true;
    case Key::End:
        move_cursor(text_.size(), event.shift);
        return true;
    case Key::Enter:
        if (on_submit) {
            on_submit(text_);
        }
        return true;
    default:
        break;
    }

    if (!event.text.empty()) {
        if (has_selection()) {
            delete_selection();
        }
        text_.insert(cursor_pos_, event.text);
        cursor_pos_ += event.text.size();
        sel_anchor_ = cursor_pos_;
        if (on_change) {
            on_change(text_);
        }
        return true;
    }

    return false;
}

void LineInput::cut() {
    if (!has_selection()) {
        return;
    }
    Clipboard::set_text(text_.substr(sel_start(), sel_end() - sel_start()));
    delete_selection();
}

void LineInput::copy() {
    if (!has_selection()) {
        return;
    }
    Clipboard::set_text(text_.substr(sel_start(), sel_end() - sel_start()));
}

void LineInput::paste() {
    auto clip = Clipboard::get_text();
    if (clip.empty()) {
        return;
    }
    if (has_selection()) {
        delete_selection();
    }
    text_.insert(cursor_pos_, clip);
    cursor_pos_ += clip.size();
    sel_anchor_ = cursor_pos_;
    if (on_change) {
        on_change(text_);
    }
}

void LineInput::show_context_menu(Point pos) {
    if (!window()) {
        return;
    }

    // FIXME: use i18n for menu text
    std::vector<MenuItem> items;
    items.push_back(MenuItem::action("Cut", [this] { cut(); }, [this] { return has_selection(); }));
    items.push_back(
        MenuItem::action("Copy", [this] { copy(); }, [this] { return has_selection(); }));
    items.push_back(MenuItem::action(
        "Paste", [this] { paste(); }, [] { return !Clipboard::get_text().empty(); }));
    items.push_back(MenuItem::sep());
    items.push_back(MenuItem::action(
        "Select All",
        [this] {
            sel_anchor_ = 0;
            cursor_pos_ = text_.size();
            reset_cursor_blink();
        },
        [this] { return !text_.empty(); }));
    items.push_back(MenuItem::action(
        "Delete", [this] { delete_selection(); }, [this] { return has_selection(); }));

    context_menu_ = std::make_unique<ContextMenu>(std::move(items));
    context_menu_->show(window(), pos);
}

Size LineInput::size_hint() const {
    auto const &style = Theme::current().line_input;
    // FIXME: what is this constant?
    auto h = style.font_size + style.padding.top + style.padding.bottom + 8.0f;
    return {0.0f, h};
}

} // namespace toolkit
