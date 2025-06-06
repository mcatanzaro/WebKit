/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
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

#include "CachedHTMLCollection.h"
#include "HTMLTableElement.h"

namespace WebCore {

class HTMLTableRowElement;

class HTMLTableRowsCollection final : public CachedHTMLCollection<HTMLTableRowsCollection, CollectionTypeTraits<CollectionType::TableRows>::traversalType> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(HTMLTableRowsCollection);
public:
    static Ref<HTMLTableRowsCollection> create(HTMLTableElement&, CollectionType);

    HTMLTableElement& tableElement() const { return downcast<HTMLTableElement>(ownerNode()); }

    static HTMLTableRowElement* rowAfter(HTMLTableElement&, HTMLTableRowElement*);
    static HTMLTableRowElement* lastRow(HTMLTableElement&);

    // For CachedHTMLCollection.
    Element* customElementAfter(Element*) const;

private:
    explicit HTMLTableRowsCollection(HTMLTableElement&);
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_HTMLCOLLECTION(HTMLTableRowsCollection, CollectionType::TableRows)
