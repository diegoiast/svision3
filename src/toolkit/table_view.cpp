// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/table_view.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>
#include <numeric>

namespace toolkit {

StringTableModel::StringTableModel(std::vector<std::string> headers,
                                   std::vector<std::vector<std::string>> rows)
    : headers_(std::move(headers)), rows_(std::move(rows)) {}

int StringTableModel::row_count() const { return static_cast<int>(rows_.size()); }

int StringTableModel::column_count() const { return static_cast<int>(headers_.size()); }

std::string_view StringTableModel::header_text(int column) const {
    if (column < 0 || column >= static_cast<int>(headers_.size())) {
        return {};
    }
    return headers_[column];
}

std::string_view StringTableModel::cell_text(int row, int column) const {
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return {};
    }
    auto const &r = rows_[row];
    if (column < 0 || column >= static_cast<int>(r.size())) {
        return {};
    }
    return r[column];
}

void StringTableModel::set_data(std::vector<std::string> headers,
                                std::vector<std::vector<std::string>> rows) {
    headers_ = std::move(headers);
    rows_ = std::move(rows);
    if (on_data_changed) {
        on_data_changed();
    }
}

void StringTableModel::append_row(std::vector<std::string> row) {
    rows_.push_back(std::move(row));
    if (on_data_changed) {
        on_data_changed();
    }
}

void StringTableModel::remove_row(int index) {
    if (index >= 0 && index < static_cast<int>(rows_.size())) {
        rows_.erase(rows_.begin() + index);
        if (on_data_changed) {
            on_data_changed();
        }
    }
}

TableView::TableView(std::shared_ptr<TableModel> model) : model_(std::move(model)) {
    focusable_ = true;
    if (model_) {
        model_->on_data_changed = [this] {
            rebuild_sort_index();
            auto_fit_columns();
            clamp_scroll();
            if (window()) {
                window()->request_redraw("table selection");
            }
        };
        ensure_column_widths();
        rebuild_sort_index();
    }
}

void TableView::set_model(std::shared_ptr<TableModel> model) {
    model_ = std::move(model);
    column_widths_.clear();
    sort_indices_.clear();
    selection_.clear();
    anchor_row_ = cursor_row_ = hovered_row_ = -1;
    scroll_y_ = scroll_x_ = 0;
    sort_column_ = -1;
    sort_order_ = SortOrder::None;
    if (model_) {
        model_->on_data_changed = [this] {
            rebuild_sort_index();
            auto_fit_columns();
            clamp_scroll();
            if (window()) {
                window()->request_redraw("table selection");
            }
        };
        auto_fit_columns();
        rebuild_sort_index();
    }
}

int TableView::model_row(int display_row) const {
    if (display_row < 0 || display_row >= static_cast<int>(sort_indices_.size())) {
        return display_row;
    }
    return sort_indices_[display_row];
}

void TableView::rebuild_sort_index() {
    if (!model_) {
        sort_indices_.clear();
        return;
    }

    auto n = model_->row_count();
    sort_indices_.resize(n);
    std::iota(sort_indices_.begin(), sort_indices_.end(), 0);

    if (sort_column_ < 0 || sort_order_ == SortOrder::None) {
        return;
    }

    auto col = sort_column_;
    auto *m = model_.get();
    auto ascending = (sort_order_ == SortOrder::Ascending);

    // FIXME: we need to allow users a custom sort method
    std::stable_sort(sort_indices_.begin(), sort_indices_.end(), [m, col, ascending](int a, int b) {
        auto ta = m->cell_text(a, col);
        auto tb = m->cell_text(b, col);
        return ascending ? (ta < tb) : (ta > tb);
    });
}

void TableView::ensure_column_widths() {
    if (!model_) {
        return;
    }
    auto cols = model_->column_count();
    auto const &style = Theme::current().table_view;

    while (static_cast<int>(column_widths_.size()) < cols) {
        column_widths_.push_back(style.default_column_width);
    }
}

