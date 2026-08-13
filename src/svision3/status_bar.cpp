// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "svision3/status_bar.hpp"
#include "svision3/theme.hpp"
#include "svision3/window.hpp"
#include <algorithm>

namespace svision3 {

AppearEffect::AppearEffect(int char_interval_ms)
    : char_interval_ms_(std::max(char_interval_ms, 16)) {}

auto AppearEffect::apply(std::string const &text, float elapsed_sec) const -> std::string {
    auto count = static_cast<int>(elapsed_sec * 1000.0f / char_interval_ms_);
    count = std::min(count, static_cast<int>(text.size()));
    count = std::max(count, 0);
    return text.substr(0, static_cast<size_t>(count));
}

auto AppearEffect::interval() const -> float {
    return static_cast<float>(char_interval_ms_) / 1000.0f;
}

auto AppearEffect::is_done(std::string const &text, float elapsed_sec) const -> bool {
    auto count = static_cast<int>(elapsed_sec * 1000.0f / char_interval_ms_);
    return count >= static_cast<int>(text.size());
}

auto AppearEffect::clone() const -> std::unique_ptr<TextEffect> {
    return std::make_unique<AppearEffect>(char_interval_ms_);
}

SpinnerEffect::SpinnerEffect(int frame_interval_ms)
    : frame_interval_ms_(std::max(frame_interval_ms, 16)) {}

auto SpinnerEffect::apply(std::string const &text, float elapsed_sec) const -> std::string {
    static constexpr auto frames = std::string_view{"/|\\-"};
    auto frame = static_cast<int>(elapsed_sec * 1000.0f / frame_interval_ms_) % frames.size();
    auto revealed = std::min(static_cast<int>(elapsed_sec * 1000.0f / char_interval_ms_),
                             static_cast<int>(text.size()));
    revealed = std::max(revealed, 0);
    return text.substr(0, static_cast<size_t>(revealed)) + " " + frames[static_cast<size_t>(frame)];
}

auto SpinnerEffect::interval() const -> float {
    return static_cast<float>(std::min(frame_interval_ms_, char_interval_ms_)) / 1000.0f;
}

auto SpinnerEffect::is_done(std::string const &text, float elapsed_sec) const -> bool {
    auto revealed = static_cast<int>(elapsed_sec * 1000.0f / char_interval_ms_);
    return revealed >= static_cast<int>(text.size());
}

auto SpinnerEffect::clone() const -> std::unique_ptr<TextEffect> {
    return std::make_unique<SpinnerEffect>(frame_interval_ms_);
}

PulseEffect::PulseEffect(int interval_ms) : interval_ms_(std::max(interval_ms, 16)) {}

auto PulseEffect::apply(std::string const &text, float elapsed_sec) const -> std::string {
    auto total = static_cast<int>(text.size());
    if (total == 0) {
        return {};
    }
    auto step = static_cast<int>(elapsed_sec * 1000.0f / interval_ms_);
    step = std::min(step, 2 * total - 1);

    if (step < total) {
        // Phase 1: pulse * characters one by one
        return std::string(static_cast<size_t>(step + 1), '*');
    }
    // Phase 2: reveal original text, replace * chars
    auto revealed = step - total + 1;
    auto stars = total - revealed;
    return text.substr(0, static_cast<size_t>(revealed)) +
           std::string(static_cast<size_t>(stars), '*');
}

auto PulseEffect::interval() const -> float { return static_cast<float>(interval_ms_) / 1000.0f; }

auto PulseEffect::is_done(std::string const &text, float elapsed_sec) const -> bool {
    auto step = static_cast<int>(elapsed_sec * 1000.0f / interval_ms_);
    return step >= 2 * static_cast<int>(text.size());
}

auto PulseEffect::clone() const -> std::unique_ptr<TextEffect> {
    return std::make_unique<PulseEffect>(interval_ms_);
}

StatusBarSection::StatusBarSection(std::string id, std::string text)
    : id_(std::move(id)), text_(std::move(text)) {}

auto StatusBarSection::show() -> StatusBarSection & {
    visible_ = true;
    elapsed_ = 0.0f;
    return *this;
}

auto StatusBarSection::hide() -> StatusBarSection & {
    visible_ = false;
    return *this;
}

auto StatusBarSection::set_text(std::string const &t) -> StatusBarSection & {
    text_ = t;
    elapsed_ = 0.0f;
    return *this;
}

auto StatusBarSection::appear(int char_interval_ms) -> StatusBarSection & {
    effect_ = std::make_unique<AppearEffect>(char_interval_ms);
    elapsed_ = 0.0f;
    return *this;
}

auto StatusBarSection::spinner(int frame_interval_ms) -> StatusBarSection & {
    effect_ = std::make_unique<SpinnerEffect>(frame_interval_ms);
    elapsed_ = 0.0f;
    return *this;
}

auto StatusBarSection::pulse(int frame_interval_ms) -> StatusBarSection & {
    effect_ = std::make_unique<PulseEffect>(frame_interval_ms);
    elapsed_ = 0.0f;
    return *this;
}

auto StatusBarSection::set_effect(std::unique_ptr<TextEffect> effect) -> StatusBarSection & {
    effect_ = std::move(effect);
    elapsed_ = 0.0f;
    return *this;
}

auto StatusBarSection::clear_effect() -> StatusBarSection & {
    effect_.reset();
    elapsed_ = 0.0f;
    return *this;
}

auto StatusBarSection::is_effect_done() const -> bool {
    return effect_ && effect_->is_done(text_, elapsed_);
}

auto StatusBarSection::display_text() const -> std::string {
    if (effect_ && !effect_->is_done(text_, elapsed_)) {
        return effect_->apply(text_, elapsed_);
    }
    return text_;
}

void StatusBarSection::update_elapsed(float delta) { elapsed_ += delta; }

StatusBar::StatusBar() {}

StatusBar::~StatusBar() {
    if (window_ && timer_id_ >= 0) {
        window_->stop_timer(timer_id_);
    }
}

auto StatusBar::find_section(std::string const &id) -> StatusBarSection * {
    auto it = std::find_if(sections_.begin(), sections_.end(),
                           [&](auto const &s) { return s->id() == id; });
    return it != sections_.end() ? it->get() : nullptr;
}

auto StatusBar::add_section(std::string id, std::string text) -> StatusBarSection & {
    auto *existing = find_section(id);
    if (existing) {
        existing->set_text(std::move(text));
        return *existing;
    }
    auto section = std::make_unique<StatusBarSection>(std::move(id), std::move(text));
    auto *ptr = section.get();
    sections_.push_back(std::move(section));
    update_timer();
    invalidate_layout();
    return *ptr;
}

auto StatusBar::section(std::string const &id) -> StatusBarSection * { return find_section(id); }

auto StatusBar::remove_section(std::string const &id) -> void {
    auto it = std::remove_if(sections_.begin(), sections_.end(),
                             [&](auto const &s) { return s->id() == id; });
    if (it != sections_.end()) {
        sections_.erase(it, sections_.end());
        update_timer();
        invalidate_layout();
    }
}

auto StatusBar::clear() -> void {
    sections_.clear();
    update_timer();
    invalidate_layout();
}

void StatusBar::set_window(Window *w) {
    Widget::set_window(w);
    if (w && timer_id_ < 0) {
        update_timer();
    }
}

void StatusBar::on_timer_tick() {
    auto any_active = false;
    for (auto &s : sections_) {
        if (s->is_visible() && s->effect_ && !s->is_effect_done()) {
            s->update_elapsed(s->effect_->interval());
            any_active = true;
        }
    }
    if (any_active && window_) {
        window_->request_redraw("status bar tick");
    }
    if (!any_active && timer_id_ >= 0) {
        window_->stop_timer(timer_id_);
        timer_id_ = -1;
    }
}

void StatusBar::update_timer() {
    if (!window_) {
        return;
    }

    auto min_interval = std::numeric_limits<float>::max();
    auto has_active_effects = false;
    for (auto &s : sections_) {
        if (s->is_visible() && s->effect_ && !s->is_effect_done()) {
            min_interval = std::min(min_interval, s->effect_->interval());
            has_active_effects = true;
        }
    }

    if (timer_id_ >= 0) {
        window_->stop_timer(timer_id_);
        timer_id_ = -1;
    }

    if (has_active_effects) {
        timer_id_ = window_->start_timer(min_interval, [this] { on_timer_tick(); });
    }
}

void StatusBar::paint(Painter &painter) {
    auto palette = Theme::current().palette;
    auto fs = palette.fonts.size;
    auto fm = painter.font_metrics(fs);
    auto height = rect_.height;
    auto x = 4.0f;
    auto baseline = (height - fm.height) / 2.0f + fm.ascent;
    auto bg = palette.window;
    if (!window_->is_active()) {
        if (palette.window_inactive) {
            bg = palette.window_inactive.value();
        }
    }
    painter.fill_rect({0, 0, rect_.width, height}, bg);
    for (auto &s : sections_) {
        if (!s->is_visible()) {
            continue;
        }
        auto display = s->display_text();
        if (display.empty()) {
            continue;
        }
        auto tw = painter.measure_text(display, fs).width;
        painter.draw_text(display, {x, baseline}, palette.text, fs);
        x += tw + 16.0f;
    }
}

bool StatusBar::handle_mouse(MouseEvent const &) { return false; }

Size StatusBar::size_hint() const {
    auto pallete = Theme::current().palette;
    auto fs = pallete.fonts.size;
    auto h = font_metrics(fs).height + 6.0f;
    return {0, h};
}

} // namespace svision3
