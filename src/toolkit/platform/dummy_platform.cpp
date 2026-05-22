#include "toolkit/platform/dummy_platform.hpp"
#include "toolkit/stb_image_loader.hpp"

namespace toolkit {

std::unique_ptr<PlatformWindow> DummyPlatformApplication::create_window(std::string_view, Size,
                                                                        Window *) {
    return std::make_unique<DummyPlatformWindow>();
}

std::unique_ptr<ImageLoaderInterface> DummyPlatformApplication::create_image_loader() {
    return std::make_unique<StbImageLoader>();
}

} // namespace toolkit
