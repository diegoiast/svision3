// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/line_input.hpp"
#include "toolkit/clipboard.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/utf8.hpp"
#include "toolkit/window.hpp"
#include <cctype>
#include <nlohmann/json.hpp>

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

    sync_commands();
}

nlohmann::json LineInput::to_json() const {
    auto j = Widget::to_json();
    j["text"] = text_;
    j["placeholder"] = placeholder_;
    j["read_only"] = read_only_;
    j["is_password"] = password_mode_;
    j["cursor"] = cursor_pos_;
    return j;
}

void LineInput::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("text")) {
        text_ = j["text"];
    }
    if (j.contains("placeholder")) {
        placeholder_ = j["placeholder"];
    }
    if (j.contains("read_only")) {
        read_only_ = j["read_only"];
    }
    if (j.contains("is_password")) {
        password_mode_ = j["is_password"];
    }
    if (j.contains("cursor")) {
        cursor_pos_ = j["cursor"];
    }
}

LineInput &LineInput::set_focused(bool focused) {
    Widget::set_focused(focused);
    if (!focused) {
        sel_anchor_ = cursor_pos_;
    }
    sync_commands();
    return *this;
}

LineInput &LineInput::set_text(std::string const &text) {
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

LineInput &LineInput::set_password_mode(bool enable) {
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
    std::string old_text = text_.substr(s, e - s);

    text_.erase(s, e - s);
    undo_stack_.push(std::make_unique<TextCommand>(&text_, old_text, "", s));
    cursor_pos_ = s;
    sel_anchor_ = s;
    sync_commands();
    if (on_change) {
        on_change(text_, *this);
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

void LineInput::undo() {
    if (undo_stack_.undo()) {
        sync_commands();
        if (window_) {
            window_->request_redraw("input state");
        }
    }
}

void LineInput::redo() {
    if (undo_stack_.redo()) {
        sync_commands();
        if (window_) {
            window_->request_redraw("input state");
        }
    }
}

float LineInput::clear_btn_size() const {
    auto const &palette = Theme::current().palette;
    return palette.fonts.size + 2.0f;
}

float LineInput::peek_btn_size() const {
    auto const &palette = Theme::current().palette;
    return palette.fonts.size + 2.0f;
}

float LineInput::content_right_inset() const {
    auto const &style = Theme::current().style.lineInput;
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
    auto const &style = Theme::current().style.lineInput;
    return rect_.width - style.padding.left - content_right_inset();
}

bool LineInput::hit_clear_btn(Point pos) const {
    if (text_.empty() || read_only_) {
        return false;
    }
    auto sz = clear_btn_size();
    auto const &style = Theme::current().style.lineInput;
    auto bx = rect_.width - style.padding.right - sz;
    auto by = (rect_.height - sz) / 2.0f;

    return pos.x >= bx && pos.x <= bx + sz && pos.y >= by && pos.y <= by + sz;
}

bool LineInput::hit_peek_btn(Point pos) const {
    if (!is_password_field_) {
        return false;
    }
    auto sz = peek_btn_size();
    auto const &style = Theme::current().style.lineInput;
    float bx = rect_.width - style.padding.right - sz;
    bool clear_visible = !text_.empty() && !read_only_;
    if (clear_visible) {
        bx -= clear_btn_size() + 4.0f;
    }
    auto by = (rect_.height - sz) / 2.0f;
    return pos.x >= bx && pos.x <= bx + sz && pos.y >= by && pos.y <= by + sz;
}

size_t LineInput::pos_from_x(float x) const {
    auto const &style = Theme::current().style.lineInput;
    auto const &palette = Theme::current().palette;
    auto click_x = x - style.padding.left + scroll_offset_;

    if (password_mode_) {
        // Password mode: uniform character spacing, use simple linear positions
        auto char_w = measure_text("8", palette.fonts.size).width;
        size_t utf8_len = 0;
        auto codepoints = 0;
        while (utf8_len < text_.size()) {
            utf8_len = Utf8Iterator::next(text_, utf8_len);
            codepoints++;
        }
        auto char_idx = static_cast<size_t>(click_x / char_w);
        if (char_idx >= static_cast<size_t>(codepoints)) {
            return text_.size();
        }
        return Utf8Iterator::find_char(text_, char_idx);
    }

    if (click_x <= 0) {
        return 0;
    }

    auto positions = text_cursor_positions(text_, palette.fonts.size);
    auto best = 0;
    auto best_dist = std::abs(positions[0] - static_cast<double>(click_x));

    auto cp_start = 0;
    while (cp_start < text_.size()) {
        size_t cp_end = Utf8Iterator::next(text_, cp_start);
        if (cp_end < positions.size()) {
            auto dist = std::abs(positions[cp_end] - static_cast<double>(click_x));
            if (dist < best_dist) {
                best_dist = dist;
                best = cp_end;
            }
        }
        cp_start = cp_end;
    }
    return best;
}

bool LineInput::is_valid() const {
    if (validator_) {
        return validator_(text_, *this);
    }
    return true;
}

void LineInput::paint(Painter &painter) {
    auto const &theme = Theme::current();
    auto const &style = theme.style.lineInput;
    auto rect = Rect{0, 0, rect_.width, rect_.height};
    auto sel_start_pos = has_selection() ? static_cast<int>(sel_start()) : -1;
    auto sel_end_pos = has_selection() ? static_cast<int>(sel_end()) : -1;

    std::optional<Color> bg;
    if (validation_mode_ == ValidationMode::Notify && !is_valid()) {
        bg = theme.palette.error;
    }

    ensure_cursor_visible(painter);

    auto d_text = password_mode_ ? get_masked_text(text_) : text_;
    auto d_cursor_pos = static_cast<int>(cursor_pos_);
    auto d_sel_start = sel_start_pos;
    auto d_sel_end = sel_end_pos;

    if (password_mode_) {
        // Convert byte offsets from original text to masked text,
        // where each codepoint occupies exactly 1 byte ("8" per char).
        auto to_masked = [this](size_t byte_pos) {
            int codepoints = 0;
            size_t i = 0;
            while (i < text_.size() && i < byte_pos) {
                i = Utf8Iterator::next(text_, i);
                codepoints++;
            }
            return codepoints;
        };
        d_cursor_pos = to_masked(cursor_pos_);
        if (sel_start_pos >= 0) {
            d_sel_start = to_masked(static_cast<size_t>(sel_start_pos));
        }
        if (sel_end_pos >= 0) {
            d_sel_end = to_masked(static_cast<size_t>(sel_end_pos));
        }
    }

    auto cursor_visible = true;
    if (is_focused()) {
        auto elapsed = std::chrono::steady_clock::now() - cursor_blink_time_;
        cursor_visible =
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() % 1000 < 500;
    }

    auto wstate = WidgetState{
        .interaction = ButtonState::Normal,
        .focused = is_focused(),
        .enabled = !read_only_,
        .window_active = window_ ? window_->is_active() : true,
    };
    theme.draw_line_input(painter, rect, d_text, placeholder_, d_cursor_pos, d_sel_start, d_sel_end,
                          wstate, password_mode_, scroll_offset_, bg, cursor_visible,
                          content_right_inset());

    paint_buttons(painter);
}

// FIXME: move this to use the theme paint buttons primitives
void LineInput::paint_buttons(Painter &painter) {
    auto const &palette = Theme::current().palette;
    auto clear_visible = !text_.empty() && !read_only_;
    if (clear_visible || is_password_field_) {
        auto const &style = Theme::current().style.lineInput;

        if (clear_visible) {
            auto sz = clear_btn_size();
            auto bx = rect_.width - style.padding.right - sz;
            auto by = (rect_.height - sz) / 2.0f;
            auto cx = bx + sz / 2.0f;
            auto cy = by + sz / 2.0f;
            auto r = sz / 2.0f;

            if (clear_hovered_ || clear_pressed_) {
                Color circle_bg = palette.text;
                circle_bg.a = clear_pressed_ ? 0.22f : 0.12f;
                painter.fill_rounded_rect({bx, by, sz, sz}, circle_bg, r);
            }

            auto x_col = palette.text;
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
                Color circle_bg = palette.text;
                circle_bg.a = peek_pressed_ ? 0.22f : 0.12f;
                painter.fill_rounded_rect({bx, by, sz, sz}, circle_bg, r);
            }

            auto eye_col = palette.text;
            eye_col.a = peek_hovered_ ? 0.8f : 0.45f;

            auto eye_radius = sz * 0.35f;
            auto pupil_radius = sz * 0.15f;

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
                    on_change(text_, *this);
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
    auto const &palette = Theme::current().palette;
    auto content_w = content_available_width();
    auto cursor_x = 0.0f;

    if (password_mode_) {
        auto before_str = get_masked_text(text_, cursor_pos_);
        cursor_x =
            before_str.empty() ? 0.0f : painter.measure_text(before_str, palette.fonts.size).width;
    } else {
        auto positions = painter.text_cursor_positions(text_, palette.fonts.size);
        cursor_x = static_cast<float>(positions[cursor_pos_]);
    }

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
                if (!validator_(next_text, *this)) {
                    return true;
                }
            }
            delete_selection();
        } else if (cursor_pos_ > 0) {
            size_t prev = Utf8Iterator::prev(text_, cursor_pos_);
            if (validation_mode_ == ValidationMode::Block && validator_) {
                std::string next_text = text_;
                next_text.erase(prev, cursor_pos_ - prev);
                if (!validator_(next_text, *this)) {
                    return true;
                }
            }
            std::string old_text = text_.substr(prev, cursor_pos_ - prev);
            undo_stack_.push(std::make_unique<TextCommand>(&text_, old_text, "", prev));
            text_.erase(prev, cursor_pos_ - prev);
            cursor_pos_ = prev;
            sel_anchor_ = cursor_pos_;
            sync_commands();
            if (on_change) {
                on_change(text_, *this);
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
                if (!validator_(next_text, *this)) {
                    return true;
                }
            }
            delete_selection();
        } else if (cursor_pos_ < text_.size()) {
            auto next = Utf8Iterator::next(text_, cursor_pos_);

            if (validation_mode_ == ValidationMode::Block && validator_) {
                std::string next_text = text_;
                next_text.erase(cursor_pos_, next - cursor_pos_);
                if (!validator_(next_text, *this)) {
                    return true;
                }
            }
            text_.erase(cursor_pos_, next - cursor_pos_);
            sync_commands();
            if (on_change) {
                on_change(text_, *this);
            }
        }
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    case Key::Left: {
        auto rtl_dir = false;
        if (!password_mode_ && !text_.empty()) {
            auto const &p = Theme::current().palette;
            auto pos = text_cursor_positions(text_, p.fonts.size);
            rtl_dir = pos.size() > 1 && pos[0] > pos[text_.size()];
        }
        if (event.alt) {
            move_word_left(event.shift);
        } else if (!event.shift && has_selection()) {
            move_cursor(rtl_dir ? sel_end() : sel_start(), false);
        } else if (rtl_dir) {
            if (cursor_pos_ < text_.size()) {
                move_cursor(Utf8Iterator::next(text_, cursor_pos_), event.shift);
            }
        } else if (cursor_pos_ > 0) {
            move_cursor(Utf8Iterator::prev(text_, cursor_pos_), event.shift);
        }
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    }
    case Key::Right: {
        auto rtl_dir = false;
        if (!password_mode_ && !text_.empty()) {
            auto const &p = Theme::current().palette;
            auto pos = text_cursor_positions(text_, p.fonts.size);
            rtl_dir = pos.size() > 1 && pos[0] > pos[text_.size()];
        }
        if (event.alt) {
            move_word_right(event.shift);
        } else if (!event.shift && has_selection()) {
            move_cursor(rtl_dir ? sel_start() : sel_end(), false);
        } else if (rtl_dir) {
            if (cursor_pos_ > 0) {
                move_cursor(Utf8Iterator::prev(text_, cursor_pos_), event.shift);
            }
        } else if (cursor_pos_ < text_.size()) {
            move_cursor(Utf8Iterator::next(text_, cursor_pos_), event.shift);
        }
        if (window()) {
            window()->request_redraw("input state");
        }
        return true;
    }
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
            on_submit(text_, *this);
        }
        return true;
    default:
        break;
    }

    if (!event.text.empty() && !event.ctrl && !event.alt && !event.super) {
        if (read_only_) {
            return false;
        }

        std::string next_text = text_;
        size_t start_pos = cursor_pos_;
        std::string old_sel;
        if (has_selection()) {
            start_pos = sel_start();
            old_sel = text_.substr(start_pos, sel_end() - start_pos);
            next_text.erase(start_pos, sel_end() - start_pos);
        }
        next_text.insert(start_pos, event.text);

        if (validation_mode_ == ValidationMode::Block && validator_ &&
            !validator_(next_text, *this)) {
            return true;
        }

        if (has_selection()) {
            delete_selection();
        }

        text_.insert(start_pos, event.text);
        undo_stack_.push(std::make_unique<TextCommand>(&text_, old_sel, event.text, start_pos));
        cursor_pos_ = start_pos + event.text.size();
        sel_anchor_ = cursor_pos_;
        sync_commands();
        if (on_change) {
            on_change(text_, *this);
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
        auto next_text = text_;
        if (has_selection()) {
            next_text.erase(sel_start(), sel_end() - sel_start());
        }
        auto insert_pos = has_selection() ? sel_start() : cursor_pos_;

        next_text.insert(insert_pos, clip);
        if (!validator_(next_text, *this)) {
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
        on_change(text_, *this);
    }
}

void LineInput::show_context_menu(Point pos) {
    if (!window()) {
        return;
    }

    auto has_sel = has_selection();
    auto not_empty = !text_.empty();
    auto can_paste = !Clipboard::get_text().empty();

    // FIXME: use i18n for menu text
    std::vector<MenuItem> items;
    items.push_back(MenuItem::action(undo_cmd));
    items.push_back(MenuItem::action(redo_cmd));
    items.push_back(MenuItem::sep());
    items.push_back(MenuItem::action(cut_cmd));
    items.push_back(MenuItem::action(copy_cmd));
    items.push_back(MenuItem::action(paste_cmd));
    items.push_back(MenuItem::sep());
    items.push_back(MenuItem::action(select_all_cmd));

    auto delete_cmd =
        Command::create("Delete", [this] { delete_selection(); }, !read_only_ && has_sel);
    delete_cmd->set_shortcut("Delete");
    items.push_back(MenuItem::action(delete_cmd));

    context_menu_ = std::make_unique<ContextMenu>(std::move(items));
    context_menu_->show(window(), map_to_window(pos));
}

Size LineInput::size_hint() const {
    auto const &style = Theme::current().style.lineInput;
    auto const &palette = Theme::current().palette;
    auto fm = font_metrics(palette.fonts.size);
    auto h = fm.height + style.padding.top + style.padding.bottom;
    return {150.0f, h};
}

} // namespace toolkit
