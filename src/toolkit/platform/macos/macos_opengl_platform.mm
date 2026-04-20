#include "macos_opengl_platform.hpp"
#include "toolkit/painters/gl_offscreen.hpp"
#include "toolkit/painters/gl_painter.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#import <OpenGL/glext.h>
#import <CoreText/CoreText.h>
#import <ImageIO/ImageIO.h>
#include <cmath>
#include <string>
#include <vector>

// ── CoreTextRasterizer ──────────────────────────────────────────────────────

namespace toolkit {

class CoreTextRasterizer : public TextRasterizer {
  public:
    static NSFont *ns_font(float size, FontFamily family) {
        if (family == FontFamily::Monospace)
            return [NSFont monospacedSystemFontOfSize:size weight:NSFontWeightRegular];
        return [NSFont systemFontOfSize:size];
    }

    RasterizedText rasterize(std::string_view text, float font_size,
                             float scale, FontFamily family = FontFamily::System) override {
        NSFont *font = ns_font(font_size, family);
        NSString *str =
            [[NSString alloc] initWithBytes:text.data()
                                     length:text.size()
                                   encoding:NSUTF8StringEncoding];
        if (!str || str.length == 0) return {};

        NSDictionary *attrs = @{
            NSFontAttributeName : font,
            NSForegroundColorAttributeName :
                [NSColor colorWithRed:1 green:1 blue:1 alpha:1]
        };
        NSAttributedString *astr =
            [[NSAttributedString alloc] initWithString:str attributes:attrs];
        CTLineRef line = CTLineCreateWithAttributedString(
            (__bridge CFAttributedStringRef)astr);

        NSSize sz = [str sizeWithAttributes:attrs];
        int tw = static_cast<int>(std::ceil(sz.width));
        int th = static_cast<int>(std::ceil(sz.height));
        if (tw <= 0 || th <= 0) { CFRelease(line); return {}; }

        int ptw = static_cast<int>(std::ceil(tw * scale));
        int pth = static_cast<int>(std::ceil(th * scale));

        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(
            nullptr, ptw, pth, 8, ptw * 4, cs,
            kCGImageAlphaPremultipliedLast |
                static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big));
        CGColorSpaceRelease(cs);
        if (!ctx) { CFRelease(line); return {}; }

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
        NSString *str =
            [[NSString alloc] initWithBytes:text.data()
                                     length:text.size()
                                   encoding:NSUTF8StringEncoding];
        if (!str) return {0, 0};
        NSDictionary *attrs = @{NSFontAttributeName : font};
        NSSize sz = [str sizeWithAttributes:attrs];
        return {static_cast<float>(sz.width), static_cast<float>(sz.height)};
    }

    Painter::FontMetrics metrics(float font_size,
                                 FontFamily family = FontFamily::System) override {
        NSFont *font = ns_font(font_size, family);
        float ascent = static_cast<float>(font.ascender);
        float descent = static_cast<float>(-font.descender);
        return {ascent, descent, ascent + descent};
    }
};

} // namespace toolkit

@interface TKGLWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) toolkit::Window *owner;
@end

@implementation TKGLWindowDelegate
- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize {
    if (!self.owner) return frameSize;
    NSRect cr = [sender contentRectForFrameRect:NSMakeRect(0, 0, frameSize.width,
                                                           frameSize.height)];
    auto mn = self.owner->min_size();
    auto mx = self.owner->max_size();
    if (mn.width > 0 && cr.size.width < mn.width) cr.size.width = mn.width;
    if (mn.height > 0 && cr.size.height < mn.height) cr.size.height = mn.height;
    if (mx.width > 0 && cr.size.width > mx.width) cr.size.width = mx.width;
    if (mx.height > 0 && cr.size.height > mx.height) cr.size.height = mx.height;
    return [sender frameRectForContentRect:cr].size;
}
- (void)windowDidResignKey:(NSNotification *)n {
    if (self.owner) self.owner->hide_tooltip();
}
@end

@interface TKGLTooltipWindow : NSWindow
@end
@implementation TKGLTooltipWindow
- (BOOL)canBecomeKeyWindow { return NO; }
- (BOOL)canBecomeMainWindow { return NO; }
@end

@interface TKGLTooltipView : NSView
@property (nonatomic, strong) NSImage *renderedImage;
@end

@implementation TKGLTooltipView
- (void)drawRect:(NSRect)dirtyRect {
    if (self.renderedImage) {
        [self.renderedImage drawInRect:[self bounds]];
    }
}
@end

@interface TKGLView : NSOpenGLView
@property (nonatomic, assign) toolkit::Window *owner;
@end

