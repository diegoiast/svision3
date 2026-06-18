// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/lunasvg_image_loader.hpp"
#include <lunasvg/lunasvg.h>
#include <spdlog/spdlog.h>

namespace toolkit {

auto LunasvgImageLoader::load(std::string_view path) -> Icon { return load_svg(path, 0, 0); }

auto LunasvgImageLoader::load_from_memory(const uint8_t *data, size_t size) -> Icon {
    return load_svg_from_memory(data, size, 0, 0);
}

auto LunasvgImageLoader::load_svg(std::string_view path, int width, int height) -> Icon {
    auto document = lunasvg::Document::loadFromFile(std::string(path));
    if (!document) {
        spdlog::error("lunasvg: failed to load SVG from file: {}", path);
        return nullptr;
    }

    if (width <= 0) {
        width = -1;
    }
    if (height <= 0) {
        height = -1;
    }
    auto bitmap = document->renderToBitmap(width, height);
    if (bitmap.isNull()) {
        spdlog::error("lunasvg: failed to render SVG to bitmap: {}", path);
        return nullptr;
    }

    auto img = std::make_shared<ImageData>();
    img->width = static_cast<int>(bitmap.width());
    img->height = static_cast<int>(bitmap.height());
    img->channels = 4;

    size_t data_size = static_cast<size_t>(img->width) * img->height * 4;
    img->pixels.assign(bitmap.data(), bitmap.data() + data_size);

    return img;
}

auto LunasvgImageLoader::load_svg_from_memory(const uint8_t *data, size_t size, int width,
                                              int height) -> Icon {
    auto document = lunasvg::Document::loadFromData(reinterpret_cast<const char *>(data), size);
    if (!document) {
        spdlog::error("lunasvg: failed to load SVG from memory");
        return nullptr;
    }

    if (width <= 0) {
        width = -1;
    }
    if (height <= 0) {
        height = -1;
    }
    auto bitmap = document->renderToBitmap(width, height);
    if (bitmap.isNull()) {
        spdlog::error("lunasvg: failed to render SVG to bitmap from memory");
        return nullptr;
    }

    auto img = std::make_shared<ImageData>();
    img->width = static_cast<int>(bitmap.width());
    img->height = static_cast<int>(bitmap.height());
    img->channels = 4;

    size_t pixel_size = static_cast<size_t>(img->width) * img->height * 4;
    img->pixels.assign(bitmap.data(), bitmap.data() + pixel_size);

    return img;
}

auto LunasvgImageLoader::supported_extensions() const -> std::vector<std::string> {
    return {".svg"};
}

auto LunasvgImageLoader::save(ImageData const &image, std::string_view path) -> bool {
    return false;
}

} // namespace toolkit
