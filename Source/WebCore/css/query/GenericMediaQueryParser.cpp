/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "GenericMediaQueryParser.h"

#include "CSSCustomPropertyValue.h"
#include "CSSParser.h"
#include "CSSPropertyParser.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+IntegerDefinitions.h"
#include "CSSPropertyParserConsumer+LengthDefinitions.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+Ratio.h"
#include "CSSPropertyParserConsumer+ResolutionDefinitions.h"
#include "CSSPropertyParserState.h"
#include "CSSRatioValue.h"
#include "CSSValue.h"
#include "CSSVariableParser.h"
#include "MediaQueryParserContext.h"
#include <wtf/text/MakeString.h>

namespace WebCore {
namespace MQ {

static AtomString consumeFeatureName(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return nullAtom();
    auto name = range.consumeIncludingWhitespace().value();
    if (isCustomPropertyName(name))
        return name.toAtomString();
    return name.convertToASCIILowercaseAtom();
}

std::optional<Feature> FeatureParser::consumeFeature(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    auto rangeCopy = range;
    if (auto feature = consumeBooleanOrPlainFeature(range, context))
        return feature;

    range = rangeCopy;
    return consumeRangeFeature(range, context);
};

static RefPtr<CSSValue> consumeCustomPropertyValue(AtomString propertyName, CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    auto valueRange = range;
    range.consumeAll();

    // Syntax is that of a valid declaration so !important is allowed. It just gets ignored.
    CSSParser::consumeTrailingImportantAndWhitespace(valueRange);

    if (valueRange.atEnd())
        return CSSCustomPropertyValue::createEmpty(propertyName);

    return CSSVariableParser::parseDeclarationValue(propertyName, valueRange, context.context);
}

std::optional<Feature> FeatureParser::consumeBooleanOrPlainFeature(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    auto consumePlainFeatureName = [&]() -> std::pair<AtomString, ComparisonOperator> {
        auto name = consumeFeatureName(range);
        if (name.isEmpty())
            return { };
        if (isCustomPropertyName(name))
            return { name, ComparisonOperator::Equal };
        if (name.startsWith("min-"_s))
            return { StringView(name).substring(4).toAtomString(), ComparisonOperator::GreaterThanOrEqual };
        if (name.startsWith("max-"_s))
            return { StringView(name).substring(4).toAtomString(), ComparisonOperator::LessThanOrEqual };
        if (name.startsWith("-webkit-min-"_s))
            return { makeAtomString("-webkit-"_s, StringView(name).substring(12)), ComparisonOperator::GreaterThanOrEqual };
        if (name.startsWith("-webkit-max-"_s))
            return { makeAtomString("-webkit-"_s, StringView(name).substring(12)), ComparisonOperator::LessThanOrEqual };

        return { name, ComparisonOperator::Equal };
    };

    auto [featureName, op] = consumePlainFeatureName();
    if (featureName.isEmpty())
        return { };

    range.consumeWhitespace();

    if (range.atEnd()) {
        if (op != ComparisonOperator::Equal)
            return { };

        return Feature { featureName, Syntax::Boolean, { }, { } };
    }

    if (range.peek().type() != ColonToken)
        return { };

    range.consumeIncludingWhitespace();

    RefPtr value = isCustomPropertyName(featureName) ? consumeCustomPropertyValue(featureName, range, context) : consumeValue(range, context);

    if (!value)
        return { };

    if (!range.atEnd())
        return { };

    return Feature { featureName, Syntax::Plain, { }, Comparison { op, WTFMove(value) } };
}

std::optional<Feature> FeatureParser::consumeRangeFeature(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    auto consumeRangeOperator = [&]() -> std::optional<ComparisonOperator> {
        if (range.atEnd())
            return { };
        auto opToken = range.consume();
        if (range.atEnd() || opToken.type() != DelimiterToken)
            return { };

        switch (opToken.delimiter()) {
        case '=':
            range.consumeWhitespace();
            return ComparisonOperator::Equal;
        case '<':
            if (range.peek().type() == DelimiterToken && range.peek().delimiter() == '=') {
                range.consumeIncludingWhitespace();
                return ComparisonOperator::LessThanOrEqual;
            }
            range.consumeWhitespace();
            return ComparisonOperator::LessThan;
        case '>':
            if (range.peek().type() == DelimiterToken && range.peek().delimiter() == '=') {
                range.consumeIncludingWhitespace();
                return ComparisonOperator::GreaterThanOrEqual;
            }
            range.consumeWhitespace();
            return ComparisonOperator::GreaterThan;
        default:
            return { };
        }
    };

    bool didFailParsing = false;

    auto consumeLeftComparison = [&]() -> std::optional<Comparison> {
        if (range.peek().type() == IdentToken)
            return { };
        RefPtr value = consumeValue(range, context);
        if (!value)
            return { };
        auto op = consumeRangeOperator();
        if (!op) {
            didFailParsing = true;
            return { };
        }

        return Comparison { *op, WTFMove(value) };
    };

    auto consumeRightComparison = [&]() -> std::optional<Comparison> {
        auto op = consumeRangeOperator();
        if (!op)
            return { };
        RefPtr value = consumeValue(range, context);
        if (!value) {
            didFailParsing = true;
            return { };
        }

        return Comparison { *op, WTFMove(value) };
    };

    auto leftComparison = consumeLeftComparison();

    auto featureName = consumeFeatureName(range);
    if (featureName.isEmpty())
        return { };

    auto rightComparison = consumeRightComparison();

    auto validateComparisons = [&] {
        if (didFailParsing)
            return false;
        if (!leftComparison && !rightComparison)
            return false;
        if (!leftComparison || !rightComparison)
            return true;
        // Disallow comparisons like (a=b=c), (a=b<c).
        if (leftComparison->op == ComparisonOperator::Equal || rightComparison->op == ComparisonOperator::Equal)
            return false;
        // Disallow comparisons like (a<b>c).
        bool leftIsLess = leftComparison->op == ComparisonOperator::LessThan || leftComparison->op == ComparisonOperator::LessThanOrEqual;
        bool rightIsLess = rightComparison->op == ComparisonOperator::LessThan || rightComparison->op == ComparisonOperator::LessThanOrEqual;
        return leftIsLess == rightIsLess;
    };

    if (!range.atEnd() || !validateComparisons())
        return { };

    return Feature { WTFMove(featureName), Syntax::Range, WTFMove(leftComparison), WTFMove(rightComparison) };
}

RefPtr<CSSValue> FeatureParser::consumeValue(CSSParserTokenRange& range, const MediaQueryParserContext& context)
{
    using namespace CSSPropertyParserHelpers;

    if (range.atEnd())
        return nullptr;

    if (RefPtr value = consumeIdent(range))
        return value;

    auto parserState = CSS::PropertyParserState {
        .context = context.context,
    };

    if (RefPtr value = consumeRatioWithBothNumeratorAndDenominator(range, parserState))
        return value;
    if (RefPtr value = CSSPrimitiveValueResolver<CSS::Integer<>>::consumeAndResolve(range, parserState))
        return value;
    if (RefPtr value = CSSPrimitiveValueResolver<CSS::Number<>>::consumeAndResolve(range, parserState))
        return value;
    // FIXME: Figure out and document why overrideParserMode is explicitly set to HTMLStandardMode here.
    if (RefPtr value = CSSPrimitiveValueResolver<CSS::Length<>>::consumeAndResolve(range, parserState, { .overrideParserMode = HTMLStandardMode }))
        return value;
    if (RefPtr value = CSSPrimitiveValueResolver<CSS::Resolution<>>::consumeAndResolve(range, parserState))
        return value;

    return nullptr;
}

bool FeatureParser::validateFeatureAgainstSchema(Feature& feature, const FeatureSchema& schema)
{
    auto validateValue = [&](auto& value) {
        RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
        switch (schema.valueType) {
        case FeatureSchema::ValueType::Integer:
            return primitiveValue && primitiveValue->isInteger();

        case FeatureSchema::ValueType::Number:
            return primitiveValue && primitiveValue->isNumberOrInteger();

        case FeatureSchema::ValueType::Length:
            if (!primitiveValue)
                return false;
            if (primitiveValue->isInteger() && !primitiveValue->resolveAsIntegerDeprecated())
                return true;
            return primitiveValue->isLength();

        case FeatureSchema::ValueType::Resolution:
            return primitiveValue && primitiveValue->isResolution();

        case FeatureSchema::ValueType::Identifier:
            return primitiveValue && primitiveValue->isValueID() && schema.valueIdentifiers.contains(primitiveValue->valueID());

        case FeatureSchema::ValueType::Ratio:
            if (primitiveValue && primitiveValue->isNumberOrInteger()) {
                auto number = primitiveValue->template resolveAsNumberDeprecated<double>();
                if (number < 0)
                    return false;
                value = CSSRatioValue::create(CSS::Ratio { number, 1.0 });
                return true;
            }
            return is<CSSRatioValue>(value.get());

        case FeatureSchema::ValueType::CustomProperty:
            return value && value->isCustomPropertyValue();
        }
        ASSERT_NOT_REACHED();
        return false;
    };

    auto isValid = [&] {
        if (schema.type == FeatureSchema::Type::Discrete) {
            if (feature.syntax == Syntax::Range)
                return false;
            if (feature.rightComparison && feature.rightComparison->op != ComparisonOperator::Equal)
                return false;
        }
        if (schema.valueType == FeatureSchema::ValueType::CustomProperty) {
            if (!isCustomPropertyName(feature.name))
                return false;
        }
        if (feature.leftComparison) {
            if (!validateValue(feature.leftComparison->value))
                return false;
        }
        if (feature.rightComparison) {
            if (!validateValue(feature.rightComparison->value))
                return false;
        }
        return true;
    }();

    feature.schema = isValid ? &schema : nullptr;
    return isValid;
}

}
}
