// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/list_view.hpp"
#include "toolkit/application.hpp"
#include "toolkit/stopwatch.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"
#include <algorithm>
#include <cctype>
#include <thread>

namespace toolkit {

StringListAdapter::StringListAdapter(std::vector<std::string> items) : items_(std::move(items)) {}

// FIXME: this should be size_t right? Do we support negative indexes?
std::string StringListAdapter::text_at(int index) const {
    if (index < 0 || index >= static_cast<int>(items_.size())) {
        return {};
    }
    return items_[index];
}

void StringListAdapter::set_items(std::vector<std::string> items) {
    items_ = std::move(items);
    if (on_data_changed) {
        on_data_changed();
    }
}

void StringListAdapter::append(std::string item) {
    items_.push_back(std::move(item));
    if (on_data_changed) {
        on_data_changed();
    }
}

// FIXME: this should be size_t right? Do we support negative indexes?
void StringListAdapter::remove(int index) {
    if (index >= 0 && index < static_cast<int>(items_.size())) {
        items_.erase(items_.begin() + index);
        if (on_data_changed) {
            on_data_changed();
        }
    }
}

// ── FilterAdapter ────────────────────────────────────────────────────────────

FilterAdapter::FilterAdapter(std::shared_ptr<ListAdapter> source) : source_(std::move(source)) {
    rebuild_sync();
    if (source_) {
        source_->on_data_changed = [this] {
            rebuild_sync();
            if (on_data_changed) {
                on_data_changed();
            }
        };
    }
}

FilterAdapter::~FilterAdapter() { generation_->fetch_add(1); }

std::string FilterAdapter::text_at(int index) const {
    if (index < 0 || index >= static_cast<int>(indices_.size())) {
        return {};
    }
    return source_->text_at(indices_[index]);
}

// FIXME: this should be an optional right? or error?
int FilterAdapter::source_index(int filtered_index) const {
    if (filtered_index < 0 || filtered_index >= static_cast<int>(indices_.size())) {
        return -1;
    }
    return indices_[filtered_index];
}

void FilterAdapter::set_filter(std::string const &filter) {
    filter_ = filter;
    if (delay_per_item_ms_ > 0) {
        rebuild_async();
    } else {
        rebuild_sync();
        if (on_data_changed) {
            on_data_changed();
        }
    }
}

// FIXME: this is bad, as this only works with ASCII.
static bool contains_icase(std::string const &hay, std::string const &needle) {
    if (needle.empty()) {
        return true;
    }
    auto it = std::search(hay.begin(), hay.end(), needle.begin(), needle.end(), [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    });
    return it != hay.end();
}

void FilterAdapter::rebuild_sync() {
    indices_.clear();
    if (!source_) {
        return;
    }
    auto n = source_->count();
    for (auto i = 0; i < n; i++) {
        if (contains_icase(source_->text_at(i), filter_)) {
            indices_.push_back(i);
        }
    }
}

void FilterAdapter::rebuild_async() {
    auto gen = generation_->fetch_add(1) + 1;

    if (on_progress) {
        on_progress(0.0f);
    }

    auto source = source_;
    auto filter = filter_;
    auto generation = generation_;
    auto delay_ms = delay_per_item_ms_;
    auto self = shared_from_this();

    // FIXME: port to jthread.
    std::thread([self, source, filter, generation, gen, delay_ms]() {
        Stopwatch sw;
        std::vector<int> result;

        if (source) {
            auto n = source->count();
            auto last_percentage = -1;
            // FIXME: this filter method is built in, we need to provide API to modify it
            for (auto i = 0; i < n; i++) {
                if (generation->load() != gen) {
                    return;
                }

                if (contains_icase(source->text_at(i), filter)) {
                    result.push_back(i);
                }

                auto percentage = (n > 0) ? ((i + 1) * 100 / n) : 100;
                if (percentage != last_percentage) {
                    auto progress = static_cast<float>(i + 1) / static_cast<float>(n);
                    last_percentage = percentage;
                    Application::post_to_main_thread([self, generation, gen, progress]() {
                        if (generation->load() != gen) {
                            return;
                        }
                        if (self->on_progress) {
                            self->on_progress(progress);
                        }
                    });
                }

                if (delay_ms > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                }
            }
        }

        if (generation->load() != gen) {
            return;
        }

        auto elapsed = sw.elapsed_ms();
        spdlog::info("Filter '{}': {} results in {:.2f} ms", filter, result.size(), elapsed);

        Application::post_to_main_thread([self, generation, gen, result = std::move(result)]() {
            if (generation->load() != gen) {
                return;
            }
            self->indices_ = std::move(result);
            if (self->on_progress) {
                self->on_progress(-1.0f);
            }
            if (self->on_data_changed) {
                self->on_data_changed();
            }
        });
    }).detach();
}

ListView::ListView(std::shared_ptr<ListAdapter> adapter) {
    adapter_ = std::move(adapter);
    state.focusable = true;
    if (adapter_) {
        adapter_->on_data_changed = [this] {
            clamp_scroll();
            if (window()) {
                window()->request_redraw("list selection");
            }
        };
    }
}

ListView &ListView::set_adapter(std::shared_ptr<ListAdapter> adapter) {
    adapter_ = std::move(adapter);
    selection_.clear();
    anchor_ = -1;
    cursor_ = -1;
    scroll_offset_ = 0;
    if (adapter_) {
        adapter_->on_data_changed = [this] {
            clamp_scroll();
            if (window()) {
                window()->request_redraw("list selection");
            }
        };
    }
    return *this;
}

ListView &ListView::set_selected(int index) {
    if (!adapter_) {
        return *this;
    }
    auto n = adapter_->count();

    if (index < 0 || index >= n) {
        clear_selection();
        return *this;
    }
    selection_.clear();
    selection_.insert(index);
    anchor_ = index;
    cursor_ = index;
    notify_selection();
    return *this;
}

ListView &ListView::set_selection(std::set<int> indices) {
    selection_ = std::move(indices);
    if (!selection_.empty()) {
        anchor_ = *selection_.begin();
        cursor_ = *selection_.rbegin();
    } else {
        anchor_ = cursor_ = -1;
    }
    notify_selection();
    return *this;
}

ListView &ListView::select_all() {
    if (!adapter_) {
        return *this;
    }
    auto n = adapter_->count();
    selection_.clear();
    for (auto i = 0; i < n; i++) {
        selection_.insert(i);
    }
    anchor_ = 0;
    cursor_ = n - 1;
    notify_selection();
    return *this;
}

ListView &ListView::clear_selection() {
    selection_.clear();
    anchor_ = cursor_ = -1;
    notify_selection();
    return *this;
}

void ListView::select_range_from_anchor() {
    selection_.clear();
    auto lo = std::min(anchor_, cursor_);
    auto hi = std::max(anchor_, cursor_);
    for (auto i = lo; i <= hi; i++) {
        selection_.insert(i);
    }
}

void ListView::notify_selection() {
    if (on_selection_changed) {
        on_selection_changed(cursor_);
    }
}

void ListView::scroll_to(int index) {
    auto const &palette = Theme::current().palette;
    auto bw = palette.border_width;
    auto ih = item_height();
    auto top = ih * static_cast<float>(index);
    auto bottom = top + ih;
    auto visible_h = rect_.height - bw * 2;
    if (bottom > scroll_offset_ + visible_h) {
        scroll_offset_ = bottom - visible_h;
    }
    if (top < scroll_offset_) {
        scroll_offset_ = top;
    }
    clamp_scroll();
}

float ListView::item_height() const {
    auto const &style = Theme::current().list_view;
    auto const &palette = Theme::current().palette;
    auto fm = Painter::measure_font_metrics(palette.fonts.size);
    return fm.height + style.item_padding * 2;
}

float ListView::total_content_height() const {
    if (!adapter_) {
        return 0;
    }
    return item_height() * static_cast<float>(adapter_->count());
}

void ListView::clamp_scroll() {
    auto const &palette = Theme::current().palette;
    auto bw = palette.border_width;
    auto visible = rect_.height - bw * 2;
    auto content = total_content_height();
    auto max_scroll = std::max(0.0f, content - visible);
    scroll_offset_ = std::clamp(scroll_offset_, 0.0f, max_scroll);
}

int ListView::item_at_y(float y) const {
    if (!adapter_) {
        return -1;
    }

    auto const &palette = Theme::current().palette;
    auto bw = palette.border_width;
    auto local_y = y - bw + scroll_offset_;
    if (local_y < 0) {
        return -1;
    }
    auto idx = static_cast<int>(local_y / item_height());
    if (idx < 0 || idx >= adapter_->count()) {
        return -1;
    }
    return idx;
}

void ListView::paint(Painter &painter) {
    auto const &theme = Theme::current();  
    theme.draw_list_background(painter, {0, 0, rect_.width, rect_.height}, is_focused());

    if (!adapter_) {
        return;
    }

    auto const &style = theme.list_view;
    auto const &palette = theme.palette;
    auto ih = item_height();
    auto fm = painter.font_metrics(palette.fonts.size);
    auto n = adapter_->count();
    auto bw = palette.border_width;
    auto is_dark = palette.window.luma() < 0.5f;
    auto first_visible = std::max(0, static_cast<int>(scroll_offset_ / ih));
    auto last_visible = std::min(n - 1, static_cast<int>((scroll_offset_ + rect_.height - bw * 2) / ih));
    auto inner_w = rect_.width - bw * 2;
    auto row_sel = palette.highlight;
    auto alt_color = is_dark ? palette.base.lighten(0.03f) : palette.base.darken(0.02f);
    for (auto i = first_visible; i <= last_visible; i++) {
        auto iy = ih * static_cast<float>(i) - scroll_offset_;
        auto item_rect = Rect{0, iy, inner_w, ih};
        auto selected = is_selected(i);
        auto hovered = (i == hovered_) && !selected;
        auto alt_row = alternating_ && (i % 2 == 1);

        theme.draw_list_item(painter, item_rect, adapter_->text_at(i), {}, selected, hovered,
                             alt_row);
    }

    auto content_h = total_content_height();
    if (content_h > rect_.height - bw * 2) {
        auto bar_h = std::max(20.0f, (rect_.height - bw * 2) * ((rect_.height - bw * 2) / content_h));
        auto bar_y = (scroll_offset_ / content_h) * (rect_.height - bw * 2);
        auto bar_x = rect_.width - bw * 2 - 6.0f;
        auto sb = Rect{bar_x, bar_y, 4.0f, bar_h};

        // FIXME: what is this 2.0f?
        painter.fill_rounded_rect(sb, palette.text, 2.0f);
    }
}
bool ListView::handle_mouse(MouseEvent const &event) {
    if (!adapter_) {
        return false;
    }

    auto const local_rect = Rect{0, 0, rect_.width, rect_.height};

    if (event.type == MouseEvent::Type::Scroll) {
        if (!local_rect.contains(event.position)) {
            return false;
        }
        scroll_offset_ -= event.scroll_dy;
        clamp_scroll();
        return true;
    }

    if (event.type == MouseEvent::Type::Move) {
        if (local_rect.contains(event.position)) {
            hovered_ = item_at_y(event.position.y);
            return true;
        }
        hovered_ = -1;
        return false;
    }

    if (event.type == MouseEvent::Type::Press) {
        if (!local_rect.contains(event.position)) {
            return false;
        }
        auto idx = item_at_y(event.position.y);
        if (idx < 0) {
            return false;
        }

        auto toggle_mod = event.super || event.ctrl;

        if (multi_select_ && event.shift && anchor_ >= 0) {
            cursor_ = idx;
            select_range_from_anchor();
            notify_selection();
        } else if (multi_select_ && toggle_mod) {
            if (is_selected(idx)) {
                selection_.erase(idx);
            } else {
                selection_.insert(idx);
            }
            anchor_ = idx;
            cursor_ = idx;
            notify_selection();
        } else {
            selection_.clear();
            selection_.insert(idx);
            anchor_ = idx;
            cursor_ = idx;
            notify_selection();
        }
        return true;
    }

    return false;
}

bool ListView::handle_key(KeyEvent const &event) {
    if (!is_focused() || !adapter_ || event.type != KeyEvent::Type::Press) {
        return false;
    }

    auto n = adapter_->count();
    if (n == 0) {
        return false;
    }

    switch (event.key) {
    case Key::Down: {
        auto next = std::min((cursor_ < 0 ? 0 : cursor_ + 1), n - 1);
        if (multi_select_ && event.shift) {
            if (anchor_ < 0) {
                anchor_ = next;
            }
            cursor_ = next;
            select_range_from_anchor();
        } else {
            set_selected(next);
        }
        scroll_to(cursor_);
        notify_selection();
        return true;
    }
    case Key::Up: {
        auto next = std::max((cursor_ < 0 ? 0 : cursor_ - 1), 0);
        if (multi_select_ && event.shift) {
            if (anchor_ < 0) {
                anchor_ = next;
            }
            cursor_ = next;
            select_range_from_anchor();
        } else {
            set_selected(next);
        }
        scroll_to(cursor_);
        notify_selection();
        return true;
    }
    case Key::Home: {
        if (multi_select_ && event.shift) {
            if (anchor_ < 0) {
                anchor_ = 0;
            }
            cursor_ = 0;
            select_range_from_anchor();
            notify_selection();
        } else {
            set_selected(0);
        }
        scroll_to(0);
        return true;
    }
    case Key::End: {
        if (multi_select_ && event.shift) {
            if (anchor_ < 0) {
                anchor_ = n - 1;
            }
            cursor_ = n - 1;
            select_range_from_anchor();
            notify_selection();
        } else {
            set_selected(n - 1);
        }
        scroll_to(n - 1);
        return true;
    }
    default:
        // Just to keep the compiler happy
        break;
    }

    if (multi_select_ && event.text == "a" && (event.super || event.ctrl)) {
        select_all();
        return true;
    }

    return false;
}

Size ListView::size_hint() const {
    auto item_measured_height = item_height();
    // FIXME what is this 8 here...?
    auto hz = item_measured_height * 8;
    return {0, hz};
}

} // namespace toolkit
