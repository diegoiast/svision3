#include "macos_opengl_platform.hpp"
#include "toolkit/painters/gl_offscreen.hpp"
#include "toolkit/painters/gl_painter.hpp"
#include "toolkit/pixel_format.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>
#import <ImageIO/ImageIO.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#import <OpenGL/glext.h>
#include <cmath>
#include <string>
#include <vector>

// ── CoreTextRasterizer ──────────────────────────────────────────────────────

namespace toolkit {

class CoreTextRasterizer : public TextRasterizer {
  public:
    static NSFont *ns_font(float size, FontFamily family) {
        if (family == FontFamily::Monospace) {
            return [NSFont monospacedSystemFontOfSize:size weight:NSFontWeightRegular];
        }
        return [NSFont systemFontOfSize:size];
    }

    RasterizedText rasterize(std::string_view text, float font_size, float scale,
                             Color const &color, FontFamily family = FontFamily::System,
                             bool bold = false, bool italic = false) override {
        NSFont *font = ns_font(font_size, family);
        NSString *str = [[NSString alloc] initWithBytes:text.data()
                                                 length:text.size()
                                               encoding:NSUTF8StringEncoding];
        if (!str || str.length == 0) {
            return {};
        }

        NSDictionary *attrs = @{
            NSFontAttributeName : font,
            NSForegroundColorAttributeName : [NSColor colorWithRed:color.r
                                                             green:color.g
                                                              blue:color.b
                                                             alpha:color.a]
        };
        NSAttributedString *astr = [[NSAttributedString alloc] initWithString:str attributes:attrs];
        CTLineRef line = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)astr);

        NSSize sz = [str sizeWithAttributes:attrs];
        int tw = static_cast<int>(std::ceil(sz.width));
        int th = static_cast<int>(std::ceil(sz.height));
        if (tw <= 0 || th <= 0) {
            CFRelease(line);
            return {};
        }

        int ptw = static_cast<int>(std::ceil(tw * scale));
        int pth = static_cast<int>(std::ceil(th * scale));

        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(
            nullptr, ptw, pth, 8, ptw * 4, cs,
            kCGImageAlphaPremultipliedLast | static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big));
        CGColorSpaceRelease(cs);
        if (!ctx) {
            CFRelease(line);
            return {};
        }

        CGContextScaleCTM(ctx, scale, scale);
        CGContextTranslateCTM(ctx, 0, th);
        CGContextScaleCTM(ctx, 1.0, -1.0);
        float ascent = static_cast<float>(font.ascender);
        CGContextSaveGState(ctx);
        CGContextTranslateCTM(ctx, 0, ascent);
        CGContextScaleCTM(ctx, 1.0, -1.0);
        CGContextSetTextPosition(ctx, 0, 0);
        CTLineDraw(line, ctx);
        CGContextRestoreGState(ctx);
        CFRelease(line);

        auto *data = static_cast<uint8_t *>(CGBitmapContextGetData(ctx));
        RasterizedText result;
        result.pixels.assign(data, data + ptw * pth * 4);
        result.width = ptw;
        result.height = pth;
        result.ascent = ascent;
        CGContextRelease(ctx);
        return result;
    }

    Size measure(std::string_view text, float font_size,
                 FontFamily family = FontFamily::System) override {
        NSFont *font = ns_font(font_size, family);
        NSString *str = [[NSString alloc] initWithBytes:text.data()
                                                 length:text.size()
                                               encoding:NSUTF8StringEncoding];
        if (!str) {
            return {0, 0};
        }
        NSDictionary *attrs = @{NSFontAttributeName : font};
        NSSize sz = [str sizeWithAttributes:attrs];
        return {static_cast<float>(sz.width), static_cast<float>(sz.height)};
    }

    Painter::FontMetrics metrics(float font_size, FontFamily family = FontFamily::System) override {
        NSFont *font = ns_font(font_size, family);
        float ascent = static_cast<float>(font.ascender);
        float descent = static_cast<float>(-font.descender);
        return {ascent, descent, ascent + descent};
    }

    void draw_text(Painter &p, std::string_view text, Point position, Color const &color,
                   float font_size, FontFamily font, Painter::TextOrientation orientation,
                   bool bold, bool italic) override {
        // Fallback for macOS: rasterize and draw as image
        // FIXME: we need the scale.
        float scale = 1.0f;
        auto rt = rasterize(text, font_size, scale, color, font, bold, italic);
        if (rt.pixels.empty()) {
            return;
        }

        // FIXME: we need to apply 'color' if draw_image doesn't tint
        p.draw_image(ImageData{std::move(rt.pixels), rt.width, rt.height},
                     {position.x, position.y - rt.ascent});
    }
};

} // namespace toolkit

