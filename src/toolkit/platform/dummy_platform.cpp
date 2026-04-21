#include "toolkit/platform/dummy_platform.hpp"

namespace toolkit {

std::unique_ptr<PlatformWindow> DummyPlatformApplication::create_window(std::string_view, Size,
                                                                        Window *) {
    return std::make_unique<DummyPlatformWindow>();
}


} // namespace toolkit
