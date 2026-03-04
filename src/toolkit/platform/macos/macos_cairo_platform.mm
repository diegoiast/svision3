#include "macos_cairo_platform.hpp"
#include "toolkit/painters/cairo_painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#import <Cocoa/Cocoa.h>
#include <cairo.h>
#include <spdlog/spdlog.h>

@interface TKWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) toolkit::Window *owner;
@end

@implementation TKWindowDelegate
- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize {
    if (!self.owner) return frameSize;
    NSRect contentRect = [sender contentRectForFrameRect:NSMakeRect(0, 0, frameSize.width, frameSize.height)];
    auto min_s = self.owner->min_size();
    auto max_s = self.owner->max_size();
    if (min_s.width > 0 && contentRect.size.width < min_s.width)
        contentRect.size.width = min_s.width;
    if (min_s.height > 0 && contentRect.size.height < min_s.height)
        contentRect.size.height = min_s.height;
    if (max_s.width > 0 && contentRect.size.width > max_s.width)
        contentRect.size.width = max_s.width;
    if (max_s.height > 0 && contentRect.size.height > max_s.height)
        contentRect.size.height = max_s.height;
    NSRect newFrame = [sender frameRectForContentRect:contentRect];
    return newFrame.size;
}
- (void)windowDidResignKey:(NSNotification *)notification {
    if (self.owner) self.owner->hide_tooltip();
}
@end

@interface TKTooltipWindow : NSWindow
@end
@implementation TKTooltipWindow
- (BOOL)canBecomeKeyWindow { return NO; }
- (BOOL)canBecomeMainWindow { return NO; }
@end

@interface TKView : NSView
@property (nonatomic, assign) toolkit::Window *owner;
@end

@implementation TKView

- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    if (!self.owner) return;
    NSRect bounds = [self bounds];
    CGFloat scale = self.window ? self.window.backingScaleFactor : 1.0;
    int pw = static_cast<int>(bounds.size.width * scale);
    int ph = static_cast<int>(bounds.size.height * scale);
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
    cairo_t *cr = cairo_create(surface);
    cairo_scale(cr, scale, scale);
    toolkit::CairoPainter painter(cr);
    self.owner->handle_paint(painter);
    cairo_surface_flush(surface);
    unsigned char *data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef provider = CGDataProviderCreateWithData(nullptr, data, stride * ph, nullptr);
    CGImageRef image = CGImageCreate(
        pw, ph, 8, 32, stride, cs,
        kCGImageAlphaPremultipliedFirst | static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little),
        provider, nullptr, false, kCGRenderingIntentDefault);
    CGContextRef cgCtx = [[NSGraphicsContext currentContext] CGContext];
    CGContextSaveGState(cgCtx);
    CGContextTranslateCTM(cgCtx, 0, bounds.size.height);
    CGContextScaleCTM(cgCtx, 1.0, -1.0);
    CGContextDrawImage(cgCtx, CGRectMake(0, 0, bounds.size.width, bounds.size.height), image);
    CGContextRestoreGState(cgCtx);
    CGImageRelease(image);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(cs);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    if (self.owner) {
        self.owner->handle_resize({static_cast<float>(newSize.width),
                                   static_cast<float>(newSize.height)});
        [self setNeedsDisplay:YES];
    }
}

- (toolkit::Point)convertMouseEvent:(NSEvent *)event {
    NSPoint loc = [self convertPoint:[event locationInWindow] fromView:nil];
    return {static_cast<float>(loc.x), static_cast<float>(loc.y)};
}

- (void)mouseDown:(NSEvent *)event {
    auto pos = [self convertMouseEvent:event];
    NSEventModifierFlags mods = [event modifierFlags];
    toolkit::MouseEvent e;
    e.type = toolkit::MouseEvent::Type::Press;
    e.position = pos;
    e.button = 0;
    e.click_count = static_cast<int>([event clickCount]);
    e.shift = (mods & NSEventModifierFlagShift) != 0;
    e.ctrl  = (mods & NSEventModifierFlagControl) != 0;
    e.super = (mods & NSEventModifierFlagCommand) != 0;
    self.owner->handle_mouse(e);
}

- (void)rightMouseDown:(NSEvent *)event {
    auto pos = [self convertMouseEvent:event];
    toolkit::MouseEvent e{};
    e.type = toolkit::MouseEvent::Type::Press;
    e.position = pos;
    e.button = 1;
    self.owner->handle_mouse(e);
}

