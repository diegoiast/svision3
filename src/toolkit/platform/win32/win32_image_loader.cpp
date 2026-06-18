#include "win32_image_loader.hpp"
#include <gdiplus.h>
#include <algorithm>
#include <shlwapi.h>

namespace toolkit {

std::wstring Win32ImageLoader::utf8_to_wide(std::string_view s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), len);
    return result;
}

bool Win32ImageLoader::GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return false;
    auto* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
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

auto Win32ImageLoader::load(std::string_view path) -> Icon {
    Gdiplus::Bitmap bitmap(utf8_to_wide(path).c_str());
    if (bitmap.GetLastStatus() != Gdiplus::Ok) return nullptr;

    int w = bitmap.GetWidth();
    int h = bitmap.GetHeight();

    auto img = std::make_shared<ImageData>();
    img->width = w;
    img->height = h;
    img->channels = 4;
    img->pixels.resize(w * h * 4);

    Gdiplus::BitmapData bitmapData;
    Gdiplus::Rect rect(0, 0, w, h);
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) == Gdiplus::Ok) {
        uint8_t* src = (uint8_t*)bitmapData.Scan0;
        for (int i = 0; i < w * h; ++i) {
            img->pixels[i * 4 + 0] = src[i * 4 + 2]; // R
            img->pixels[i * 4 + 1] = src[i * 4 + 1]; // G
            img->pixels[i * 4 + 2] = src[i * 4 + 0]; // B
            img->pixels[i * 4 + 3] = src[i * 4 + 3]; // A
        }
        bitmap.UnlockBits(&bitmapData);
        return img;
    }
    return nullptr;
}

auto Win32ImageLoader::load_from_memory(const uint8_t *data, size_t size) -> Icon {
    IStream* stream = SHCreateMemStream(data, static_cast<UINT>(size));
    if (!stream) return nullptr;

    Gdiplus::Bitmap bitmap(stream);
    stream->Release();

    if (bitmap.GetLastStatus() != Gdiplus::Ok) return nullptr;

    int w = bitmap.GetWidth();
    int h = bitmap.GetHeight();

    auto img = std::make_shared<ImageData>();
    img->width = w;
    img->height = h;
    img->channels = 4;
    img->pixels.resize(w * h * 4);

    Gdiplus::BitmapData bitmapData;
    Gdiplus::Rect rect(0, 0, w, h);
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) == Gdiplus::Ok) {
        uint8_t* src = (uint8_t*)bitmapData.Scan0;
        for (int i = 0; i < w * h; ++i) {
            img->pixels[i * 4 + 0] = src[i * 4 + 2]; // R
            img->pixels[i * 4 + 1] = src[i * 4 + 1]; // G
            img->pixels[i * 4 + 2] = src[i * 4 + 0]; // B
            img->pixels[i * 4 + 3] = src[i * 4 + 3]; // A
        }
        bitmap.UnlockBits(&bitmapData);
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
    
    if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &bitmapData) == Gdiplus::Ok) {
        uint8_t* dst = (uint8_t*)bitmapData.Scan0;
        const uint8_t* src = image.pixels.data();
        for (int i = 0; i < image.width * image.height; ++i) {
            dst[i * 4 + 0] = src[i * 4 + 2]; // B
            dst[i * 4 + 1] = src[i * 4 + 1]; // G
            dst[i * 4 + 2] = src[i * 4 + 0]; // R
            dst[i * 4 + 3] = src[i * 4 + 3]; // A
        }
        bitmap.UnlockBits(&bitmapData);
    } else {
        return false;
    }

    CLSID clsid;
    std::wstring wpath = utf8_to_wide(path);
    std::wstring ext = wpath.substr(wpath.find_last_of(L".") + 1);
    std::wstring mime = L"image/png";
    if (ext == L"jpg" || ext == L"jpeg") mime = L"image/jpeg";
    else if (ext == L"bmp") mime = L"image/bmp";
    
    if (GetEncoderClsid(mime.c_str(), &clsid)) {
        return bitmap.Save(wpath.c_str(), &clsid, nullptr) == Gdiplus::Ok;
    }
    return false;
}

} // namespace toolkit
