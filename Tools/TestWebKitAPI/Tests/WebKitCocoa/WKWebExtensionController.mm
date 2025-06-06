/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#import "HTTPServer.h"
#import "TestCocoa.h"
#import "TestNavigationDelegate.h"
#import "WebExtensionUtilities.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebExtensionContextPrivate.h>
#import <WebKit/WKWebExtensionControllerPrivate.h>
#import <WebKit/WKWebExtensionPrivate.h>

namespace TestWebKitAPI {

TEST(WKWebExtensionController, Configuration)
{
    WKWebExtensionController *testController = [[WKWebExtensionController alloc] init];
    EXPECT_TRUE(testController.configuration.persistent);
    EXPECT_NULL(testController.configuration.identifier);

    testController = [[WKWebExtensionController alloc] initWithConfiguration:WKWebExtensionControllerConfiguration.nonPersistentConfiguration];
    EXPECT_FALSE(testController.configuration.persistent);
    EXPECT_NULL(testController.configuration.identifier);

    NSUUID *identifier = [NSUUID UUID];
    WKWebExtensionControllerConfiguration *configuration = [WKWebExtensionControllerConfiguration configurationWithIdentifier:identifier];

    testController = [[WKWebExtensionController alloc] initWithConfiguration:configuration];
    EXPECT_TRUE(testController.configuration.persistent);
    EXPECT_NS_EQUAL(testController.configuration.identifier, identifier);
}

TEST(WKWebExtensionController, LoadingAndUnloadingContexts)
{
    WKWebExtensionController *testController = [[WKWebExtensionController alloc] initWithConfiguration:WKWebExtensionControllerConfiguration.nonPersistentConfiguration];
    NSError *error;

    EXPECT_EQ(testController.extensions.count, 0ul);
    EXPECT_EQ(testController.extensionContexts.count, 0ul);

#if TARGET_OS_IPHONE
    WKWebExtension *invalidPersistenceExtension = [[WKWebExtension alloc] _initWithManifestDictionary:@{ @"manifest_version": @2, @"name": @"Invalid Persistence", @"description": @"Invalid Persistence", @"version": @"1.0", @"background": @{ @"page": @"background.html", @"persistent": @YES } }];
    WKWebExtensionContext *invalidPersistenceContext = [[WKWebExtensionContext alloc] initForExtension:invalidPersistenceExtension];

    EXPECT_FALSE(invalidPersistenceContext.loaded);
    EXPECT_FALSE([testController loadExtensionContext:invalidPersistenceContext error:&error]);
    EXPECT_NOT_NULL(error);
    EXPECT_NS_EQUAL(error.domain, WKWebExtensionErrorDomain);
    EXPECT_EQ(error.code, WKWebExtensionErrorInvalidBackgroundPersistence);

    EXPECT_EQ(testController.extensions.count, 0ul);
    EXPECT_EQ(testController.extensionContexts.count, 0ul);
#endif // TARGET_OS_IPHONE

    WKWebExtension *testExtensionOne = [[WKWebExtension alloc] _initWithManifestDictionary:@{ @"manifest_version": @2, @"name": @"Test One", @"description": @"Test One", @"version": @"1.0" }];
    WKWebExtensionContext *testContextOne = [[WKWebExtensionContext alloc] initForExtension:testExtensionOne];

    EXPECT_EQ(testExtensionOne.errors.count, 0ul);
    EXPECT_FALSE(testContextOne.loaded);
    EXPECT_NULL([testController extensionContextForExtension:testExtensionOne]);

    WKWebExtension *testExtensionTwo = [[WKWebExtension alloc] _initWithManifestDictionary:@{ @"manifest_version": @2, @"name": @"Test Two", @"description": @"Test Two", @"version": @"1.0" }];
    WKWebExtensionContext *testContextTwo = [[WKWebExtensionContext alloc] initForExtension:testExtensionTwo];

    EXPECT_EQ(testExtensionTwo.errors.count, 0ul);
    EXPECT_FALSE(testContextTwo.loaded);
    EXPECT_NULL([testController extensionContextForExtension:testExtensionTwo]);

    EXPECT_TRUE([testController loadExtensionContext:testContextOne error:&error]);
    EXPECT_NULL(error);

    EXPECT_EQ(testExtensionOne.errors.count, 0ul);
    EXPECT_TRUE(testContextOne.loaded);

    EXPECT_EQ(testController.extensions.count, 1ul);
    EXPECT_EQ(testController.extensionContexts.count, 1ul);

    EXPECT_FALSE([testController loadExtensionContext:testContextOne error:&error]);
    EXPECT_NOT_NULL(error);
    EXPECT_NS_EQUAL(error.domain, WKWebExtensionContextErrorDomain);
    EXPECT_EQ(error.code, WKWebExtensionContextErrorAlreadyLoaded);

    EXPECT_EQ(testExtensionOne.errors.count, 0ul);
    EXPECT_TRUE(testContextOne.loaded);

    EXPECT_EQ(testController.extensions.count, 1ul);
    EXPECT_EQ(testController.extensionContexts.count, 1ul);

    EXPECT_TRUE([testController loadExtensionContext:testContextTwo error:&error]);
    EXPECT_NULL(error);

    EXPECT_EQ(testExtensionTwo.errors.count, 0ul);
    EXPECT_TRUE(testContextTwo.loaded);

    EXPECT_EQ(testController.extensions.count, 2ul);
    EXPECT_EQ(testController.extensionContexts.count, 2ul);

    EXPECT_TRUE([testController unloadExtensionContext:testContextOne error:&error]);
    EXPECT_NULL(error);

    EXPECT_EQ(testExtensionTwo.errors.count, 0ul);
    EXPECT_FALSE(testContextOne.loaded);

    EXPECT_EQ(testController.extensions.count, 1ul);
    EXPECT_EQ(testController.extensionContexts.count, 1ul);

    EXPECT_TRUE([testController unloadExtensionContext:testContextTwo error:&error]);
    EXPECT_NULL(error);

    EXPECT_EQ(testExtensionTwo.errors.count, 0ul);
    EXPECT_FALSE(testContextTwo.loaded);

    EXPECT_EQ(testController.extensions.count, 0ul);
    EXPECT_EQ(testController.extensionContexts.count, 0ul);

    EXPECT_FALSE([testController unloadExtensionContext:testContextOne error:&error]);
    EXPECT_NOT_NULL(error);
    EXPECT_NS_EQUAL(error.domain, WKWebExtensionContextErrorDomain);
    EXPECT_EQ(error.code, WKWebExtensionContextErrorNotLoaded);

    EXPECT_EQ(testExtensionTwo.errors.count, 0ul);
    EXPECT_FALSE(testContextOne.loaded);
}

TEST(WKWebExtensionController, BackgroundPageLoading)
{
    NSDictionary *resources = @{
        @"background.html": @"<body>Hello world!</body>",
        @"background.js": @"console.log('Hello World!')"
    };

    NSMutableDictionary *manifest = [@{ @"manifest_version": @2, @"name": @"Test One", @"description": @"Test One", @"version": @"1.0", @"background": @{ @"page": @"background.html", @"persistent": @NO } } mutableCopy];

    WKWebExtension *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources];
    WKWebExtensionContext *testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
    WKWebExtensionController *testController = [[WKWebExtensionController alloc] initWithConfiguration:WKWebExtensionControllerConfiguration.nonPersistentConfiguration];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    NSError *error;
    EXPECT_TRUE([testController loadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    // Wait for the background to load.
    TestWebKitAPI::Util::runFor(4_s);

    // No errors means success.
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    EXPECT_TRUE([testController unloadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    manifest[@"background"] = @{ @"service_worker": @"background.js" };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    EXPECT_TRUE([testController loadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    // Wait for the background to load.
    TestWebKitAPI::Util::runFor(4_s);

    // No errors means success.
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    EXPECT_TRUE([testController unloadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    [WKWebExtensionMatchPattern registerCustomURLScheme:@"test-extension"];
    testContext.baseURL = [NSURL URLWithString:@"test-extension://aaabbbcccddd"];

    EXPECT_TRUE([testController loadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    // Wait for the background to load.
    TestWebKitAPI::Util::runFor(4_s);

    // No errors means success.
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    EXPECT_TRUE([testController unloadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
}

TEST(WKWebExtensionController, BackgroundPageWithModulesLoading)
{
    NSDictionary *resources = @{
        @"main.js": @"import { x } from './exports.js'; x;",
        @"exports.js": @"const x = 805; export { x };",
    };

    NSMutableDictionary *manifest = [@{ @"manifest_version": @2, @"name": @"Test One", @"description": @"Test One", @"version": @"1.0", @"background": @{ @"scripts": @[ @"main.js", @"exports.js" ], @"type": @"module", @"persistent": @NO } } mutableCopy];

    WKWebExtension *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources];
    WKWebExtensionContext *testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
    WKWebExtensionController *testController = [[WKWebExtensionController alloc] initWithConfiguration:WKWebExtensionControllerConfiguration.nonPersistentConfiguration];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    NSError *error;
    EXPECT_TRUE([testController loadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    // Wait for the background to load.
    TestWebKitAPI::Util::runFor(4_s);

    // No errors means success.
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    EXPECT_TRUE([testController unloadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    manifest[@"background"] = @{ @"service_worker": @"main.js", @"type": @"module" };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    EXPECT_TRUE([testController loadExtensionContext:testContext error:&error]);
    EXPECT_NULL(error);

    // Wait for the background to load.
    TestWebKitAPI::Util::runFor(4_s);

    // No errors means success.
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
}

TEST(WKWebExtensionController, BackgroundWithServiceWorkerPreferredEnvironment)
{
    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",

        @"background": @{
            @"preferred_environment": @[ @"service_worker", @"document" ],
            @"service_worker": @"service_worker.js",
            @"scripts": @[ @"background.js" ],
            @"page": @"background.html"
        }
    };

    auto *serviceWorkerScript = Util::constructScript(@[
        @"browser.test.assertTrue('ServiceWorkerGlobalScope' in self && self instanceof ServiceWorkerGlobalScope, 'Global scope should be ServiceWorkerGlobalScope');",

        @"browser.test.notifyPass()"
    ]);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.notifyFail('This background script should not be used')"
    ]);

    auto *resources = @{
        @"service_worker.js": serviceWorkerScript,
        @"background.js": backgroundScript,
        @"background.html": @"<script src='background.js'></script>",
    };

    Util::loadAndRunExtension(manifest, resources);
}

TEST(WKWebExtensionController, BackgroundWithPageDocumentPreferredEnvironment)
{
    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",

        @"background": @{
            @"preferred_environment": @[ @"document", @"service_worker" ],
            @"service_worker": @"service_worker.js",
            @"scripts": @[ @"other-background.js" ],
            @"page": @"background.html"
        }
    };

    auto *serviceWorkerScript = Util::constructScript(@[
        @"browser.test.notifyFail('Service worker should not be used')"
    ]);

    auto *notUsedbackgroundScript = Util::constructScript(@[
        @"browser.test.notifyFail('This background script should not be used')"
    ]);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertTrue('Window' in self && self instanceof Window, 'Global scope should be Window')",

        @"browser.test.notifyPass()"
    ]);

    auto *resources = @{
        @"service_worker.js": serviceWorkerScript,
        @"other-background.js": notUsedbackgroundScript,
        @"background.js": backgroundScript,
        @"background.html": @"<script src='background.js'></script>",
    };

    Util::loadAndRunExtension(manifest, resources);
}

TEST(WKWebExtensionController, BackgroundWithScriptsDocumentPreferredEnvironment)
{
    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",

        @"background": @{
            @"preferred_environment": @"document",
            @"scripts": @[ @"background.js" ]
        }
    };

    auto *serviceWorkerScript = Util::constructScript(@[
        @"browser.test.notifyFail('Service worker should not be used')"
    ]);

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertTrue('Window' in self && self instanceof Window, 'Global scope should be Window')",

        @"browser.test.notifyPass()"
    ]);

    auto *resources = @{
        @"service_worker.js": serviceWorkerScript,
        @"background.js": backgroundScript,
    };

    Util::loadAndRunExtension(manifest, resources);
}

TEST(WKWebExtensionController, BackgroundWithMultipleDocumentModuleScripts)
{
    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",

        @"background": @{
            @"preferred_environment": @"document",
            @"scripts": @[ @"module1.js", @"module2.js" ],
            @"type": @"module"
        }
    };

    auto *module1 = Util::constructScript(@[
        @"self.testValue = 'Test value set in Module 1';"
    ]);

    auto *module2 = Util::constructScript(@[
        @"import { valueFromModule3 } from './module3.js'",

        @"browser.test.assertEq(self.testValue, 'Test value set in Module 1', 'Module 1 value should be accessible')",
        @"browser.test.assertEq(valueFromModule3, 'Value from Module 3', 'Value from Module 3 should be accessible')",

        @"browser.test.notifyPass();"
    ]);

    auto *module3 = Util::constructScript(@[
        @"export const valueFromModule3 = 'Value from Module 3';"
    ]);

    auto *resources = @{
        @"module1.js": module1,
        @"module2.js": module2,
        @"module3.js": module3,
    };

    Util::loadAndRunExtension(manifest, resources);
}

TEST(WKWebExtensionController, BackgroundWithMultipleServiceWorkerScripts)
{
    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",

        @"background": @{
            @"preferred_environment": @"service_worker",
            @"scripts": @[ @"script1.js", @"script2.js" ]
        }
    };

    auto *script1 = Util::constructScript(@[
        @"self.testValue = 'Test value set in Script 1'"
    ]);

    auto *script2 = Util::constructScript(@[
        @"browser.test.assertEq(self.testValue, 'Test value set in Script 1')",

        @"browser.test.notifyPass()"
    ]);

    auto *resources = @{
        @"script1.js": script1,
        @"script2.js": script2,
    };

    Util::loadAndRunExtension(manifest, resources);
}

TEST(WKWebExtensionController, BackgroundWithMultipleServiceWorkerModuleScripts)
{
    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",

        @"background": @{
            @"preferred_environment": @"service_worker",
            @"scripts": @[ @"module1.js", @"module2.js" ],
            @"type": @"module"
        }
    };

    auto *module1 = Util::constructScript(@[
        @"self.testValue = 'Test value set in Module 1'"
    ]);

    auto *module2 = Util::constructScript(@[
        @"import { valueFromModule3 } from './module3.js'",

        @"browser.test.assertEq(self.testValue, 'Test value set in Module 1')",
        @"browser.test.assertEq(valueFromModule3, 'Value from Module 3')",

        @"browser.test.notifyPass()"
    ]);

    auto *module3 = Util::constructScript(@[
        @"export const valueFromModule3 = 'Value from Module 3'"
    ]);

    auto *resources = @{
        @"module1.js": module1,
        @"module2.js": module2,
        @"module3.js": module3,
    };

    Util::loadAndRunExtension(manifest, resources);
}

TEST(WKWebExtensionController, ContentScriptLoading)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "Hello World"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Https);

    auto *manifest = @{ @"manifest_version": @3, @"content_scripts": @[ @{ @"js": @[ @"content.js" ], @"matches": @[ @"*://localhost/*" ] } ] };

    auto *contentScript = Util::constructScript(@[
        // Exposed to content scripts
        @"browser.test.assertEq(typeof browser.runtime.id, 'string')",
        @"browser.test.assertEq(typeof browser.runtime.getManifest(), 'object')",
        @"browser.test.assertEq(typeof browser.runtime.getURL(''), 'string')",

        // Not exposed to content scripts
        @"browser.test.assertEq(browser.runtime.getPlatformInfo, undefined)",
        @"browser.test.assertEq(browser.runtime.lastError, undefined)",

        // Finish
        @"browser.test.notifyPass()"
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"content.js": contentScript });

    WKWebExtensionMatchPattern *matchPattern = [WKWebExtensionMatchPattern matchPatternWithString:@"*://localhost/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().webExtensionController = manager.get().controller;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration.get()]);
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);

    navigationDelegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    };

