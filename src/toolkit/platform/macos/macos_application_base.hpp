#pragma once

#include "toolkit/platform.hpp"

namespace toolkit {

class MacOSPlatformApplicationBase : public PlatformApplication {
  public:
    MacOSPlatformApplicationBase();
    ~MacOSPlatformApplicationBase() override;
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

} // namespace toolkit
