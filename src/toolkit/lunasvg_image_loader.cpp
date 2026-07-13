// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#include "toolkit/lunasvg_image_loader.hpp"
#include "toolkit/pixel_format.hpp"
#include "toolkit/theme.hpp"
#include <algorithm>
#include <lunasvg/lunasvg.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

namespace toolkit {

namespace {

auto to_hex(Color const &c) -> std::string {
    auto channel = [](float v) {
        return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return fmt::format("#{:02x}{:02x}{:02x}", channel(c.r), channel(c.g), channel(c.b));
}

// Many icon themes (Breeze and its derivatives) ship "-symbolic" SVGs that don't hardcode a
// fill color: paths use `fill="currentColor"` + `class="ColorScheme-X"`, resolved via a
// `.ColorScheme-X { color: ... }` rule in a `<style id="current-color-scheme">` block baked in
// by the theme author for their intended panel background (e.g. breeze-dark bakes in a near-
// white #fcfcfc, meant for a dark panel -- invisible on a light one). Overriding that rule via
// Document::applyStyleSheet() after load wins the CSS cascade over the icon's own embedded
// style, so we can recolor these icons to match svision3's live theme instead. This is a no-op
// for icons that don't reference these classes.
auto color_scheme_stylesheet() -> std::string {
    auto const &p = Theme::current().palette;
    return fmt::format(".ColorScheme-Text {{ color: {0}; }}"
                       ".ColorScheme-Background {{ color: {1}; }}"
                       ".ColorScheme-Highlight {{ color: {2}; }}"
                       ".ColorScheme-HighlightText {{ color: {3}; }}"
                       ".ColorScheme-ButtonText {{ color: {0}; }}"
                       ".ColorScheme-ButtonBackground {{ color: {4}; }}"
                       ".ColorScheme-ViewText {{ color: {0}; }}"
                       ".ColorScheme-ViewBackground {{ color: {4}; }}"
                       ".ColorScheme-PositiveText {{ color: {5}; }}"
                       ".ColorScheme-NeutralText {{ color: {6}; }}"
                       ".ColorScheme-NegativeText {{ color: {7}; }}",
                       to_hex(p.text), to_hex(p.window), to_hex(p.highlight),
                       to_hex(p.highlighted_text), to_hex(p.base), to_hex(p.success),
                       to_hex(p.warning), to_hex(p.error));
}

auto render_document(lunasvg::Document &document, int width, int height) -> lunasvg::Bitmap {
    document.applyStyleSheet(color_scheme_stylesheet());
    document.forceLayout();

    if (width <= 0) {
        width = -1;
    }
    if (height <= 0) {
        height = -1;
    }
    return document.renderToBitmap(width, height);
}

// lunasvg renders to ARGB32_Premultiplied, which in memory (little-endian) is byte order
// B,G,R,A -- lunasvg's native format. Only swaps if a different format was requested; always
// un-premultiplies, since that's independent of channel order.
auto pixels_from_bitmap(lunasvg::Bitmap const &bitmap, PixelFormat format) -> std::vector<uint8_t> {
    auto width = bitmap.width();
    auto height = bitmap.height();
    auto *src = bitmap.data();

    auto pixel_count = static_cast<size_t>(width) * height;
    std::vector<uint8_t> out(src, src + pixel_count * 4);
    pixel::unpremultiply(out.data(), pixel_count);
    if (format == PixelFormat::RGBA) {
        pixel::swap_rb(out.data(), pixel_count);
    }
    return out;
}

} // namespace

auto LunasvgImageLoader::load(std::string_view path, PixelFormat format) -> Icon {
    return load_svg(path, 0, 0, format);
}

auto LunasvgImageLoader::load_from_memory(const uint8_t *data, size_t size, PixelFormat format)
    -> Icon {
    return load_svg_from_memory(data, size, 0, 0, format);
}

auto LunasvgImageLoader::load_svg(std::string_view path, int width, int height,
                                  PixelFormat format) -> Icon {
    auto document = lunasvg::Document::loadFromFile(std::string(path));
    if (!document) {
        spdlog::error("lunasvg: failed to load SVG from file: {}", path);
        return nullptr;
    }

    auto bitmap = render_document(*document, width, height);
    if (bitmap.isNull()) {
        spdlog::error("lunasvg: failed to render SVG to bitmap: {}", path);
        return nullptr;
    }

    auto img = std::make_shared<ImageData>();
    img->width = static_cast<int>(bitmap.width());
    img->height = static_cast<int>(bitmap.height());
    img->channels = 4;
    img->format = format;
    img->pixels = pixels_from_bitmap(bitmap, format);

    return img;
}

auto LunasvgImageLoader::load_svg_from_memory(const uint8_t *data, size_t size, int width,
                                              int height, PixelFormat format) -> Icon {
    auto document = lunasvg::Document::loadFromData(reinterpret_cast<const char *>(data), size);
    if (!document) {
        spdlog::error("lunasvg: failed to load SVG from memory");
        return nullptr;
    }

    auto bitmap = render_document(*document, width, height);
    if (bitmap.isNull()) {
        spdlog::error("lunasvg: failed to render SVG to bitmap from memory");
        return nullptr;
    }

    auto img = std::make_shared<ImageData>();
    img->width = static_cast<int>(bitmap.width());
    img->height = static_cast<int>(bitmap.height());
    img->channels = 4;
    img->format = format;
    img->pixels = pixels_from_bitmap(bitmap, format);

    return img;
}

auto LunasvgImageLoader::supported_extensions() const -> std::vector<std::string> {
    return {".svg"};
}

auto LunasvgImageLoader::save(ImageData const &image, std::string_view path) -> bool {
    return false;
}

} // namespace toolkit
