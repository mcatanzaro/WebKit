/*
    This file is part of the WebKit open source project.
    This file has been generated by generate-bindings.pl. DO NOT MODIFY!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "JSTestDictionaryNoToNative.h"

#include "JSDOMConvertNumbers.h"
#include "JSDOMGlobalObject.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/ObjectConstructor.h>



namespace WebCore {
using namespace JSC;

JSC::JSObject* convertDictionaryToJS(JSC::JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject, const TestDictionaryNoToNative& dictionary)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = JSC::getVM(&lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto result = constructEmptyObject(&lexicalGlobalObject, globalObject.objectPrototype());

    if (!IDLDouble::isNullValue(dictionary.member)) {
        auto memberValue = toJS<IDLDouble>(lexicalGlobalObject, throwScope, IDLDouble::extractValueFromNullable(dictionary.member));
        RETURN_IF_EXCEPTION(throwScope, { });
        result->putDirect(vm, JSC::Identifier::fromString(vm, "member"_s), memberValue);
    }
    return result;
}

template<> ConversionResult<IDLDictionary<TestDictionaryNoToNative::GenerateKeyword>> convertDictionary<TestDictionaryNoToNative::GenerateKeyword>(JSGlobalObject& lexicalGlobalObject, JSValue value)
{
    SUPPRESS_UNCOUNTED_LOCAL auto& vm = JSC::getVM(&lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    bool isNullOrUndefined = value.isUndefinedOrNull();
    auto* object = isNullOrUndefined ? nullptr : value.getObject();
    if (!isNullOrUndefined && !object) [[unlikely]] {
        throwTypeError(&lexicalGlobalObject, throwScope);
        return ConversionResultException { };
    }
    TestDictionaryNoToNative::GenerateKeyword result;
    JSValue memberValue;
    if (isNullOrUndefined)
        memberValue = jsUndefined();
    else {
        memberValue = object->get(&lexicalGlobalObject, Identifier::fromString(vm, "member"_s));
        RETURN_IF_EXCEPTION(throwScope, ConversionResultException { });
    }
    if (!memberValue.isUndefined()) {
        auto memberConversionResult = convert<IDLDouble>(lexicalGlobalObject, memberValue);
        if (memberConversionResult.hasException(throwScope)) [[unlikely]]
            return ConversionResultException { };
        result.member = memberConversionResult.releaseReturnValue();
    }
    return result;
}

} // namespace WebCore