@implementation TKGLView {
    toolkit::CoreTextRasterizer rasterizer_;
}

- (instancetype)initWithFrame:(NSRect)frame {
    NSOpenGLPixelFormatAttribute attrs[] = {NSOpenGLPFADoubleBuffer,
                                            NSOpenGLPFAColorSize, 24,
                                            NSOpenGLPFAAlphaSize, 8,
                                            0};
    NSOpenGLPixelFormat *pf =
        [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    self = [super initWithFrame:frame pixelFormat:pf];
    if (self) {
        [self setWantsBestResolutionOpenGLSurface:YES];
    }
    return self;
}

- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }

- (void)prepareOpenGL {
    [super prepareOpenGL];
    GLint swap = 1;
    [[self openGLContext] setValues:&swap
                      forParameter:NSOpenGLContextParameterSwapInterval];
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
        self.owner->handle_resize({static_cast<float>(b.size.width),
                                   static_cast<float>(b.size.height)});
    }
}

- (void)drawRect:(NSRect)dirtyRect {
    if (!self.owner) return;
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
    bool has_mod = ke.alt || ke.super || ke.ctrl;
    NSString *chars =
        has_mod ? [event charactersIgnoringModifiers] : [event characters];
    if (chars.length > 0 && ke.key == toolkit::Key::NoKey) {
        unichar c = [chars characterAtIndex:0];
        if (c >= 32 && c < 127) ke.text = [chars UTF8String];
    }
    if (ke.super && ke.key == toolkit::Key::Left) ke.key = toolkit::Key::Home;
    if (ke.super && ke.key == toolkit::Key::Right) ke.key = toolkit::Key::End;
    self.owner->handle_key(ke);
}

