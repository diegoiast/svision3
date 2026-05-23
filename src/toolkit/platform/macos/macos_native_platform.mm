#include "macos_native_platform.hpp"
#include "toolkit/theme.hpp"
#include "toolkit/window.hpp"

#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>
#import <ImageIO/ImageIO.h>
#include <cmath>
#include <string>

// ── CoreGraphicsPainter ─────────────────────────────────────────────────────

namespace toolkit {

class CoreGraphicsPainter : public Painter {
  public:
    explicit CoreGraphicsPainter(CGContextRef ctx) : ctx_(ctx) {}

    void push_clip(Rect const &r) override {
        CGContextSaveGState(ctx_);
        CGContextClipToRect(ctx_, CGRectMake(r.x, r.y, r.width, r.height));
    }

    void pop_clip() override { CGContextRestoreGState(ctx_); }

    void push_translation(Point p) override {
        CGContextSaveGState(ctx_);
        CGContextTranslateCTM(ctx_, p.x, p.y);
    }

    void pop_translation() override { CGContextRestoreGState(ctx_); }

    void set_line_style(Painter::LineStyle style) override { style_ = style; }

    void fill_rect(Rect const &r, Color const &c) override {
        CGContextSetLineDash(ctx_, 0, nullptr, 0);
        CGContextSetRGBFillColor(ctx_, c.r, c.g, c.b, c.a);
        CGContextFillRect(ctx_, CGRectMake(r.x, r.y, r.width, r.height));
    }

    void draw_rect(Rect const &r, Color const &c, float lw) override {
        CGContextSetRGBStrokeColor(ctx_, c.r, c.g, c.b, c.a);
        CGContextSetLineWidth(ctx_, lw);
        apply_line_style(lw);
        CGContextStrokeRect(ctx_, CGRectMake(r.x, r.y, r.width, r.height));
    }

    void fill_rounded_rect(Rect const &r, Color const &c,
                           float radius) override {
        CGContextSetLineDash(ctx_, 0, nullptr, 0);
        float rad = std::min({radius, r.width / 2.0f, r.height / 2.0f});
        CGPathRef path = CGPathCreateWithRoundedRect(
            CGRectMake(r.x, r.y, r.width, r.height), rad, rad, nullptr);
        CGContextAddPath(ctx_, path);
        CGContextSetRGBFillColor(ctx_, c.r, c.g, c.b, c.a);
        CGContextFillPath(ctx_);
        CGPathRelease(path);
    }

    void draw_rounded_rect(Rect const &r, Color const &c, float radius,
                           float lw) override {
        float rad = std::min({radius, r.width / 2.0f, r.height / 2.0f});
        CGPathRef path = CGPathCreateWithRoundedRect(
            CGRectMake(r.x, r.y, r.width, r.height), rad, rad, nullptr);
        CGContextAddPath(ctx_, path);
        CGContextSetRGBStrokeColor(ctx_, c.r, c.g, c.b, c.a);
        CGContextSetLineWidth(ctx_, lw);
        apply_line_style(lw);
        CGContextStrokePath(ctx_);
        CGPathRelease(path);
    }

    void fill_triangle(Point a, Point b, Point c, Color const &color) override {
        CGContextSetLineDash(ctx_, 0, nullptr, 0);
        CGContextSetRGBFillColor(ctx_, color.r, color.g, color.b, color.a);
        CGContextBeginPath(ctx_);
        CGContextMoveToPoint(ctx_, a.x, a.y);
        CGContextAddLineToPoint(ctx_, b.x, b.y);
        CGContextAddLineToPoint(ctx_, c.x, c.y);
        CGContextClosePath(ctx_);
        CGContextFillPath(ctx_);
    }

    void draw_line(Point a, Point b, Color const &c, float lw) override {
        CGContextSetRGBStrokeColor(ctx_, c.r, c.g, c.b, c.a);
        CGContextSetLineWidth(ctx_, lw);
        apply_line_style(lw);
        CGContextMoveToPoint(ctx_, a.x, a.y);
        CGContextAddLineToPoint(ctx_, b.x, b.y);
        CGContextStrokePath(ctx_);
    }

