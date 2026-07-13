// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/image.hpp"

namespace toolkit {

class StbImageLoader : public ImageLoaderInterface {
  public:
    // stb_image's native output is R,G,B,A -- requesting PixelFormat::RGBA costs nothing;
    // PixelFormat::BGRA costs one swap_rb pass. Defaults repeated from the interface (see
    // image.hpp) since virtual default arguments bind to the static type of the call, so calls
    // through a concrete StbImageLoader need it here too.
    auto load(std::string_view path, PixelFormat format = detail::default_pixel_format())
        -> Icon override;
    auto load_from_memory(const uint8_t *data, size_t size,
                          PixelFormat format = detail::default_pixel_format()) -> Icon override;
    auto supported_extensions() const -> std::vector<std::string> override;
    // Writes out via stb_image_write, which always expects R,G,B,A -- image.format is consulted
    // to decide whether a swap is needed first, regardless of what format was requested at load.
    auto save(ImageData const &image, std::string_view path) -> bool override;
};

} // namespace toolkit
