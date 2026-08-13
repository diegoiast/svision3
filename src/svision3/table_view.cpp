// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/table_view.hpp"
#include "svision3/painter.hpp"
#include "svision3/theme.hpp"
#include "svision3/window.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <numeric>

namespace svision3 {

TableView::TableView(std::shared_ptr<ItemModel> model) : model_(std::move(model)) {
    set_frame(true, true);
    set_focusable(true);
    if (model_) {
        model_->on_data_changed = [this] {
            rebuild_sort_index();
            auto_fit_columns();
            update_scroll_state();
            // Refresh rather than leave stale: a row tooltip built from data
            // at the last hover-changed moment would otherwise keep showing
            // whatever was there before this update (e.g. a filter box
            // re-rendering the table while the mouse sits still over a row).
            update_row_tooltip();
            if (window()) {
                window()->request_redraw("table selection");
            }
        };
        ensure_column_widths();
        rebuild_sort_index();
    }
}

nlohmann::json TableView::to_json() const {
    auto j = Widget::to_json();
    j["selected_row"] = cursor_row_ ? nlohmann::json(*cursor_row_) : nlohmann::json(nullptr);
    j["selection"] = selection_;
    j["multi_select"] = multi_select_;
    j["alternating_row_colors"] = alternating_;
    j["show_header"] = show_header_;
    j["sort_column"] = sort_column_;
    j["sort_order"] = static_cast<int>(sort_order_);
    if (model_) {
        j["row_count"] = model_->row_count();
    }
    return j;
}

void TableView::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("selected_row") && !j["selected_row"].is_null()) {
        set_selected_row(j["selected_row"].get<size_t>());
    }
    if (j.contains("selection")) {
        set_selection(j["selection"].get<std::set<size_t>>());
    }
    if (j.contains("multi_select")) {
        set_multi_select(j["multi_select"]);
    }
    if (j.contains("alternating_row_colors")) {
        set_alternating_row_colors(j["alternating_row_colors"]);
    }
    if (j.contains("show_header")) {
        set_show_header(j["show_header"]);
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
            update_scroll_state();
            // Refresh rather than leave stale: a row tooltip built from data
            // at the last hover-changed moment would otherwise keep showing
            // whatever was there before this update (e.g. a filter box
            // re-rendering the table while the mouse sits still over a row).
            update_row_tooltip();
            if (window()) {
                window()->request_redraw("table selection");
            }
        };
        auto_fit_columns();
        rebuild_sort_index();
    }
    return *this;
}

void TableView::on_scroll(float /*x*/, float /*y*/) {
    if (window()) {
        window()->request_redraw("table scroll");
    }
}

void TableView::update_scroll_state() {
    update_scrollbars({total_content_width(), total_content_height() + header_height()});
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

    auto it = column_comparators_.find(sort_column_);
    if (it != column_comparators_.end()) {
        auto const &less = it->second;
        std::stable_sort(sort_indices_.begin(), sort_indices_.end(),
                         [m, col, ascending, &less](size_t a, size_t b) {
                             return ascending ? less(m->cell_text(a, col), m->cell_text(b, col))
                                              : less(m->cell_text(b, col), m->cell_text(a, col));
                         });
        return;
    }

    std::stable_sort(sort_indices_.begin(), sort_indices_.end(),
                     [m, col, ascending](size_t a, size_t b) {
                         return ascending ? (m->cell_text(a, col) < m->cell_text(b, col))
                                          : (m->cell_text(a, col) > m->cell_text(b, col));
                     });
}

TableView &TableView::set_row_tooltip_provider(RowTooltipProvider provider) {
    row_tooltip_provider_ = std::move(provider);
    row_tooltip_markdown_ = false;
    update_row_tooltip();
    return *this;
}

TableView &TableView::set_row_markdown_tooltip_provider(RowTooltipProvider provider) {
    row_tooltip_provider_ = std::move(provider);
    row_tooltip_markdown_ = true;
    update_row_tooltip();
    return *this;
}

void TableView::update_row_tooltip() {
    if (!row_tooltip_provider_) {
        return;
    }
    // The row count can shrink out from under an already-hovered row (e.g.
    // filtering the model down via set_data() while the mouse sits still) --
    // model_row() would otherwise fall back to treating the stale display
    // index as a model row directly, which may not even exist anymore.
    if (hovered_row_ && model_ && *hovered_row_ >= model_->row_count()) {
        hovered_row_ = std::nullopt;
    }
    if (!hovered_row_) {
        set_tooltip({});
        return;
    }
    auto text = row_tooltip_provider_(model_row(*hovered_row_));
    if (row_tooltip_markdown_) {
        set_markdown_tooltip(std::move(text));
    } else {
        set_tooltip(std::move(text));
    }
}

TableView &TableView::set_row_context_menu_handler(RowContextMenuHandler handler) {
    row_context_menu_handler_ = std::move(handler);
    return *this;
}

