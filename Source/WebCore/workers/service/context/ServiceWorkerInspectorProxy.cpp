/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ServiceWorkerInspectorProxy.h"

#include "SWContextManager.h"
#include "ScriptExecutionContext.h"
#include "ServiceWorkerDebuggable.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerThreadProxy.h"
#include "WorkerInspectorController.h"
#include "WorkerRunLoop.h"
#include <JavaScriptCore/InspectorAgentBase.h>
#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(ServiceWorkerInspectorProxy);

ServiceWorkerInspectorProxy::ServiceWorkerInspectorProxy(ServiceWorkerThreadProxy& serviceWorkerThreadProxy)
    : m_serviceWorkerThreadProxy(serviceWorkerThreadProxy)
{
    ASSERT(isMainThread());
}

ServiceWorkerInspectorProxy::~ServiceWorkerInspectorProxy()
{
    ASSERT(isMainThread());
    ASSERT(!m_channel);
}

void ServiceWorkerInspectorProxy::serviceWorkerTerminated()
{
    m_channel = nullptr;
}

void ServiceWorkerInspectorProxy::connectToWorker(FrontendChannel& channel, bool isAutomaticConnection, bool immediatelyPause)
{
    m_channel = &channel;

    RefPtr serviceWorkerThreadProxy = m_serviceWorkerThreadProxy.get();
    SWContextManager::singleton().setAsInspected(serviceWorkerThreadProxy->identifier(), true);
    serviceWorkerThreadProxy->thread().runLoop().postDebuggerTask([isAutomaticConnection, immediatelyPause] (ScriptExecutionContext& context) {
        downcast<WorkerGlobalScope>(context).inspectorController().connectFrontend(isAutomaticConnection, immediatelyPause);
    });
}

void ServiceWorkerInspectorProxy::disconnectFromWorker(FrontendChannel& channel)
{
    ASSERT_UNUSED(channel, &channel == m_channel);
    m_channel = nullptr;

    RefPtr serviceWorkerThreadProxy = m_serviceWorkerThreadProxy.get();
    SWContextManager::singleton().setAsInspected(serviceWorkerThreadProxy->identifier(), false);
    serviceWorkerThreadProxy->thread().runLoop().postDebuggerTask([] (ScriptExecutionContext& context) {
        downcast<WorkerGlobalScope>(context).inspectorController().disconnectFrontend(DisconnectReason::InspectorDestroyed);

        // In case the worker is paused running debugger tasks, ensure we break out of
        // the pause since this will be the last debugger task we send to the worker.
        downcast<WorkerGlobalScope>(context).protectedThread()->stopRunningDebuggerTasks();
    });
}

void ServiceWorkerInspectorProxy::sendMessageToWorker(String&& message)
{
    m_serviceWorkerThreadProxy.get()->thread().runLoop().postDebuggerTask([message = WTFMove(message).isolatedCopy()] (ScriptExecutionContext& context) {
        downcast<WorkerGlobalScope>(context).inspectorController().dispatchMessageFromFrontend(message);
    });
}

void ServiceWorkerInspectorProxy::sendMessageFromWorkerToFrontend(String&& message)
{
    if (!m_channel)
        return;

    m_channel->sendMessageToFrontend(WTFMove(message));
}

} // namespace WebCore
