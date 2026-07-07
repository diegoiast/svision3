#pragma once
#include "toolkit/image.hpp"
#include <gdiplus.h>
#include <objidl.h>
#include <string>
#include <vector>
#include <windows.h>

namespace toolkit {

class Win32ImageLoader : public ImageLoaderInterface {
  public:
    auto load(std::string_view path) -> Icon override;
    auto load_from_memory(const uint8_t *data, size_t size) -> Icon override;
    auto supported_extensions() const -> std::vector<std::string> override;
    auto save(ImageData const &image, std::string_view path) -> bool override;

  private:
    static bool GetEncoderClsid(const WCHAR *format, CLSID *pClsid);
    static std::wstring utf8_to_wide(std::string_view s);
};

} // namespace toolkit
