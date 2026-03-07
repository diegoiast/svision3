#include "toolkit/platform/dummy_platform.hpp"

namespace toolkit {

std::unique_ptr<PlatformWindow> DummyPlatformApplication::create_window(std::string_view, Size, Window *) {
    return std::make_unique<DummyPlatformWindow>();
}

Size DummyPlatformApplication::measure_text(std::string_view text, float font_size, FontFamily) {
    return { static_cast<float>(text.size()) * font_size * 0.6f, font_size + 2.0f };
}

Painter::FontMetrics DummyPlatformApplication::measure_font_metrics(float font_size, FontFamily) {
    return { font_size * 0.8f, font_size * 0.2f, font_size };
}

} // namespace toolkit
