/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionControllerConfiguration.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "SandboxUtilities.h"
#import "WKWebViewConfiguration.h"
#import <wtf/FileSystem.h>

namespace WebKit {

String WebExtensionControllerConfiguration::createStorageDirectoryPath(std::optional<WTF::UUID> identifier)
{
    String libraryPath = [NSFileManager.defaultManager URLForDirectory:NSLibraryDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:NO error:nullptr].path;
    RELEASE_ASSERT(!libraryPath.isEmpty());

    String identifierPath = identifier ? identifier->toString().convertToASCIIUppercase() : "Default"_s;

    if (processHasContainer())
        return FileSystem::pathByAppendingComponents(libraryPath, std::initializer_list<StringView>({ "WebKit"_s, "WebExtensions"_s, identifierPath }));

    String appDirectoryName = NSBundle.mainBundle.bundleIdentifier ?: NSProcessInfo.processInfo.processName;
    return FileSystem::pathByAppendingComponents(libraryPath, std::initializer_list<StringView>({ "WebKit"_s, appDirectoryName, "WebExtensions"_s, identifierPath }));
}

String WebExtensionControllerConfiguration::createTemporaryStorageDirectoryPath()
{
    return FileSystem::createTemporaryDirectory(@"WebExtensions");
}

Ref<WebExtensionControllerConfiguration> WebExtensionControllerConfiguration::copy() const
{
    RefPtr<WebExtensionControllerConfiguration> result;

    if (m_identifier)
        result = create(m_identifier.value());
    else if (storageIsTemporary())
        result = createTemporary();
    else if (storageIsPersistent())
        result = createDefault();
    else
        result = createNonPersistent();

    result->setStorageDirectory(storageDirectory());
    result->setWebViewConfiguration([m_webViewConfiguration copy]);
    result->setDefaultWebsiteDataStore(m_defaultWebsiteDataStore.get());

    return result.releaseNonNull();
}

WKWebViewConfiguration *WebExtensionControllerConfiguration::webViewConfiguration()
{
    if (!m_webViewConfiguration)
        m_webViewConfiguration = [[WKWebViewConfiguration alloc] init];
    return m_webViewConfiguration.get();
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