- (void)mouseUp:(NSEvent *)event {
    auto pos = [self convertMouseEvent:event];
    toolkit::MouseEvent e{toolkit::MouseEvent::Type::Release, pos, 0};
    self.owner->handle_mouse(e);
}

- (void)mouseMoved:(NSEvent *)event {
    auto pos = [self convertMouseEvent:event];
    toolkit::MouseEvent e{toolkit::MouseEvent::Type::Move, pos, 0};
    self.owner->handle_mouse(e);
}

- (void)mouseDragged:(NSEvent *)event {
    auto pos = [self convertMouseEvent:event];
    toolkit::MouseEvent e{toolkit::MouseEvent::Type::Move, pos, 0};
    e.type = toolkit::MouseEvent::Type::Drag;
    self.owner->handle_mouse(e);
}

- (void)scrollWheel:(NSEvent *)event {
    auto pos = [self convertMouseEvent:event];
    toolkit::MouseEvent e{};
    e.type = toolkit::MouseEvent::Type::Scroll;
    e.position = pos;
    e.scroll_dx = static_cast<float>([event scrollingDeltaX]);
    e.scroll_dy = static_cast<float>([event scrollingDeltaY]);
    if ([event hasPreciseScrollingDeltas]) {
        e.scroll_dy *= 0.5f;
        e.scroll_dx *= 0.5f;
    } else {
        e.scroll_dy *= 20.0f;
        e.scroll_dx *= 20.0f;
    }
    self.owner->handle_mouse(e);
}

- (void)sendTabKey:(BOOL)shift {
    if (!self.owner) return;
    toolkit::KeyEvent ke;
    ke.type = toolkit::KeyEvent::Type::Press;
    ke.key = toolkit::Key::Tab;
    ke.shift = shift;
    self.owner->handle_key(ke);
}

- (void)insertTab:(id)sender { [self sendTabKey:NO]; }
- (void)insertBacktab:(id)sender { [self sendTabKey:YES]; }

- (void)keyDown:(NSEvent *)event {
    if (!self.owner) return;
    if ([event keyCode] == 48) { [self interpretKeyEvents:@[event]]; return; }
    toolkit::KeyEvent ke;
    ke.type = toolkit::KeyEvent::Type::Press;
    NSEventModifierFlags mods = [event modifierFlags];
    ke.shift = (mods & NSEventModifierFlagShift) != 0;
    ke.ctrl  = (mods & NSEventModifierFlagControl) != 0;
    ke.alt   = (mods & NSEventModifierFlagOption) != 0;
    ke.super = (mods & NSEventModifierFlagCommand) != 0;
    switch ([event keyCode]) {
    case 51:  ke.key = toolkit::Key::Backspace; break;
    case 117: ke.key = toolkit::Key::Delete;    break;
    case 123: ke.key = toolkit::Key::Left;      break;
    case 124: ke.key = toolkit::Key::Right;     break;
    case 125: ke.key = toolkit::Key::Down;      break;
    case 126: ke.key = toolkit::Key::Up;        break;
    case 115: ke.key = toolkit::Key::Home;      break;
    case 119: ke.key = toolkit::Key::End;       break;
    case 116: ke.key = toolkit::Key::PageUp;    break;
    case 121: ke.key = toolkit::Key::PageDown;  break;
    case 36:  ke.key = toolkit::Key::Enter;     break;
    case 53:  ke.key = toolkit::Key::Escape;    break;
    default:  break;
    }
    bool has_modifier = ke.alt || ke.super || ke.ctrl;
    NSString *chars = has_modifier ? [event charactersIgnoringModifiers] : [event characters];
    if (chars.length > 0 && ke.key == toolkit::Key::NoKey) {
        unichar c = [chars characterAtIndex:0];
        if (c >= 32 && c < 127) ke.text = [chars UTF8String];
    }
    if (ke.super && ke.key == toolkit::Key::Left)  ke.key = toolkit::Key::Home;
    if (ke.super && ke.key == toolkit::Key::Right) ke.key = toolkit::Key::End;
    self.owner->handle_key(ke);
}

- (BOOL)performKeyEquivalent:(NSEvent *)event {
    if (!self.owner) return NO;
    NSEventModifierFlags mods = [event modifierFlags];
    bool cmd = (mods & NSEventModifierFlagCommand) != 0;
    bool ctrl = (mods & NSEventModifierFlagControl) != 0;
    if (cmd || ctrl) { [self keyDown:event]; return YES; }
    return [super performKeyEquivalent:event];
}

