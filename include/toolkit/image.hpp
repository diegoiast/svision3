// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "toolkit/image_loader.hpp"
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace toolkit {

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
    virtual auto load(std::string_view path) -> std::shared_ptr<ImageData> = 0;

    /**
     * @brief Load an image from memory.
     * @param data Pointer to the image data in memory.
     * @param size Size of the image data in bytes.
     * @return A shared pointer to the loaded image data, or nullptr if loading failed.
     */
    virtual auto load(const uint8_t *data, size_t size) -> std::shared_ptr<ImageData> = 0;

    /**
     * @brief Get the list of supported file extensions (e.g., ".png", ".jpg").
     */
    virtual auto supported_extensions() const -> std::vector<std::string> = 0;
};

} // namespace toolkit