void TableView::auto_fit_column(int col) {
    if (!model_ || col < 0 || col >= model_->column_count()) {
        return;
    }
    auto const &style = Theme::current().table_view;
    auto padding = style.item_padding_h * 2;
    auto nrows = model_->row_count();
    auto sort_arrow_w = Painter::measure_text(" \xe2\x96\xb2", style.font_size).width;
    ensure_column_widths();

    auto header{model_->header_text(col)};
    auto max_w = Painter::measure_text(header, style.font_size).width + sort_arrow_w;
    auto sample = std::min(nrows, 100);

    for (auto r = 0; r < sample; r++) {
        auto w = Painter::measure_text(model_->cell_text(r, col), style.font_size).width;
        if (w > max_w) {
            max_w = w;
        }
    }
    column_widths_[col] = std::max(max_w + padding, style.min_column_width);
}

void TableView::auto_fit_columns() {
    if (!model_) {
        return;
    }
    auto ncols = model_->column_count();
    column_widths_.resize(ncols);
    for (int c = 0; c < ncols; c++) {
        auto_fit_column(c);
    }
}

CursorShape TableView::cursor() const {
    if (resize_col_ >= 0 || over_resize_grip_) {
        return CursorShape::ResizeEW;
    }
    return CursorShape::Arrow;
}

void TableView::set_column_width(int column, float width) {
    ensure_column_widths();
    auto const &style = Theme::current().table_view;
    if (column >= 0 && column < static_cast<int>(column_widths_.size())) {
        column_widths_[column] = std::max(width, style.min_column_width);
    }
}

float TableView::column_width(int column) const {
    if (column >= 0 && column < static_cast<int>(column_widths_.size())) {
        return column_widths_[column];
    }
    return Theme::current().table_view.default_column_width;
}

// ── Selection ───────────────────────────────────────────────────────────────

void TableView::set_selected_row(int row) {
    if (!model_) {
        return;
    }
    if (row < 0 || row >= model_->row_count()) {
        clear_selection();
        return;
    }
    selection_.clear();
    selection_.insert(row);
    anchor_row_ = row;
    cursor_row_ = row;
    notify_selection();
}

void TableView::set_selection(std::set<int> rows) {
    selection_ = std::move(rows);
    if (!selection_.empty()) {
        anchor_row_ = *selection_.begin();
        cursor_row_ = *selection_.rbegin();
    } else {
        anchor_row_ = cursor_row_ = -1;
    }
    notify_selection();
}

void TableView::select_all() {
    if (!model_) {
        return;
    }

    auto n = model_->row_count();

    selection_.clear();
    for (int i = 0; i < n; i++) {
        selection_.insert(i);
    }
    anchor_row_ = 0;
    cursor_row_ = n - 1;
    notify_selection();
}

void TableView::clear_selection() {
    selection_.clear();
    anchor_row_ = cursor_row_ = -1;
    notify_selection();
}

void TableView::select_range_from_anchor() {
    selection_.clear();
    auto lo = std::min(anchor_row_, cursor_row_);
    auto hi = std::max(anchor_row_, cursor_row_);
    for (int i = lo; i <= hi; i++) {
        selection_.insert(i);
    }
}

void TableView::notify_selection() {
    if (on_selection_changed) {
        on_selection_changed(cursor_row_ >= 0 ? model_row(cursor_row_) : -1);
    }
}

// ── Geometry ────────────────────────────────────────────────────────────────

float TableView::row_height() const {
    auto const &style = Theme::current().table_view;
    auto fm = Painter::measure_font_metrics(style.font_size);
    return fm.height + style.item_padding * 2;
}

float TableView::header_height() const {
    auto const &style = Theme::current().table_view;
    auto fm = Painter::measure_font_metrics(style.font_size);
    return fm.height + style.header_padding_v * 2;
}

float TableView::total_content_height() const {
    if (!model_) {
        return 0.0f;
    }
    return row_height() * static_cast<float>(model_->row_count());
}

float TableView::total_content_width() const {
    auto w = 0.0f;
    for (auto cw : column_widths_) {
        w += cw;
    }
    return w;
}

void TableView::clamp_scroll() {
    auto hh = header_height();
    auto visible_h = rect_.height - hh;
    auto content_h = total_content_height();
    auto max_y = std::max(0.0f, content_h - visible_h);
    auto visible_w = rect_.width;
    auto content_w = total_content_width();
    auto max_x = std::max(0.0f, content_w - visible_w);

    scroll_y_ = std::clamp(scroll_y_, 0.0f, max_y);
    scroll_x_ = std::clamp(scroll_x_, 0.0f, max_x);
}

