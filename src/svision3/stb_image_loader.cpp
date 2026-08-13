// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "svision3/pixel_format.hpp"
#include "svision3/stb_image_loader.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace svision3 {

// stb_image/stb_image_write's native format is R,G,B,A. Only swap if BGRA was requested.

auto StbImageLoader::load(std::string_view path, PixelFormat format) -> Icon {
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
    img->format = format;
    img->pixels.assign(data, data + (static_cast<size_t>(w) * h * 4));
    stbi_image_free(data);
    if (format == PixelFormat::BGRA) {
        pixel::swap_rb(img->pixels.data(), static_cast<size_t>(w) * h);
    }

    return img;
}

auto StbImageLoader::load_from_memory(const uint8_t *data, size_t size, PixelFormat format)
    -> Icon {
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
    img->format = format;
    img->pixels.assign(stb_data, stb_data + (static_cast<size_t>(w) * h * 4));
    stbi_image_free(stb_data);
    if (format == PixelFormat::BGRA) {
        pixel::swap_rb(img->pixels.data(), static_cast<size_t>(w) * h);
    }

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

    // stb_image_write always expects R,G,B,A; image.format says what image.pixels actually is.
    // Rather than copy the whole buffer, swap in place and swap back before returning -- `image`
    // is only const at this call site, not at its point of definition, so this is well-defined
    // and leaves the caller's data byte-for-byte unchanged once save() returns.
    auto pixel_count = static_cast<size_t>(image.width) * image.height;
    auto *pixels = const_cast<uint8_t *>(image.pixels.data());
    auto needs_swap = image.format == PixelFormat::BGRA;
    if (needs_swap) {
        pixel::swap_rb(pixels, pixel_count);
    }

    bool ok;
    if (ext == ".jpg" || ext == ".jpeg") {
        ok = stbi_write_jpg(s_path.c_str(), image.width, image.height, image.channels, pixels,
                            90) != 0;
    } else if (ext == ".bmp") {
        ok = stbi_write_bmp(s_path.c_str(), image.width, image.height, image.channels, pixels) !=
             0;
    } else if (ext == ".tga") {
        ok = stbi_write_tga(s_path.c_str(), image.width, image.height, image.channels, pixels) !=
             0;
    } else {
        // Default to PNG
        ok = stbi_write_png(s_path.c_str(), image.width, image.height, image.channels, pixels,
                            image.width * image.channels) != 0;
    }

    if (needs_swap) {
        pixel::swap_rb(pixels, pixel_count);
    }
    return ok;
}

} // namespace svision3
