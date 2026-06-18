// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "toolkit/stb_image_loader.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace toolkit {

auto StbImageLoader::load(std::string_view path) -> Icon {
    int w = 0, h = 0, c = 0;
    auto *data = stbi_load(std::string(path).c_str(), &w, &h, &c, STBI_rgb_alpha);

    if (!data) {
        spdlog::error("stb: failed image {} ", std::string(path));
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

auto StbImageLoader::load_from_memory(const uint8_t *data, size_t size) -> Icon {
    int w = 0, h = 0, c = 0;
    auto *stb_data =
        stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &c, STBI_rgb_alpha);

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

auto StbImageLoader::save(ImageData const &image, std::string_view path) -> bool {
    if (image.pixels.empty() || image.width <= 0 || image.height <= 0) {
        return false;
    }

    std::string s_path(path);
    std::string ext;
    auto pos = s_path.find_last_of('.');
    if (pos != std::string::npos) {
        ext = s_path.substr(pos);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return std::tolower(c); });
    }

    if (ext == ".jpg" || ext == ".jpeg") {
        return stbi_write_jpg(s_path.c_str(), image.width, image.height, image.channels,
                              image.pixels.data(), 90) != 0;
    } else if (ext == ".bmp") {
        return stbi_write_bmp(s_path.c_str(), image.width, image.height, image.channels,
                              image.pixels.data()) != 0;
    } else if (ext == ".tga") {
        return stbi_write_tga(s_path.c_str(), image.width, image.height, image.channels,
                              image.pixels.data()) != 0;
    }

    // Default to PNG
    return stbi_write_png(s_path.c_str(), image.width, image.height, image.channels,
                          image.pixels.data(), image.width * image.channels) != 0;
}

} // namespace toolkit