int TableView::row_at_y(float y) const {
    if (!model_) {
        return -1;
    }
    auto hh = header_height();
    auto local_y = y - hh + scroll_y_;

    if (local_y < 0) {
        return -1;
    }

    auto idx = static_cast<int>(local_y / row_height());

    if (idx >= model_->row_count()) {
        return -1;
    }
    return idx;
}

int TableView::column_at_x(float x) const {
    auto cx = -scroll_x_;
    for (auto c = 0; c < static_cast<int>(column_widths_.size()); c++) {
        cx += column_widths_[c];
        if (x < cx) {
            return c;
        }
    }
    return -1;
}

int TableView::header_resize_hit(float x, float y) const {
    auto hh = header_height();
    if (y < 0 || y > hh) {
        return -1;
    }

    auto constexpr grip = 6.0f;
    auto cx = -scroll_x_;
    for (auto c = 0; c < static_cast<int>(column_widths_.size()); c++) {
        cx += column_widths_[c];
        if (x >= cx - grip && x <= cx + grip) {
            return c;
        }
    }
    return -1;
}

void TableView::scroll_to_row(int row) {
    // FIXME: modify variables to more descriptive names
    auto rh = row_height();
    auto hh = header_height();
    auto visible_h = rect_.height - hh;
    auto top = rh * static_cast<float>(row);
    auto bot = top + rh;

    if (bot > scroll_y_ + visible_h) {
        scroll_y_ = bot - visible_h;
    }
    if (top < scroll_y_) {
        scroll_y_ = top;
    }
    clamp_scroll();
}

void TableView::paint(Painter &painter) {
    if (!model_) {
        return;
    }
    ensure_column_widths();

    auto const &style = Theme::current().table_view;
    auto rh = row_height();
    auto hh = header_height();
    auto fm = painter.font_metrics(style.font_size);
    auto nrows = model_->row_count();
    auto ncols = model_->column_count();

    if (style.beveled) {
        painter.draw_frame({0, 0, rect_.width, rect_.height}, style.background, style.border, style,
                           true);
    } else {
        painter.fill_rounded_rect({0, 0, rect_.width, rect_.height}, style.background,
                                  style.corner_radius);
        if (style.border_width > 0) {
            painter.draw_rounded_rect({0, 0, rect_.width, rect_.height}, style.border,
                                      style.corner_radius, style.border_width);
        }
    }

    painter.push_clip({0, 0, rect_.width, rect_.height});

    // ── Header ──────────────────────────────────────────────────────────────
    auto header_rect = Rect{0, 0, rect_.width, hh};
    auto hx = -scroll_x_;

    painter.fill_rect(header_rect, style.header_bg);
    for (auto c = 0; c < ncols; c++) {
        auto cw = column_widths_[c];
        auto sep_x = hx + cw;

        if (sep_x > 0 && hx < rect_.width) {
            auto text_y = (hh - fm.height) / 2.0f + fm.ascent;
            auto text = std::string{model_->header_text(c)};

            painter.push_clip(
                {std::max(hx, 0.0f), 0, std::min(cw, rect_.width - std::max(hx, 0.0f)), hh});
            if (c == sort_column_ && sort_order_ != SortOrder::None) {
                text += sort_order_ == SortOrder::Ascending ? " \xe2\x96\xb2" : " \xe2\x96\xbc";
            }
            painter.draw_text(text, {hx + style.item_padding_h, text_y}, style.header_text,
                              style.font_size);
            painter.pop_clip();
        }

        if (sep_x > 0 && sep_x < rect_.width) {
            painter.draw_line({sep_x, 0}, {sep_x, hh}, style.header_border);
        }
        hx += cw;
    }

    painter.draw_line({0, hh}, {rect_.width, hh}, style.header_border);

    auto body_clip = Rect{0, hh, rect_.width, rect_.height - hh};
    auto first_visible = std::max(0, static_cast<int>(scroll_y_ / rh));
    auto last_visible = std::min(nrows - 1, static_cast<int>((scroll_y_ + rect_.height - hh) / rh));

    painter.push_clip(body_clip);
    for (int r = first_visible; r <= last_visible; r++) {
        auto mr = model_row(r);
        auto ry = hh + rh * static_cast<float>(r) - scroll_y_;
        auto selected = is_selected(r);
        auto hovered = (r == hovered_row_) && !selected;
        auto alt_row = alternating_ && (r % 2 == 1);
        auto row_rect = Rect{0, ry, rect_.width, rh};

        if (selected) {
            painter.fill_rect(row_rect,
                              alt_row ? style.selected_bg.darken(0.06f) : style.selected_bg);
        } else if (hovered) {
            auto hover = style.selected_bg;
            hover.a = 0.45f;
            painter.fill_rect(row_rect, hover);
        } else if (alt_row) {
            painter.fill_rect(row_rect, style.alternate_bg);
        }

        auto text_col = (selected || hovered) ? style.selected_text : style.text;
        auto text_y = ry + (rh - fm.height) / 2.0f + fm.ascent;
        auto cx = -scroll_x_;

        for (auto c = 0; c < ncols; c++) {
            auto cw = column_widths_[c];
            if (cx + cw > 0 && cx < rect_.width) {
                painter.push_clip(
                    {std::max(cx, 0.0f), ry, std::min(cw, rect_.width - std::max(cx, 0.0f)), rh});
                painter.draw_text(model_->cell_text(mr, c), {cx + style.item_padding_h, text_y},
                                  text_col, style.font_size);
                painter.pop_clip();
            }

            auto sep_x = cx + cw;
            if (sep_x > 0 && sep_x < rect_.width) {
                painter.draw_line({sep_x, ry}, {sep_x, ry + rh}, style.grid_line, 0.5f);
            }
            cx += cw;
        }
    }

    // ── Scrollbars ──────────────────────────────────────────────────────────
    auto content_h = total_content_height();
    auto visible_h = rect_.height - hh;
    if (content_h > visible_h) {
        auto bar_h = std::max(20.0f, visible_h * (visible_h / content_h));
        auto bar_y = hh + (scroll_y_ / content_h) * visible_h;
        Rect sb{rect_.width - 6.0f, bar_y, 4.0f, bar_h};
        painter.fill_rounded_rect(sb, Color::rgba(style.text.r, style.text.g, style.text.b, 0.25f),
                                  2.0f);
    }

    auto content_w = total_content_width();
    if (content_w > rect_.width) {
        auto bar_w = std::max(20.0f, rect_.width * (rect_.width / content_w));
        auto bar_x = (scroll_x_ / content_w) * rect_.width;
        Rect sb{bar_x, rect_.height - 6.0f, bar_w, 4.0f};
        painter.fill_rounded_rect(sb, Color::rgba(style.text.r, style.text.g, style.text.b, 0.25f),
                                  2.0f);
    }

    painter.pop_clip(); // body
    painter.pop_clip(); // outer

    // FIXME: table view ring should be drawen by owner
    if (focused_) {
        painter.draw_focus_ring({0, 0, rect_.width, rect_.height}, style.corner_radius);
    }
}

