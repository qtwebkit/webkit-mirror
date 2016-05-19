/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "PlatformScreen.h"

#import "CoreGraphicsSPI.h"
#import "FloatRect.h"
#import "FrameView.h"
#import "HostWindow.h"

#import <ColorSync/ColorSync.h>

extern "C" {
bool CGDisplayUsesInvertedPolarity(void);
bool CGDisplayUsesForceToGray(void);
}

namespace WebCore {

static PlatformDisplayID displayIDFromScreen(NSScreen *screen)
{
    return (PlatformDisplayID)[[[screen deviceDescription] objectForKey:@"NSScreenNumber"] intValue];
}

// These functions scale between screen and page coordinates because JavaScript/DOM operations
// assume that the screen and the page share the same coordinate system.

static PlatformDisplayID displayFromWidget(Widget* widget)
{
    if (!widget)
        return 0;

    FrameView* view = widget->root();
    if (!view)
        return 0;

    return view->hostWindow()->displayID();
}

static NSScreen *screenForWidget(Widget* widget, NSWindow *window)
{
    // Widget is in an NSWindow, use its screen.
    if (window)
        return screenForWindow(window);

    // Didn't get an NSWindow; probably WebKit2. Try using the Widget's display ID.
    if (NSScreen *screen = screenForDisplayID(displayFromWidget(widget)))
        return screen;

    // Widget's window is offscreen, or no screens. Fall back to the first screen if available.
    return screenForWindow(nil);
}

int screenDepth(Widget* widget)
{
    NSWindow *window = widget ? [widget->platformWidget() window] : nil;
    NSScreen *screen = screenForWidget(widget, window);
    return NSBitsPerPixelFromDepth(screen.depth);
}

int screenDepthPerComponent(Widget* widget)
{
    NSWindow *window = widget ? [widget->platformWidget() window] : nil;
    NSScreen *screen = screenForWidget(widget, window);
    return NSBitsPerSampleFromDepth(screen.depth);
}

bool screenIsMonochrome(Widget*)
{
    // At the moment this is a system-wide accessibility setting,
    // so we don't need to check the screen we're using.
    return CGDisplayUsesForceToGray();
}

bool screenHasInvertedColors()
{
    // At the moment this is a system-wide accessibility setting,
    // so we don't need to check the screen we're using.
    return CGDisplayUsesInvertedPolarity();
}

FloatRect screenRect(Widget* widget)
{
    NSWindow *window = widget ? [widget->platformWidget() window] : nil;
    NSScreen *screen = screenForWidget(widget, window);
    return toUserSpace([screen frame], window);
}

FloatRect screenAvailableRect(Widget* widget)
{
    NSWindow *window = widget ? [widget->platformWidget() window] : nil;
    NSScreen *screen = screenForWidget(widget, window);
    return toUserSpace([screen visibleFrame], window);
}

NSScreen *screenForWindow(NSWindow *window)
{
    NSScreen *screen = [window screen]; // nil if the window is off-screen
    if (screen)
        return screen;

    NSArray *screens = [NSScreen screens];
    if ([screens count] > 0)
        return [screens objectAtIndex:0]; // screen containing the menubar

    return nil;
}

NSScreen *screenForDisplayID(PlatformDisplayID displayID)
{
    for (NSScreen *screen in [NSScreen screens]) {
        if (displayIDFromScreen(screen) == displayID)
            return screen;
    }
    return nil;
}

bool screenSupportsExtendedColor(Widget* widget)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED < 101100
    UNUSED_PARAM(widget);
    return false;
#else
    if (!widget)
        return false;

    NSWindow *window = [widget->platformWidget() window];
    NSScreen *screen = screenForWidget(widget, window);
    CGColorSpaceRef colorSpace = screen.colorSpace.CGColorSpace;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
    return CGColorSpaceIsWideGamutRGB(colorSpace);
#else
    bool isWideGamut = false;
    RetainPtr<CFDataRef> iccData = adoptCF(CGColorSpaceCopyICCProfile(colorSpace));
    RetainPtr<ColorSyncProfileRef> profile = adoptCF(ColorSyncProfileCreate(iccData.get(), NULL));
    if (profile)
        isWideGamut = ColorSyncProfileIsWideGamut(profile.get());
    return isWideGamut;
#endif
#endif
}

FloatRect toUserSpace(const NSRect& rect, NSWindow *destination)
{
    FloatRect userRect = rect;
    userRect.setY(NSMaxY([screenForWindow(destination) frame]) - (userRect.y() + userRect.height())); // flip
    return userRect;
}

NSRect toDeviceSpace(const FloatRect& rect, NSWindow *source)
{
    FloatRect deviceRect = rect;
    deviceRect.setY(NSMaxY([screenForWindow(source) frame]) - (deviceRect.y() + deviceRect.height())); // flip
    return deviceRect;
}

NSPoint flipScreenPoint(const NSPoint& screenPoint, NSScreen *screen)
{
    NSPoint flippedPoint = screenPoint;
    flippedPoint.y = NSMaxY([screen frame]) - flippedPoint.y;
    return flippedPoint;
}

} // namespace WebCore