- (BOOL)performKeyEquivalent:(NSEvent *)event {
    if (!self.owner) return NO;
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
MacOSOpenGLPlatformApplication::create_window(std::string_view title, Size size,
                                              Window *owner) {
    return std::make_unique<MacOSOpenGLPlatformWindow>(title, size, owner);
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

MacOSOpenGLPlatformWindow::MacOSOpenGLPlatformWindow(std::string_view title,
                                                     Size size,
                                                     Window *owner)
    : impl_(std::make_unique<Impl>()), owner_(owner) {
    NSRect frame = NSMakeRect(200, 200, size.width, size.height);
    NSWindowStyleMask style = NSWindowStyleMaskTitled |
                              NSWindowStyleMaskClosable |
                              NSWindowStyleMaskMiniaturizable |
                              NSWindowStyleMaskResizable;
    impl_->ns_window =
        [[NSWindow alloc] initWithContentRect:frame
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
             options:(NSTrackingMouseMoved | NSTrackingActiveAlways |
                      NSTrackingInVisibleRect)
               owner:impl_->view
            userInfo:nil];
    [impl_->view addTrackingArea:tracking];
}

MacOSOpenGLPlatformWindow::~MacOSOpenGLPlatformWindow() {
    for (NSTimer *t in impl_->timers.allValues)
        [t invalidate];
    [impl_->timers removeAllObjects];
}

void MacOSOpenGLPlatformWindow::show() {
    [impl_->ns_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

void MacOSOpenGLPlatformWindow::close() { [impl_->ns_window close]; }

void MacOSOpenGLPlatformWindow::set_size(Size s) {
    [impl_->ns_window setContentSize:NSMakeSize(s.width, s.height)];
}

void MacOSOpenGLPlatformWindow::request_redraw() {
    [impl_->view setNeedsDisplay:YES];
}

void MacOSOpenGLPlatformWindow::set_min_size(Size s) {
    [impl_->ns_window setContentMinSize:NSMakeSize(s.width, s.height)];
}

void MacOSOpenGLPlatformWindow::set_max_size(Size s) {
    if (s.width > 0 && s.height > 0)
        [impl_->ns_window setContentMaxSize:NSMakeSize(s.width, s.height)];
}

int MacOSOpenGLPlatformWindow::start_timer(float interval_sec,
                                           std::function<void()> callback,
                                           bool repeats) {
    int tid = impl_->next_timer_id++;
    auto cb = std::make_shared<std::function<void()>>(std::move(callback));
    TKGLView *view = impl_->view;
    NSMutableDictionary *timers = impl_->timers;
    NSNumber *key = @(tid);
    NSTimer *timer =
        [NSTimer scheduledTimerWithTimeInterval:interval_sec
                                        repeats:repeats
                                          block:^(NSTimer *t) {
                                            (*cb)();
                                            [view setNeedsDisplay:YES];
                                            if (!repeats)
                                                [timers
                                                    removeObjectForKey:key];
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
    case CursorShape::IBeam:      nc = [NSCursor IBeamCursor]; break;
    case CursorShape::Hand:       nc = [NSCursor pointingHandCursor]; break;
    case CursorShape::NotAllowed: nc = [NSCursor operationNotAllowedCursor]; break;
    case CursorShape::ResizeEW:   nc = [NSCursor resizeLeftRightCursor]; break;
    default:                      nc = [NSCursor arrowCursor]; break;
    }
    [nc set];
}

void MacOSOpenGLPlatformWindow::show_tooltip_window(std::string const &text,
                                                    Point local_pos) {
    auto const &style = Theme::current().tooltip;
    float pad = style.padding, fs = style.font_size;
    auto tsz = Painter::measure_text(text, fs);
    auto fm = Painter::measure_font_metrics(fs);
    float w = tsz.width + pad * 2, h = fm.height + pad * 2;
    NSPoint vp = NSMakePoint(local_pos.x, local_pos.y);
    NSPoint wp = [impl_->view convertPoint:vp toView:nil];
    NSPoint sp = [impl_->ns_window convertPointToScreen:wp];
    float sx = static_cast<float>(sp.x);
    float sy = static_cast<float>(sp.y) - h - 4.0f;
    NSScreen *scr = [impl_->ns_window screen] ?: [NSScreen mainScreen];
    NSRect vis = [scr visibleFrame];
    if (sx + w > vis.origin.x + vis.size.width)
        sx = static_cast<float>(vis.origin.x + vis.size.width) - w - 2.0f;
    if (sx < vis.origin.x) sx = static_cast<float>(vis.origin.x) + 2.0f;
    if (sy < vis.origin.y) sy = static_cast<float>(sp.y) + 20.0f;
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
        static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst),
        dp, nullptr, false, kCGRenderingIntentDefault);
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(dp);
    NSImage *nsImg = [[NSImage alloc] initWithCGImage:cgImg size:NSMakeSize(w, h)];
    CGImageRelease(cgImg);

    TKGLTooltipView *tv =
        [[TKGLTooltipView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
    tv.renderedImage = nsImg;
    [impl_->tooltip_window setContentView:tv];
    [impl_->tooltip_window orderFront:nil];
}

void MacOSOpenGLPlatformWindow::hide_tooltip_window() {
    if (impl_->tooltip_window) [impl_->tooltip_window orderOut:nil];
}

bool MacOSOpenGLPlatformWindow::save_to_png(std::string const &path) {
    int w = static_cast<int>(owner_->size().width);
    int h = static_cast<int>(owner_->size().height);
    if (w <= 0 || h <= 0) return false;

    // Create standalone offscreen CGL context
    CGLPixelFormatAttribute pfa[] = {
        kCGLPFAColorSize, (CGLPixelFormatAttribute)32,
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
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, w, h);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                 GL_RENDERBUFFER_EXT, rbo);

    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLPainter painter(static_cast<float>(h), 1.0f, &impl_->rasterizer);
    owner_->handle_paint(painter);
    glFinish();

    // Read pixels (GL reads bottom-up, flip later)
    std::vector<uint8_t> pixels(w * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // Cleanup GL
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glDeleteRenderbuffersEXT(1, &rbo);
    glDeleteFramebuffersEXT(1, &fbo);
    CGLSetCurrentContext(nullptr);
    CGLDestroyContext(cgl);

    // Flip vertically
    std::vector<uint8_t> flipped(w * h * 4);
    for (int y = 0; y < h; y++)
        memcpy(&flipped[y * w * 4], &pixels[(h - 1 - y) * w * 4], w * 4);

    // Save via ImageIO
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef cgCtx = CGBitmapContextCreate(
        flipped.data(), w, h, 8, w * 4, cs,
        kCGImageAlphaPremultipliedLast | static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big));
    CGColorSpaceRelease(cs);
    CGImageRef image = CGBitmapContextCreateImage(cgCtx);
    CGContextRelease(cgCtx);

    NSString *nsPath = [NSString stringWithUTF8String:path.c_str()];
    NSURL *url = [NSURL fileURLWithPath:nsPath];
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL(
        (__bridge CFURLRef)url, CFSTR("public.png"), 1, nullptr);
    bool ok = false;
    if (dest) {
        CGImageDestinationAddImage(dest, image, nullptr);
        ok = CGImageDestinationFinalize(dest);
        CFRelease(dest);
    }
    CGImageRelease(image);
    return ok;
}

float MacOSOpenGLPlatformWindow::scale_factor() const {
    return static_cast<float>(impl_->ns_window.backingScaleFactor);
}

} // namespace toolkit

#pragma clang diagnostic pop
