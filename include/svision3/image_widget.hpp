// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "svision3/command.hpp"
#include "svision3/scrollable_widget.hpp"
#include <memory>
#include <string>

namespace svision3 {

class ImageWidget : public ScrollableWidget {
    DECLARE_WIDGET(ImageWidget)
  public:
    ImageWidget();
    ~ImageWidget() override = default;
    nlohmann::json to_json() const override;
    void from_json(nlohmann::json const &j) override;

    void load(std::string_view path);
    void load(const uint8_t *data, size_t size);
    void set_image(std::shared_ptr<ImageData> image);

    // The pixels on show, or null when nothing is loaded. Exposed so an
    // application can report on the image -- dimensions, format, memory -- without
    // this widget having to decide what such a report looks like or how it is
    // presented; that is an application's business, not a widget's.
    std::shared_ptr<ImageData> const &image() const { return image_; }

    // Where the image was loaded from, or empty for one handed over directly as
    // pixels. Only load(path) sets it -- there is nothing to report for an image
    // that never came from a file.
    std::string const &source_path() const { return source_path_; }

    void set_show_checkerboard(bool show);
    bool show_checkerboard() const { return show_checkerboard_; }

    void set_zoom(float zoom);
    float zoom() const { return zoom_; }
    void fit_to_widget();

    // The commands this widget contributes to whatever is showing the active
    // tab's actions. All tagged "view", so they land in a View menu rather than
    // in the Edit menu a text widget fills.
    Command::Ptr zoom_in_cmd;
    Command::Ptr zoom_out_cmd;
    Command::Ptr actual_size_cmd;
    Command::Ptr fit_cmd;
    Command::Ptr checkerboard_cmd;

    void paint(Painter &painter) override;
    bool handle_mouse(MouseEvent const &event) override;
    bool handle_key(KeyEvent const &event) override;
    Size size_hint() const override;
    CursorShape cursor() const override;

  private:
    std::shared_ptr<ImageData> image_;
    std::string source_path_;
    bool show_checkerboard_ = true;
    float zoom_ = 1.0f;

    // For panning
    bool dragging_ = false;
    Point last_mouse_pos_;

    float content_w() const;
    float content_h() const;
    void on_scroll(float x, float y) override;
};

} // namespace svision3
