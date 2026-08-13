#include "svision3/platform/dummy_platform.hpp"
#include "svision3/lunasvg_image_loader.hpp"

// The dummy platform is compiled on every OS, so it must use whichever raster loader
// is actually built for the target: the WIC/GDI+ loader on Windows, stb elsewhere.
#ifdef _WIN32
#include "win32_image_loader.hpp"
#else
#include "svision3/stb_image_loader.hpp"
#endif

namespace svision3 {

std::unique_ptr<PlatformWindow> DummyPlatformApplication::create_window(std::string_view, Size,
                                                                        Window *, WindowOptions) {
    return std::make_unique<DummyPlatformWindow>();
}
std::shared_ptr<ImageLoaderInterface> DummyPlatformApplication::get_image_loader() {
    if (!image_loader_) {
#ifdef _WIN32
        image_loader_ = std::make_shared<Win32ImageLoader>();
#else
        image_loader_ = std::make_shared<StbImageLoader>();
#endif
    }
    return image_loader_;
}

std::shared_ptr<SVGLoaderInterface> DummyPlatformApplication::get_svg_loader() {
    if (!svg_loader_) {
        svg_loader_ = std::make_shared<LunasvgImageLoader>();
    }
    return svg_loader_;
}

} // namespace svision3
