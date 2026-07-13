// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/image.hpp"

namespace toolkit {

class LunasvgImageLoader : public SVGLoaderInterface {
  public:
    // lunasvg's native output is B,G,R,A (see pixels_from_bitmap in the .cpp) -- requesting
    // PixelFormat::BGRA costs nothing; PixelFormat::RGBA costs one swap_rb pass. Defaults
    // repeated from the interface (see image.hpp) since virtual default arguments bind to the
    // static type of the call, so calls through a concrete LunasvgImageLoader need it here too.
    auto load(std::string_view path, PixelFormat format = detail::default_pixel_format())
        -> Icon override;
    auto load_from_memory(const uint8_t *data, size_t size,
                          PixelFormat format = detail::default_pixel_format()) -> Icon override;

    auto load_svg(std::string_view path, int width, int height,
                  PixelFormat format = detail::default_pixel_format()) -> Icon override;
    auto load_svg_from_memory(const uint8_t *data, size_t size, int width, int height,
                              PixelFormat format = detail::default_pixel_format())
        -> Icon override;
    auto supported_extensions() const -> std::vector<std::string> override;
    auto save(ImageData const &image, std::string_view path) -> bool override;
};

} // namespace toolkit
