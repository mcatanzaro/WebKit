/*
 * Copyright (C) 2017 Igalia S.L.
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

#include "GridPositionsResolver.h"
#include "LayoutUnit.h"
#include "RenderBox.h"

namespace WebCore {

enum class ItemPosition : uint8_t;
class RenderElement;
class RenderGrid;

struct ExtraMarginsFromSubgrids {
    inline LayoutUnit extraTrackStartMargin() const { return m_extraMargins.first; }
    inline LayoutUnit extraTrackEndMargin() const { return m_extraMargins.second; }
    inline LayoutUnit extraTotalMargin() const { return m_extraMargins.first + m_extraMargins.second; }

    ExtraMarginsFromSubgrids& operator+=(const ExtraMarginsFromSubgrids& rhs)
    {
        m_extraMargins.first += rhs.extraTrackStartMargin();
        m_extraMargins.second += rhs.extraTrackEndMargin();
        return *this;
    }

    void addTrackStartMargin(LayoutUnit extraMargin) { m_extraMargins.first += extraMargin; }
    void addTrackEndMargin(LayoutUnit extraMargin) { m_extraMargins.second += extraMargin; }

    std::pair<LayoutUnit, LayoutUnit> m_extraMargins;
};

namespace GridLayoutFunctions {

LayoutUnit computeMarginLogicalSizeForGridItem(const RenderGrid&, GridTrackSizingDirection, const RenderBox&);
LayoutUnit marginLogicalSizeForGridItem(const RenderGrid&, GridTrackSizingDirection, const RenderBox&);
void setOverridingContentSizeForGridItem(const RenderGrid&, RenderBox& gridItem, LayoutUnit, GridTrackSizingDirection);
void clearOverridingContentSizeForGridItem(const RenderGrid&, RenderBox& gridItem, GridTrackSizingDirection);
bool isOrthogonalGridItem(const RenderGrid&, const RenderBox&);
bool isGridItemInlineSizeDependentOnBlockConstraints(const RenderBox& gridItem, const RenderGrid& parentGrid, ItemPosition gridItemAlignSelf);
bool isOrthogonalParent(const RenderGrid&, const RenderElement& parent);
bool isAspectRatioBlockSizeDependentGridItem(const RenderBox&);
GridTrackSizingDirection flowAwareDirectionForGridItem(const RenderGrid&, const RenderBox&, GridTrackSizingDirection);
GridTrackSizingDirection flowAwareDirectionForParent(const RenderGrid&, const RenderElement& parent, GridTrackSizingDirection);
std::optional<RenderBox::GridAreaSize> overridingContainingBlockContentSizeForGridItem(const RenderBox&, GridTrackSizingDirection);
bool hasRelativeOrIntrinsicSizeForGridItem(const RenderBox& gridItem, GridTrackSizingDirection);

bool isFlippedDirection(const RenderGrid&, GridTrackSizingDirection);
bool isSubgridReversedDirection(const RenderGrid&, GridTrackSizingDirection outerDirection, const RenderGrid& subgrid);
ExtraMarginsFromSubgrids extraMarginForSubgridAncestors(GridTrackSizingDirection, const RenderBox& gridItem);

unsigned alignmentContextForBaselineAlignment(const GridSpan&, const ItemPosition& alignment);

}

} // namespace WebCore

