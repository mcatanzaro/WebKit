/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "Event.h"

namespace WebCore {

class ProgressEvent : public Event {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(ProgressEvent);
public:
    static Ref<ProgressEvent> create(const AtomString& type, bool lengthComputable, double loaded, double total)
    {
        return adoptRef(*new ProgressEvent(EventInterfaceType::ProgressEvent, type, lengthComputable, loaded, total));
    }

    struct Init : EventInit {
        bool lengthComputable { false };
        double loaded { 0 };
        double total { 0 };
    };

    static Ref<ProgressEvent> create(const AtomString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new ProgressEvent(EventInterfaceType::ProgressEvent, type, initializer, isTrusted));
    }

    bool lengthComputable() const { return m_lengthComputable; }
    double loaded() const { return m_loaded; }
    double total() const { return m_total; }

protected:
    ProgressEvent(enum EventInterfaceType, const AtomString& type, bool lengthComputable, double loaded, double total);
    ProgressEvent(enum EventInterfaceType, const AtomString&, const Init&, IsTrusted);

private:
    bool m_lengthComputable;
    double m_loaded;
    double m_total;
};

} // namespace WebCore
