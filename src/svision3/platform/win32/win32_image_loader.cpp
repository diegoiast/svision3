#include "win32_image_loader.hpp"
#include "svision3/win32/win32_utils.hpp"
#include "svision3/pixel_format.hpp"

// clang-format off
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlwapi.h>
// clang-format on

#include <algorithm>
#include <cstring>

namespace svision3 {
namespace {

bool GetEncoderClsid(const WCHAR *format, CLSID *pClsid) {
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) {
        return false;
    }
    auto *pImageCodecInfo = (Gdiplus::ImageCodecInfo *)(malloc(size));
    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return true;
        }
    }
    free(pImageCodecInfo);
    return false;
}

} // namespace

auto Win32ImageLoader::load(std::string_view path, PixelFormat pixel_format) -> Icon {
    Gdiplus::Bitmap bitmap(utf8_to_wide(path).c_str());
    if (bitmap.GetLastStatus() != Gdiplus::Ok) {
        return nullptr;
    }

    int w = bitmap.GetWidth();
    int h = bitmap.GetHeight();

    auto img = std::make_shared<ImageData>();
    img->width = w;
    img->height = h;
    img->channels = 4;
    img->format = pixel_format;
    img->pixels.resize(w * h * 4);

    // UNVERIFIED: not compile-checked on Windows, please build-check before trusting. GDI+
    // PixelFormat32bppARGB is B,G,R,A in memory -- matches ImageData::pixels directly when
    // PixelFormat::BGRA was requested (no swap); PixelFormat::RGBA needs one swap_rb pass.
    Gdiplus::BitmapData bitmapData;
    Gdiplus::Rect rect(0, 0, w, h);
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) ==
        Gdiplus::Ok) {
        uint8_t *src = (uint8_t *)bitmapData.Scan0;
        std::memcpy(img->pixels.data(), src, static_cast<size_t>(w) * h * 4);
        bitmap.UnlockBits(&bitmapData);
        if (pixel_format == PixelFormat::RGBA) {
            pixel::swap_rb(img->pixels.data(), static_cast<size_t>(w) * h);
        }
        return img;
    }
    return nullptr;
}

auto Win32ImageLoader::load_from_memory(const uint8_t *data, size_t size,
                                        PixelFormat pixel_format) -> Icon {
    IStream *stream = SHCreateMemStream(data, static_cast<UINT>(size));
    if (!stream) {
        return nullptr;
    }

    Gdiplus::Bitmap bitmap(stream);
    stream->Release();

    if (bitmap.GetLastStatus() != Gdiplus::Ok) {
        return nullptr;
    }

    int w = bitmap.GetWidth();
    int h = bitmap.GetHeight();

    auto img = std::make_shared<ImageData>();
    img->width = w;
    img->height = h;
    img->channels = 4;
    img->format = pixel_format;
    img->pixels.resize(w * h * 4);

    // UNVERIFIED: not compile-checked on Windows, please build-check before trusting. See load()
    // above.
    Gdiplus::BitmapData bitmapData;
    Gdiplus::Rect rect(0, 0, w, h);
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) ==
        Gdiplus::Ok) {
        uint8_t *src = (uint8_t *)bitmapData.Scan0;
        std::memcpy(img->pixels.data(), src, static_cast<size_t>(w) * h * 4);
        bitmap.UnlockBits(&bitmapData);
        if (pixel_format == PixelFormat::RGBA) {
            pixel::swap_rb(img->pixels.data(), static_cast<size_t>(w) * h);
        }
        return img;
    }
    return nullptr;
}

auto Win32ImageLoader::supported_extensions() const -> std::vector<std::string> {
    return {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tiff"};
}

auto Win32ImageLoader::save(ImageData const &image, std::string_view path) -> bool {
    Gdiplus::Bitmap bitmap(image.width, image.height, PixelFormat32bppARGB);
    Gdiplus::BitmapData bitmapData;
    Gdiplus::Rect rect(0, 0, image.width, image.height);

    // UNVERIFIED: not compile-checked on Windows, please build-check before trusting. GDI+'s
    // native PixelFormat32bppARGB is B,G,R,A -- image.format says whether image.pixels already
    // matches that (no swap) or needs one first.
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &bitmapData) ==
        Gdiplus::Ok) {
        uint8_t *dst = (uint8_t *)bitmapData.Scan0;
        std::memcpy(dst, image.pixels.data(), static_cast<size_t>(image.width) * image.height * 4);
        if (image.format == PixelFormat::RGBA) {
            pixel::swap_rb(dst, static_cast<size_t>(image.width) * image.height);
        }
        bitmap.UnlockBits(&bitmapData);
    } else {
        return false;
    }

    CLSID clsid;
    std::wstring wpath = utf8_to_wide(path);
    std::wstring ext = wpath.substr(wpath.find_last_of(L".") + 1);
    std::wstring mime = L"image/png";
    if (ext == L"jpg" || ext == L"jpeg") {
        mime = L"image/jpeg";
    } else if (ext == L"bmp") {
        mime = L"image/bmp";
    }

    if (GetEncoderClsid(mime.c_str(), &clsid)) {
        return bitmap.Save(wpath.c_str(), &clsid, nullptr) == Gdiplus::Ok;
    }
    return false;
}

} // namespace svision3
