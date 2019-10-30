/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "Test.h"

#if ENABLE(WEB_AUTHN)

#import "PlatformUtilities.h"
#import "TCPServer.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <WebKit/_WKWebAuthenticationPanel.h>
#import <wtf/BlockPtr.h>
#import <wtf/text/StringConcatenateNumbers.h>

static bool webAuthenticationPanelRan = false;
static bool webAuthenticationPanelFailed = false;
static bool webAuthenticationPanelSucceded = false;
static RetainPtr<_WKWebAuthenticationPanel> gPanel;

@interface TestWebAuthenticationPanelDelegate : NSObject <_WKWebAuthenticationPanelDelegate>
@end

@implementation TestWebAuthenticationPanelDelegate

- (void)panel:(_WKWebAuthenticationPanel *)panel dismissWebAuthenticationPanelWithResult:(_WKWebAuthenticationResult)result
{
    ASSERT_NE(panel, nil);
    if (result == _WKWebAuthenticationResultFailed) {
        webAuthenticationPanelFailed = true;
        return;
    }
    if (result == _WKWebAuthenticationResultSucceeded) {
        webAuthenticationPanelSucceded = true;
        return;
    }
}

@end

@interface TestWebAuthenticationPanelUIDelegate : NSObject <WKUIDelegatePrivate>
@property bool isRacy;
- (instancetype)init;
@end

@implementation TestWebAuthenticationPanelUIDelegate {
    RetainPtr<TestWebAuthenticationPanelDelegate> _delegate;
    BlockPtr<void(_WKWebAuthenticationPanelResult)> _callback;
}

- (instancetype)init
{
    if (self = [super init])
        self.isRacy = false;
    return self;
}

- (void)_webView:(WKWebView *)webView runWebAuthenticationPanel:(_WKWebAuthenticationPanel *)panel initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(_WKWebAuthenticationPanelResult))completionHandler
{
    webAuthenticationPanelRan = true;

    _delegate = adoptNS([[TestWebAuthenticationPanelDelegate alloc] init]);
    ASSERT_NE(panel, nil);
    gPanel = panel;
    [gPanel setDelegate:_delegate.get()];

    EXPECT_TRUE([[gPanel relyingPartyID] isEqual:@""] || [[gPanel relyingPartyID] isEqual:@"localhost"]);

    if (_isRacy) {
        if (!_callback) {
            _callback = makeBlockPtr(completionHandler);
            return;
        }
        _callback(_WKWebAuthenticationPanelResultUnavailable);
    }
    completionHandler(_WKWebAuthenticationPanelResultPresented);
}

@end

namespace TestWebKitAPI {

namespace {

const char parentFrame[] = "<html><iframe id='theFrame' src='iFrame.html'></iframe></html>";
const char subFrame[] =
"<html>"
"<input type='text' id='input'>"
"<script>"
"    if (window.internals) {"
"        internals.setMockWebAuthenticationConfiguration({ hid: { expectCancel: true } });"
"        internals.withUserGesture(() => { input.focus(); });"
"    }"
"    const options = {"
"        publicKey: {"
"            challenge: new Uint8Array(16)"
"        }"
"    };"
"    navigator.credentials.get(options);"
"</script>"
"</html>";

static _WKExperimentalFeature *webAuthenticationExperimentalFeature()
{
    static RetainPtr<_WKExperimentalFeature> theFeature;
    if (theFeature)
        return theFeature.get();

    NSArray *features = [WKPreferences _experimentalFeatures];
    for (_WKExperimentalFeature *feature in features) {
        if ([feature.key isEqual:@"WebAuthenticationEnabled"]) {
            theFeature = feature;
            break;
        }
    }
    return theFeature.get();
}

static void reset()
{
    webAuthenticationPanelRan = false;
    webAuthenticationPanelFailed = false;
    webAuthenticationPanelSucceded = false;
    gPanel = nullptr;
}

} // namesapce;

TEST(WebAuthenticationPanel, NoPanelTimeout)
{
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-nfc" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Operation timed out."];
}

TEST(WebAuthenticationPanel, NoPanelHidSuccess)
{
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}

TEST(WebAuthenticationPanel, PanelTimeout)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelFailed);
}

TEST(WebAuthenticationPanel, PanelHidSuccess)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelSucceded);
}

#if HAVE(NEAR_FIELD)
// This test aims to see if the callback for the first ceremony could affect the second one.
// Therefore, the first callback will be held to return at the time when the second arrives.
// The first callback will return _WKWebAuthenticationPanelResultUnavailable which leads to timeout for NFC.
// The second callback will return _WKWebAuthenticationPanelResultPresented which leads to success.
TEST(WebAuthenticationPanel, PanelRacy1)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-nfc" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [delegate setIsRacy:true];
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    [webView waitForMessage:@"Succeeded!"];
}