// ── Mouse ───────────────────────────────────────────────────────────────────

bool TableView::handle_mouse(MouseEvent const &event) {
    if (!model_) {
        return false;
    }

    auto const local_rect = Rect{0, 0, rect_.width, rect_.height};

    if (event.type == MouseEvent::Type::Scroll) {
        if (!local_rect.contains(event.position)) {
            return false;
        }
        scroll_y_ -= event.scroll_dy;
        scroll_x_ -= event.scroll_dx;
        clamp_scroll();
        return true;
    }

    // Column resize drag
    if (event.type == MouseEvent::Type::Press) {
        if (!local_rect.contains(event.position)) {
            return false;
        }

        int col = header_resize_hit(event.position.x, event.position.y);
        if (col >= 0) {
            if (event.click_count >= 2) {
                auto_fit_column(col);
                resize_col_ = col;
                return true;
            }
            resize_col_ = col;
            resize_start_x_ = event.position.x;
            resize_start_w_ = column_widths_[col];
            return true;
        }
    }

    if (event.type == MouseEvent::Type::Drag && resize_col_ >= 0) {
        float delta = event.position.x - resize_start_x_;
        float min_w = Theme::current().table_view.min_column_width;
        column_widths_[resize_col_] = std::max(min_w, resize_start_w_ + delta);
        return true;
    }

    if (event.type == MouseEvent::Type::Release && resize_col_ >= 0) {
        resize_col_ = -1;
        return true;
    }

    if (event.type == MouseEvent::Type::Move) {
        if (local_rect.contains(event.position)) {
            over_resize_grip_ = header_resize_hit(event.position.x, event.position.y) >= 0;
            hovered_row_ = row_at_y(event.position.y);
            return true;
        }
        over_resize_grip_ = false;
        hovered_row_ = -1;
        return false;
    }

    if (event.type == MouseEvent::Type::Press) {
        if (!local_rect.contains(event.position)) {
            return false;
        }

        auto hh = header_height();
        if (event.position.y < hh) {
            // Header click, but not on resize grip (handled above).
            // We return true to consume the press so we can sort on release.
            return true;
        }

        // Row click -> select
        auto row = row_at_y(event.position.y);
        if (row < 0) {
            return false;
        }

        auto toggle_mod = event.super || event.ctrl;
        if (multi_select_ && event.shift && anchor_row_ >= 0) {
            cursor_row_ = row;
            select_range_from_anchor();
            notify_selection();
        } else if (multi_select_ && toggle_mod) {
            if (is_selected(row)) {
                selection_.erase(row);
            } else {
                selection_.insert(row);
            }
            anchor_row_ = row;
            cursor_row_ = row;
            notify_selection();
        } else {
            selection_.clear();
            selection_.insert(row);
            anchor_row_ = row;
            cursor_row_ = row;
            notify_selection();
        }
        return true;
    }

    // Header click -> sort (on release, not during resize)
    if (event.type == MouseEvent::Type::Release) {
        if (!local_rect.contains(event.position)) {
            return false;
        }

        auto hh = header_height();
        if (event.position.y < hh) {
            auto col = column_at_x(event.position.x);
            if (col >= 0) {
                if (sort_column_ == col) {
                    sort_order_ = sort_order_ == SortOrder::Ascending ? SortOrder::Descending
                                                                      : SortOrder::Ascending;
                } else {
                    sort_column_ = col;
                    sort_order_ = SortOrder::Ascending;
                }
                selection_.clear();
                anchor_row_ = cursor_row_ = -1;
                rebuild_sort_index();
                if (on_sort_requested) {
                    on_sort_requested(sort_column_, sort_order_);
                }
            }
            return true;
        }
    }

    return false;
}

