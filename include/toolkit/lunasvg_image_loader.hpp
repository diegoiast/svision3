// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/image.hpp"

namespace toolkit {

class LunasvgImageLoader : public SVGLoaderInterface {
  public:
    auto load(std::string_view path) -> Icon override;
    auto load_from_memory(const uint8_t *data, size_t size) -> Icon override;

    auto load_svg(std::string_view path, int width, int height) -> Icon override;
    auto load_svg_from_memory(const uint8_t *data, size_t size, int width, int height)
        -> Icon override;
    auto supported_extensions() const -> std::vector<std::string> override;
    auto save(ImageData const &image, std::string_view path) -> bool override;
};

} // namespace toolkit
