/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "URLSearchParams.h"

#include "DOMURL.h"
#include <ranges>
#include <wtf/URLParser.h>

namespace WebCore {

URLSearchParams::URLSearchParams(const String& init, DOMURL* associatedURL)
    : m_associatedURL(associatedURL)
    , m_pairs(init.startsWith('?') ? WTF::URLParser::parseURLEncodedForm(StringView(init).substring(1)) : WTF::URLParser::parseURLEncodedForm(init))
{
}

URLSearchParams::URLSearchParams(const Vector<KeyValuePair<String, String>>& pairs)
    : m_pairs(pairs)
{
}

URLSearchParams::~URLSearchParams() = default;

ExceptionOr<Ref<URLSearchParams>> URLSearchParams::create(Variant<Vector<Vector<String>>, Vector<KeyValuePair<String, String>>, String>&& variant)
{
    auto visitor = WTF::makeVisitor([&](const Vector<Vector<String>>& vector) -> ExceptionOr<Ref<URLSearchParams>> {
        Vector<KeyValuePair<String, String>> pairs;
        for (const auto& pair : vector) {
            if (pair.size() != 2)
                return Exception { ExceptionCode::TypeError };
            pairs.append({pair[0], pair[1]});
        }
        return adoptRef(*new URLSearchParams(WTFMove(pairs)));
    }, [&](const Vector<KeyValuePair<String, String>>& pairs) -> ExceptionOr<Ref<URLSearchParams>> {
        return adoptRef(*new URLSearchParams(pairs));
    }, [&](const String& string) -> ExceptionOr<Ref<URLSearchParams>> {
        return adoptRef(*new URLSearchParams(string, nullptr));
    });
    return WTF::visit(visitor, variant);
}

String URLSearchParams::get(const String& name) const
{
    for (const auto& pair : m_pairs) {
        if (pair.key == name)
            return pair.value;
    }
    return String();
}

bool URLSearchParams::has(const String& name, const String& value) const
{
    for (const auto& pair : m_pairs) {
        if (pair.key == name && (value.isNull() || pair.value == value))
            return true;
    }
    return false;
}

void URLSearchParams::sort()
{
    std::ranges::stable_sort(m_pairs, WTF::codePointCompareLessThan, &PairType::key);
    updateURL();
}

void URLSearchParams::set(const String& name, const String& value)
{
    for (auto& pair : m_pairs) {
        if (pair.key != name)
            continue;
        if (pair.value != value)
            pair.value = value;
        bool skippedFirstMatch = false;
        m_pairs.removeAllMatching([&] (const auto& pair) {
            if (pair.key == name) {
                if (skippedFirstMatch)
                    return true;
                skippedFirstMatch = true;
            }
            return false;
        });
        updateURL();
        return;
    }
    m_pairs.append({name, value});
    updateURL();
}

void URLSearchParams::append(const String& name, const String& value)
{
    m_pairs.append({name, value});
    updateURL();
}

Vector<String> URLSearchParams::getAll(const String& name) const
{
    return WTF::compactMap(m_pairs, [&](auto& pair) -> std::optional<String> {
        if (pair.key == name)
            return pair.value;
        return std::nullopt;
    });
}

void URLSearchParams::remove(const String& name, const String& value)
{
    m_pairs.removeAllMatching([&] (const auto& pair) {
        return pair.key == name && (value.isNull() || pair.value == value);
    });
    updateURL();
}

String URLSearchParams::toString() const
{
    return WTF::URLParser::serialize(m_pairs);
}

void URLSearchParams::updateURL()
{
    if (m_associatedURL)
        m_associatedURL->setSearch(WTF::URLParser::serialize(m_pairs));
}

void URLSearchParams::updateFromAssociatedURL()
{
    ASSERT(m_associatedURL);
    String search = m_associatedURL->search();
    m_pairs = search.startsWith('?') ? WTF::URLParser::parseURLEncodedForm(StringView(search).substring(1)) : WTF::URLParser::parseURLEncodedForm(search);
}

std::optional<KeyValuePair<String, String>> URLSearchParams::Iterator::next()
{
    auto& pairs = m_target->pairs();
    if (m_index >= pairs.size())
        return std::nullopt;

    auto& pair = pairs[m_index++];
    return KeyValuePair<String, String> { pair.key, pair.value };
}

URLSearchParams::Iterator::Iterator(URLSearchParams& params)
    : m_target(params)
{
}
    
}
