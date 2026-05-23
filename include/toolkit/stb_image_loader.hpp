// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/image.hpp"

namespace toolkit {

class StbImageLoader : public ImageLoaderInterface {
  public:
    auto load(std::string_view path) -> Icon override;
    auto load(const uint8_t *data, size_t size) -> Icon override;
    auto supported_extensions() const -> std::vector<std::string> override;
};

} // namespace toolkit
