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

#include "Authenticator.h"
#include "AuthenticatorPresenterCoordinator.h"
#include "AuthenticatorTransportService.h"
#include "WebAuthenticationRequestData.h"
#include <WebCore/AuthenticatorResponse.h>
#include <WebCore/ExceptionData.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

OBJC_CLASS LAContext;

namespace API {
class WebAuthenticationPanel;
}

namespace WebKit {

class AuthenticatorManager : public RefCounted<AuthenticatorManager>, public AuthenticatorTransportServiceObserver, public AuthenticatorObserver {
    WTF_MAKE_TZONE_ALLOCATED(AuthenticatorManager);
    WTF_MAKE_NONCOPYABLE(AuthenticatorManager);
public:
    using Respond = Variant<Ref<WebCore::AuthenticatorResponse>, WebCore::ExceptionData>;
    using Callback = CompletionHandler<void(Respond&&)>;
    using TransportSet = HashSet<WebCore::AuthenticatorTransport, WTF::IntHash<WebCore::AuthenticatorTransport>, WTF::StrongEnumHashTraits<WebCore::AuthenticatorTransport>>;

    USING_CAN_MAKE_WEAKPTR(AuthenticatorTransportServiceObserver);

    const static size_t maxTransportNumber;

    static Ref<AuthenticatorManager> create();
    virtual ~AuthenticatorManager() = default;

    void handleRequest(WebAuthenticationRequestData&&, Callback&&);
    void cancelRequest(const WebCore::PageIdentifier&, const std::optional<WebCore::FrameIdentifier>&); // Called from WebPageProxy/WebProcessProxy.
    void cancelRequest(const API::WebAuthenticationPanel&); // Called from panel clients.
    void cancel(); // Called from the presenter.

    virtual bool isMock() const { return false; }
    virtual bool isVirtual() const { return false; }

    void enableNativeSupport();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

protected:
    AuthenticatorManager();

    RunLoop::Timer& requestTimeOutTimer() { return m_requestTimeOutTimer; }
    void clearStateAsync(); // To void cyclic dependence.
    void clearState();
    void invokePendingCompletionHandler(Respond&&);

    void decidePolicyForLocalAuthenticator(CompletionHandler<void(LocalAuthenticatorPolicy)>&&);
    TransportSet getTransports() const;
    virtual void runPanel();
    void selectAssertionResponse(Vector<Ref<WebCore::AuthenticatorAssertionResponse>>&&, WebAuthenticationSource, CompletionHandler<void(WebCore::AuthenticatorAssertionResponse*)>&&);
    void startDiscovery(const TransportSet&);

private:
    enum class Mode {
        Compatible,
        Native,
    };

    // AuthenticatorTransportServiceObserver
    void authenticatorAdded(Ref<Authenticator>&&) final;
    void serviceStatusUpdated(WebAuthenticationStatus) final;
    bool isAuthenticatorManager() const final { return true; }

    // AuthenticatorObserver
    void respondReceived(Respond&&) final;
    void downgrade(Authenticator& id, Ref<Authenticator>&& downgradedAuthenticator) final;
    void authenticatorStatusUpdated(WebAuthenticationStatus) final;
    void requestPin(uint64_t retries, CompletionHandler<void(const WTF::String&)>&&) final;
    void requestNewPin(uint64_t minLength, CompletionHandler<void(const WTF::String&)>&&) final;
    void requestLAContextForUserVerification(CompletionHandler<void(LAContext *)>&&) final;
    void cancelRequest() final;

    // Overriden by MockAuthenticatorManager.
    virtual Ref<AuthenticatorTransportService> createService(WebCore::AuthenticatorTransport, AuthenticatorTransportServiceObserver&) const;
    // Overriden to return every exception for tests to confirm.
    virtual void respondReceivedInternal(Respond&&) { }
    virtual void filterTransports(TransportSet&) const;
    virtual void runPresenterInternal(const TransportSet&);

    void initTimeOutTimer();
    void timeOutTimerFired();
    void runPresenter();
    void restartDiscovery();
    void dispatchPanelClientCall(Function<void(const API::WebAuthenticationPanel&)>&&) const;

    // Request: We only allow one request per time. A new request will cancel any pending ones.
    WebAuthenticationRequestData m_pendingRequestData;
    Callback m_pendingCompletionHandler; // Should not be invoked directly, use invokePendingCompletionHandler.
    RunLoop::Timer m_requestTimeOutTimer;
    RefPtr<AuthenticatorPresenterCoordinator> m_presenter;

    Vector<Ref<AuthenticatorTransportService>> m_services;
    HashSet<Ref<Authenticator>> m_authenticators;

    Mode m_mode { Mode::Compatible };
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::AuthenticatorManager)
static bool isType(const WebKit::AuthenticatorTransportServiceObserver& observer) { return observer.isAuthenticatorManager(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEB_AUTHN)