    void fill_circle(Point center, float radius,
                     Color const &c) override {
        CGContextSetLineDash(ctx_, 0, nullptr, 0);
        CGRect er = CGRectMake(center.x - radius, center.y - radius,
                               radius * 2, radius * 2);
        CGContextSetRGBFillColor(ctx_, c.r, c.g, c.b, c.a);
        CGContextFillEllipseInRect(ctx_, er);
    }

    void draw_circle(Point center, float radius, Color const &c,
                     float lw) override {
        CGRect er = CGRectMake(center.x - radius, center.y - radius,
                               radius * 2, radius * 2);
        CGContextSetRGBStrokeColor(ctx_, c.r, c.g, c.b, c.a);
        CGContextSetLineWidth(ctx_, lw);
        apply_line_style(lw);
        CGContextStrokeEllipseInRect(ctx_, er);
    }

    static NSFont *ns_font(float size, FontFamily family) {
        if (family == FontFamily::Monospace)
            return [NSFont monospacedSystemFontOfSize:size weight:NSFontWeightRegular];
        return [NSFont systemFontOfSize:size];
    }

    void draw_text(std::string_view text, Point pos, Color const &c,
                   float font_size, FontFamily family,
                   TextOrientation orientation) override {
        NSFont *font = ns_font(font_size, family);
        NSString *str = [[NSString alloc] initWithBytes:text.data()
                                                 length:text.size()
                                               encoding:NSUTF8StringEncoding];
        if (!str) return;
        NSDictionary *attrs = @{
            NSFontAttributeName : font,
            NSForegroundColorAttributeName :
                [NSColor colorWithRed:c.r green:c.g blue:c.b alpha:c.a]
        };
        NSAttributedString *astr =
            [[NSAttributedString alloc] initWithString:str attributes:attrs];
        CTLineRef line =
            CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)astr);
        CGContextSaveGState(ctx_);
        CGContextTranslateCTM(ctx_, pos.x, pos.y);
        if (orientation == TextOrientation::VerticalCCW) {
            CGContextRotateCTM(ctx_, -M_PI / 2.0);
        } else if (orientation == TextOrientation::VerticalCW) {
            CGContextRotateCTM(ctx_, M_PI / 2.0);
        }
        CGContextScaleCTM(ctx_, 1.0, -1.0);
        CGContextSetTextPosition(ctx_, 0, 0);
        CTLineDraw(line, ctx_);
        CGContextRestoreGState(ctx_);
        CFRelease(line);
    }

    Size measure_text(std::string_view text, float font_size,
                   FontFamily family = FontFamily::System) override {
        NSFont *font = ns_font(font_size, family);
        NSString *str = [[NSString alloc] initWithBytes:text.data()
                                                 length:text.size()
                                               encoding:NSUTF8StringEncoding];
        if (!str) return {0, 0};
        NSDictionary *attrs = @{NSFontAttributeName : font};
        NSSize sz = [str sizeWithAttributes:attrs];
        return {static_cast<float>(sz.width), static_cast<float>(sz.height)};
    }

    FontMetrics font_metrics(float font_size,
                             FontFamily family = FontFamily::System) override {
        NSFont *font = ns_font(font_size, family);
        float ascent = static_cast<float>(font.ascender);
        float descent = static_cast<float>(-font.descender);
        return {ascent, descent, ascent + descent};
    }

    std::string_view name() const override { return "Native"; }

  private:
    CGContextRef ctx_;
    Painter::LineStyle style_ = Painter::LineStyle::Solid;

    void apply_line_style(float lw) {
        switch (style_) {
        case Painter::LineStyle::Dashed: {
            CGFloat dashes[] = {lw * 4.0f, lw * 4.0f};
            CGContextSetLineDash(ctx_, 0, dashes, 2);
            break;
        }
        case Painter::LineStyle::Dotted: {
            CGFloat dashes[] = {lw, lw * 2.0f};
            CGContextSetLineDash(ctx_, 0, dashes, 2);
            break;
        }
        case Painter::LineStyle::Solid:
        default:
            CGContextSetLineDash(ctx_, 0, nullptr, 0);
            break;
        }
    }
};

} // namespace toolkit

