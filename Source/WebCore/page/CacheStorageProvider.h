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

#pragma once

#include "CacheStorageConnection.h"
#include <wtf/NativePromise.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>

namespace WebCore {

class CacheStorageProvider : public RefCountedAndCanMakeWeakPtr<CacheStorageProvider> {
public:
    class DummyCacheStorageConnection final : public WebCore::CacheStorageConnection {
    public:
        static Ref<DummyCacheStorageConnection> create() { return adoptRef(*new DummyCacheStorageConnection()); }

    private:
        DummyCacheStorageConnection()
        {
        }

        Ref<OpenPromise> open(const ClientOrigin&, const String&) final { return OpenPromise::createAndReject(DOMCacheEngine::Error::Stopped); }
        Ref<RemovePromise> remove(DOMCacheIdentifier) final { return RemovePromise::createAndReject(DOMCacheEngine::Error::Stopped); }
        Ref<RetrieveCachesPromise> retrieveCaches(const ClientOrigin&, uint64_t)  final { return RetrieveCachesPromise::createAndReject(DOMCacheEngine::Error::Stopped); }
        Ref<RetrieveRecordsPromise> retrieveRecords(DOMCacheIdentifier, RetrieveRecordsOptions&&)  final { return RetrieveRecordsPromise::createAndReject(DOMCacheEngine::Error::Stopped); }
        Ref<BatchPromise> batchDeleteOperation(DOMCacheIdentifier, const ResourceRequest&, CacheQueryOptions&&)  final { return BatchPromise::createAndReject(DOMCacheEngine::Error::Stopped); }
        Ref<BatchPromise> batchPutOperation(DOMCacheIdentifier, Vector<DOMCacheEngine::CrossThreadRecord>&&)  final { return BatchPromise::createAndReject(DOMCacheEngine::Error::Stopped); }
        void reference(DOMCacheIdentifier) final { }
        void dereference(DOMCacheIdentifier) final { }
        void lockStorage(const ClientOrigin&) final { }
        void unlockStorage(const ClientOrigin&) final { }
    };

    static Ref<CacheStorageProvider> create() { return adoptRef(*new CacheStorageProvider); }
    virtual Ref<CacheStorageConnection> createCacheStorageConnection() { return DummyCacheStorageConnection::create(); }
    virtual ~CacheStorageProvider() { };

protected:
    CacheStorageProvider() = default;
};

}
