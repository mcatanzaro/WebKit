/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "APIFeatureStatus.h"
#include "APIObject.h"
#include <span>
#include <wtf/TypeCasts.h>
#include <wtf/text/WTFString.h>

namespace API {

class Feature final : public ObjectImpl<Object::Type::Feature> {
public:

    template <FeatureStatus Status, bool DefaultValue>
    static Ref<Feature> create(const WTF::String& name, const WTF::String& key, FeatureConstant<Status> status, FeatureCategory category, const WTF::String& details, std::bool_constant<DefaultValue> defaultValue, bool hidden)
    {
#if ENABLE(FEATURE_DEFAULT_VALIDATION)
        constexpr auto impliedDefaultValue = API::defaultValueForFeatureStatus(Status);
        if constexpr (impliedDefaultValue && *impliedDefaultValue)
            static_assert(defaultValue, "Feature's status implies it should be on by default");
        else if constexpr (impliedDefaultValue && !*impliedDefaultValue)
            static_assert(!defaultValue, "Feature's status implies it should be off by default");
#endif

        return uncheckedCreate(name, key, status, category, details, defaultValue, hidden);
    }

    template <FeatureStatus Status>
    static Ref<Feature> create(const WTF::String& name, const WTF::String& key, FeatureConstant<Status> status, FeatureCategory category, const WTF::String& details, bool defaultValue, bool hidden)
    {
        return uncheckedCreate(name, key, status, category, details, defaultValue, hidden);
    }

    virtual ~Feature() = default;

    WTF::String name() const { return m_name; }
    WTF::String key() const { return m_key; }
    FeatureStatus status() const { return m_status; }
    FeatureCategory category() const { return m_category; }
    WTF::String details() const { return m_details; }
    bool defaultValue() const { return m_defaultValue; }
    bool isHidden() const { return m_hidden; }
    
private:
    explicit Feature(const WTF::String& name, const WTF::String& key, FeatureStatus, FeatureCategory, const WTF::String& details, bool defaultValue, bool hidden);

    static Ref<Feature> uncheckedCreate(const WTF::String& name, const WTF::String& key, FeatureStatus, FeatureCategory, const WTF::String& details, bool defaultValue, bool hidden);

    WTF::String m_name;
    WTF::String m_key;
    WTF::String m_details;
    FeatureStatus m_status;
    FeatureCategory m_category;
    bool m_defaultValue;
    bool m_hidden;
};

}

SPECIALIZE_TYPE_TRAITS_API_OBJECT(Feature);
