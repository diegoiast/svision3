#include "macos_application_base.hpp"

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

Size MacOSPlatformApplicationBase::measure_text(std::string_view text,
                                                float font_size,
                                                FontFamily family) {
    NSFont *font = pick_font(font_size, family);
    NSString *str = [[NSString alloc] initWithBytes:text.data()
                                             length:text.size()
                                           encoding:NSUTF8StringEncoding];
    if (!str) return {0, 0};
    NSDictionary *attrs = @{NSFontAttributeName : font};
    NSSize sz = [str sizeWithAttributes:attrs];
    return {static_cast<float>(sz.width), static_cast<float>(sz.height)};
}

Painter::FontMetrics
MacOSPlatformApplicationBase::measure_font_metrics(float font_size,
                                                   FontFamily family) {
    NSFont *font = pick_font(font_size, family);
    float ascent = static_cast<float>(font.ascender);
    float descent = static_cast<float>(-font.descender);
    return {ascent, descent, ascent + descent};
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