TableView &TableView::set_column_comparator(int column, CellComparator less) {
    if (less) {
        column_comparators_[column] = std::move(less);
    } else {
        column_comparators_.erase(column);
    }
    if (sort_column_ == column) {
        rebuild_sort_index();
    }
    return *this;
}

void TableView::ensure_column_widths() {
    if (!model_) {
        return;
    }
    auto cols = model_->column_count();
    auto const &style = Theme::current().style.tableView;

    while (column_widths_.size() < cols) {
        column_widths_.push_back(style.default_column_width);
    }
}

void TableView::auto_fit_column(int col) {
    if (!model_ || col < 0 || static_cast<size_t>(col) >= model_->column_count()) {
        return;
    }
    auto const &theme = Theme::current();
    auto const &style = Theme::current().style.tableView;
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
    auto const &style = Theme::current().style.tableView;
    if (column >= 0 && column < static_cast<int>(column_widths_.size())) {
        column_widths_[column] = std::max(width, style.min_column_width);
    }
    return *this;
}

float TableView::column_width(int column) const {
    if (column >= 0 && column < static_cast<int>(column_widths_.size())) {
        return column_widths_[column];
    }
    return Theme::current().style.tableView.default_column_width;
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
    auto const &style = Theme::current().style.tableView;
    auto const &palette = Theme::current().palette;
    auto fm = font_metrics(palette.fonts.size);
    return fm.height + style.item_padding * 2;
}

