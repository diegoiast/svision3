// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/image_loader.hpp"
#include "toolkit/widget.hpp"
#include <memory>

namespace toolkit {

class ImageWidget : public Widget {
  public:
    ImageWidget();
    ~ImageWidget() override = default;

    void load(std::string_view path);
    void load(const uint8_t *data, size_t size);
    void set_image(std::shared_ptr<ImageData> image);

    void set_show_checkerboard(bool show);
    bool show_checkerboard() const { return show_checkerboard_; }

    void set_zoom(float zoom);
    float zoom() const { return zoom_; }

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    CursorShape cursor() const override;

  private:
    std::shared_ptr<ImageData> image_;
    bool show_checkerboard_ = true;
    float zoom_ = 1.0f;
    float scroll_x_ = 0.0f;
    float scroll_y_ = 0.0f;

    // For panning
    bool dragging_ = false;
    Point last_mouse_pos_;

    void update_size();
    void clamp_scroll();
    float content_w() const;
    float content_h() const;
};

} // namespace toolkit