    webView.get().navigationDelegate = navigationDelegate.get();

    [webView loadRequest:server.requestWithLocalhost()];

    [manager run];
}

TEST(WKWebExtensionController, CSSUserOrigin)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<body style='color: red'></body>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",

        @"content_scripts": @[ @{
            @"matches": @[ @"*://localhost/*" ],
            @"css": @[ @"style.css" ],
            @"js": @[ @"content.js" ],
            @"css_origin": @"user"
        } ]
    };

    auto *styleSheet = @"body { color: green !important }";

    auto *contentScript = Util::constructScript(@[
        @"let computedColor = window.getComputedStyle(document.body).color",
        @"browser.test.assertEq(computedColor, 'rgb(0, 128, 0)', 'Color should be green')",

        @"browser.test.notifyPass()",
    ]);

    auto resources = @{
        @"style.css": styleSheet,
        @"content.js": contentScript
    };

    auto manager = Util::loadExtension(manifest, resources);

    auto *urlRequest = server.requestWithLocalhost();

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionController, CSSAuthorOrigin)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<style> body { color: red } </style>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",

        @"content_scripts": @[ @{
            @"matches": @[ @"*://localhost/*" ],
            @"css": @[ @"style.css" ],
            @"js": @[ @"content.js" ],
            @"css_origin": @"author"
        } ]
    };

    auto *styleSheet = @"body { color: green !important }";

    auto *contentScript = Util::constructScript(@[
        @"let computedColor = getComputedStyle(document.body).color",
        @"browser.test.assertEq(computedColor, 'rgb(0, 128, 0)', 'Color should be green')",

        @"browser.test.notifyPass()",
    ]);

    auto resources = @{
        @"style.css": styleSheet,
        @"content.js": contentScript
    };

    auto manager = Util::loadExtension(manifest, resources);

    auto *urlRequest = server.requestWithLocalhost();

    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionController, ContentSecurityPolicyV2BlockingImageLoad)
{
    TestWebKitAPI::HTTPServer server({
        { "/image.svg"_s, { { { "Content-Type"_s, "image/svg+xml"_s } }, "<svg xmlns='http://www.w3.org/2000/svg'></svg>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();

    auto *backgroundScript = Util::constructScript(@[
        @"var img = document.createElement('img')",
        [NSString stringWithFormat:@"img.src = '%@image.svg'", urlRequest.URL.absoluteString],

        @"img.onerror = () => {",
        @"  browser.test.notifyPass()",
        @"}",

        @"img.onload = () => {",
        @"  browser.test.notifyFail('The image should not load')",
        @"}",

        @"document.body.appendChild(img)"
    ]);

    auto *manifest = @{
        @"manifest_version": @2,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"content_security_policy": @"script-src 'self'; img-src 'none'"
    };

    Util::loadAndRunExtension(manifest, @{
        @"background.js": backgroundScript
    });
}

TEST(WKWebExtensionController, ContentSecurityPolicyV3BlockingImageLoad)
{
    TestWebKitAPI::HTTPServer server({
        { "/image.svg"_s, { { { "Content-Type"_s, "image/svg+xml"_s } }, "<svg xmlns='http://www.w3.org/2000/svg'></svg>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *urlRequest = server.requestWithLocalhost();

    auto *backgroundScript = Util::constructScript(@[
        @"var img = document.createElement('img')",
        [NSString stringWithFormat:@"img.src = '%@image.svg'", urlRequest.URL.absoluteString],

        @"img.onerror = () => {",
        @"  browser.test.notifyPass()",
        @"}",

        @"img.onload = () => {",
        @"  browser.test.notifyFail('The image should not load')",
        @"}",

        @"document.body.appendChild(img)"
    ]);

    auto *manifest = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"content_security_policy": @{
            @"extension_pages": @"script-src 'self'; img-src 'none'"
        }
    };

    Util::loadAndRunExtension(manifest, @{
        @"background.js": backgroundScript
    });
}

TEST(WKWebExtensionController, WebAccessibleResources)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *contentScript = Util::constructScript(@[
        @"var imgGood = document.createElement('img')",
        @"imgGood.src = browser.runtime.getURL('good.svg')",

        @"var imgBad = document.createElement('img')",
        @"imgBad.src = browser.runtime.getURL('bad.svg')",

        @"var goodLoaded = false",
        @"var badFailed = false",

        @"imgGood.onload = () => {",
        @"  goodLoaded = true",
        @"  if (badFailed)",
        @"    browser.test.notifyPass()",
        @"}",

        @"imgGood.onerror = () => {",
        @"  browser.test.notifyFail('The good image should load')",
        @"}",

        @"imgBad.onload = () => {",
        @"  browser.test.notifyFail('The bad image should not load')",
        @"}",

        @"imgBad.onerror = () => {",
        @"  badFailed = true",
        @"  if (goodLoaded)",
        @"    browser.test.notifyPass()",
        @"}",

        @"document.body.appendChild(imgGood)",
        @"document.body.appendChild(imgBad)"
    ]);

    auto *manifest = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",

        @"content_scripts": @[ @{
            @"js": @[ @"content.js" ],
            @"matches": @[ @"*://localhost/*" ],
        } ],

        @"web_accessible_resources": @[ @{
            @"resources": @[ @"g*.svg" ],
            @"matches": @[ @"*://localhost/*" ]
        } ]
    };

    auto *resources = @{
        @"content.js": contentScript,
        @"good.svg": @"<svg xmlns='http://www.w3.org/2000/svg'></svg>",
        @"bad.svg": @"<svg xmlns='http://www.w3.org/2000/svg'></svg>"
    };

    auto manager = Util::loadExtension(manifest, resources);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionController, WebAccessibleResourcesWithLeadingSlash)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *contentScript = Util::constructScript(@[
        @"var img = document.createElement('img')",
        @"img.src = browser.runtime.getURL('img.svg')",

        @"img.onload = () => {",
        @"  browser.test.notifyPass()",
        @"}",

        @"img.onerror = () => {",
        @"  browser.test.notifyFail('The image should load')",
        @"}",

        @"document.body.appendChild(img)"
    ]);

    auto *manifest = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",

        @"content_scripts": @[ @{
            @"js": @[ @"content.js" ],
            @"matches": @[ @"*://localhost/*" ],
        } ],

        @"web_accessible_resources": @[ @{
            @"resources": @[ @"/img.svg" ],
            @"matches": @[ @"*://localhost/*" ]
        } ]
    };

    auto *resources = @{
        @"content.js": contentScript,
        @"img.svg": @"<svg xmlns='http://www.w3.org/2000/svg'></svg>"
    };

    auto manager = Util::loadExtension(manifest, resources);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionController, WebAccessibleResourceInSubframeFromAboutBlank)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *extensionManifest = @{
        @"manifest_version": @3,

        @"name": @"Runtime Test",
        @"description": @"Runtime Test",
        @"version": @"1",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"content_scripts": @[ @{
            @"js": @[ @"content.js" ],
            @"matches": @[ @"*://localhost/*" ],
        } ],

        @"web_accessible_resources": @[ @{
            @"resources": @[ @"*.html" ],
            @"matches": @[ @"*://localhost/*" ]
        } ],
    };

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.sendMessage('Load Tab')",
    ]);

    auto *iframeScript = Util::constructScript(@[
        @"browser.test.notifyPass()",
    ]);

    auto *contentScript = Util::constructScript(@[
        @"(function() {",
        @"  const iframe = document.createElement('iframe')",
        @"  document.documentElement.appendChild(iframe)",
        @"  iframe.contentWindow.location = new URL(browser.runtime.getURL('extension-frame.html')).href",
        @"})()",
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript,
        @"extension-frame.js": iframeScript,
        @"extension-frame.html": @"<script type='module' src='extension-frame.js'></script>",
    };

    auto manager = Util::loadExtension(extensionManifest, resources);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    [manager runUntilTestMessage:@"Load Tab"];

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionController, WebAccessibleResourcesV2)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *contentScript = Util::constructScript(@[
        @"var imgGood = document.createElement('img')",
        @"imgGood.src = browser.runtime.getURL('good.svg')",

        @"var imgBad = document.createElement('img')",
        @"imgBad.src = browser.runtime.getURL('bad.svg')",

        @"var goodLoaded = false",
        @"var badFailed = false",

        @"imgGood.onload = () => {",
        @"  goodLoaded = true",
        @"  if (badFailed)",
        @"    browser.test.notifyPass()",
        @"}",

        @"imgGood.onerror = () => {",
        @"  browser.test.notifyFail('The good image should load')",
        @"}",

        @"imgBad.onload = () => {",
        @"  browser.test.notifyFail('The bad image should not load')",
        @"}",

        @"imgBad.onerror = () => {",
        @"  badFailed = true",
        @"  if (goodLoaded)",
        @"    browser.test.notifyPass()",
        @"}",

        @"document.body.appendChild(imgGood)",
        @"document.body.appendChild(imgBad)"
    ]);

    auto *manifest = @{
        @"manifest_version": @2,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",

        @"content_scripts": @[ @{
            @"js": @[ @"content.js" ],
            @"matches": @[ @"*://localhost/*" ],
        } ],

        @"web_accessible_resources": @[ @"good.svg" ]
    };

    auto *resources = @{
        @"content.js": contentScript,
        @"good.svg": @"<svg xmlns='http://www.w3.org/2000/svg'></svg>",
        @"bad.svg": @"<svg xmlns='http://www.w3.org/2000/svg'></svg>"
    };

    auto manager = Util::loadExtension(manifest, resources);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
