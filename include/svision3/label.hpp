// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/widget.hpp"
#include <optional>
#include <string>
#include <vector>

namespace svision3 {

class Label : public Widget, public Fluent<Label> {
    DECLARE_WIDGET(Label)
  public:
    explicit Label();
    explicit Label(std::string text);

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    // A label never acts on mouse input (handle_mouse() is a no-op above), so a click/hover
    // resolving here should be treated by containers as "nothing interactive at this point"
    // rather than the label swallowing the event -- see Widget::blocks_hit_test().
    bool blocks_hit_test() const override { return false; }
    Size size_hint() const override;
    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;
    Widget *find_focusable_at(Point p) override {
        if (buddy_ && buddy_->is_enabled() && buddy_->is_focusable() && hit_test(p)) {
            return buddy_;
        }
        return nullptr;
    }

    bool trigger_mnemonic(std::string_view key) override;
    void collect_mnemonics(std::vector<Widget *> &out) override {
        if (!mnemonic_key_.empty()) {
            out.push_back(this);
        }
    }

    Label &set_text(std::string const &text);
    std::string const &text() const { return text_; }

    Label &set_buddy(Widget *buddy) {
        buddy_ = buddy;
        return *this;
    }
    Widget *buddy() const { return buddy_; }

    Label &set_color(Color const &color) {
        color_override_ = color;
        return *this;
    }
    Label &set_font_size(float size) {
        font_size_override_ = size;
        return *this;
    }
    Label &set_shrinkable(bool s) {
        shrinkable_ = s;
        return *this;
    }
    bool shrinkable() const { return shrinkable_; }

    Label &set_alignment(Alignment a) {
        alignment_ = a;
        return *this;
    }
    Alignment alignment() const { return alignment_; }

    Label &set_elide(bool e) {
        elide_ = e;
        return *this;
    }
    bool elide() const { return elide_; }

  private:
    std::string text_;
    std::string mnemonic_key_;
    Widget *buddy_ = nullptr;
    std::optional<Color> color_override_;
    std::optional<float> font_size_override_;
    bool shrinkable_ = false;
    Alignment alignment_ = Alignment::Start;
    bool elide_ = true;
};

} // namespace svision3
