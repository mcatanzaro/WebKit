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
#include "PerformanceObserverEntryList.h"

#include "PerformanceEntry.h"
#include <ranges>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PerformanceObserverEntryList);

Ref<PerformanceObserverEntryList> PerformanceObserverEntryList::create(Vector<Ref<PerformanceEntry>>&& entries)
{
    return adoptRef(*new PerformanceObserverEntryList(WTFMove(entries)));
}

PerformanceObserverEntryList::PerformanceObserverEntryList(Vector<Ref<PerformanceEntry>>&& entries)
    : m_entries(WTFMove(entries))
{
    ASSERT(!m_entries.isEmpty());

    std::ranges::stable_sort(m_entries, PerformanceEntry::startTimeCompareLessThan);
}

Vector<Ref<PerformanceEntry>> PerformanceObserverEntryList::getEntriesByType(const String& entryType) const
{
    return getEntriesByName(String(), entryType);
}

Vector<Ref<PerformanceEntry>> PerformanceObserverEntryList::getEntriesByName(const String& name, const String& entryType) const
{
    Vector<Ref<PerformanceEntry>> entries;

    // PerformanceObservers can only be registered for valid types.
    // So if the incoming entryType is an unknown type, there will be no matches.
    std::optional<PerformanceEntry::Type> type;
    if (!entryType.isNull()) {
        type = PerformanceEntry::parseEntryTypeString(entryType);
        if (!type)
            return entries;
    }

    for (auto& entry : m_entries) {
        if (!name.isNull() && entry->name() != name)
            continue;
        if (type && entry->performanceEntryType() != *type)
            continue;
        entries.append(entry.copyRef());
    }

    return entries;
}

} // namespace WebCore
