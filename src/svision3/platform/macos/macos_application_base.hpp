#pragma once

#include "svision3/platform.hpp"

namespace svision3 {

class MacOSPlatformApplicationBase : public PlatformApplication {
    std::shared_ptr<ImageLoaderInterface> image_loader_;
    std::shared_ptr<SVGLoaderInterface> svg_loader_;

  public:
    MacOSPlatformApplicationBase();
    ~MacOSPlatformApplicationBase() override;
    std::shared_ptr<ImageLoaderInterface> get_image_loader() override;
    std::shared_ptr<SVGLoaderInterface> get_svg_loader() override;
    int run() override;
    void quit() override;
    void post_to_main_thread(std::function<void()> fn) override;
    std::string clipboard_get_text() override;
    void clipboard_set_text(std::string const &text) override;
    std::string_view name() const override;
    std::string_view painter_name() const override;
    float scale_factor() const override;
    SystemFonts system_fonts() const override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace svision3
