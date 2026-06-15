// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/xdg_icons.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace toolkit {

struct ImageData {
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    int channels = 4;
};

using Icon = std::shared_ptr<ImageData>;

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
     * @return A shared pointer to the loaded image data, or nullptr if loading failed.
     */
    virtual auto load(std::string_view path) -> Icon = 0;

    /**
     * @brief Load an image from memory.
     * @param data Pointer to the image data in memory.
     * @param size Size of the image data in bytes.
     * @return A shared pointer to the loaded image data, or nullptr if loading failed.
     */
    virtual auto load_from_memory(const uint8_t *data, size_t size) -> Icon = 0;

    /**
     * @brief Get the list of supported file extensions (e.g., ".png", ".jpg").
     */
    virtual auto supported_extensions() const -> std::vector<std::string> = 0;
};

class SVGLoaderInterface : public ImageLoaderInterface {
  public:
    virtual auto load_svg(std::string_view path, int width, int height) -> Icon = 0;
    virtual auto load_svg_from_memory(const uint8_t *data, size_t size, int width, int height)
        -> Icon = 0;
};

class IconProvider {
  public:
    virtual ~IconProvider() = default;

    // For names, use xdg_icons.hpp, XDG::IconContexts, XDG::IconActions or similar
    virtual auto load(std::string_view icon_name, int size, std::string_view context = "")
        -> Icon = 0;
};

std::shared_ptr<ImageData> parse_xpm(std::string_view xpm_data);

} // namespace toolkit
