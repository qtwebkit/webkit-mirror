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

#import "PlatformUtilities.h"
#import "TCPServer.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebsiteDataRecordPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/text/WTFString.h>

static bool readyToContinue;

static RetainPtr<NSURLCredential> persistentCredential;
static bool usePersistentCredentialStorage = false;

@interface NavigationTestDelegate : NSObject <WKNavigationDelegate>
@end

@implementation NavigationTestDelegate {
    bool _hasFinishedNavigation;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    
    _hasFinishedNavigation = false;
    
    return self;
}

- (void)waitForDidFinishNavigation
{
    TestWebKitAPI::Util::run(&_hasFinishedNavigation);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    _hasFinishedNavigation = true;
}

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))completionHandler
{
    static bool firstChallenge = true;
    if (firstChallenge) {
        firstChallenge = false;
        persistentCredential = adoptNS([[NSURLCredential alloc] initWithUser:@"username" password:@"password" persistence:(usePersistentCredentialStorage ? NSURLCredentialPersistencePermanent: NSURLCredentialPersistenceForSession)]);
        return completionHandler(NSURLSessionAuthChallengeUseCredential, persistentCredential.get());
    }
    return completionHandler(NSURLSessionAuthChallengeUseCredential, nil);
}
@end

namespace TestWebKitAPI {

TEST(WKWebsiteDataStore, RemoveAndFetchData)
{
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] fetchDataRecordsOfTypes:[WKWebsiteDataStore _allWebsiteDataTypesIncludingPrivate] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        ASSERT_EQ(0u, dataRecords.count);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

TEST(WKWebsiteDataStore, RemoveEphemeralData)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    __block bool done = false;
    [[configuration websiteDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler: ^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

static void respondWithChallengeThenOK(int socket)
{
    char readBuffer[1000];
    auto bytesRead = ::read(socket, readBuffer, sizeof(readBuffer));
    EXPECT_GT(bytesRead, 0);
    EXPECT_TRUE(static_cast<size_t>(bytesRead) < sizeof(readBuffer));
    
    const char* challengeHeader =
    "HTTP/1.1 401 Unauthorized\r\n"
    "Date: Sat, 23 Mar 2019 06:29:01 GMT\r\n"
    "Content-Length: 0\r\n"
    "WWW-Authenticate: Basic realm=\"testrealm\"\r\n\r\n";
    auto bytesWritten = ::write(socket, challengeHeader, strlen(challengeHeader));
    EXPECT_EQ(static_cast<size_t>(bytesWritten), strlen(challengeHeader));
    
    bytesRead = ::read(socket, readBuffer, sizeof(readBuffer));
    EXPECT_GT(bytesRead, 0);
    EXPECT_TRUE(static_cast<size_t>(bytesRead) < sizeof(readBuffer));
    
    const char* responseHeader =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 13\r\n\r\n"
    "Hello, World!";
    bytesWritten = ::write(socket, responseHeader, strlen(responseHeader));
    EXPECT_EQ(static_cast<size_t>(bytesWritten), strlen(responseHeader));
}
    
TEST(WKWebsiteDataStore, FetchNonPersistentCredentials)
{
    TCPServer server(respondWithChallengeThenOK);
    
    usePersistentCredentialStorage = false;
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    [configuration setWebsiteDataStore:websiteDataStore];
    auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        int credentialCount = dataRecords.count;
        ASSERT_EQ(credentialCount, 1);
        for (WKWebsiteDataRecord *record in dataRecords)
            ASSERT_TRUE([[record displayName] isEqualToString:@"127.0.0.1"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKWebsiteDataStore, FetchPersistentCredentials)
{
    TCPServer server(respondWithChallengeThenOK);
    
    usePersistentCredentialStorage = true;
    auto websiteDataStore = [WKWebsiteDataStore defaultDataStore];
    auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        int credentialCount = dataRecords.count;
        ASSERT_GT(credentialCount, 0);
        bool foundExpectedRecord = false;
        for (WKWebsiteDataRecord *record in dataRecords) {
            auto name = [record displayName];
            if ([name isEqualToString:@"127.0.0.1"]) {
                foundExpectedRecord = true;
                break;
            }
        }
        EXPECT_TRUE(foundExpectedRecord);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    
    __block bool removedCredential = false;
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        [websiteDataStore removeDataOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] forDataRecords:dataRecords completionHandler:^(void) {
            removedCredential = true;
        }];
    }];
    TestWebKitAPI::Util::run(&removedCredential);
}

TEST(WKWebsiteDataStore, RemovePersistentCredentials)
{
    TCPServer server(respondWithChallengeThenOK);

    usePersistentCredentialStorage = true;
    auto websiteDataStore = [WKWebsiteDataStore defaultDataStore];
    auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    __block RetainPtr<WKWebsiteDataRecord> expectedRecord;
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        int credentialCount = dataRecords.count;
        ASSERT_GT(credentialCount, 0);
        for (WKWebsiteDataRecord *record in dataRecords) {
            auto name = [record displayName];
            if ([name isEqualToString:@"127.0.0.1"]) {
                expectedRecord = record;
                break;
            }
        }
        EXPECT_TRUE(expectedRecord);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [websiteDataStore removeDataOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] forDataRecords:[NSArray arrayWithObject:expectedRecord.get()] completionHandler:^(void) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        bool foundLocalHostRecord = false;
        for (WKWebsiteDataRecord *record in dataRecords) {
            auto name = [record displayName];
            if ([name isEqualToString:@"127.0.0.1"]) {
                foundLocalHostRecord = true;
                break;
            }
        }
        EXPECT_FALSE(foundLocalHostRecord);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WKWebsiteDataStore, RemoveNonPersistentCredentials)
{
    TCPServer server(respondWithChallengeThenOK);

    usePersistentCredentialStorage = false;
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    auto websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    [configuration setWebsiteDataStore:websiteDataStore];
    auto navigationDelegate = adoptNS([[NavigationTestDelegate alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", server.port()]]]];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool done = false;
    __block RetainPtr<WKWebsiteDataRecord> expectedRecord;
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        int credentialCount = dataRecords.count;
        ASSERT_GT(credentialCount, 0);
        for (WKWebsiteDataRecord *record in dataRecords) {
            auto name = [record displayName];
            if ([name isEqualToString:@"127.0.0.1"]) {
                expectedRecord = record;
                break;
            }
        }
        EXPECT_TRUE(expectedRecord);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [websiteDataStore removeDataOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] forDataRecords:[NSArray arrayWithObject:expectedRecord.get()] completionHandler:^(void) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [websiteDataStore fetchDataRecordsOfTypes:[NSSet setWithObject:_WKWebsiteDataTypeCredentials] completionHandler:^(NSArray<WKWebsiteDataRecord *> *dataRecords) {
        bool foundLocalHostRecord = false;
        for (WKWebsiteDataRecord *record in dataRecords) {
            auto name = [record displayName];
            if ([name isEqualToString:@"127.0.0.1"]) {
                foundLocalHostRecord = true;
                break;
            }
        }
        EXPECT_FALSE(foundLocalHostRecord);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

}
