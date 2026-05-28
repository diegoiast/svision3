// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/image_widget.hpp"
#include "spdlog/spdlog.h"
#include "toolkit/application.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/platform.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>

namespace toolkit {

ImageWidget::ImageWidget() {
    state.focusable = true;
    state.non_focus_input = true;
}

nlohmann::json ImageWidget::to_json() const {
    auto j = Widget::to_json();
    j["checkboard"] = show_checkerboard_;
    j["zoom"] = zoom_;
    j["scroll_y"] = scroll_y_;
    j["scroll_x"] = scroll_x_;
    return j;
}

void ImageWidget::from_json(nlohmann::json const &j) {
    Widget::from_json(j);
    if (j.contains("checkboard")) {
        show_checkerboard_ = j["checkboard"];
    }
    if (j.contains("zoom")) {
        zoom_ = j["zoom"];
    }
    if (j.contains("scroll_y")) {
        scroll_y_ = j["scroll_y"];
    }
    if (j.contains("scroll_x")) {
        scroll_x_ = j["scroll_x"];
    }
}

void ImageWidget::load(std::string_view path) {
    auto loader = detail::current_platform()->create_image_loader();
    set_image(loader->load(path));
}

void ImageWidget::load(const uint8_t *data, size_t size) {
    auto loader = detail::current_platform()->create_image_loader();
    set_image(loader->load(data, size));
}

void ImageWidget::set_image(std::shared_ptr<ImageData> image) {
    image_ = image;
    if (window_) {
        window_->request_redraw("ImageWidget::set_image");
    }
}

void ImageWidget::set_show_checkerboard(bool show) {
    show_checkerboard_ = show;
    if (window_) {
        window_->request_redraw("ImageWidget::set_show_checkerboard");
    }
}

void ImageWidget::set_zoom(float zoom) {
    zoom_ = std::clamp(zoom, 0.1f, 10.0f);
    clamp_scroll();
    if (window_) {
        window_->request_redraw("ImageWidget::set_zoom");
    }
}

Size ImageWidget::size_hint() const { return {100, 100}; }

float ImageWidget::content_w() const {
    if (!image_) {
        return 0.0f;
    }
    return (float)image_->width * zoom_;
}

float ImageWidget::content_h() const {
    if (!image_) {
        return 0.0f;
    }
    return (float)image_->height * zoom_;
}

void ImageWidget::clamp_scroll() {
    auto w = content_w();
    auto h = content_h();

    scroll_x_ = std::clamp(scroll_x_, 0.0f, std::max(0.0f, w - rect_.width));
    scroll_y_ = std::clamp(scroll_y_, 0.0f, std::max(0.0f, h - rect_.height));
}

void ImageWidget::update_size() { clamp_scroll(); }

void ImageWidget::fit_to_widget() {
    if (!image_) {
        return;
    }
    if (rect_.width <= 0 || rect_.height <= 0) {
        return;
    }

    float aspect = (float)image_->width / image_->height;
    float view_aspect = rect_.width / rect_.height;
    if (aspect > view_aspect) {
        set_zoom(rect_.width / image_->width);
    } else {
        set_zoom(rect_.height / image_->height);
    }
    scroll_x_ = 0;
    scroll_y_ = 0;
    if (window_) {
        window_->request_redraw("ImageWidget::fit_to_widget");
    }
}

void ImageWidget::paint(Painter &painter) {
    if (show_checkerboard_) {
        auto size = 10.0f;
        for (auto y = 0.0f; y < rect_.height; y += size) {
            for (auto x = 0.0f; x < rect_.width; x += size) {
                auto light = ((int)(x / size) + (int)(y / size)) % 2 == 0;
                painter.fill_rect({x, y, size, size},
                                  light ? Color::with_gray(0.9f) : Color::with_gray(0.7f));
            }
        }
    }

    if (image_) {
        auto w = content_w();
        auto h = content_h();
        auto x = -scroll_x_;
        auto y = -scroll_y_;

        if (w < rect_.width) {
            x = (rect_.width - w) / 2.0f;
        }
        if (h < rect_.height) {
            y = (rect_.height - h) / 2.0f;
        }

        Rect dest = {x, y, w, h};
        painter.draw_image_scaled(*image_, dest);
    }

    // Draw scrollbars
    auto needs_h = content_w() > rect_.width;
    auto needs_v = content_h() > rect_.height;

    if (needs_h) {
        auto ratio = rect_.width / content_w();
        auto thumb_len = std::max(20.0f, ratio * rect_.width);
        auto thumb_x = (scroll_x_ / (content_w() - rect_.width)) * (rect_.width - thumb_len);
        painter.fill_rect({thumb_x, rect_.height - 6, thumb_len, 6}, Color::with_gray(0.5f));
    }
    if (needs_v) {
        auto ratio = rect_.height / content_h();
        auto thumb_len = std::max(20.0f, ratio * rect_.height);
        auto thumb_y = (scroll_y_ / (content_h() - rect_.height)) * (rect_.height - thumb_len);
        painter.fill_rect({rect_.width - 6, thumb_y, 6, thumb_len}, Color::with_gray(0.5f));
    }
}

bool ImageWidget::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Scroll) {
        auto factor = 1.1f;
        if (event.scroll_dy > 0) {
            set_zoom(zoom_ * factor);
        } else if (event.scroll_dy < 0) {
            set_zoom(zoom_ / factor);
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Press /*&& event.button == 1*/) {
        spdlog::info("Started dragging");
        if (window_) {
            window_->set_focused_widget(this);
        }
        dragging_ = true;
        last_mouse_pos_ = event.position;
        return true;
    }

    if (event.type == MouseEvent::Type::Release) {
        dragging_ = false;
        return true;
    }

    if (dragging_) {
        if (event.type == MouseEvent::Type::Drag) {
            scroll_x_ -= (event.position.x - last_mouse_pos_.x);
            scroll_y_ -= (event.position.y - last_mouse_pos_.y);
            clamp_scroll();
            last_mouse_pos_ = event.position;
            if (window_) {
                window_->request_redraw("ImageWidget::handle_mouse");
            }
        }
        return true;
    }

    return false;
}

CursorShape ImageWidget::cursor() const {
    return dragging_ ? CursorShape::Move : CursorShape::Arrow;
}

bool ImageWidget::handle_key(KeyEvent const &event) {
    if (event.type == KeyEvent::Type::Press) {
        auto scroll_step = 20.0f;
        if (event.key == Key::Up) {
            scroll_y_ -= scroll_step;
        } else if (event.key == Key::Down) {
            scroll_y_ += scroll_step;
        } else if (event.key == Key::Left) {
            scroll_x_ -= scroll_step;
        } else if (event.key == Key::Right) {
            scroll_x_ += scroll_step;
        } else if (event.key == Key::Plus) {
            set_zoom(zoom_ * 1.1f);
            return true;
        } else if (event.key == Key::Minus) {
            set_zoom(zoom_ / 1.1f);
            return true;
        } else if (event.key == Key::Number0) {
            set_zoom(1.0f);
            return true;
        } else {
            return false;
        }
        clamp_scroll();
        if (window_) {
            window_->request_redraw("ImageWidget::handle_key");
        }
        return true;
    }
    return false;
}

} // namespace toolkit