@end

namespace toolkit {

std::unique_ptr<PlatformWindow>
MacOSCairoPlatformApplication::create_window(std::string_view title, Size size,
                                        Window *owner) {
    return std::make_unique<MacOSCairoPlatformWindow>(title, size, owner);
}

Size MacOSCairoPlatformApplication::measure_text(std::string_view text,
                                            float font_size,
                                            FontFamily font) {
    return cairo_measure_text(text, font_size, font);
}

Painter::FontMetrics
MacOSCairoPlatformApplication::measure_font_metrics(float font_size,
                                                    FontFamily font) {
    return cairo_measure_font_metrics(font_size, font);
}

// --- MacOSCairoPlatformWindow ---

struct MacOSCairoPlatformWindow::Impl {
    NSWindow *ns_window = nil;
    TKView *view = nil;
    TKWindowDelegate *delegate = nil;
    NSMutableDictionary<NSNumber *, NSTimer *> *timers = nil;
    int next_timer_id = 1;
    NSWindow *tooltip_window = nil;
};

MacOSCairoPlatformWindow::MacOSCairoPlatformWindow(std::string_view title, Size size,
                                         Window *owner)
    : impl_(std::make_unique<Impl>()), owner_(owner) {
    NSRect frame = NSMakeRect(200, 200, size.width, size.height);
    NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                              NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    impl_->ns_window = [[NSWindow alloc] initWithContentRect:frame
                                                   styleMask:style
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    std::string t(title);
    [impl_->ns_window setTitle:[NSString stringWithUTF8String:t.c_str()]];
    impl_->timers = [[NSMutableDictionary alloc] init];
    impl_->delegate = [[TKWindowDelegate alloc] init];
    impl_->delegate.owner = owner;
    [impl_->ns_window setDelegate:impl_->delegate];
    impl_->view = [[TKView alloc] initWithFrame:frame];
    impl_->view.owner = owner;
    [impl_->ns_window setContentView:impl_->view];
    NSTrackingArea *tracking = [[NSTrackingArea alloc]
        initWithRect:NSZeroRect
             options:(NSTrackingMouseMoved | NSTrackingActiveAlways | NSTrackingInVisibleRect)
               owner:impl_->view
            userInfo:nil];
    [impl_->view addTrackingArea:tracking];
}

MacOSCairoPlatformWindow::~MacOSCairoPlatformWindow() {
    for (NSTimer *t in impl_->timers.allValues) [t invalidate];
    [impl_->timers removeAllObjects];
}

void MacOSCairoPlatformWindow::show() {
    [impl_->ns_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

void MacOSCairoPlatformWindow::close() {
    [impl_->ns_window close];
}

void MacOSCairoPlatformWindow::request_redraw() {
    [impl_->view setNeedsDisplay:YES];
}

void MacOSCairoPlatformWindow::set_min_size(Size s) {
    [impl_->ns_window setContentMinSize:NSMakeSize(s.width, s.height)];
}

void MacOSCairoPlatformWindow::set_max_size(Size s) {
    if (s.width > 0 && s.height > 0)
        [impl_->ns_window setContentMaxSize:NSMakeSize(s.width, s.height)];
}

int MacOSCairoPlatformWindow::start_timer(float interval_sec,
                                     std::function<void()> callback,
                                     bool repeats) {
    int tid = impl_->next_timer_id++;
    auto cb = std::make_shared<std::function<void()>>(std::move(callback));
    TKView *view = impl_->view;
    NSMutableDictionary *timers = impl_->timers;
    NSNumber *key = @(tid);
    NSTimer *timer = [NSTimer scheduledTimerWithTimeInterval:interval_sec
                                                     repeats:repeats
                                                       block:^(NSTimer *t) {
        (*cb)();
        [view setNeedsDisplay:YES];
        if (!repeats) [timers removeObjectForKey:key];
    }];
    [impl_->timers setObject:timer forKey:key];
    return tid;
}

void MacOSCairoPlatformWindow::stop_timer(int timer_id) {
    NSNumber *key = @(timer_id);
    NSTimer *timer = [impl_->timers objectForKey:key];
    if (timer) { [timer invalidate]; [impl_->timers removeObjectForKey:key]; }
}

void MacOSCairoPlatformWindow::set_cursor(CursorShape shape) {
    NSCursor *ns_cursor;
    switch (shape) {
    case CursorShape::IBeam:      ns_cursor = [NSCursor IBeamCursor]; break;
    case CursorShape::Hand:       ns_cursor = [NSCursor pointingHandCursor]; break;
    case CursorShape::NotAllowed: ns_cursor = [NSCursor operationNotAllowedCursor]; break;
    case CursorShape::ResizeEW:   ns_cursor = [NSCursor resizeLeftRightCursor]; break;
    default:                      ns_cursor = [NSCursor arrowCursor]; break;
    }
    [ns_cursor set];
}

void MacOSCairoPlatformWindow::show_tooltip_window(std::string const &text,
                                              Point local_pos) {
    auto const &style = Theme::current().tooltip;
    float pad = style.padding, font_sz = style.font_size;
    auto text_sz = Painter::measure_text(text, font_sz);
    auto fm = Painter::measure_font_metrics(font_sz);
    float w = text_sz.width + pad * 2, h = fm.height + pad * 2;
    NSPoint view_pt = NSMakePoint(local_pos.x, local_pos.y);
    NSPoint win_pt = [impl_->view convertPoint:view_pt toView:nil];
    NSPoint screen_pt = [impl_->ns_window convertPointToScreen:win_pt];
    float sx = static_cast<float>(screen_pt.x);
    float sy = static_cast<float>(screen_pt.y) - h - 4.0f;
    NSScreen *screen = [impl_->ns_window screen] ?: [NSScreen mainScreen];
    NSRect vis = [screen visibleFrame];
    if (sx + w > vis.origin.x + vis.size.width)
        sx = static_cast<float>(vis.origin.x + vis.size.width) - w - 2.0f;
    if (sx < vis.origin.x) sx = static_cast<float>(vis.origin.x) + 2.0f;
    if (sy < vis.origin.y) sy = static_cast<float>(screen_pt.y) + 20.0f;
    NSRect tip_frame = NSMakeRect(sx, sy, w, h);
    if (!impl_->tooltip_window) {
        impl_->tooltip_window = [[TKTooltipWindow alloc]
            initWithContentRect:tip_frame
                      styleMask:NSWindowStyleMaskBorderless
                        backing:NSBackingStoreBuffered
                          defer:YES];
        [impl_->tooltip_window setOpaque:NO];
        [impl_->tooltip_window setBackgroundColor:[NSColor clearColor]];
        [impl_->tooltip_window setLevel:NSStatusWindowLevel];
        [impl_->tooltip_window setIgnoresMouseEvents:YES];
        [impl_->tooltip_window setHasShadow:YES];
    }
    [impl_->tooltip_window setFrame:tip_frame display:NO];
    CGFloat scale = impl_->ns_window.backingScaleFactor;
    int pw = static_cast<int>(w * scale), ph = static_cast<int>(h * scale);
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
    cairo_t *cr = cairo_create(surface);
    cairo_scale(cr, scale, scale);
    CairoPainter painter(cr);
    Rect r{0, 0, w, h};
    painter.fill_rounded_rect(r, style.background, style.corner_radius);
    painter.draw_rounded_rect(r, style.border, style.corner_radius, style.border_width);
    painter.draw_text(text, {pad, pad + fm.ascent}, style.text, font_sz);
    cairo_surface_flush(surface);
    unsigned char *data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef provider = CGDataProviderCreateWithData(nullptr, data, stride * ph, nullptr);
    CGImageRef cgImage = CGImageCreate(
        pw, ph, 8, 32, stride, cs,
        kCGImageAlphaPremultipliedFirst | static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little),
        provider, nullptr, false, kCGRenderingIntentDefault);
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:cgImage];
    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(w, h)];
    [image addRepresentation:rep];
    NSImageView *iv = [[NSImageView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    [iv setImage:image];
    [impl_->tooltip_window setContentView:iv];
    CGImageRelease(cgImage);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(cs);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    [impl_->tooltip_window orderFront:nil];
}

void MacOSCairoPlatformWindow::hide_tooltip_window() {
    if (impl_->tooltip_window) [impl_->tooltip_window orderOut:nil];
}

bool MacOSCairoPlatformWindow::save_to_png(std::string const &path) {
    return cairo_save_to_png(owner_, path);
}

float MacOSCairoPlatformWindow::scale_factor() const {
    return static_cast<float>(impl_->ns_window.backingScaleFactor);
}

} // namespace toolkit
