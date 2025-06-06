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

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestScriptMessageHandler.h"
#import "TestURLSchemeHandler.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)

static void keepNetworkProcessActive()
{
    [WKWebsiteDataStore.defaultDataStore fetchDataRecordsOfTypes:WKWebsiteDataStore.allWebsiteDataTypes completionHandler:^(NSArray<WKWebsiteDataRecord *> *) {
        keepNetworkProcessActive();
    }];
}

// FIXME: Re-enable this test once webkit.org/b/290203 is resolved.
TEST(IndexedDB, DISABLED_IndexedDBSuspendImminently)
{
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    auto handler = adoptNS([TestScriptMessageHandler new]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"IndexedDBSuspendImminently" withExtension:@"html"]];
    [webView loadRequest:request];
    EXPECT_WK_STREQ([handler waitForMessage].body, @"Continue");

    [configuration.get().websiteDataStore _sendNetworkProcessWillSuspendImminently];
    EXPECT_WK_STREQ([handler waitForMessage].body, @"Expected Abort For Suspension");

    keepNetworkProcessActive();
    [configuration.get().websiteDataStore _sendNetworkProcessDidResume];
    EXPECT_WK_STREQ([handler waitForMessage].body, @"Expected Success After Resume");
}

TEST(IndexedDB, SuspendImminentlyForThirdPartyDatabases)
{
    static NSString *mainFrameString = @"<script> \
        function postResult(event) { \
            window.webkit.messageHandlers.testHandler.postMessage(event.data); \
        } \
        addEventListener('message', postResult, false); \
        </script> \
        <iframe src='iframe://'>";

    static const char* iframeBytes = R"TESTRESOURCE(
    <script>
    function postResult(result) {
        if (window.parent != window.top) {
            parent.postMessage(result, '*');
        } else {
            window.webkit.messageHandlers.testHandler.postMessage(result);
        }
    }

    try {
        var request = window.indexedDB.open('IndexedDBSuspendImminentlyForThirdPartyDatabases');
        request.onupgradeneeded = function(event) {
            var db = event.target.result;
            var os = db.createObjectStore('TestObjectStore');
            var transaction = event.target.transaction;
            transaction.onabort = function(event) {
                postResult('transaction is aborted');
            }
            transaction.oncomplete = function(event) {
                postResult('transaction is completed');
            }

            postResult('database is created');

            for (let i = 0; i < 1000; i ++)
                os.put('TestValue', 'TestKey');
        }
        request.onerror = function(event) {
            postResult('database error: ' + event.target.error.name + ' - ' + event.target.error.message);
        }
    } catch(err) {
        postResult('database error: ' + err.name + ' - ' + err.message);
    }
    </script>
    )TESTRESOURCE";

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([TestScriptMessageHandler new]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        auto response = adoptNS([[NSURLResponse alloc] initWithURL:task.request.URL MIMEType:@"text/html" expectedContentLength:0 textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[NSData dataWithBytes:iframeBytes length:strlen(iframeBytes)]];
        [task didFinish];
    }];
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"iframe"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:mainFrameString baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    EXPECT_WK_STREQ([handler waitForMessage].body, @"database is created");

    [configuration.get().websiteDataStore _sendNetworkProcessWillSuspendImminently];
    [configuration.get().websiteDataStore _sendNetworkProcessDidResume];

    EXPECT_WK_STREQ([handler waitForMessage].body, @"transaction is completed");
}

#endif // PLATFORM(IOS_FAMILY)