float TableView::header_height() const {
    if (!show_header_) {
        return 0.0f;
    }
    auto const &style = Theme::current().style.tableView;
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

void TableView::set_rect(Rect const &r) {
    Widget::set_rect(r);
    update_scroll_state();
}

std::optional<size_t> TableView::row_at_y(float y) const {
    if (!model_) {
        return std::nullopt;
    }
    auto vr = viewport_rect();
    auto hh = header_height();
    auto local_y = y - vr.y + scroll_y_;

    if (local_y < hh) {
        return std::nullopt;
    }
    auto idx = static_cast<size_t>((local_y - hh) / row_height());
    if (idx >= model_->row_count()) {
        return std::nullopt;
    }
    return idx;
}

int TableView::column_at_x(float x) const {
    auto current_x = 0.0f;
    for (size_t i = 0; i < column_widths_.size(); ++i) {
        if (x >= current_x && x < current_x + column_widths_[i]) {
            return static_cast<int>(i);
        }
        current_x += column_widths_[i];
    }
    return -1;
}

int TableView::header_resize_hit(float x, float y) const {
    if (!show_header_) {
        return -1;
    }

    auto vr = viewport_rect();
    auto hh = header_height();
    if (y < vr.y || y >= vr.y + hh) {
        return -1;
    }

    auto current_x = vr.x - scroll_x_;
    auto grip_width = 8.0f;

    for (size_t i = 0; i < column_widths_.size(); ++i) {
        current_x += column_widths_[i];
        if (std::abs(x - current_x) < grip_width) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

void TableView::scroll_to_row(size_t row) {
    auto rh = row_height();
    auto hh = header_height();
    auto vr = viewport_rect();
    auto top = rh * static_cast<float>(row);
    auto bot = top + rh;

    if (bot > scroll_y_ + vr.height - hh) {
        scroll_y_ = bot - vr.height + hh;
    }
    if (top < scroll_y_) {
        scroll_y_ = top;
    }
    update_scroll_state();
}

void TableView::paint(Painter &painter) {
    auto const &theme = Theme::current();

    if (!model_) {
        draw_scrollbars(painter);
        return;
    }
    auto const &palette = theme.palette;
    auto const &style = theme.style.tableView;
    ensure_column_widths();

    auto rh = row_height();
    auto hh = header_height();
    auto nrows = model_->row_count();
    auto ncols = model_->column_count();
    auto is_dark = palette.window.luma() < 0.5f;
    auto vr = viewport_rect();

    // ── Header ───────────────────────────────────────────────────────────────
    if (show_header_) {
        auto header_rect = Rect{vr.x, vr.y, vr.width, hh};
        auto hx = vr.x - scroll_x_;
        auto header_bg = is_dark ? palette.base : palette.alternate;

        painter.push_clip(header_rect);
        painter.fill_rect(header_rect, header_bg);

        for (auto c = size_t{0}; c < ncols; c++) {
            auto cw = column_widths_[c];
            auto sep_x = hx + cw;

            if (sep_x > vr.x && hx < vr.x + vr.width) {
                auto fm = painter.font_metrics(palette.fonts.size);
                auto text_y = vr.y + (hh - fm.height) / 2.0f + fm.ascent;
                auto text = model_->header_text(c);

                if (static_cast<int>(c) == sort_column_ && sort_order_ != SortOrder::None) {
                    text +=
                        (sort_order_ == SortOrder::Ascending) ? " \xe2\x96\xb2" : " \xe2\x96\xbc";
                }

                painter.push_clip({std::max(hx, vr.x), vr.y,
                                   std::min(cw, vr.x + vr.width - std::max(hx, vr.x)), hh});
                painter.draw_text(text, {hx + style.item_padding_h, text_y}, palette.text,
                                  palette.fonts.size);
                painter.pop_clip();
            }
            if (sep_x > vr.x && sep_x < vr.x + vr.width) {
                auto border = is_dark ? palette.border.lighten(0.2f) : palette.border;
                painter.draw_line({sep_x, vr.y}, {sep_x, vr.y + hh}, border, 0.5f);
            }
            hx += cw;
        }
        painter.draw_line({vr.x, vr.y + hh}, {vr.x + vr.width, vr.y + hh}, palette.border);
        painter.pop_clip();
    }

    auto body_clip = Rect{vr.x, vr.y + hh, vr.width, vr.height - hh};
    auto first_visible = static_cast<size_t>(std::max(0.0f, scroll_y_) / rh);
    auto last_visible =
        nrows > 0 ? std::min(nrows - 1, static_cast<size_t>((scroll_y_ + body_clip.height) / rh))
                  : size_t{0};

    painter.push_clip(body_clip);
    painter.fill_rect(body_clip, palette.base);
    for (auto i = first_visible; i <= last_visible; i++) {
        auto mr = model_row(i);
        auto ry = body_clip.y + rh * static_cast<float>(i) - scroll_y_;
        auto selected = is_selected(i);
        auto hovered = (hovered_row_ == i) && !selected;
        auto alt_row = alternating_ && (i % 2 == 1);

        auto cx = vr.x - scroll_x_;
        for (auto c = size_t{0}; c < ncols; c++) {
            auto cw = column_widths_[c];
            if (cx + cw > vr.x && cx < vr.x + vr.width) {
                auto cell_rect = Rect{cx, ry, cw, rh};
                painter.push_clip({std::max(cx, vr.x), ry,
                                   std::min(cw, vr.x + vr.width - std::max(cx, vr.x)), rh});

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
    painter.pop_clip(); // body

    draw_scrollbars(painter);
}

bool TableView::handle_mouse(MouseEvent const &event) {
    if (!model_) {
        return false;
    }

    // Leave's position is wherever the mouse ended up outside this widget, so
    // it would otherwise always be rejected by the viewport bounds check
    // below -- meaning hovered_row_ (and any row tooltip/highlight tied to
    // it) would never clear when the mouse moves off the table entirely
    // (e.g. up into a filter box above it).
    if (event.type == MouseEvent::Type::Leave) {
        over_resize_grip_ = false;
        if (hovered_row_) {
            hovered_row_ = std::nullopt;
            update_row_tooltip();
            if (window()) {
                window()->request_redraw("table hover leave");
            }
        }
        return true;
    }

    if (handle_scrollbar_mouse(event)) {
        return true;
    }

    auto vr = viewport_rect();
    if (!vr.contains(event.position)) {
        return false;
    }

    auto local_x = event.position.x - vr.x + scroll_x_;
    auto local_y = event.position.y - vr.y + scroll_y_;

    if (event.type == MouseEvent::Type::Press) {
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
        auto min_w = Theme::current().style.tableView.min_column_width;
        column_widths_[resize_col_] = std::max(min_w, resize_start_w_ + delta);
        update_scroll_state();
        return true;
    }

    if (event.type == MouseEvent::Type::Release && resize_col_ >= 0) {
        resize_col_ = -1;
        return true;
    }

    if (event.type == MouseEvent::Type::Move) {
        over_resize_grip_ = header_resize_hit(event.position.x, event.position.y) >= 0;
        auto new_hovered = row_at_y(event.position.y);
        if (new_hovered != hovered_row_) {
            hovered_row_ = new_hovered;
            update_row_tooltip();
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Press) {
        if (event.button == 3 && on_back_requested) {
            on_back_requested();
            return true;
        }

        auto hh = header_height();
        if (event.position.y < vr.y + hh) {
            return true;
        }

        auto row = row_at_y(event.position.y);
        if (!row) {
            return false;
        }

        if (event.button == 1 && row_context_menu_handler_) {
            selection_.clear();
            selection_.insert(*row);
            anchor_row_ = row;
            cursor_row_ = row;
            notify_selection();
            row_context_menu_handler_(model_row(*row), map_to_window(event.position));
            return true;
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
        auto hh = header_height();
        if (event.position.y < vr.y + hh) {
            auto col = column_at_x(local_x);
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
        auto bw = Theme::current().Theme::current().style.border_width;
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
        auto bw = Theme::current().Theme::current().style.border_width;
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

} // namespace svision3
