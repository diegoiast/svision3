// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/line_input.hpp"
#include "toolkit/clipboard.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/utf8.hpp"
#include "toolkit/window.hpp"

#include <cctype>

namespace toolkit {

static std::string get_masked_text(std::string_view text, size_t byte_limit = std::string::npos) {
    std::string result;
    size_t i = 0;
    while (i < text.size() && i < byte_limit) {
        i = Utf8Iterator::next(text, i);
        // We use a character that has a representative width for a "pro" dot
        result += "8";
    }
    return result;
}

LineInput::LineInput(std::string placeholder)
    : placeholder_(std::move(placeholder)), cursor_blink_time_(std::chrono::steady_clock::now()) {
    state.focusable = true;
    state.focused = false;
    read_only_ = false;

    // FIXME: control+a is not working.
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

    sync_commands();
}

LineInput &LineInput::set_focused(bool focused) {
    Widget::set_focused(focused);
    if (!focused) {
        sel_anchor_ = cursor_pos_;
    }
    sync_commands();
    return *this;
}

LineInput & LineInput::set_text(std::string const &text) {
    if (text_ == text) {
        return *this;
    }
    text_ = text;
    cursor_pos_ = text_.size();
    sel_anchor_ = cursor_pos_;
    sync_commands();
    if (window_) {
        window_->request_redraw("input state");
    }
    return *this;
}

LineInput & LineInput::set_password_mode(bool enable) {
    password_mode_ = enable;
    if (enable) {
        is_password_field_ = true;
    }
    sync_commands();
    if (window_) {
        window_->request_redraw("input state");
    }
    return *this;
}

LineInput &LineInput::set_read_only(bool enable) {
    read_only_ = enable;
    return *this;
}

void LineInput::reset_cursor_blink() { cursor_blink_time_ = std::chrono::steady_clock::now(); }

void LineInput::delete_selection() {
    if (!has_selection()) {
        return;
    }
    auto s = sel_start();
    auto e = sel_end();

    text_.erase(s, e - s);
    cursor_pos_ = s;
    sel_anchor_ = s;
    sync_commands();
    if (on_change) {
        on_change(text_);
    }
}

void LineInput::move_cursor(size_t pos, bool extend_selection) {
    cursor_pos_ = pos;
    if (!extend_selection) {
        sel_anchor_ = cursor_pos_;
    }
    sync_commands();
    reset_cursor_blink();
}

void LineInput::move_word_left(bool extend_selection) {
    auto p = cursor_pos_;
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
    sync_commands();
    reset_cursor_blink();
}

void LineInput::select_all() {
    sel_anchor_ = 0;
    cursor_pos_ = text_.size();
    sync_commands();
    if (window_) {
        window_->request_redraw("input state");
    }
}

void LineInput::sync_commands() {
    bool has_sel = has_selection();
    select_all_cmd->set_enabled(!text_.empty());
    cut_cmd->set_enabled(has_sel && !password_mode_ && !read_only_);
    copy_cmd->set_enabled(has_sel && !password_mode_);
    paste_cmd->set_enabled(!read_only_ && !Clipboard::get_text().empty());
}

float LineInput::clear_btn_size() const {
    auto const &style = Theme::current().line_input;
    return style.font_size + 2.0f;
}

float LineInput::peek_btn_size() const {
    auto const &style = Theme::current().line_input;
    return style.font_size + 2.0f;
}

float LineInput::content_right_inset() const {
    auto const &style = Theme::current().line_input;
    auto inset = style.padding.right;
    auto clear_visible = !text_.empty() && !read_only_;

    if (clear_visible) {
        inset += clear_btn_size() + 4.0f;
    }
    if (is_password_field_) {
        inset += peek_btn_size() + 4.0f;
    }
    return inset;
}

float LineInput::content_available_width() const {
    auto const &style = Theme::current().line_input;
    return rect_.width - style.padding.left - content_right_inset();
}

bool LineInput::hit_clear_btn(Point pos) const {
    if (text_.empty() || read_only_) {
        return false;
    }
    auto sz = clear_btn_size();
    auto const &style = Theme::current().line_input;
    auto bx = rect_.width - style.padding.right - sz;
    auto by = (rect_.height - sz) / 2.0f;

    return pos.x >= bx && pos.x <= bx + sz && pos.y >= by && pos.y <= by + sz;
}

bool LineInput::hit_peek_btn(Point pos) const {
    if (!is_password_field_) {
        return false;
    }
    auto sz = peek_btn_size();
    auto const &style = Theme::current().line_input;
    float bx = rect_.width - style.padding.right - sz;
    bool clear_visible = !text_.empty() && !read_only_;
    if (clear_visible) {
        bx -= clear_btn_size() + 4.0f;
    }
    auto by = (rect_.height - sz) / 2.0f;
    return pos.x >= bx && pos.x <= bx + sz && pos.y >= by && pos.y <= by + sz;
}

size_t LineInput::pos_from_x(float x) const {
    auto const &style = Theme::current().line_input;
    auto click_x = x - style.padding.left + scroll_offset_;
    size_t current_pos = 0;

    if (click_x <= 0) {
        return 0;
    }

    while (current_pos < text_.size()) {
        size_t next_pos = Utf8Iterator::next(text_, current_pos);
        std::string before =
            password_mode_ ? get_masked_text(text_, next_pos) : text_.substr(0, next_pos);
        auto sz = Painter::measure_text(before, style.font_size);
        if (sz.width > click_x) {
            // Check if we are closer to the previous or next character
            std::string before_prev =
                password_mode_ ? get_masked_text(text_, current_pos) : text_.substr(0, current_pos);
            auto prev_sz = Painter::measure_text(before_prev, style.font_size);
            if (click_x - prev_sz.width < sz.width - click_x) {
                return current_pos;
            } else {
                return next_pos;
            }
        }
        current_pos = next_pos;
    }
    return current_pos;
}

bool LineInput::is_valid() const {
    if (validator_) {
        return validator_(text_);
    }
    return true;
}

void LineInput::paint(Painter &painter) {
    auto const &theme = Theme::current();
    auto const &style = theme.line_input;

    std::optional<Color> bg;
    if (validation_mode_ == ValidationMode::Notify && !is_valid()) {
        bg = theme.error_color();
    }

    auto rect = Rect{0, 0, rect_.width, rect_.height};

    auto sel_start_pos = has_selection() ? static_cast<int>(sel_start()) : -1;
    auto sel_end_pos = has_selection() ? static_cast<int>(sel_end()) : -1;

    ensure_cursor_visible(painter);

    auto d_text = password_mode_ ? get_masked_text(text_) : text_;

    theme.draw_line_input(painter, rect, d_text, placeholder_, static_cast<int>(cursor_pos_),
                          sel_start_pos, sel_end_pos, is_focused(), !read_only_, password_mode_,
                          scroll_offset_, bg);

    paint_buttons(painter);
}

void LineInput::paint_buttons(Painter &painter) {
    bool clear_visible = !text_.empty() && !read_only_;
    if (clear_visible || is_password_field_) {
        auto const &style = Theme::current().line_input;

        if (clear_visible) {
            auto sz = clear_btn_size();
            auto bx = rect_.width - style.padding.right - sz;
            auto by = (rect_.height - sz) / 2.0f;
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
            x_col.a = clear_hovered_ ? 0.8f : 0.45f;
            painter.draw_line({cx - half, cy - half}, {cx + half, cy + half}, x_col, 1.5f);
            painter.draw_line({cx - half, cy + half}, {cx + half, cy - half}, x_col, 1.5f);
        }

        if (is_password_field_) {
            auto sz = peek_btn_size();
            auto bx = rect_.width - style.padding.right - sz;
            if (clear_visible) {
                bx -= clear_btn_size() + 4.0f;
            }
            auto by = (rect_.height - sz) / 2.0f;
            auto cx = bx + sz / 2.0f;
            auto cy = by + sz / 2.0f;
            auto r = sz / 2.0f;

            if (peek_hovered_ || peek_pressed_) {
                Color circle_bg = style.text;
                circle_bg.a = peek_pressed_ ? 0.22f : 0.12f;
                painter.fill_rounded_rect({bx, by, sz, sz}, circle_bg, r);
            }

            auto eye_col = style.text;
            eye_col.a = peek_hovered_ ? 0.8f : 0.45f;

            float eye_radius = sz * 0.35f;
            float pupil_radius = sz * 0.15f;

            painter.draw_circle({cx, cy}, eye_radius, eye_col, 1.2f);

            if (password_mode_) {
                painter.fill_circle({cx, cy}, pupil_radius * 0.6f, eye_col);
            } else {
                painter.fill_circle({cx, cy}, pupil_radius, eye_col);
            }
        }
    }
}

bool LineInput::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Move) {
        bool over_clear = hit_clear_btn(event.position);
        bool changed = false;
        if (over_clear != clear_hovered_) {
            clear_hovered_ = over_clear;
            changed = true;
        }
        bool over_peek = hit_peek_btn(event.position);
        if (over_peek != peek_hovered_) {
            peek_hovered_ = over_peek;
            changed = true;
        }
        if (changed && window()) {
            window()->request_redraw("input state");
        }
        return changed;
    }

    if (event.type == MouseEvent::Type::Leave) {
        if (clear_hovered_ || peek_hovered_) {
            clear_hovered_ = false;
            peek_hovered_ = false;
            if (window()) {
                window()->request_redraw("input state");
            }
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Press) {
        auto const local_rect = Rect{0, 0, rect_.width, rect_.height};

        if (!local_rect.contains(event.position)) {
            return false;
        }

        if (event.button == 1) {
            show_context_menu(event.position);
            return true;
        }

        if (hit_clear_btn(event.position)) {
            clear_pressed_ = true;
            if (window()) {
                window()->request_redraw("input state");
            }
            return true;
        }

        if (hit_peek_btn(event.position)) {
            peek_pressed_ = true;
            if (window()) {
                window()->request_redraw("input state");
            }
            return true;
        }

        auto pos = pos_from_x(event.position.x);

        if (event.click_count == 2) {
            select_word_at(pos);
        } else if (event.click_count >= 3) {
            sel_anchor_ = 0;
            cursor_pos_ = text_.size();
            sync_commands();
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
                sync_commands();
                if (on_change) {
                    on_change(text_);
                }
            }
            if (window()) {
                window()->request_redraw("input state");
            }
            return true;
        }
        if (peek_pressed_) {
            peek_pressed_ = false;
            if (hit_peek_btn(event.position)) {
                password_mode_ = !password_mode_;
                sync_commands();
            }
            if (window()) {
                window()->request_redraw("input state");
            }
            return true;
        }
        dragging_ = false;
        return false;
    }

    if (event.type == MouseEvent::Type::Drag && dragging_) {
        cursor_pos_ = pos_from_x(event.position.x);
        sync_commands();
        reset_cursor_blink();
        return true;
    }

    return false;
}

