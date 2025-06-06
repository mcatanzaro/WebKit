/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "WebPushMessage.h"

#import <wtf/RetainPtr.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {

#define WebKitPushDataKey @"WebKitPushData"
#define WebKitPushRegistrationURLKey @"WebKitPushRegistrationURL"
#define WebKitPushPartitionKey @"WebKitPushPartition"
#define WebKitNotificationPayloadKey @"WebKitNotificationPayload"

std::optional<WebPushMessage> WebPushMessage::fromDictionary(NSDictionary *dictionary)
{
    RetainPtr url = [dictionary objectForKey:WebKitPushRegistrationURLKey];
    if (!url || ![url isKindOfClass:[NSURL class]])
        return std::nullopt;

    RetainPtr<id> pushData = [dictionary objectForKey:WebKitPushDataKey];
    BOOL isNull = [pushData isEqual:[NSNull null]];
    BOOL isData = [pushData isKindOfClass:[NSData class]];

    if (!isNull && !isData)
        return std::nullopt;

    RetainPtr<NSString> pushPartition = [dictionary objectForKey:WebKitPushPartitionKey];
    if (!pushPartition || ![pushPartition isKindOfClass:[NSString class]])
        return std::nullopt;

#if ENABLE(DECLARATIVE_WEB_PUSH)
    RetainPtr<id> payloadDictionary = [dictionary objectForKey:WebKitNotificationPayloadKey];
    isNull = [payloadDictionary isEqual:[NSNull null]];
    BOOL isCorrectType = [payloadDictionary isKindOfClass:[NSDictionary class]];

    if (!isNull && !isCorrectType)
        return std::nullopt;

    std::optional<WebCore::NotificationPayload> payload;
    if (isCorrectType) {
        payload = WebCore::NotificationPayload::fromDictionary(payloadDictionary.get());
        if (!payload)
            return std::nullopt;
    }

    WebPushMessage message { { }, String { pushPartition.get() }, URL { url.get() }, WTFMove(payload) };
#else
    WebPushMessage message { { }, String { pushPartition.get() }, URL { url.get() }, { } };
#endif

    if (isData)
        message.pushData = makeVector((NSData *)pushData.get());

    return message;
}

NSDictionary *WebPushMessage::toDictionary() const
{
    RetainPtr<NSData> nsData;
    if (pushData)
        nsData = nsData = toNSData(pushData->span());

    RetainPtr<NSDictionary> nsPayload;
#if ENABLE(DECLARATIVE_WEB_PUSH)
    if (notificationPayload)
        nsPayload = notificationPayload->dictionaryRepresentation();
#endif

    return @{
        WebKitPushDataKey : nsData ? nsData.get() : [NSNull null],
        WebKitPushRegistrationURLKey : registrationURL.createNSURL().get(),
        WebKitPushPartitionKey : pushPartitionString.createNSString().get(),
        WebKitNotificationPayloadKey : nsPayload ? nsPayload.get() : [NSNull null]
    };
}

} // namespace WebKit
