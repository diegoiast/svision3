#pragma once
#include "toolkit/image.hpp"

// clang-format off
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
// clang-format on

#include <string>
#include <vector>

namespace toolkit {

class Win32ImageLoader : public ImageLoaderInterface {
  public:
    // GDI+'s native format here is B,G,R,A -- requesting PixelFormat::BGRA costs nothing;
    // PixelFormat::RGBA costs one swap_rb pass. Default repeated from the interface (see
    // image.hpp) since virtual default arguments bind to the static type of the call.
    // UNVERIFIED: not compile-checked on Windows.
    auto load(std::string_view path,
             PixelFormat pixel_format = detail::default_pixel_format()) -> Icon override;
    auto load_from_memory(const uint8_t *data, size_t size,
                          PixelFormat pixel_format = detail::default_pixel_format())
        -> Icon override;
    auto supported_extensions() const -> std::vector<std::string> override;
    auto save(ImageData const &image, std::string_view path) -> bool override;

  private:
    static bool GetEncoderClsid(const WCHAR *format, CLSID *pClsid);
    static std::wstring utf8_to_wide(std::string_view s);
};

} // namespace toolkit