void LineInput::ensure_cursor_visible(Painter &painter) {
    auto const &style = Theme::current().line_input;
    auto content_w = content_available_width();
    std::string before_str =
        password_mode_ ? get_masked_text(text_, cursor_pos_) : text_.substr(0, cursor_pos_);
    auto cursor_x =
        before_str.empty() ? 0.0f : painter.text_size(before_str, style.font_size).width;

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
    if (Widget::handle_key(event)) {
        return true;
    }
    if (event.type != KeyEvent::Type::Press) {
        return false;
    }
    reset_cursor_blink();

    switch (event.key) {
    case Key::Backspace:
        if (read_only_) {
            return true;
        }
        if (has_selection()) {
            if (validation_mode_ == ValidationMode::Block && validator_) {
                std::string next_text = text_;
                next_text.erase(sel_start(), sel_end() - sel_start());
                if (!validator_(next_text)) {
                    return true;
                }
            }
            delete_selection();
        } else if (event.alt) {
            if (validation_mode_ == ValidationMode::Block && validator_) {
                size_t old = cursor_pos_;
                // We need to simulate move_word_left without changing state
                size_t p = cursor_pos_;
                while (p > 0 && std::isspace(static_cast<unsigned char>(text_[p - 1]))) {
                    p--;
                }
                while (p > 0 && !std::isspace(static_cast<unsigned char>(text_[p - 1]))) {
                    p--;
                }
                std::string next_text = text_;
                next_text.erase(p, old - p);
                if (!validator_(next_text)) {
                    return true;
                }
            }
            size_t old = cursor_pos_;
            move_word_left(false);
            text_.erase(cursor_pos_, old - cursor_pos_);
            sel_anchor_ = cursor_pos_;
            sync_commands();
            if (on_change) {
                on_change(text_);
            }
        } else if (cursor_pos_ > 0) {
            size_t prev = Utf8Iterator::prev(text_, cursor_pos_);
            if (validation_mode_ == ValidationMode::Block && validator_) {
                std::string next_text = text_;
                next_text.erase(prev, cursor_pos_ - prev);
                if (!validator_(next_text)) {
                    return true;
                }
            }
            text_.erase(prev, cursor_pos_ - prev);
            cursor_pos_ = prev;
            sel_anchor_ = cursor_pos_;
            sync_commands();
            if (on_change) {
                on_change(text_);
            }
        }
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    case Key::Delete:
        if (read_only_) {
            return true;
        }
        if (has_selection()) {
            if (validation_mode_ == ValidationMode::Block && validator_) {
                std::string next_text = text_;
                next_text.erase(sel_start(), sel_end() - sel_start());
                if (!validator_(next_text)) {
                    return true;
                }
            }
            delete_selection();
        } else if (cursor_pos_ < text_.size()) {
            size_t next = Utf8Iterator::next(text_, cursor_pos_);
            if (validation_mode_ == ValidationMode::Block && validator_) {
                std::string next_text = text_;
                next_text.erase(cursor_pos_, next - cursor_pos_);
                if (!validator_(next_text)) {
                    return true;
                }
            }
            text_.erase(cursor_pos_, next - cursor_pos_);
            sync_commands();
            if (on_change) {
                on_change(text_);
            }
        }
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    case Key::Left:
        if (event.alt) {
            move_word_left(event.shift);
        } else if (!event.shift && has_selection()) {
            move_cursor(sel_start(), false);
        } else if (cursor_pos_ > 0) {
            move_cursor(Utf8Iterator::prev(text_, cursor_pos_), event.shift);
        }
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    case Key::Right:
        if (event.alt) {
            move_word_right(event.shift);
        } else if (!event.shift && has_selection()) {
            move_cursor(sel_end(), false);
        } else if (cursor_pos_ < text_.size()) {
            move_cursor(Utf8Iterator::next(text_, cursor_pos_), event.shift);
        }
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    case Key::Home:
        move_cursor(0, event.shift);
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    case Key::End:
        move_cursor(text_.size(), event.shift);
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    case Key::Enter:
        if (read_only_) {
            return true;
        }
        if (on_submit) {
            on_submit(text_);
        }
        return true;
    default:
        break;
    }

    if (!event.text.empty() && !event.ctrl && !event.alt && !event.super) {
        if (read_only_) {
            return false;
        }
        if (validation_mode_ == ValidationMode::Block && validator_) {
            auto next_text = text_;
            if (has_selection()) {
                next_text.erase(sel_start(), sel_end() - sel_start());
            }
            // Use local pos since cursor_pos_ might be updated if we had selection
            auto insert_pos = has_selection() ? sel_start() : cursor_pos_;
            next_text.insert(insert_pos, event.text);
            if (!validator_(next_text)) {
                return true;
            }
        }
        if (has_selection()) {
            delete_selection();
        }
        text_.insert(cursor_pos_, event.text);
        cursor_pos_ += event.text.size();
        sel_anchor_ = cursor_pos_;
        sync_commands();
        if (on_change) {
            on_change(text_);
        }
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    }

    return false;
}

