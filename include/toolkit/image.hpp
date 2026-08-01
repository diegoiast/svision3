// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/pixel_format.hpp"
#include "toolkit/xdg_icons.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace toolkit {

struct ImageData {
    // Straight-alpha bytes per pixel, in `format` order. Every loader sets `format` to whatever
    // it was asked to produce (see ImageLoaderInterface::load()'s `format` parameter), so this
    // is never a guess -- consumers that read pixels directly (or save() implementations that
    // write to a format-fixed C API like stb_image_write) check `format` rather than assuming a
    // fixed layout, and convert on demand via the swap/unpremultiply helpers in
    // toolkit/pixel_format.hpp if it doesn't already match what they need.
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    int channels = 4;
    PixelFormat format = PixelFormat::BGRA;

    // Stable identity for painter-side resource caches (e.g. the GDI+ device-resolution bitmap
    // cache). Monotonic and never reused, so a cache entry belonging to a destroyed image can
    // never be mistaken for a live one -- eviction is then purely a memory question rather than
    // a correctness one. Copies deliberately share the id: a copy has identical pixels, so it
    // should hit the same cache entry. Mutating pixels in place after a draw would leave the
    // entry stale; nothing in the toolkit does that today (loaders build an image once).
    uint64_t id = next_id();

    static uint64_t next_id() {
        static std::atomic<uint64_t> counter{1};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }
};

using Icon = std::shared_ptr<ImageData>;

namespace detail {
// Defined in platform_factory.cpp, where PlatformApplication is visible -- declared here (rather
// than image.hpp including platform.hpp) to avoid a circular include, since platform.hpp already
// includes image.hpp. Resolves to current_platform()->native_pixel_format(), or
// PixelFormat::BGRA if no Application exists yet. Used as the default for every load*() format
// parameter below so ordinary callers never have to think about pixel format at all.
PixelFormat default_pixel_format();
} // namespace detail

/**
 * @brief An interface for loading images.
 *
 * Each platform should provide an implementation of this interface that
 * uses platform-specific APIs for efficient image loading.
 */
class ImageLoaderInterface {
  public:
    virtual ~ImageLoaderInterface() = default;

    /**
     * @brief Load an image from a file path.
     * @param path The path to the image file.
     * @param format Pixel format the caller wants back. Defaults to whatever the active
     *        painter's native format is (see PlatformApplication::native_pixel_format()) -- if
     *        it differs from the loader's own native format, the loader flips the R/B byte
     *        before returning; if it matches, there's no conversion cost at all.
     * @return A shared pointer to the loaded image data, or nullptr if loading failed.
     */
    virtual auto load(std::string_view path,
                      PixelFormat format = detail::default_pixel_format()) -> Icon = 0;

    /**
     * @brief Load an image from memory.
     * @param data Pointer to the image data in memory.
     * @param size Size of the image data in bytes.
     * @param format See load().
     * @return A shared pointer to the loaded image data, or nullptr if loading failed.
     */
    virtual auto load_from_memory(const uint8_t *data, size_t size,
                                  PixelFormat format = detail::default_pixel_format()) -> Icon = 0;

    /**
     * @brief Get the list of supported file extensions (e.g., ".png", ".jpg").
     */
    virtual auto supported_extensions() const -> std::vector<std::string> = 0;

    /**
     * @brief Save an image to a file path.
     *
     * Unlike load(), this takes no format parameter: image.format already says which convention
     * image.pixels is in, and the implementation converts on demand to whatever its underlying
     * write API requires (e.g. stb_image_write always wants R,G,B,A) -- if image.format already
     * matches, there's no conversion cost; if not, the implementation swaps R/B internally before
     * writing. Callers never need to convert image.pixels themselves before calling save().
     *
     * @param image The image data to save.
     * @param path The path to the output file.
     * @return true if saving succeeded, false otherwise.
     */
    virtual auto save(ImageData const &image, std::string_view path) -> bool = 0;
};

class SVGLoaderInterface : public ImageLoaderInterface {
  public:
    virtual auto load_svg(std::string_view path, int width, int height,
                          PixelFormat format = detail::default_pixel_format()) -> Icon = 0;
    virtual auto load_svg_from_memory(const uint8_t *data, size_t size, int width, int height,
                                      PixelFormat format = detail::default_pixel_format())
        -> Icon = 0;
};

class IconProvider {
  public:
    virtual ~IconProvider() = default;

    // For names, use xdg_icons.hpp, XDG::IconContexts, XDG::IconActions or similar. `format`:
    // see ImageLoaderInterface::load() -- kept last (after `context`, which real callers do
    // specify) so a caller that wants a non-default context but a default format doesn't have
    // to spell out the format just to reach it.
    virtual auto load(std::string_view icon_name, int size, std::string_view context = "",
                      PixelFormat format = detail::default_pixel_format()) -> Icon = 0;
};

std::shared_ptr<ImageData> parse_xpm(std::string_view xpm_data, PixelFormat format);

} // namespace toolkit
