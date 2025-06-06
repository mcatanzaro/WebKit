/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEB_AUTHN)

#include "APIWebAuthenticationPanel.h"
#include "FrameInfoData.h"
#include "WebAuthenticationFlags.h"
#include "WebPageProxy.h"
#include <WebCore/CredentialRequestOptions.h>
#include <WebCore/GlobalFrameIdentifier.h>
#include <WebCore/PublicKeyCredentialCreationOptions.h>
#include <WebCore/PublicKeyCredentialRequestOptions.h>
#include <WebCore/WebAuthenticationConstants.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class SecurityOriginData;
}

namespace WebKit {

class WebPageProxy;

struct WebAuthenticationRequestData {
    Vector<uint8_t> hash;
    Variant<WebCore::PublicKeyCredentialCreationOptions, WebCore::PublicKeyCredentialRequestOptions> options;

    // FIXME<rdar://problem/71509848>: Remove the following deprecated fields.
    WeakPtr<WebPageProxy> page;
    WebAuthenticationPanelResult panelResult { WebAuthenticationPanelResult::Unavailable };
    RefPtr<API::WebAuthenticationPanel> panel;
    std::optional<WebCore::GlobalFrameIdentifier> globalFrameID;
    std::optional<WebKit::FrameInfoData> frameInfo;

    String cachedPin; // Only used to improve NFC Client PIN experience.
    WeakPtr<API::WebAuthenticationPanel> weakPanel;
    std::optional<WebCore::MediationRequirement> mediation;
    std::optional<WebCore::SecurityOriginData> parentOrigin;
};

WebCore::ClientDataType getClientDataType(const Variant<WebCore::PublicKeyCredentialCreationOptions, WebCore::PublicKeyCredentialRequestOptions>&);
WebCore::UserVerificationRequirement getUserVerificationRequirement(const Variant<WebCore::PublicKeyCredentialCreationOptions, WebCore::PublicKeyCredentialRequestOptions>&);

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
