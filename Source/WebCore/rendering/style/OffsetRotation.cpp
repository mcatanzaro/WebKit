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

#include "config.h"
#include "OffsetRotation.h"

#include "AnimationUtilities.h"
#include <wtf/MathExtras.h>

namespace WebCore {

bool OffsetRotation::canBlend(const OffsetRotation& to) const
{
    return m_hasAuto == to.hasAuto();
}

OffsetRotation OffsetRotation::blend(const OffsetRotation& to, const BlendingContext& context) const
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : *this;
    }

    ASSERT(canBlend(to));
    return OffsetRotation(m_hasAuto, clampTo<float>(WebCore::blend(m_angle, to.angle(), context)));
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const OffsetRotation& rotation)
{
    ts.dumpProperty("angle"_s, rotation.angle());
    ts.dumpProperty("hasAuto"_s, rotation.hasAuto());

    return ts;
}

} // namespace WebCore
