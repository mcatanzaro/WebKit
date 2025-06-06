/*
 * Copyright (C) 2025 Sosuke Suzuki <aosukeke@gmail.com>.
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

#include "config.h"
#include "DisposableStackPrototype.h"

#include "BuiltinNames.h"
#include "InterpreterInlines.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSDisposableStack.h"
#include "VMEntryScopeInlines.h"

namespace JSC {

const ClassInfo DisposableStackPrototype::s_info = { "DisposableStack"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DisposableStackPrototype) };

static JSC_DECLARE_HOST_FUNCTION(disposableStackProtoDisposedGetter);

void DisposableStackPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->adopt, disposableStackPrototypeAdoptCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->deferKeyword, disposableStackPrototypeDeferMethodCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSFunction* disposeFunction = JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->dispose, disposableStackPrototypeDisposeCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->disposed, disposableStackProtoDisposedGetter, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    putDirectWithoutTransition(vm, vm.propertyNames->disposeSymbol, disposeFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->use, disposableStackPrototypeUseCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->move, disposableStackPrototypeMoveCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

JSC_DEFINE_HOST_FUNCTION(disposableStackProtoDisposedGetter, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* disposableStack = jsDynamicCast<JSDisposableStack*>(thisValue);
    if (!disposableStack) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "DisposableStack.prototype.disposed getter requires that |this| be a DisposableStack object"_s);

    return JSValue::encode(jsBoolean(disposableStack->disposed()));
}

} // namespace JSC
