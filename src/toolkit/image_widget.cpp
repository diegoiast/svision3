// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/image_widget.hpp"
#include "toolkit/application.hpp"
#include "toolkit/painter.hpp"
#include "toolkit/platform.hpp"
#include <algorithm>

namespace toolkit {

ImageWidget::ImageWidget() {
    state.focusable = true;
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
    update_size();
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
    update_size();
    if (window_) {
        window_->request_redraw("ImageWidget::set_zoom");
    }
}

Size ImageWidget::size_hint() const {
    if (!image_) {
        return {100, 100};
    }
    // Maintain aspect ratio
    float aspect = (float)image_->width / image_->height;
    float w = image_->width * zoom_;
    float h = w / aspect;
    return {w, h};
}

void ImageWidget::update_size() {
    auto sz = size_hint();
    set_rect({rect_.x, rect_.y, sz.width, sz.height});
}

void ImageWidget::paint(Painter &painter) {
    if (show_checkerboard_) {
        float size = 10.0f;
        for (float y = 0; y < rect_.height; y += size) {
            for (float x = 0; x < rect_.width; x += size) {
                bool light = ((int)(x / size) + (int)(y / size)) % 2 == 0;
                painter.fill_rect({x, y, size, size}, light ? Color::with_gray(0.9f) : Color::with_gray(0.7f));
            }
        }
    }

    if (image_) {
        // Draw image keeping aspect ratio within rect_
        float aspect = (float)image_->width / image_->height;
        float view_aspect = rect_.width / rect_.height;
        Rect dest;
        if (aspect > view_aspect) {
            dest.width = rect_.width;
            dest.height = rect_.width / aspect;
            dest.x = 0;
            dest.y = (rect_.height - dest.height) / 2.0f;
        } else {
            dest.height = rect_.height;
            dest.width = rect_.height * aspect;
            dest.y = 0;
            dest.x = (rect_.width - dest.width) / 2.0f;
        }
        painter.draw_image_scaled(*image_, dest);
    }
}

bool ImageWidget::handle_mouse(MouseEvent const &event) {
    if (event.type == MouseEvent::Type::Scroll) {
        float factor = 1.1f;
        if (event.scroll_dy > 0) {
            set_zoom(zoom_ * factor);
        } else if (event.scroll_dy < 0) {
            set_zoom(zoom_ / factor);
        }
        return true;
    }

    if (event.type == MouseEvent::Type::Press && event.button == 1) {
        dragging_ = true;
        last_mouse_pos_ = event.position;
        return true;
    }

    if (event.type == MouseEvent::Type::Release) {
        dragging_ = false;
        return true;
    }

    if (dragging_ && event.type == MouseEvent::Type::Drag) {
        Point delta = {event.position.x - last_mouse_pos_.x, event.position.y - last_mouse_pos_.y};
        set_rect({rect_.x + delta.x, rect_.y + delta.y, rect_.width, rect_.height});
        last_mouse_pos_ = event.position;
        return true;
    }

    return false;
}

bool ImageWidget::handle_key(KeyEvent const &event) {
    if (event.type == KeyEvent::Type::Press) {
        if (event.key == Key::Up) {
            set_zoom(zoom_ * 1.1f);
            return true;
        } else if (event.key == Key::Down) {
            set_zoom(zoom_ / 1.1f);
            return true;
        }
    }
    return false;
}

} // namespace toolkit
