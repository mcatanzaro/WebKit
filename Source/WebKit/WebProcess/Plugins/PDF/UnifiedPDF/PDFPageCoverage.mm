/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "PDFPageCoverage.h"

#if ENABLE(UNIFIED_PDF)

namespace WebKit {

TextStream& operator<<(TextStream& ts, const PerPageInfo& pageInfo)
{
    ts << "PerPageInfo "_s << pageInfo.pageIndex << " bounds "_s << pageInfo.pageBounds << " rect in page bounds "_s << pageInfo.rectInPageLayoutCoordinates;
    return ts;
}

TextStream& operator<<(TextStream& ts, const PDFPageCoverageAndScales& coverage)
{
    ts << "PDFPageCoverage "_s << coverage.pages << " pdfDocumentScale "_s << coverage.pdfDocumentScale << ' ' << " tiling scale "_s << coverage.tilingScaleFactor << " contents offset "_s << coverage.contentsOffset;
    return ts;
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_PDF)
