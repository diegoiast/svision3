// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/button_state.hpp"
#include "svision3/widget.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace svision3 {

class RadioButton;

class RadioGroup {
  public:
    void add(RadioButton *rb);
    void select(RadioButton *rb);

    std::function<void(int index)> on_change;

  private:
    // FIXME: naked pointers are bad,
    std::vector<RadioButton *> buttons_;
    RadioButton *selected_ = nullptr;
};

class RadioButton : public Widget {
    DECLARE_WIDGET(RadioButton)
  public:
    RadioButton(std::string text, RadioGroup &group);
    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    bool trigger_mnemonic(std::string_view key) override;
    void collect_mnemonics(std::vector<Widget *> &out) override {
        if (!mnemonic_key_.empty()) {
            out.push_back(this);
        }
    }

    bool selected() const { return selected_; }
    void set_selected(bool s);

  private:
    friend class RadioGroup;
    void on_state_changed();
    bool should_fire_click() const;

    std::string text_;
    std::string mnemonic_key_;
    RadioGroup &group_;
    bool selected_ = false;
    ButtonStateHandler state_handler_;
};

} // namespace svision3