@interface TKNWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) toolkit::Window *owner;
@end

@implementation TKNWindowDelegate
- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize {
    if (!self.owner) return frameSize;
    NSRect cr = [sender contentRectForFrameRect:NSMakeRect(0, 0, frameSize.width, frameSize.height)];
    auto mn = self.owner->min_size();
    auto mx = self.owner->max_size();
    if (mn.width > 0 && cr.size.width < mn.width) cr.size.width = mn.width;
    if (mn.height > 0 && cr.size.height < mn.height) cr.size.height = mn.height;
    if (mx.width > 0 && cr.size.width > mx.width) cr.size.width = mx.width;
    if (mx.height > 0 && cr.size.height > mx.height) cr.size.height = mx.height;
    return [sender frameRectForContentRect:cr].size;
}
- (void)windowDidBecomeKey:(NSNotification *)notification {
    if (self.owner) self.owner->handle_activate(true);
}
- (void)windowDidResignKey:(NSNotification *)notification {
    if (self.owner) {
        self.owner->hide_tooltip();
        self.owner->handle_activate(false);
    }
}
@end

@interface TKNTooltipWindow : NSWindow
@end
@implementation TKNTooltipWindow
- (BOOL)canBecomeKeyWindow { return NO; }
- (BOOL)canBecomeMainWindow { return NO; }
@end

@interface TKNTooltipView : NSView
@property (nonatomic, copy) NSString *text;
@property (nonatomic) float fontSize;
@property (nonatomic) toolkit::Color bgColor;
@property (nonatomic) toolkit::Color borderColor;
@property (nonatomic) toolkit::Color textColor;
@property (nonatomic) float cornerRadius;
@property (nonatomic) float borderWidth;
@property (nonatomic) float padding;
@end

@implementation TKNTooltipView
- (BOOL)isFlipped { return YES; }
- (void)drawRect:(NSRect)dirtyRect {
    NSRect bounds = [self bounds];
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    toolkit::CoreGraphicsPainter painter(ctx);
    toolkit::Rect r{0, 0, static_cast<float>(bounds.size.width),
                    static_cast<float>(bounds.size.height)};
    painter.fill_rounded_rect(r, self.bgColor, self.cornerRadius);
    painter.draw_rounded_rect(r, self.borderColor, self.cornerRadius, self.borderWidth);
    auto fm = painter.font_metrics(self.fontSize);
    painter.draw_text(std::string([self.text UTF8String]),
                      {self.padding, self.padding + fm.ascent},
                      self.textColor, self.fontSize);
}
@end

@interface TKNView : NSView
@property (nonatomic, assign) toolkit::Window *owner;
@end

@implementation TKNView

- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    if (!self.owner) return;
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    toolkit::CoreGraphicsPainter painter(ctx);
    self.owner->handle_paint(painter);
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
    e.ctrl = (mods & NSEventModifierFlagControl) != 0;
    e.super = (mods & NSEventModifierFlagCommand) != 0;
    self.owner->handle_mouse(e);
}

- (void)rightMouseDown:(NSEvent *)event {
    auto pos = [self convertMouseEvent:event];
    toolkit::MouseEvent e{};
    e.type = toolkit::MouseEvent::Type::Press;
    e.position = pos;
    e.button = 1;
    e.click_count = static_cast<int>([event clickCount]);
    self.owner->handle_mouse(e);
}

- (void)otherMouseDown:(NSEvent *)event {
    auto pos = [self convertMouseEvent:event];
    toolkit::MouseEvent e{};
    e.type = toolkit::MouseEvent::Type::Press;
    e.position = pos;
    e.button = static_cast<int>([event buttonNumber]);
    e.click_count = static_cast<int>([event clickCount]);
    self.owner->handle_mouse(e);
}

- (void)mouseUp:(NSEvent *)event {
    auto pos = [self convertMouseEvent:event];
    toolkit::MouseEvent e{toolkit::MouseEvent::Type::Release, pos, 0};
    self.owner->handle_mouse(e);
}

