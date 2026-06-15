#include "macos_application_base.hpp"
#include "toolkit/stb_image_loader.hpp"

#import <Cocoa/Cocoa.h>
#include <spdlog/spdlog.h>

@interface TKAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation TKAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    spdlog::debug("Application launched");
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}
@end

namespace toolkit {

struct MacOSPlatformApplicationBase::Impl {
    TKAppDelegate *delegate = nil;
};

MacOSPlatformApplicationBase::MacOSPlatformApplicationBase()
    : impl_(std::make_unique<Impl>()) {
    [NSApplication sharedApplication];
    impl_->delegate = [[TKAppDelegate alloc] init];
    [NSApp setDelegate:impl_->delegate];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

MacOSPlatformApplicationBase::~MacOSPlatformApplicationBase() = default;

std::shared_ptr<ImageLoaderInterface> MacOSPlatformApplicationBase::get_image_loader() {
    if (!image_loader_) {
        image_loader_ = std::make_shared<StbImageLoader>();
    }
    return image_loader_;
}

std::shared_ptr<SVGLoaderInterface> MacOSPlatformApplicationBase::get_svg_loader() {
    if (!svg_loader_) {
        svg_loader_ = std::make_shared<LunasvgImageLoader>();
    }
    return svg_loader_;
}

int MacOSPlatformApplicationBase::run() {
    [NSApp run];
    return 0;
}

void MacOSPlatformApplicationBase::quit() { [NSApp terminate:nil]; }

void MacOSPlatformApplicationBase::post_to_main_thread(
    std::function<void()> fn) {
    auto b = std::make_shared<std::function<void()>>(std::move(fn));
    dispatch_async(dispatch_get_main_queue(), ^{ (*b)(); });
}

std::string MacOSPlatformApplicationBase::clipboard_get_text() {
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    NSString *str = [pb stringForType:NSPasteboardTypeString];
    return str ? std::string([str UTF8String]) : std::string{};
}

void MacOSPlatformApplicationBase::clipboard_set_text(
    std::string const &text) {
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    [pb setString:[NSString stringWithUTF8String:text.c_str()]
          forType:NSPasteboardTypeString];
}

static NSFont *pick_font(float size, toolkit::FontFamily family) {
    if (family == toolkit::FontFamily::Monospace)
        return [NSFont monospacedSystemFontOfSize:size weight:NSFontWeightRegular];
    return [NSFont systemFontOfSize:size];
}


std::string_view MacOSPlatformApplicationBase::name() const { return "macOS"; }
std::string_view MacOSPlatformApplicationBase::painter_name() const { return "Native"; }
float MacOSPlatformApplicationBase::scale_factor() const {
    return static_cast<float>([[NSScreen mainScreen] backingScaleFactor]);
}

SystemFonts MacOSPlatformApplicationBase::system_fonts() const {
    NSFont *sys = [NSFont systemFontOfSize:0];
    NSFont *mono = [NSFont userFixedPitchFontOfSize:0];
    return {[[sys familyName] UTF8String], [[mono familyName] UTF8String],
            static_cast<float>([sys pointSize])};
}

} // namespace toolkit