@interface TKGLWindowDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) toolkit::Window *owner;
@end

@implementation TKGLWindowDelegate
- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize {
    if (!self.owner) {
        return frameSize;
    }
    NSRect cr =
        [sender contentRectForFrameRect:NSMakeRect(0, 0, frameSize.width, frameSize.height)];
    auto mn = self.owner->min_size();
    auto mx = self.owner->max_size();
    if (mn.width > 0 && cr.size.width < mn.width) {
        cr.size.width = mn.width;
    }
    if (mn.height > 0 && cr.size.height < mn.height) {
        cr.size.height = mn.height;
    }
    if (mx.width > 0 && cr.size.width > mx.width) {
        cr.size.width = mx.width;
    }
    if (mx.height > 0 && cr.size.height > mx.height) {
        cr.size.height = mx.height;
    }
    return [sender frameRectForContentRect:cr].size;
}
- (void)windowDidBecomeKey:(NSNotification *)n {
    if (self.owner) {
        self.owner->handle_activate(true);
    }
}
- (void)windowDidResignKey:(NSNotification *)n {
    if (self.owner) {
        self.owner->hide_tooltip();
        self.owner->handle_activate(false);
    }
}
@end

@interface TKGLTooltipWindow : NSWindow
@end
@implementation TKGLTooltipWindow
- (BOOL)canBecomeKeyWindow {
    return NO;
}
- (BOOL)canBecomeMainWindow {
    return NO;
}
@end

@interface TKGLTooltipView : NSView
@property(nonatomic, strong) NSImage *renderedImage;
@end

@implementation TKGLTooltipView
- (void)drawRect:(NSRect)dirtyRect {
    if (self.renderedImage) {
        [self.renderedImage drawInRect:[self bounds]];
    }
}
@end

@interface TKGLView : NSOpenGLView
@property(nonatomic, assign) toolkit::Window *owner;
@end

@implementation TKGLView {
    toolkit::CoreTextRasterizer rasterizer_;
}

- (instancetype)initWithFrame:(NSRect)frame {
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFADoubleBuffer, NSOpenGLPFAColorSize, 24, NSOpenGLPFAAlphaSize, 8, 0};
    NSOpenGLPixelFormat *pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    self = [super initWithFrame:frame pixelFormat:pf];
    if (self) {
        [self setWantsBestResolutionOpenGLSurface:YES];
    }
    return self;
}

- (BOOL)isFlipped {
    return YES;
}
- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)prepareOpenGL {
    [super prepareOpenGL];
    GLint swap = 1;
    [[self openGLContext] setValues:&swap forParameter:NSOpenGLContextParameterSwapInterval];
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
}

- (void)reshape {
    [super reshape];
    [[self openGLContext] makeCurrentContext];
    NSRect b = [self bounds];
    NSRect bp = [self convertRectToBacking:b];
    glViewport(0, 0, (GLsizei)bp.size.width, (GLsizei)bp.size.height);
    if (self.owner) {
        self.owner->handle_resize(
            {static_cast<float>(b.size.width), static_cast<float>(b.size.height)});
    }
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    if (self.owner && self.window) {
        self.owner->handle_scale_changed(static_cast<float>(self.window.backingScaleFactor));
    }
}

