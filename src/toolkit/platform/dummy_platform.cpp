#include "toolkit/platform/dummy_platform.hpp"
#include "toolkit/lunasvg_image_loader.hpp"

namespace toolkit {

std::unique_ptr<PlatformWindow> DummyPlatformApplication::create_window(std::string_view, Size,
                                                                        Window *, WindowOptions) {
    return std::make_unique<DummyPlatformWindow>();
}
std::shared_ptr<ImageLoaderInterface> DummyPlatformApplication::get_image_loader() {
    return {};
}

std::shared_ptr<SVGLoaderInterface> DummyPlatformApplication::get_svg_loader() {
    if (!svg_loader_) {
        svg_loader_ = std::make_shared<LunasvgImageLoader>();
    }
    return svg_loader_;
}

} // namespace toolkit