void LineInput::on_focus() {
    reset_cursor_blink();
    if (window_ && blink_timer_id_ == 0) {
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

void LineInput::on_blur() {
    if (window_ && blink_timer_id_ != 0) {
        window_->stop_timer(blink_timer_id_);
        blink_timer_id_ = 0;
    }
}


void LineInput::cut() {
    if (!has_selection() || password_mode_ || read_only_) {
        return;
    }
    Clipboard::set_text(text_.substr(sel_start(), sel_end() - sel_start()));
    delete_selection();
}

void LineInput::copy() {
    if (!has_selection() || password_mode_) {
        return;
    }
    Clipboard::set_text(text_.substr(sel_start(), sel_end() - sel_start()));
}

void LineInput::paste() {
    if (read_only_) {
        return;
    }
    auto clip = Clipboard::get_text();
    if (clip.empty()) {
        return;
    }
    if (validation_mode_ == ValidationMode::Block && validator_) {
        std::string next_text = text_;
        if (has_selection()) {
            next_text.erase(sel_start(), sel_end() - sel_start());
        }
        size_t insert_pos = has_selection() ? sel_start() : cursor_pos_;
        next_text.insert(insert_pos, clip);
        if (!validator_(next_text)) {
            return;
        }
    }
    if (has_selection()) {
        delete_selection();
    }
    text_.insert(cursor_pos_, clip);
    cursor_pos_ += clip.size();
    sel_anchor_ = cursor_pos_;
    sync_commands();
    if (on_change) {
        on_change(text_);
    }
}

void LineInput::show_context_menu(Point pos) {
    if (!window()) {
        return;
    }

    bool has_sel = has_selection();
    bool not_empty = !text_.empty();
    bool can_paste = !Clipboard::get_text().empty();

    // FIXME: use i18n for menu text
    std::vector<MenuItem> items;
    items.push_back(
        MenuItem::action("Cut", [this] { cut(); }, has_sel && !password_mode_ && !read_only_));
    items.push_back(MenuItem::action("Copy", [this] { copy(); }, has_sel && !password_mode_));
    items.push_back(MenuItem::action("Paste", [this] { paste(); }, !read_only_ && can_paste));
    items.push_back(MenuItem::sep());
    items.push_back(MenuItem::action("Select All", [this] { select_all(); }, not_empty));
    items.push_back(
        MenuItem::action("Delete", [this] { delete_selection(); }, !read_only_ && has_sel));

    context_menu_ = std::make_unique<ContextMenu>(std::move(items));
    context_menu_->show(window(), map_to_window(pos));
}

Size LineInput::size_hint() const {
    auto const &style = Theme::current().line_input;
    // FIXME: what is this constant?
    auto h = style.font_size + style.padding.top + style.padding.bottom + 8.0f;
    return {150.0f, h};
}

} // namespace toolkit
