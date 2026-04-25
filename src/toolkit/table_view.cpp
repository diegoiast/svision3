// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/table_view.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>
#include <numeric>

namespace toolkit {

TableView::TableView(std::shared_ptr<ItemModel> model) : model_(std::move(model)) {
    set_focusable(true);
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

TableView &TableView::set_model(std::shared_ptr<ItemModel> model) {
    model_ = std::move(model);
    column_widths_.clear();
    sort_indices_.clear();
    selection_.clear();
    anchor_row_ = cursor_row_ = hovered_row_ = std::nullopt;
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
    return *this;
}

size_t TableView::model_row(size_t display_row) const {
    if (display_row >= sort_indices_.size()) {
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
    std::iota(sort_indices_.begin(), sort_indices_.end(), size_t{0});

    if (sort_column_ < 0 || sort_order_ == SortOrder::None) {
        return;
    }

    auto col = static_cast<size_t>(sort_column_);
    auto *m = model_.get();
    auto ascending = (sort_order_ == SortOrder::Ascending);

    // FIXME: we need to allow users a custom sort method
    std::stable_sort(sort_indices_.begin(), sort_indices_.end(),
                     [m, col, ascending](size_t a, size_t b) {
                         return ascending ? (m->cell_text(a, col) < m->cell_text(b, col))
                                          : (m->cell_text(a, col) > m->cell_text(b, col));
                     });
}

void TableView::ensure_column_widths() {
    if (!model_) {
        return;
    }
    auto cols = model_->column_count();
    auto const &style = Theme::current().table_view;

    while (column_widths_.size() < cols) {
        column_widths_.push_back(style.default_column_width);
    }
}

void TableView::auto_fit_column(int col) {
    if (!model_ || col < 0 || static_cast<size_t>(col) >= model_->column_count()) {
        return;
    }
    auto const &theme = Theme::current();
    auto const &style = Theme::current().table_view;
    auto const &palette = theme.palette;

    auto padding = style.item_padding_h * 2;
    auto nrows = model_->row_count();
    auto sort_arrow_w = measure_text(" \xe2\x96\xb2", palette.fonts.size).width;
    ensure_column_widths();

    auto header = model_->header_text(static_cast<size_t>(col));
    auto max_w = measure_text(header, palette.fonts.size).width + sort_arrow_w;
    auto sample = std::min(nrows, size_t{100});

    for (auto r = size_t{0}; r < sample; r++) {
        auto w =
            measure_text(model_->cell_text(r, static_cast<size_t>(col)), palette.fonts.size).width;
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
    for (auto c = size_t{0}; c < ncols; c++) {
        auto_fit_column(static_cast<int>(c));
    }
}

CursorShape TableView::cursor() const {
    if (resize_col_ >= 0 || over_resize_grip_) {
        return CursorShape::ResizeEW;
    }
    return CursorShape::Arrow;
}

TableView &TableView::set_column_width(int column, float width) {
    ensure_column_widths();
    auto const &style = Theme::current().table_view;
    if (column >= 0 && column < static_cast<int>(column_widths_.size())) {
        column_widths_[column] = std::max(width, style.min_column_width);
    }
    return *this;
}

float TableView::column_width(int column) const {
    if (column >= 0 && column < static_cast<int>(column_widths_.size())) {
        return column_widths_[column];
    }
    return Theme::current().table_view.default_column_width;
}

// ── Selection ────────────────────────────────────────────────────────────────

TableView &TableView::set_selected_row(std::optional<size_t> row) {
    if (!model_) {
        return *this;
    }
    if (!row || *row >= model_->row_count()) {
        clear_selection();
        return *this;
    }
    selection_.clear();
    selection_.insert(*row);
    anchor_row_ = row;
    cursor_row_ = row;
    notify_selection();
    return *this;
}

TableView &TableView::set_selection(std::set<size_t> rows) {
    selection_ = std::move(rows);
    if (!selection_.empty()) {
        anchor_row_ = *selection_.begin();
        cursor_row_ = *selection_.rbegin();
    } else {
        anchor_row_ = cursor_row_ = std::nullopt;
    }
    notify_selection();
    return *this;
}

TableView &TableView::select_all() {
    if (!model_) {
        return *this;
    }
    auto n = model_->row_count();
    selection_.clear();
    for (auto i = size_t{0}; i < n; i++) {
        selection_.insert(i);
    }
    anchor_row_ = size_t{0};
    cursor_row_ = n > 0 ? std::optional<size_t>{n - 1} : std::nullopt;
    notify_selection();
    return *this;
}

TableView &TableView::clear_selection() {
    selection_.clear();
    anchor_row_ = cursor_row_ = std::nullopt;
    notify_selection();
    return *this;
}

void TableView::select_range_from_anchor() {
    if (!anchor_row_ || !cursor_row_) {
        return;
    }
    selection_.clear();
    auto lo = std::min(*anchor_row_, *cursor_row_);
    auto hi = std::max(*anchor_row_, *cursor_row_);
    for (auto i = lo; i <= hi; i++) {
        selection_.insert(i);
    }
}

TableView &TableView::set_multi_select(bool enabled) {
    multi_select_ = enabled;
    return *this;
}

TableView &TableView::set_alternating_row_colors(bool enabled) {
    alternating_ = enabled;
    return *this;
}

TableView &TableView::set_show_header(bool show) {
    if (show_header_ != show) {
        show_header_ = show;
        invalidate_layout();
    }
    return *this;
}

void TableView::notify_selection() {
    if (on_selection_changed) {
        on_selection_changed(cursor_row_ ? std::optional<size_t>{model_row(*cursor_row_)}
                                         : std::nullopt);
    }
}

// ── Geometry ─────────────────────────────────────────────────────────────────

float TableView::row_height() const {
    auto const &style = Theme::current().table_view;
    auto const &palette = Theme::current().palette;
    auto fm = font_metrics(palette.fonts.size);
    return fm.height + style.item_padding * 2;
}

float TableView::header_height() const {
    if (!show_header_) {
        return 0.0f;
    }
    auto const &style = Theme::current().table_view;
    auto const &palette = Theme::current().palette;
    auto fm = font_metrics(palette.fonts.size);
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
    auto bw = Theme::current().palette.border_width;
    auto hh = header_height();
    auto visible_h = rect_.height - hh - bw * 2;
    auto content_h = total_content_height();
    auto max_y = std::max(0.0f, content_h - visible_h);
    auto visible_w = rect_.width - bw * 2;
    auto content_w = total_content_width();
    auto max_x = std::max(0.0f, content_w - visible_w);

    scroll_y_ = std::clamp(scroll_y_, 0.0f, max_y);
    scroll_x_ = std::clamp(scroll_x_, 0.0f, max_x);
    if (window()) {
        window()->request_redraw("TableView::clamp_scroll");
    }
}

std::optional<size_t> TableView::row_at_y(float y) const {
    if (!model_) {
        return std::nullopt;
    }
    auto bw = Theme::current().palette.border_width;
    auto hh = header_height();
    auto local_y = y - hh - bw + scroll_y_;

    if (local_y < 0) {
        return std::nullopt;
    }

    auto idx = static_cast<size_t>(local_y / row_height());
    if (idx >= model_->row_count()) {
        return std::nullopt;
    }
    return idx;
}

int TableView::column_at_x(float x) const {
    auto bw = Theme::current().palette.border_width;
    auto cx = bw - scroll_x_;
    for (auto c = 0; c < static_cast<int>(column_widths_.size()); c++) {
        cx += column_widths_[c];
        if (x < cx) {
            return c;
        }
    }
    return -1;
}

int TableView::header_resize_hit(float x, float y) const {
    auto bw = Theme::current().palette.border_width;
    auto hh = header_height();
    if (y < bw || y > hh + bw) {
        return -1;
    }

    auto constexpr grip = 6.0f;
    auto cx = bw - scroll_x_;
    for (auto c = 0; c < static_cast<int>(column_widths_.size()); c++) {
        cx += column_widths_[c];
        if (x >= cx - grip && x <= cx + grip) {
            return c;
        }
    }
    return -1;
}

void TableView::scroll_to_row(size_t row) {
    // FIXME: modify variables to more descriptive names
    auto bw = Theme::current().palette.border_width;
    auto rh = row_height();
    auto hh = header_height();
    auto visible_h = rect_.height - hh - bw * 2;
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
    auto const &theme = Theme::current();
    theme.draw_table_background(painter, {0, 0, rect_.width, rect_.height}, is_focused());

    if (!model_) {
        return;
    }
    auto const &palette = theme.palette;
    auto const &style = theme.table_view;
    ensure_column_widths();

    auto rh = row_height();
    auto hh = header_height();
    auto nrows = model_->row_count();
    auto ncols = model_->column_count();
    auto is_dark = palette.window.luma() < 0.5f;

    auto bw = palette.border_width;
    if (show_header_) {
        auto header_rect = Rect{bw, bw, rect_.width - bw * 2, hh};
        auto hx = bw - scroll_x_;
        auto header_bg = is_dark ? palette.base : palette.alternate;

        if (palette.corner_radius > 0) {
            auto cr = std::max(0.0f, palette.corner_radius - bw);
            painter.push_clip(header_rect);
            painter.fill_rounded_rect(
                {header_rect.x, header_rect.y, header_rect.width, header_rect.height + cr},
                header_bg, cr);
            painter.pop_clip();
        } else {
            painter.fill_rect(header_rect, header_bg);
        }

        for (auto c = size_t{0}; c < ncols; c++) {
            auto cw = column_widths_[c];
            auto sep_x = hx + cw;

            if (sep_x > bw && hx < rect_.width - bw) {
                auto text_y = bw + (hh - painter.font_metrics(palette.fonts.size).height) / 2.0f +
                              painter.font_metrics(palette.fonts.size).ascent;
                auto text = model_->header_text(c);

                if (static_cast<int>(c) == sort_column_ && sort_order_ != SortOrder::None) {
                    text +=
                        (sort_order_ == SortOrder::Ascending) ? " \xe2\x96\xb2" : " \xe2\x96\xbc";
                }

                // FIXME: add theme primitive to draw the table header
                painter.push_clip(
                    {std::max(hx, bw), bw, std::min(cw, rect_.width - bw - std::max(hx, bw)), hh});
                painter.draw_text(text, {hx + style.item_padding_h, text_y}, palette.text,
                                  palette.fonts.size);
                painter.pop_clip();
            }
            if (sep_x > bw && sep_x < rect_.width - bw) {
                auto border = is_dark ? palette.border.lighten(0.2f) : palette.border;
                painter.draw_line({sep_x, bw}, {sep_x, bw + hh}, border, 0.5f);
            }
            hx += cw;
        }
        painter.draw_line({bw, bw + hh}, {rect_.width - bw, bw + hh}, palette.border);
    }

    auto body_clip = Rect{bw, bw + hh, rect_.width - bw * 2, rect_.height - hh - bw * 2};
    auto first_visible = static_cast<size_t>(std::max(0.0f, scroll_y_) / rh);
    auto last_visible =
        nrows > 0 ? std::min(nrows - 1,
                             static_cast<size_t>((scroll_y_ + rect_.height - hh - bw * 2) / rh))
                  : size_t{0};
    auto alt_color = is_dark ? palette.base.lighten(0.03f) : palette.base.darken(0.02f);

    painter.push_clip(body_clip);
    painter.fill_rect(body_clip, palette.base);
    for (auto i = first_visible; i <= last_visible; i++) {
        auto mr = model_row(i);
        auto ry = bw + hh + rh * static_cast<float>(i) - scroll_y_;
        auto selected = is_selected(i);
        auto hovered = (hovered_row_ == i) && !selected;
        auto alt_row = alternating_ && (i % 2 == 1);

        auto cx = bw - scroll_x_;
        for (auto c = size_t{0}; c < ncols; c++) {
            auto cw = column_widths_[c];
            if (cx + cw > bw && cx < rect_.width - bw) {
                auto cell_rect = Rect{cx, ry, cw, rh};
                painter.push_clip(
                    {std::max(cx, bw), ry, std::min(cw, rect_.width - bw - std::max(cx, bw)), rh});

                if (c == 0) {
                    auto icon = model_->icon_at(mr, 0, 16);
                    theme.draw_list_item(painter, cell_rect, model_->cell_text(mr, c), icon,
                                         selected, hovered, alt_row);
                } else {
                    theme.draw_list_item(painter, cell_rect, model_->cell_text(mr, c), nullptr,
                                         selected, hovered, alt_row);
                }
                painter.pop_clip();
            }
            cx += cw;
        }
    }

    // ── Scrollbars ───────────────────────────────────────────────────────────
    auto content_h = total_content_height();
    auto visible_h = rect_.height - hh;
    if (content_h > visible_h) {
        auto bar_h = std::max(20.0f, visible_h * (visible_h / content_h));
        auto bar_y = hh + (scroll_y_ / content_h) * visible_h;
        auto sb_color = is_dark ? Color::rgba(1, 1, 1, 0.25f) : Color::rgba(0, 0, 0, 0.25f);
        auto sb = Rect{rect_.width - 6.0f, bar_y, 4.0f, bar_h};
        painter.fill_rounded_rect(sb, sb_color, 2.0f);
    }

    auto content_w = total_content_width();
    if (content_w > rect_.width) {
        auto bar_w = std::max(20.0f, rect_.width * (rect_.width / content_w));
        auto bar_x = (scroll_x_ / content_w) * rect_.width;
        auto sb_color = is_dark ? Color::rgba(1, 1, 1, 0.25f) : Color::rgba(0, 0, 0, 0.25f);
        auto sb = Rect{bar_x, rect_.height - 6.0f, bar_w, 4.0f};
        painter.fill_rounded_rect(sb, sb_color, 2.0f);
    }

    painter.pop_clip(); // body
}

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

    if (event.type == MouseEvent::Type::Press) {
        if (!local_rect.contains(event.position)) {
            return false;
        }

        auto col = header_resize_hit(event.position.x, event.position.y);
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
        auto delta = event.position.x - resize_start_x_;
        auto min_w = Theme::current().table_view.min_column_width;
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
        hovered_row_ = std::nullopt;
        return false;
    }

    if (event.type == MouseEvent::Type::Press) {
        if (!local_rect.contains(event.position)) {
            return false;
        }

        if (event.button == 3 && on_back_requested) {
            on_back_requested();
            return true;
        }

        auto hh = header_height();
        if (event.position.y < hh) {
            return true;
        }

        auto row = row_at_y(event.position.y);
        if (!row) {
            return false;
        }

        auto toggle_mod = event.super || event.ctrl;
        if (multi_select_ && event.shift && anchor_row_) {
            cursor_row_ = row;
            select_range_from_anchor();
            notify_selection();
        } else if (multi_select_ && toggle_mod) {
            if (is_selected(*row)) {
                selection_.erase(*row);
            } else {
                selection_.insert(*row);
            }
            anchor_row_ = row;
            cursor_row_ = row;
            notify_selection();
        } else {
            selection_.clear();
            selection_.insert(*row);
            anchor_row_ = row;
            cursor_row_ = row;
            notify_selection();
        }
        if (event.click_count >= 2 && on_item_activated) {
            on_item_activated(model_row(*row));
        }
        return true;
    }

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
                anchor_row_ = cursor_row_ = std::nullopt;
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

bool TableView::handle_key(KeyEvent const &event) {
    if (!is_focused() || !model_ || event.type != KeyEvent::Type::Press) {
        return false;
    }

    auto n = model_->row_count();
    if (n == 0) {
        return false;
    }

    // FIXME: convert to switch
    if (event.key == Key::Down) {
        auto next = cursor_row_ ? std::min(*cursor_row_ + 1, n - 1) : size_t{0};
        if (multi_select_ && event.shift) {
            if (!anchor_row_) {
                anchor_row_ = next;
            }
            cursor_row_ = next;
            select_range_from_anchor();
        } else {
            set_selected_row(next);
        }
        scroll_to_row(*cursor_row_);
        notify_selection();
        return true;
    }
    if (event.key == Key::Up) {
        auto next = (cursor_row_ && *cursor_row_ > 0) ? *cursor_row_ - 1 : size_t{0};
        if (multi_select_ && event.shift) {
            if (!anchor_row_) {
                anchor_row_ = next;
            }
            cursor_row_ = next;
            select_range_from_anchor();
        } else {
            set_selected_row(next);
        }
        scroll_to_row(*cursor_row_);
        notify_selection();
        return true;
    } else if (event.key == Key::Home) {
        if (multi_select_ && event.shift) {
            if (!anchor_row_) {
                anchor_row_ = size_t{0};
            }
            cursor_row_ = size_t{0};
            select_range_from_anchor();
            notify_selection();
        } else {
            set_selected_row(size_t{0});
        }
        scroll_to_row(0);
        return true;
    } else if (event.key == Key::End) {
        auto last = n - 1;
        if (multi_select_ && event.shift) {
            if (!anchor_row_) {
                anchor_row_ = last;
            }
            cursor_row_ = last;
            select_range_from_anchor();
            notify_selection();
        } else {
            set_selected_row(last);
        }
        scroll_to_row(last);
        return true;
    } else if (event.key == Key::PageDown) {
        auto bw = Theme::current().palette.border_width;
        auto page =
            std::max(size_t{1}, static_cast<size_t>((rect_.height - bw * 2) / row_height()));
        auto next = cursor_row_ ? std::min(*cursor_row_ + page, n - 1) : size_t{0};
        if (multi_select_ && event.shift) {
            if (!anchor_row_) {
                anchor_row_ = next;
            }
            cursor_row_ = next;
            select_range_from_anchor();
            notify_selection();
        } else {
            set_selected_row(next);
        }
        scroll_to_row(*cursor_row_);
        return true;
    } else if (event.key == Key::PageUp) {
        auto bw = Theme::current().palette.border_width;
        auto page =
            std::max(size_t{1}, static_cast<size_t>((rect_.height - bw * 2) / row_height()));
        auto next = (cursor_row_ && *cursor_row_ >= page) ? *cursor_row_ - page : size_t{0};
        if (multi_select_ && event.shift) {
            if (!anchor_row_) {
                anchor_row_ = next;
            }
            cursor_row_ = next;
            select_range_from_anchor();
            notify_selection();
        } else {
            set_selected_row(next);
        }
        scroll_to_row(*cursor_row_);
        return true;
    }

    if (event.key == Key::Enter && cursor_row_ && on_item_activated) {
        on_item_activated(model_row(*cursor_row_));
        return true;
    }

    // FIXME select all should be an action
    if (multi_select_ && event.text == "a" && (event.super || event.ctrl)) {
        select_all();
        return true;
    }

    return false;
}

Size TableView::size_hint() const {
    auto rh = row_height();
    auto hh = header_height();

    // FIXME: what is this *8?
    return {0, hh + rh * 8};
}

} // namespace toolkit
