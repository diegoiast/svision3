// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/stb_image_loader.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <stb_image.h>

namespace toolkit {

auto StbImageLoader::load(std::string_view path) -> std::shared_ptr<ImageData> {
    int w = 0, h = 0, c = 0;
    auto *data = stbi_load(std::string(path).c_str(), &w, &h, &c, STBI_rgb_alpha);

    if (!data) {
        spdlog::error("stb: failed image {} ", path);
        return nullptr;
    }

    auto img = std::make_shared<ImageData>();
    img->width = w;
    img->height = h;
    img->channels = 4;
    img->pixels.assign(data, data + (static_cast<size_t>(w) * h * 4));
    stbi_image_free(data);

    return img;
}

auto StbImageLoader::load(const uint8_t *data, size_t size) -> std::shared_ptr<ImageData> {
    int w = 0, h = 0, c = 0;
    auto *stb_data = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &c, STBI_rgb_alpha);

    if (!stb_data) {
        spdlog::error("stb: failed loading image from memory");
        return nullptr;
    }

    auto img = std::make_shared<ImageData>();
    img->width = w;
    img->height = h;
    img->channels = 4;
    img->pixels.assign(stb_data, stb_data + (static_cast<size_t>(w) * h * 4));
    stbi_image_free(stb_data);

    return img;
}

auto StbImageLoader::supported_extensions() const -> std::vector<std::string> {
    return {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tga", ".hdr"};
}

} // namespace toolkit