- (void)drawRect:(NSRect)dirtyRect {
    if (!self.owner) {
        return;
    }
    [[self openGLContext] makeCurrentContext];
    NSRect b = [self bounds];
    NSRect bp = [self convertRectToBacking:b];
    float w = b.size.width, h = b.size.height;

    glViewport(0, 0, (GLsizei)bp.size.width, (GLsizei)bp.size.height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float scale = static_cast<float>(bp.size.width / b.size.width);
    toolkit::GLPainter painter(h, scale, &rasterizer_);
    self.owner->handle_paint(painter);

    [[self openGLContext] flushBuffer];
}

// ── Mouse events ──

- (toolkit::Point)tkPoint:(NSEvent *)event {
    NSPoint loc = [self convertPoint:[event locationInWindow] fromView:nil];
    return {static_cast<float>(loc.x), static_cast<float>(loc.y)};
}

- (void)mouseDown:(NSEvent *)event {
    auto pos = [self tkPoint:event];
    NSEventModifierFlags m = [event modifierFlags];
    toolkit::MouseEvent e;
    e.type = toolkit::MouseEvent::Type::Press;
    e.position = pos;
    e.button = 0;
    e.click_count = static_cast<int>([event clickCount]);
    e.shift = (m & NSEventModifierFlagShift) != 0;
    e.ctrl = (m & NSEventModifierFlagControl) != 0;
    e.alt = (m & NSEventModifierFlagOption) != 0;
    e.super = (m & NSEventModifierFlagCommand) != 0;
    self.owner->handle_mouse(e);
}

- (void)rightMouseDown:(NSEvent *)event {
    auto pos = [self tkPoint:event];
    toolkit::MouseEvent e{};
    e.type = toolkit::MouseEvent::Type::Press;
    e.position = pos;
    e.button = 1;
    self.owner->handle_mouse(e);
}

- (void)mouseUp:(NSEvent *)event {
    auto pos = [self tkPoint:event];
    toolkit::MouseEvent e{toolkit::MouseEvent::Type::Release, pos, 0};
    self.owner->handle_mouse(e);
}

- (void)mouseMoved:(NSEvent *)event {
    auto pos = [self tkPoint:event];
    toolkit::MouseEvent e{toolkit::MouseEvent::Type::Move, pos, 0};
    self.owner->handle_mouse(e);
}

- (void)mouseExited:(NSEvent *)event {
    // Clear the hovered widget's hover state once the pointer leaves the view; moved events
    // stop at the edge and would otherwise leave it stuck hovered.
    (void)event;
    if (self.owner) {
        self.owner->handle_mouse_leave();
    }
}

- (void)mouseDragged:(NSEvent *)event {
    auto pos = [self tkPoint:event];
    toolkit::MouseEvent e{};
    e.type = toolkit::MouseEvent::Type::Drag;
    e.position = pos;
    self.owner->handle_mouse(e);
}

- (void)scrollWheel:(NSEvent *)event {
    auto pos = [self tkPoint:event];
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

// ── Keyboard events ──

- (void)sendTabKey:(BOOL)shift {
    if (!self.owner) {
        return;
    }
    toolkit::KeyEvent ke;
    ke.type = toolkit::KeyEvent::Type::Press;
    ke.key = toolkit::Key::Tab;
    ke.shift = shift;
    self.owner->handle_key(ke);
}
- (void)insertTab:(id)sender {
    [self sendTabKey:NO];
}
- (void)insertBacktab:(id)sender {
    [self sendTabKey:YES];
}

- (void)keyDown:(NSEvent *)event {
    if (!self.owner) {
        return;
    }
    if ([event keyCode] == 48) {
        [self interpretKeyEvents:@[ event ]];
        return;
    }
    toolkit::KeyEvent ke;
    ke.type = toolkit::KeyEvent::Type::Press;
    NSEventModifierFlags m = [event modifierFlags];
    ke.shift = (m & NSEventModifierFlagShift) != 0;
    ke.ctrl = (m & NSEventModifierFlagControl) != 0;
    ke.alt = (m & NSEventModifierFlagOption) != 0;
    ke.super = (m & NSEventModifierFlagCommand) != 0;
    switch ([event keyCode]) {
    case 49:
        ke.key = toolkit::Key::Space;
        break;
    case 51:
        ke.key = toolkit::Key::Backspace;
        break;
    case 117:
        ke.key = toolkit::Key::Delete;
        break;
    case 123:
        ke.key = toolkit::Key::Left;
        break;
    case 124:
        ke.key = toolkit::Key::Right;
        break;
    case 125:
        ke.key = toolkit::Key::Down;
        break;
    case 126:
        ke.key = toolkit::Key::Up;
        break;
    case 115:
        ke.key = toolkit::Key::Home;
        break;
    case 119:
        ke.key = toolkit::Key::End;
        break;
    case 116:
        ke.key = toolkit::Key::PageUp;
        break;
    case 121:
        ke.key = toolkit::Key::PageDown;
        break;
    case 36:
        ke.key = toolkit::Key::Enter;
        break;
    case 53:
        ke.key = toolkit::Key::Escape;
        break;
    default:
        break;
    }
    bool has_mod = ke.alt || ke.super || ke.ctrl;
    NSString *chars = has_mod ? [event charactersIgnoringModifiers] : [event characters];
    if (chars.length > 0 && ke.key == toolkit::Key::NoKey) {
        unichar c = [chars characterAtIndex:0];
        if (c >= 32 && c < 127) {
            ke.text = [chars UTF8String];
        }
    }
    if (ke.super && ke.key == toolkit::Key::Left) {
        ke.key = toolkit::Key::Home;
    }
    if (ke.super && ke.key == toolkit::Key::Right) {
        ke.key = toolkit::Key::End;
    }
    self.owner->handle_key(ke);
}

- (BOOL)performKeyEquivalent:(NSEvent *)event {
    if (!self.owner) {
        return NO;
    }
    NSEventModifierFlags m = [event modifierFlags];
    if ((m & NSEventModifierFlagCommand) || (m & NSEventModifierFlagControl)) {
        [self keyDown:event];
        return YES;
    }
    return [super performKeyEquivalent:event];
}

@end

namespace toolkit {

std::unique_ptr<PlatformWindow>
MacOSOpenGLPlatformApplication::create_window(std::string_view title, Size size, Window *owner,
                                              WindowOptions options) {
    return std::make_unique<MacOSOpenGLPlatformWindow>(title, size, owner, options);
}

}

// ── MacOSOpenGLPlatformWindow ───────────────────────────────────────────────

struct MacOSOpenGLPlatformWindow::Impl {
    NSWindow *ns_window = nil;
    TKGLView *view = nil;
    TKGLWindowDelegate *delegate = nil;
    NSMutableDictionary<NSNumber *, NSTimer *> *timers = nil;
    int next_timer_id = 1;
    TKGLTooltipWindow *tooltip_window = nil;
    CoreTextRasterizer rasterizer;
};

MacOSOpenGLPlatformWindow::MacOSOpenGLPlatformWindow(std::string_view title, Size size,
                                                     Window *owner, WindowOptions options)
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
    impl_->delegate = [[TKGLWindowDelegate alloc] init];
    impl_->delegate.owner = owner;
    [impl_->ns_window setDelegate:impl_->delegate];
    impl_->view = [[TKGLView alloc] initWithFrame:frame];
    impl_->view.owner = owner;
    [impl_->ns_window setContentView:impl_->view];
    NSTrackingArea *tracking = [[NSTrackingArea alloc]
        initWithRect:NSZeroRect
             options:(NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited |
                      NSTrackingActiveAlways | NSTrackingInVisibleRect)
               owner:impl_->view
            userInfo:nil];
    [impl_->view addTrackingArea:tracking];
}

MacOSOpenGLPlatformWindow::~MacOSOpenGLPlatformWindow() {
    for (NSTimer *t in impl_->timers.allValues) {
        [t invalidate];
    }
    [impl_->timers removeAllObjects];
}

void MacOSOpenGLPlatformWindow::show() {
    [impl_->ns_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

void MacOSOpenGLPlatformWindow::hide() { [impl_->ns_window orderOut:nil]; }

void MacOSOpenGLPlatformWindow::close() { [impl_->ns_window close]; }

void MacOSOpenGLPlatformWindow::set_size(Size s) {
    [impl_->ns_window setContentSize:NSMakeSize(s.width, s.height)];
}

// Cocoa's screen space has its origin at the bottom-left with y growing upward, while the toolkit
// uses top-left/y-down everywhere else. Both accessors flip through the window's own screen.
static CGFloat macos_screen_height(NSWindow *window) {
    NSScreen *screen = [window screen];
    if (!screen) {
        screen = [NSScreen mainScreen];
    }
    return [screen frame].size.height;
}

Point MacOSOpenGLPlatformWindow::position() const {
    auto frame = [impl_->ns_window frame];
    auto screen_height = macos_screen_height(impl_->ns_window);
    return {static_cast<float>(frame.origin.x),
            static_cast<float>(screen_height - frame.origin.y - frame.size.height)};
}

void MacOSOpenGLPlatformWindow::set_position(Point p) {
    auto frame = [impl_->ns_window frame];
    auto screen_height = macos_screen_height(impl_->ns_window);
    [impl_->ns_window setFrameOrigin:NSMakePoint(p.x, screen_height - p.y - frame.size.height)];
}

void MacOSOpenGLPlatformWindow::request_redraw() { [impl_->view setNeedsDisplay:YES]; }

void MacOSOpenGLPlatformWindow::set_min_size(Size s) {
    [impl_->ns_window setContentMinSize:NSMakeSize(s.width, s.height)];
}

void MacOSOpenGLPlatformWindow::set_max_size(Size s) {
    if (s.width > 0 && s.height > 0) {
        [impl_->ns_window setContentMaxSize:NSMakeSize(s.width, s.height)];
    }
}

void MacOSOpenGLPlatformWindow::minimize() {}
void MacOSOpenGLPlatformWindow::maximize() {}
void MacOSOpenGLPlatformWindow::restore() {}

void MacOSOpenGLPlatformWindow::set_icon(Image const &icon) {
    if (!icon || icon->pixels.empty()) {
        return;
    }
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr
                                                                    pixelsWide:icon->width
                                                                    pixelsHigh:icon->height
                                                                 bitsPerSample:8
                                                               samplesPerPixel:4
                                                                      hasAlpha:YES
                                                                      isPlanar:NO
                                                                colorSpaceName:NSDeviceRGBColorSpace
                                                                   bytesPerRow:icon->width * 4
                                                                  bitsPerPixel:32];

    // UNVERIFIED: not compile-checked on macOS, please build-check before trusting.
    // NSBitmapImageRep has no byte-order flag (unlike CGImage) -- for NSDeviceRGBColorSpace with
    // 4 samples/pixel it always expects R,G,B,A order, but ImageData::pixels is B,G,R,A (see
    // image.hpp), so swap into a scratch copy first.
    unsigned char *bitmapData = [rep bitmapData];
    std::memcpy(bitmapData, icon->pixels.data(), icon->pixels.size());
    pixel::swap_rb(bitmapData, static_cast<size_t>(icon->width) * icon->height);

    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(icon->width, icon->height)];
    [image addRepresentation:rep];
    [impl_->ns_window setRepresentedImage:image];
}

Image MacOSOpenGLPlatformWindow::get_icon() {
    // FIXME: Implement NSImage to Image conversion
    return nullptr;
}

void MacOSOpenGLPlatformWindow::start_system_move(uint32_t /*serial*/) {}
void MacOSOpenGLPlatformWindow::start_system_resize(WindowEdge edge, uint32_t /*serial*/) {}

int MacOSOpenGLPlatformWindow::start_timer(float interval_sec, std::function<void()> callback,

                                           bool repeats) {
    int tid = impl_->next_timer_id++;
    auto cb = std::make_shared<std::function<void()>>(std::move(callback));
    TKGLView *view = impl_->view;
    NSMutableDictionary *timers = impl_->timers;
    NSNumber *key = @(tid);
    NSTimer *timer = [NSTimer scheduledTimerWithTimeInterval:interval_sec
                                                     repeats:repeats
                                                       block:^(NSTimer *t) {
                                                         (*cb)();
                                                         [view setNeedsDisplay:YES];
                                                         if (!repeats) {
                                                             [timers removeObjectForKey:key];
                                                         }
                                                       }];
    [impl_->timers setObject:timer forKey:key];
    return tid;
}

void MacOSOpenGLPlatformWindow::stop_timer(int timer_id) {
    NSNumber *key = @(timer_id);
    NSTimer *timer = [impl_->timers objectForKey:key];
    if (timer) {
        [timer invalidate];
        [impl_->timers removeObjectForKey:key];
    }
}

void MacOSOpenGLPlatformWindow::set_cursor(CursorShape shape) {
    NSCursor *nc;
    switch (shape) {
    case CursorShape::IBeam:
        nc = [NSCursor IBeamCursor];
        break;
    case CursorShape::Hand:
        nc = [NSCursor pointingHandCursor];
        break;
    case CursorShape::NotAllowed:
        nc = [NSCursor operationNotAllowedCursor];
        break;
    case CursorShape::ResizeEW:
        nc = [NSCursor resizeLeftRightCursor];
        break;
    case CursorShape::ResizeNS:
        nc = [NSCursor resizeUpDownCursor];
        break;
    default:
        nc = [NSCursor arrowCursor];
        break;
    }
    [nc set];
}

void MacOSOpenGLPlatformWindow::show_tooltip_window(std::string const &text, Point local_pos) {
    auto const &style = Theme::current().tooltip;
    float pad = style.padding, fs = style.font_size;
    auto *r = detail::current_platform();
    auto tsz = r ? r->measure_text(text, fs) : Size{};
    auto fm = r ? r->font_metrics(fs) : Painter::FontMetrics{};
    float w = tsz.width + pad * 2, h = fm.height + pad * 2;
    NSPoint vp = NSMakePoint(local_pos.x, local_pos.y);
    NSPoint wp = [impl_->view convertPoint:vp toView:nil];
    NSPoint sp = [impl_->ns_window convertPointToScreen:wp];
    float sx = static_cast<float>(sp.x);
    float sy = static_cast<float>(sp.y) - h - 4.0f;
    NSScreen *scr = [impl_->ns_window screen] ?: [NSScreen mainScreen];
    NSRect vis = [scr visibleFrame];
    if (sx + w > vis.origin.x + vis.size.width) {
        sx = static_cast<float>(vis.origin.x + vis.size.width) - w - 2.0f;
    }
    if (sx < vis.origin.x) {
        sx = static_cast<float>(vis.origin.x) + 2.0f;
    }
    if (sy < vis.origin.y) {
        sy = static_cast<float>(sp.y) + 20.0f;
    }
    NSRect tipFrame = NSMakeRect(sx, sy, w, h);
    if (!impl_->tooltip_window) {
        impl_->tooltip_window =
            [[TKGLTooltipWindow alloc] initWithContentRect:tipFrame
                                                 styleMask:NSWindowStyleMaskBorderless
                                                   backing:NSBackingStoreBuffered
                                                     defer:YES];
        [impl_->tooltip_window setOpaque:NO];
        [impl_->tooltip_window setBackgroundColor:[NSColor clearColor]];
        [impl_->tooltip_window setLevel:NSStatusWindowLevel];
        [impl_->tooltip_window setIgnoresMouseEvents:YES];
        [impl_->tooltip_window setHasShadow:YES];
    }
    [impl_->tooltip_window setFrame:tipFrame display:NO];

    float scale = static_cast<float>(impl_->ns_window.backingScaleFactor);
    int piw = std::max(1, static_cast<int>(std::ceil(w * scale)));
    int pih = std::max(1, static_cast<int>(std::ceil(h * scale)));

    [[impl_->view openGLContext] makeCurrentContext];
    std::vector<uint8_t> pixels(static_cast<size_t>(piw) * pih * 4);
    gl_render_to_buffer(piw, pih, scale, &impl_->rasterizer, pixels.data(), [&](Painter &p) {
        auto fm = p.font_metrics(fs);
        Rect r{0, 0, w, h};
        p.fill_rounded_rect(r, style.background, style.corner_radius);
        p.draw_rounded_rect(r, style.border, style.corner_radius, style.border_width);
        p.draw_text(text, {pad, pad + fm.ascent}, style.text, fs);
    });
    [[impl_->view openGLContext] makeCurrentContext]; // restore main context

    // Wrap pixels in a CGImage (BGRA, premultiplied alpha, little-endian)
    auto *pixelsCopy = new uint8_t[static_cast<size_t>(piw) * pih * 4];
    std::memcpy(pixelsCopy, pixels.data(), static_cast<size_t>(piw) * pih * 4);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef dp = CGDataProviderCreateWithData(
        pixelsCopy, pixelsCopy, static_cast<size_t>(piw) * pih * 4,
        [](void *info, const void *, size_t) { delete[] static_cast<uint8_t *>(info); });
    CGImageRef cgImg = CGImageCreate(
        piw, pih, 8, 32, piw * 4, cs,
        static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst), dp,
        nullptr, false, kCGRenderingIntentDefault);
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(dp);
    NSImage *nsImg = [[NSImage alloc] initWithCGImage:cgImg size:NSMakeSize(w, h)];
    CGImageRelease(cgImg);

    TKGLTooltipView *tv = [[TKGLTooltipView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    tv.renderedImage = nsImg;
    [impl_->tooltip_window setContentView:tv];
    [impl_->tooltip_window orderFront:nil];
}

void MacOSOpenGLPlatformWindow::hide_tooltip_window() {
    if (impl_->tooltip_window) {
        [impl_->tooltip_window orderOut:nil];
    }
}

Icon MacOSOpenGLPlatformWindow::capture() {
    float scale = scale_factor();
    int lw = static_cast<int>(owner_->size().width);
    int lh = static_cast<int>(owner_->size().height);
    if (lw <= 0 || lh <= 0) {
        return nullptr;
    }
    int pw = static_cast<int>(std::ceil(lw * scale));
    int ph = static_cast<int>(std::ceil(lh * scale));

    // Create standalone offscreen CGL context
    CGLPixelFormatAttribute pfa[] = {kCGLPFAColorSize, (CGLPixelFormatAttribute)32,
                                     kCGLPFAAlphaSize, (CGLPixelFormatAttribute)8,
                                     (CGLPixelFormatAttribute)0};
    CGLPixelFormatObj pf;
    GLint npix;
    CGLChoosePixelFormat(pfa, &pf, &npix);
    CGLContextObj cgl;
    CGLCreateContext(pf, nullptr, &cgl);
    CGLDestroyPixelFormat(pf);
    CGLSetCurrentContext(cgl);

    // Create FBO
    GLuint fbo, rbo;
    glGenFramebuffersEXT(1, &fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
    glGenRenderbuffersEXT(1, &rbo);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rbo);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, pw, ph);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT,
                                 rbo);

    glViewport(0, 0, pw, ph);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, lw, lh, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLPainter painter(static_cast<float>(lh), scale, &impl_->rasterizer);
    owner_->handle_paint(painter);
    glFinish();

    // Read pixels (GL reads bottom-up, flip later)
    auto result = std::make_shared<ImageData>();
    result->width = pw;
    result->height = ph;
    result->channels = 4;
    result->pixels.resize(pw * ph * 4);

    std::vector<uint8_t> temp_pixels(pw * ph * 4);
    glReadPixels(0, 0, pw, ph, GL_RGBA, GL_UNSIGNED_BYTE, temp_pixels.data());

    // Cleanup GL
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glDeleteRenderbuffersEXT(1, &rbo);
    glDeleteFramebuffersEXT(1, &fbo);
    CGLSetCurrentContext(nullptr);
    CGLDestroyContext(cgl);

    // Flip vertically into result
    for (int y = 0; y < ph; y++) {
        memcpy(&result->pixels[y * pw * 4], &temp_pixels[(ph - 1 - y) * pw * 4], pw * 4);
    }

    return result;
}

float MacOSOpenGLPlatformWindow::scale_factor() const {
    return static_cast<float>(impl_->ns_window.backingScaleFactor);
}

} // namespace toolkit

#pragma clang diagnostic pop
