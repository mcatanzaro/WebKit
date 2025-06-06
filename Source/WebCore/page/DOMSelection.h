/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "GetComposedRangesOptions.h"
#include "LocalDOMWindowProperty.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Variant.h>

namespace WebCore {

class Node;
class Position;
class Range;
class StaticRange;
class VisibleSelection;

struct SimpleRange;

template<typename> class ExceptionOr;

class DOMSelection : public RefCounted<DOMSelection>, public LocalDOMWindowProperty {
public:
    static Ref<DOMSelection> create(LocalDOMWindow&);

    String type() const;
    String direction() const;
    ExceptionOr<void> setBaseAndExtent(Node& anchorNode, unsigned anchorOffset, Node& focusNode, unsigned focusOffset);
    ExceptionOr<void> setPosition(Node*, unsigned offset);
    void modify(const String& alter, const String& direction, const String& granularity);

    // The anchor and focus are the start and end of the selection, and
    // reflect the direction in which the selection was made by the user.
    RefPtr<Node> anchorNode() const;
    unsigned anchorOffset() const;
    RefPtr<Node> focusNode() const;
    unsigned focusOffset() const;
    bool isCollapsed() const;
    unsigned rangeCount() const;
    ExceptionOr<void> collapse(Node*, unsigned offset);
    ExceptionOr<void> collapseToEnd();
    ExceptionOr<void> collapseToStart();
    ExceptionOr<void> extend(Node&, unsigned offset);
    ExceptionOr<Ref<Range>> getRangeAt(unsigned);
    void removeAllRanges();
    void addRange(Range&);
    ExceptionOr<void> removeRange(Range&);

    Vector<Ref<StaticRange>> getComposedRanges(std::optional<Variant<RefPtr<ShadowRoot>, GetComposedRangesOptions>>&& options = std::nullopt, FixedVector<std::reference_wrapper<ShadowRoot>>&& = { });

    void deleteFromDocument();
    bool containsNode(Node&, bool partlyContained) const;
    ExceptionOr<void> selectAllChildren(Node&);

    String toString() const;

    void empty();

private:
    explicit DOMSelection(LocalDOMWindow&);

    // FIXME: Change LocalDOMWindowProperty::frame to return RefPtr and then delete this.
    RefPtr<LocalFrame> frame() const;
    std::optional<SimpleRange> range() const;

    Position anchorPosition() const;
    Position focusPosition() const;

    RefPtr<Node> shadowAdjustedNode(const Position&) const;
    unsigned shadowAdjustedOffset(const Position&) const;
};

} // namespace WebCore
