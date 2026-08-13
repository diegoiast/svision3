#pragma once
#include "svision3/image.hpp"

namespace svision3 {

class Win32ImageLoader : public ImageLoaderInterface {
  public:
    // GDI+'s native format here is B,G,R,A -- requesting PixelFormat::BGRA costs nothing;
    // PixelFormat::RGBA costs one swap_rb pass. Default repeated from the interface (see
    // image.hpp) since virtual default arguments bind to the static type of the call.
    auto load(std::string_view path,
             PixelFormat pixel_format = detail::default_pixel_format()) -> Icon override;
    auto load_from_memory(const uint8_t *data, size_t size,
                          PixelFormat pixel_format = detail::default_pixel_format())
        -> Icon override;
    auto supported_extensions() const -> std::vector<std::string> override;
    auto save(ImageData const &image, std::string_view path) -> bool override;
};

} // namespace svision3