// Unlike the previous one, this one focuses on the order of the delegate callbacks.
TEST(WebAuthenticationPanel, PanelRacy2)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-nfc" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [delegate setIsRacy:true];
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    webAuthenticationPanelRan = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelFailed);
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelSucceded);
}
#endif // HAVE(NEAR_FIELD)

TEST(WebAuthenticationPanel, PanelTwice)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelSucceded);

    reset();
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    Util::run(&webAuthenticationPanelSucceded);
}

TEST(WebAuthenticationPanel, ReloadHidCancel)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-cancel" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [webView reload];
    Util::run(&webAuthenticationPanelFailed);
}

TEST(WebAuthenticationPanel, LocationChangeHidCancel)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-cancel" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> otherURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [webView evaluateJavaScript: [NSString stringWithFormat:@"location = '%@'", [otherURL absoluteString]] completionHandler:nil];
    Util::run(&webAuthenticationPanelFailed);
}

TEST(WebAuthenticationPanel, NewLoadHidCancel)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-cancel" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    RetainPtr<NSURL> otherURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [webView loadRequest:[NSURLRequest requestWithURL:otherURL.get()]];
    Util::run(&webAuthenticationPanelFailed);
}

TEST(WebAuthenticationPanel, CloseHidCancel)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-cancel" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [webView _close];
    Util::run(&webAuthenticationPanelFailed);
}

TEST(WebAuthenticationPanel, SubFrameChangeLocationHidCancel)
{
    TCPServer server([parentFrame = String(parentFrame), subFrame = String(subFrame)] (int socket) {
        NSString *firstResponse = [NSString stringWithFormat:
            @"HTTP/1.1 200 OK\r\n"
            "Content-Length: %d\r\n\r\n"
            "%@",
            parentFrame.length(),
            (id)parentFrame
        ];
        NSString *secondResponse = [NSString stringWithFormat:
            @"HTTP/1.1 200 OK\r\n"
            "Content-Length: %d\r\n\r\n"
            "%@",
            subFrame.length(),
            (id)subFrame
        ];

        TCPServer::read(socket);
        TCPServer::write(socket, firstResponse.UTF8String, firstResponse.length);
        TCPServer::read(socket);
        TCPServer::write(socket, secondResponse.UTF8String, secondResponse.length);
    });

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:(id)makeString("http://localhost:", static_cast<unsigned>(server.port()))]]];
    Util::run(&webAuthenticationPanelRan);
    [webView evaluateJavaScript:@"theFrame.src = 'simple.html'" completionHandler:nil];
    Util::run(&webAuthenticationPanelFailed);
}

TEST(WebAuthenticationPanel, SubFrameDestructionHidCancel)
{
    TCPServer server([parentFrame = String(parentFrame), subFrame = String(subFrame)] (int socket) {
        NSString *firstResponse = [NSString stringWithFormat:
            @"HTTP/1.1 200 OK\r\n"
            "Content-Length: %d\r\n\r\n"
            "%@",
            parentFrame.length(),
            (id)parentFrame
        ];
        NSString *secondResponse = [NSString stringWithFormat:
            @"HTTP/1.1 200 OK\r\n"
            "Content-Length: %d\r\n\r\n"
            "%@",
            subFrame.length(),
            (id)subFrame
        ];

        TCPServer::read(socket);
        TCPServer::write(socket, firstResponse.UTF8String, firstResponse.length);
        TCPServer::read(socket);
        TCPServer::write(socket, secondResponse.UTF8String, secondResponse.length);
    });

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:(id)makeString("http://localhost:", static_cast<unsigned>(server.port()))]]];
    Util::run(&webAuthenticationPanelRan);
    [webView evaluateJavaScript:@"theFrame.parentNode.removeChild(theFrame)" completionHandler:nil];
    Util::run(&webAuthenticationPanelFailed);
}

TEST(WebAuthenticationPanel, PanelHidCancel)
{
    reset();
    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"web-authentication-get-assertion-hid-cancel" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [[configuration preferences] _setEnabled:YES forExperimentalFeature:webAuthenticationExperimentalFeature()];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect configuration:configuration]);
    auto delegate = adoptNS([[TestWebAuthenticationPanelUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&webAuthenticationPanelRan);
    [gPanel cancel];
    [webView waitForMessage:@"Operation timed out."];
    EXPECT_FALSE(webAuthenticationPanelFailed);
    EXPECT_FALSE(webAuthenticationPanelSucceded);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_AUTHN)