// ── Keyboard ────────────────────────────────────────────────────────────────

bool TableView::handle_key(KeyEvent const &event) {
    if (!focused_ || !model_ || event.type != KeyEvent::Type::Press) {
        return false;
    }

    auto n = model_->row_count();
    if (n == 0) {
        return false;
    }

    if (event.key == Key::Down) {
        int next = std::min((cursor_row_ < 0 ? 0 : cursor_row_ + 1), n - 1);
        if (multi_select_ && event.shift) {
            if (anchor_row_ < 0) {
                anchor_row_ = next;
            }
            cursor_row_ = next;
            select_range_from_anchor();
        } else {
            set_selected_row(next);
        }
        scroll_to_row(cursor_row_);
        notify_selection();
        return true;
    }
    if (event.key == Key::Up) {
        int next = std::max((cursor_row_ < 0 ? 0 : cursor_row_ - 1), 0);
        if (multi_select_ && event.shift) {
            if (anchor_row_ < 0) {
                anchor_row_ = next;
            }
            cursor_row_ = next;
            select_range_from_anchor();
        } else {
            set_selected_row(next);
        }
        scroll_to_row(cursor_row_);
        notify_selection();
        return true;
    } else if (event.key == Key::Home) {
        if (multi_select_ && event.shift) {
            if (anchor_row_ < 0) {
                anchor_row_ = 0;
            }
            cursor_row_ = 0;
            select_range_from_anchor();
            notify_selection();
        } else {
            set_selected_row(0);
        }
        scroll_to_row(0);
        return true;
    } else if (event.key == Key::End) {
        if (multi_select_ && event.shift) {
            if (anchor_row_ < 0) {
                anchor_row_ = n - 1;
            }
            cursor_row_ = n - 1;
            select_range_from_anchor();
            notify_selection();
        } else {
            set_selected_row(n - 1);
        }
        scroll_to_row(n - 1);
        return true;
    }

    if (multi_select_ && event.text == "a" && (event.super || event.ctrl)) {
        select_all();
        return true;
    }

    return false;
}

Size TableView::size_hint() const {
    auto rh = row_height();
    auto hh = header_height();

    // FIXME what is this *8?
    return {0, hh + rh * 8};
}

} // namespace toolkit