- (void)rightMouseUp:(NSEvent *)event {
    auto pos = [self convertMouseEvent:event];
    toolkit::MouseEvent e{toolkit::MouseEvent::Type::Release, pos, 1};
    self.owner->handle_mouse(e);
}

- (void)otherMouseUp:(NSEvent *)event {
    auto pos = [self convertMouseEvent:event];
    toolkit::MouseEvent e{toolkit::MouseEvent::Type::Release, pos, static_cast<int>([event buttonNumber])};
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
        e.scroll_dy *= 0.5f; e.scroll_dx *= 0.5f;
    } else {
        e.scroll_dy *= 20.0f; e.scroll_dx *= 20.0f;
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
    ke.ctrl = (mods & NSEventModifierFlagControl) != 0;
    ke.alt = (mods & NSEventModifierFlagOption) != 0;
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
    case 122: ke.key = toolkit::Key::F1;        break;
    case 120: ke.key = toolkit::Key::F2;        break;
    case 99:  ke.key = toolkit::Key::F3;        break;
    case 118: ke.key = toolkit::Key::F4;        break;
    case 96:  ke.key = toolkit::Key::F5;        break;
    case 97:  ke.key = toolkit::Key::F6;        break;
    case 98:  ke.key = toolkit::Key::F7;        break;
    case 100: ke.key = toolkit::Key::F8;        break;
    case 101: ke.key = toolkit::Key::F9;        break;
    case 109: ke.key = toolkit::Key::F10;       break;
    case 103: ke.key = toolkit::Key::F11;       break;
    case 111: ke.key = toolkit::Key::F12;       break;
    // FIXME: add missing keys
    default:  break;
    }
    bool has_mod = ke.alt || ke.super || ke.ctrl;
    NSString *chars = has_mod ? [event charactersIgnoringModifiers] : [event characters];
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
    NSEventModifierFlags mods = [event modifierFlags];
    if ((mods & (NSEventModifierFlagCommand | NSEventModifierFlagControl))) {
        [self keyDown:event]; return YES;
    }
    return [super performKeyEquivalent:event];
}

@end

