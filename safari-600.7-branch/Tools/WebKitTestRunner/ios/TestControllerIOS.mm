/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "TestController.h"

#import "CrashReporterInfo.h"
#import "PlatformWebView.h"
#import "TestInvocation.h"
#import <WebKit/WKStringCF.h>
#include <wtf/MainThread.h>

namespace WTR {

void TestController::notifyDone()
{
}

void TestController::platformInitialize()
{
}

void TestController::platformDestroy()
{
}

void TestController::initializeInjectedBundlePath()
{
    NSString *nsBundlePath = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"WebKitTestRunnerInjectedBundle.bundle"];
    m_injectedBundlePath.adopt(WKStringCreateWithCFString((CFStringRef)nsBundlePath));
}

void TestController::initializeTestPluginDirectory()
{
    m_testPluginDirectory.adopt(WKStringCreateWithCFString((CFStringRef)[[NSBundle mainBundle] bundlePath]));
}

void TestController::platformWillRunTest(const TestInvocation& testInvocation)
{
    setCrashReportApplicationSpecificInformationToURL(testInvocation.url());
}

static bool shouldMakeViewportFlexible(const char* pathOrURL)
{
    return strstr(pathOrURL, "viewport/");
}

void TestController::platformConfigureViewForTest(const TestInvocation& test)
{
    if (shouldMakeViewportFlexible(test.pathOrURL())) {
        const unsigned phoneViewHeight = 480;
        const unsigned phoneViewWidth = 320;

        mainWebView()->resizeTo(phoneViewWidth, phoneViewHeight);
        // FIXME: more viewport config to do here.
    }
}

void TestController::platformRunUntil(bool& done, double timeout)
{
    NSDate *endDate = (timeout > 0) ? [NSDate dateWithTimeIntervalSinceNow:timeout] : [NSDate distantFuture];

    while (!done && [endDate compare:[NSDate date]] == NSOrderedDescending)
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:endDate];
}

void TestController::platformInitializeContext()
{
}

void TestController::runModal(PlatformWebView* view)
{
    UIWindow *window = [view->platformView() window];
    if (!window)
        return;
    // FIXME: how to perform on iOS?
//    [[UIApplication sharedApplication] runModalForWindow:window];
}

const char* TestController::platformLibraryPathForTesting()
{
    return [[@"~/Library/Application Support/WebKitTestRunner" stringByExpandingTildeInPath] UTF8String];
}

void TestController::setHidden(bool)
{
    // FIXME: implement for iOS
}

} // namespace WTR
