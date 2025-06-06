/*
 * Copyright (C) 2024 Keith Cirkel <webkit@keithcirkel.co.uk>. All rights reserved.
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
#include "InternalObserverFromScript.h"

#include "JSSubscriptionObserverCallback.h"
#include "ScriptExecutionContext.h"
#include "SubscriptionObserver.h"

namespace WebCore {

Ref<InternalObserverFromScript> InternalObserverFromScript::create(ScriptExecutionContext& context, RefPtr<JSSubscriptionObserverCallback> callback)
{
    Ref internalObserver = adoptRef(*new InternalObserverFromScript(context, callback));
    internalObserver->suspendIfNeeded();
    return internalObserver;
}

Ref<InternalObserverFromScript> InternalObserverFromScript::create(ScriptExecutionContext& context, SubscriptionObserver& subscription)
{
    Ref internalObserver = adoptRef(*new InternalObserverFromScript(context, subscription));
    internalObserver->suspendIfNeeded();
    return internalObserver;
}

void InternalObserverFromScript::next(JSC::JSValue value)
{
    if (RefPtr next = m_next)
        next->invoke(value);
}

void InternalObserverFromScript::error(JSC::JSValue value)
{
    if (RefPtr error = m_error) {
        error->invoke(value);
        return;
    }

    InternalObserver::error(value);
}

void InternalObserverFromScript::complete()
{
    if (RefPtr complete = m_complete)
        complete->invoke();

    m_active = false;
}

void InternalObserverFromScript::visitAdditionalChildren(JSC::AbstractSlotVisitor& visitor) const
{
    if (RefPtr next = m_next)
        next->visitJSFunction(visitor);

    if (RefPtr error = m_error)
        error->visitJSFunction(visitor);

    if (RefPtr complete = m_complete)
        complete->visitJSFunction(visitor);
}

InternalObserverFromScript::InternalObserverFromScript(ScriptExecutionContext& context, RefPtr<JSSubscriptionObserverCallback> callback)
    : InternalObserver(context)
    , m_next(callback)
    , m_error(nullptr)
    , m_complete(nullptr) { }

InternalObserverFromScript::InternalObserverFromScript(ScriptExecutionContext& context, SubscriptionObserver& subscription)
    : InternalObserver(context)
    , m_next(subscription.next)
    , m_error(subscription.error)
    , m_complete(subscription.complete) { }

} // namespace WebCore