namespace toolkit {

std::unique_ptr<PlatformWindow>
MacOSNativePlatformApplication::create_window(std::string_view title,
                                              Size size, Window *owner) {
    return std::make_unique<MacOSNativePlatformWindow>(title, size, owner);
}

// ── MacOSNativePlatformWindow ───────────────────────────────────────────────

struct MacOSNativePlatformWindow::Impl {
    NSWindow *ns_window = nil;
    TKNView *view = nil;
    TKNWindowDelegate *delegate = nil;
    NSMutableDictionary<NSNumber *, NSTimer *> *timers = nil;
    int next_timer_id = 1;
    TKNTooltipWindow *tooltip_window = nil;
};

MacOSNativePlatformWindow::MacOSNativePlatformWindow(std::string_view title,
                                                     Size size,
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
    impl_->delegate = [[TKNWindowDelegate alloc] init];
    impl_->delegate.owner = owner;
    [impl_->ns_window setDelegate:impl_->delegate];
    impl_->view = [[TKNView alloc] initWithFrame:frame];
    impl_->view.owner = owner;
    [impl_->ns_window setContentView:impl_->view];
    NSTrackingArea *tracking = [[NSTrackingArea alloc]
        initWithRect:NSZeroRect
             options:(NSTrackingMouseMoved | NSTrackingActiveAlways | NSTrackingInVisibleRect)
               owner:impl_->view
            userInfo:nil];
    [impl_->view addTrackingArea:tracking];
}

MacOSNativePlatformWindow::~MacOSNativePlatformWindow() {
    for (NSTimer *t in impl_->timers.allValues) [t invalidate];
    [impl_->timers removeAllObjects];
}

void MacOSNativePlatformWindow::show() {
    [impl_->ns_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

void MacOSNativePlatformWindow::close() { [impl_->ns_window close]; }

void MacOSNativePlatformWindow::set_size(Size s) {
    [impl_->ns_window setContentSize:NSMakeSize(s.width, s.height)];
}

void MacOSNativePlatformWindow::request_redraw() {
    [impl_->view setNeedsDisplay:YES];
}

void MacOSNativePlatformWindow::set_min_size(Size s) {
    [impl_->ns_window setContentMinSize:NSMakeSize(s.width, s.height)];
}

void MacOSNativePlatformWindow::set_max_size(Size s) {
    if (s.width > 0 && s.height > 0)
        [impl_->ns_window setContentMaxSize:NSMakeSize(s.width, s.height)];
}

int MacOSNativePlatformWindow::start_timer(float interval_sec,
                                           std::function<void()> callback,
                                           bool repeats) {
    int tid = impl_->next_timer_id++;
    auto cb = std::make_shared<std::function<void()>>(std::move(callback));
    TKNView *view = impl_->view;
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

void MacOSNativePlatformWindow::stop_timer(int timer_id) {
    NSNumber *key = @(timer_id);
    NSTimer *timer = [impl_->timers objectForKey:key];
    if (timer) { [timer invalidate]; [impl_->timers removeObjectForKey:key]; }
}

void MacOSNativePlatformWindow::set_cursor(CursorShape shape) {
    NSCursor *nc;
    switch (shape) {
    case CursorShape::IBeam:      nc = [NSCursor IBeamCursor]; break;
    case CursorShape::Hand:       nc = [NSCursor pointingHandCursor]; break;
    case CursorShape::NotAllowed: nc = [NSCursor operationNotAllowedCursor]; break;
    case CursorShape::ResizeEW:   nc = [NSCursor resizeLeftRightCursor]; break;
    case CursorShape::ResizeNS:   nc = [NSCursor resizeUpDownCursor]; break;
    default:                      nc = [NSCursor arrowCursor]; break;
    }
    [nc set];
}

void MacOSNativePlatformWindow::show_tooltip_window(std::string const &text,
                                                    Point local_pos) {
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
    if (sx + w > vis.origin.x + vis.size.width)
        sx = static_cast<float>(vis.origin.x + vis.size.width) - w - 2.0f;
    if (sx < vis.origin.x) sx = static_cast<float>(vis.origin.x) + 2.0f;
    if (sy < vis.origin.y) sy = static_cast<float>(sp.y) + 20.0f;
    NSRect tipFrame = NSMakeRect(sx, sy, w, h);
    if (!impl_->tooltip_window) {
        impl_->tooltip_window = [[TKNTooltipWindow alloc]
            initWithContentRect:tipFrame
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
    TKNTooltipView *tv = [[TKNTooltipView alloc]
        initWithFrame:NSMakeRect(0, 0, w, h)];
    tv.text = [NSString stringWithUTF8String:text.c_str()];
    tv.fontSize = fs;
    tv.bgColor = style.background;
    tv.borderColor = style.border;
    tv.textColor = style.text;
    tv.cornerRadius = style.corner_radius;
    tv.borderWidth = style.border_width;
    tv.padding = pad;
    [impl_->tooltip_window setContentView:tv];
    [impl_->tooltip_window orderFront:nil];
}

void MacOSNativePlatformWindow::hide_tooltip_window() {
    if (impl_->tooltip_window) [impl_->tooltip_window orderOut:nil];
}

bool MacOSNativePlatformWindow::save_to_png(std::string const &path) {
    int w = static_cast<int>(owner_->size().width);
    int h = static_cast<int>(owner_->size().height);
    if (w <= 0 || h <= 0) return false;
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        nullptr, w, h, 8, w * 4, cs,
        kCGImageAlphaPremultipliedFirst | static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little));
    CGColorSpaceRelease(cs);
    if (!ctx) return false;
    CGContextTranslateCTM(ctx, 0, h);
    CGContextScaleCTM(ctx, 1.0, -1.0);
    CoreGraphicsPainter painter(ctx);
    owner_->handle_paint(painter);
    CGImageRef image = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
    if (!image) return false;
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

float MacOSNativePlatformWindow::scale_factor() const {
    return static_cast<float>(impl_->ns_window.backingScaleFactor);
}

} // namespace toolkit
