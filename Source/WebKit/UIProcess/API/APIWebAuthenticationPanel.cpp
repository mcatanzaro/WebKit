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

#include "config.h"
#include "APIWebAuthenticationPanel.h"

#if ENABLE(WEB_AUTHN)

#include "APIWebAuthenticationPanelClient.h"
#include "AuthenticatorManager.h"
#include "MockAuthenticatorManager.h"
#include <WebCore/WebAuthenticationConstants.h>

namespace API {
using namespace WebCore;
using namespace WebKit;

Ref<WebAuthenticationPanel> WebAuthenticationPanel::create(const AuthenticatorManager& manager, const WTF::String& rpId, const TransportSet& transports, ClientDataType type, const WTF::String& userName)
{
    return adoptRef(*new WebAuthenticationPanel(manager, rpId, transports, type, userName));
}

WebAuthenticationPanel::WebAuthenticationPanel()
    : m_manager(AuthenticatorManager::create())
    , m_client(WebAuthenticationPanelClient::create())
{
    protectedManager()->enableNativeSupport();
}

RefPtr<WebKit::AuthenticatorManager> WebAuthenticationPanel::protectedManager() const
{
    return m_manager;
}

Ref<WebAuthenticationPanelClient> WebAuthenticationPanel::protectedClient() const
{
    return m_client;
}

WebAuthenticationPanel::WebAuthenticationPanel(const AuthenticatorManager& manager, const WTF::String& rpId, const TransportSet& transports, ClientDataType type, const WTF::String& userName)
    : m_client(WebAuthenticationPanelClient::create())
    , m_weakManager(manager)
    , m_rpId(rpId)
    , m_clientDataType(type)
    , m_userName(userName)
{
    m_transports.reserveInitialCapacity(AuthenticatorManager::maxTransportNumber);
    if (transports.contains(AuthenticatorTransport::Usb))
        m_transports.append(AuthenticatorTransport::Usb);
    if (transports.contains(AuthenticatorTransport::Nfc))
        m_transports.append(AuthenticatorTransport::Nfc);
    if (transports.contains(AuthenticatorTransport::Internal))
        m_transports.append(AuthenticatorTransport::Internal);
}

WebAuthenticationPanel::~WebAuthenticationPanel() = default;

void WebAuthenticationPanel::handleRequest(WebAuthenticationRequestData&& request, Callback&& callback)
{
    ASSERT(m_manager);
    request.weakPanel = *this;
    protectedManager()->handleRequest(WTFMove(request), WTFMove(callback));
}

void WebAuthenticationPanel::cancel() const
{
    if (RefPtr manager = m_weakManager.get()) {
        manager->cancelRequest(*this);
        return;
    }

    protectedManager()->cancel();
}

void WebAuthenticationPanel::setMockConfiguration(WebCore::MockWebAuthenticationConfiguration&& configuration)
{
    ASSERT(m_manager);

    if (RefPtr mockManager = dynamicDowncast<MockAuthenticatorManager>(*m_manager)) {
        mockManager->setTestConfiguration(WTFMove(configuration));
        return;
    }

    Ref manager = MockAuthenticatorManager::create(WTFMove(configuration));
    manager->enableNativeSupport();
    m_manager = WTFMove(manager);
}

void WebAuthenticationPanel::setClient(Ref<WebAuthenticationPanelClient>&& client)
{
    m_client = WTFMove(client);
}

} // namespace API

#endif // ENABLE(WEB_AUTHN)
