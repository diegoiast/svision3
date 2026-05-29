// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/widget.hpp"
#include <memory>
#include <string>
#include <vector>

namespace toolkit {

class TextEffect {
  public:
    virtual ~TextEffect() = default;
    virtual auto apply(std::string const &text, float elapsed_sec) const -> std::string = 0;
    virtual auto interval() const -> float = 0;
    virtual auto clone() const -> std::unique_ptr<TextEffect> = 0;
    virtual auto is_done(std::string const &text, float elapsed_sec) const -> bool = 0;
};

class AppearEffect : public TextEffect {
  public:
    explicit AppearEffect(int char_interval_ms = 250);
    auto apply(std::string const &text, float elapsed_sec) const -> std::string override;
    auto interval() const -> float override;
    auto clone() const -> std::unique_ptr<TextEffect> override;
    auto is_done(std::string const &text, float elapsed_sec) const -> bool override;

  private:
    int char_interval_ms_;
};

class SpinnerEffect : public TextEffect {
  public:
    explicit SpinnerEffect(int frame_interval_ms = 100);
    auto apply(std::string const &text, float elapsed_sec) const -> std::string override;
    auto interval() const -> float override;
    auto clone() const -> std::unique_ptr<TextEffect> override;
    auto is_done(std::string const &text, float elapsed_sec) const -> bool override;

  private:
    int frame_interval_ms_;
    int char_interval_ms_ = 250;
};

class PulseEffect : public TextEffect {
  public:
    explicit PulseEffect(int interval_ms = 100);
    auto apply(std::string const &text, float elapsed_sec) const -> std::string override;
    auto interval() const -> float override;
    auto clone() const -> std::unique_ptr<TextEffect> override;
    auto is_done(std::string const &text, float elapsed_sec) const -> bool override;

  private:
    int interval_ms_;
};

class StatusBarSection {
    friend class StatusBar;

  public:
    StatusBarSection(std::string id, std::string text);
    StatusBarSection(StatusBarSection &&) = default;
    StatusBarSection &operator=(StatusBarSection &&) = default;
    StatusBarSection(StatusBarSection const &) = delete;
    StatusBarSection &operator=(StatusBarSection const &) = delete;

    auto show() -> StatusBarSection &;
    auto hide() -> StatusBarSection &;
    auto set_text(std::string const &t) -> StatusBarSection &;
    auto appear(int char_interval_ms = 250) -> StatusBarSection &;
    auto spinner(int frame_interval_ms = 100) -> StatusBarSection &;
    auto pulse(int frame_interval_ms = 100) -> StatusBarSection &;
    auto set_effect(std::unique_ptr<TextEffect> effect) -> StatusBarSection &;
    auto clear_effect() -> StatusBarSection &;

    auto is_visible() const -> bool { return visible_; }
    auto text() const -> std::string const & { return text_; }
    auto id() const -> std::string const & { return id_; }
    auto display_text() const -> std::string;
    auto is_effect_done() const -> bool;

    void update_elapsed(float delta);

  private:
    std::string id_;
    std::string text_;
    bool visible_ = true;
    float elapsed_ = 0.0f;
    std::unique_ptr<TextEffect> effect_;
};

class StatusBar : public Widget, public Fluent<StatusBar> {
    DECLARE_WIDGET(StatusBar)

  public:
    StatusBar();
    ~StatusBar() override;

    auto add_section(std::string id, std::string text = "") -> StatusBarSection &;
    auto section(std::string const &id) -> StatusBarSection *;
    auto remove_section(std::string const &id) -> void;
    auto clear() -> void;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    Size size_hint() const override;
    void set_window(Window *w) override;

  private:
    auto find_section(std::string const &id) -> StatusBarSection *;
    void on_timer_tick();
    void update_timer();

    std::vector<std::unique_ptr<StatusBarSection>> sections_;
    int timer_id_ = -1;
};

} // namespace toolkit
