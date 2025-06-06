/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#include "ArgumentCoders.h"
#include "CursorContext.h"
#include "InteractionInformationRequest.h"
#include <WebCore/ElementAnimationContext.h>
#include <WebCore/ElementContext.h>
#include <WebCore/IntPoint.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/SelectionGeometry.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/TextIndicator.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

struct InteractionInformationAtPosition {
    static InteractionInformationAtPosition invalidInformation()
    {
        InteractionInformationAtPosition response;
        response.canBeValid = false;
        return response;
    }

    InteractionInformationRequest request;

    bool canBeValid { true };
    std::optional<bool> nodeAtPositionHasDoubleClickHandler;

    enum class Selectability : uint8_t {
        Selectable,
        UnselectableDueToFocusableElement,
        UnselectableDueToLargeElementBounds,
        UnselectableDueToUserSelectNoneOrQuirk,
        UnselectableDueToMediaControls,
    };

    Selectability selectability { Selectability::Selectable };

    bool isSelected { false };
    bool prefersDraggingOverTextSelection { false };
    bool isNearMarkedText { false };
    bool touchCalloutEnabled { true };
    bool isLink { false };
    bool isImage { false };
#if ENABLE(MODEL_PROCESS)
    bool isInteractiveModel { false };
#endif
    bool isAttachment { false };
    bool isAnimatedImage { false };
    bool isAnimating { false };
    bool canShowAnimationControls { false };
    bool isPausedVideo { false };
    bool isElement { false };
    bool isContentEditable { false };
    Markable<WebCore::ScrollingNodeID> containerScrollingNodeID;
#if ENABLE(DATA_DETECTION)
    bool isDataDetectorLink { false };
#endif
    bool preventTextInteraction { false };
    bool elementContainsImageOverlay { false };
    bool isImageOverlayText { false };
#if ENABLE(SPATIAL_IMAGE_DETECTION)
    bool isSpatialImage { false };
#endif
    bool isInPlugin { false };
    bool needsPointerTouchCompatibilityQuirk { false };
    WebCore::FloatPoint adjustedPointForNodeRespondingToClickEvents;
    URL url;
    URL imageURL;
    String imageMIMEType;
    String title;
    String idAttribute;
    WebCore::IntRect bounds;
#if PLATFORM(MACCATALYST)
    WebCore::IntRect caretRect;
#endif
    RefPtr<WebCore::ShareableBitmap> image;
    String textBefore;
    String textAfter;

    CursorContext cursorContext;

    RefPtr<WebCore::TextIndicator> textIndicator;
#if ENABLE(DATA_DETECTION)
    String dataDetectorIdentifier;
    RetainPtr<NSArray> dataDetectorResults;
    WebCore::IntRect dataDetectorBounds;
#endif

    std::optional<WebCore::ElementContext> elementContext;
    std::optional<WebCore::ElementContext> hostImageOrVideoElementContext;

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    Vector<WebCore::ElementAnimationContext> animationsAtPoint;
#endif

    // Copy compatible optional bits forward (for example, if we have a InteractionInformationAtPosition
    // with snapshots in it, and perform another request for the same point without requesting the snapshots,
    // we can fetch the cheap information and copy the snapshots into the new response).
    void mergeCompatibleOptionalInformation(const InteractionInformationAtPosition& oldInformation);

    bool isSelectable() const { return selectability == Selectability::Selectable; }
};

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
