/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
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

#include "NavigationHistoryEntry.h"
#include "ScriptWrappable.h"

namespace WebCore {

class NavigationHistoryEntry;
enum class NavigationNavigationType : uint8_t;

class NavigationActivation final : public RefCounted<NavigationActivation>, public ScriptWrappable {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(NavigationActivation);
public:
    static Ref<NavigationActivation> create(NavigationNavigationType type, Ref<NavigationHistoryEntry>&& entry, RefPtr<NavigationHistoryEntry>&& fromEntry)
    {
        return adoptRef(*new NavigationActivation(type, WTFMove(entry), WTFMove(fromEntry)));
    }

    NavigationNavigationType navigationType() { return m_navigationType; };
    NavigationHistoryEntry* from() const { return m_fromEntry.get(); };
    const NavigationHistoryEntry& entry() const { return m_entry; };

private:
    explicit NavigationActivation(NavigationNavigationType, Ref<NavigationHistoryEntry>&&, RefPtr<NavigationHistoryEntry>&& fromEntry);

    NavigationNavigationType m_navigationType;
    const Ref<NavigationHistoryEntry> m_entry;
    const RefPtr<NavigationHistoryEntry> m_fromEntry;
};

} // namespace WebCore
