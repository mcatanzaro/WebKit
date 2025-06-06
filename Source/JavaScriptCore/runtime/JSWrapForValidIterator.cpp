/*
 * Copyright (C) 2024 Sosuke Suzuki <aosukeke@gmail.com>.
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
#include "JSWrapForValidIterator.h"


#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"

namespace JSC {

const ClassInfo JSWrapForValidIterator::s_info = { "Iterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWrapForValidIterator) };

JSWrapForValidIterator* JSWrapForValidIterator::createWithInitialValues(VM& vm, Structure* structure)
{
    auto values = initialValues();
    JSWrapForValidIterator* iterator = new (NotNull, allocateCell<JSWrapForValidIterator>(vm)) JSWrapForValidIterator(vm, structure);
    iterator->finishCreation(vm, values[0], values[1]);
    return iterator;
}

JSWrapForValidIterator* JSWrapForValidIterator::create(VM& vm, Structure* structure, JSValue iterator, JSValue nextMethod)
{
    JSWrapForValidIterator* result = new (NotNull, allocateCell<JSWrapForValidIterator>(vm)) JSWrapForValidIterator(vm, structure);
    result->finishCreation(vm, iterator, nextMethod);
    return result;
}

void JSWrapForValidIterator::finishCreation(VM& vm, JSValue iterator, JSValue nextMethod)
{
    Base::finishCreation(vm);
    this->setIteratedIterator(vm, iterator);
    this->setIteratedNextMethod(vm, nextMethod);
}

template<typename Visitor>
void JSWrapForValidIterator::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSWrapForValidIterator*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(JSWrapForValidIterator);

JSC_DEFINE_HOST_FUNCTION(wrapForValidIteratorPrivateFuncCreate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(JSWrapForValidIterator::create(globalObject->vm(), globalObject->wrapForValidIteratorStructure(), callFrame->uncheckedArgument(0), callFrame->uncheckedArgument(1)));
}

} // namespace JSC
