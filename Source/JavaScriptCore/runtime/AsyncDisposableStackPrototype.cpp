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
#include "AsyncDisposableStackPrototype.h"

#include "BuiltinNames.h"
#include "InterpreterInlines.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSAsyncDisposableStack.h"
#include "VMEntryScopeInlines.h"

namespace JSC {

const ClassInfo AsyncDisposableStackPrototype::s_info = { "AsyncDisposableStack"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(AsyncDisposableStackPrototype) };

static JSC_DECLARE_HOST_FUNCTION(asyncDisposableStackProtoDisposedGetter);

void AsyncDisposableStackPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->adopt, asyncDisposableStackPrototypeAdoptCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->deferKeyword, asyncDisposableStackPrototypeDeferMethodCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSFunction* disposeAsyncFunction = JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->disposeAsync, asyncDisposableStackPrototypeDisposeAsyncCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_GETTER_WITHOUT_TRANSITION(vm.propertyNames->disposed, asyncDisposableStackProtoDisposedGetter, PropertyAttribute::DontEnum | PropertyAttribute::Accessor);
    putDirectWithoutTransition(vm, vm.propertyNames->asyncDisposeSymbol, disposeAsyncFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->move, asyncDisposableStackPrototypeMoveCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->use, asyncDisposableStackPrototypeUseCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

JSC_DEFINE_HOST_FUNCTION(asyncDisposableStackProtoDisposedGetter, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue();
    auto* asyncDisposableStack = jsDynamicCast<JSAsyncDisposableStack*>(thisValue);
    if (!asyncDisposableStack) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "AsyncDisposableStack.prototype.disposed getter requires that |this| be a DisposableStack object"_s);

    return JSValue::encode(jsBoolean(asyncDisposableStack->disposed()));
}

} // namespace JSC
