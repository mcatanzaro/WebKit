/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
#include "JITThunks.h"

#if ENABLE(JIT)

#include "CommonSlowPaths.h"
#include "JIT.h"
#include "JITCode.h"
#include "JSCJSValueInlines.h"
#include "LLIntThunks.h"
#include "SlowPathCall.h"
#include "ThunkGenerators.h"
#include "VM.h"
#include "YarrJIT.h"
#include <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(JITThunks);

JITThunks::JITThunks() = default;

JITThunks::~JITThunks() = default;

void JITThunks::initialize(VM& vm)
{
    ASSERT(!isCompilationThread());
#define JSC_DEFINE_COMMON_JIT_THUNK(name, func) \
    m_commonThunks[static_cast<unsigned>(CommonJITThunkID::name)] = func(vm);
JSC_FOR_EACH_COMMON_THUNK(JSC_DEFINE_COMMON_JIT_THUNK)
#undef JSC_DEFINE_COMMON_JIT_THUNK
}

static inline NativeExecutable& getMayBeDyingNativeExecutable(const Weak<NativeExecutable>& weak)
{
    // This never gets Deleted / Empty slots.
    WeakImpl* impl = weak.unsafeImpl();
    ASSERT(impl);
    // We have a callback removing entry when finalizing. This means that we never hold Deallocated entry in HashSet.
    ASSERT(impl->state() != WeakImpl::State::Deallocated);
    // Never use jsCast here. This is possible that this value is "Dead" but not "Finalized" yet. In this case,
    // we can still access to non-JS data, as we are doing in a finalize callback.
    auto* executable = static_cast<NativeExecutable*>(impl->jsValue().asCell());
    ASSERT(executable);
    return *executable;
}

inline unsigned JITThunks::WeakNativeExecutableHash::hash(const NativeExecutable* executable)
{
    return hash(executable->function(), executable->constructor(), executable->implementationVisibility(), executable->name());
}

inline unsigned JITThunks::WeakNativeExecutableHash::hash(const Weak<NativeExecutable>& key)
{
    return hash(&getMayBeDyingNativeExecutable(key));
}

inline bool JITThunks::WeakNativeExecutableHash::equal(const NativeExecutable& a, const NativeExecutable& b)
{
    if (&a == &b)
        return true;
    return a.function() == b.function() && a.constructor() == b.constructor() && a.implementationVisibility() == b.implementationVisibility() && a.name() == b.name();
}

inline bool JITThunks::WeakNativeExecutableHash::equal(const Weak<NativeExecutable>& a, const Weak<NativeExecutable>& b)
{
    return equal(getMayBeDyingNativeExecutable(a), getMayBeDyingNativeExecutable(b));
}

inline bool JITThunks::WeakNativeExecutableHash::equal(const Weak<NativeExecutable>& a, const NativeExecutable* bExecutable)
{
    return equal(getMayBeDyingNativeExecutable(a), *bExecutable);
}

inline bool JITThunks::WeakNativeExecutableHash::equal(const Weak<NativeExecutable>& a, const HostFunctionKey& b)
{
    auto& aExecutable = getMayBeDyingNativeExecutable(a);
    return aExecutable.function() == std::get<0>(b) && aExecutable.constructor() == std::get<1>(b) && aExecutable.implementationVisibility() == std::get<2>(b) && aExecutable.name() == std::get<3>(b);
}

CodePtr<JITThunkPtrTag> JITThunks::ctiNativeCall(VM&)
{
    ASSERT(Options::useJIT());
    return ctiStub(CommonJITThunkID::NativeCall).code();
}

CodePtr<JITThunkPtrTag> JITThunks::ctiNativeCallWithDebuggerHook(VM& vm)
{
    ASSERT(Options::useJIT());
    return ctiStub(vm, nativeCallWithDebuggerHookGenerator).code();
}

CodePtr<JITThunkPtrTag> JITThunks::ctiNativeConstruct(VM&)
{
    ASSERT(Options::useJIT());
    return ctiStub(CommonJITThunkID::NativeConstruct).code();
}

CodePtr<JITThunkPtrTag> JITThunks::ctiNativeConstructWithDebuggerHook(VM& vm)
{
    ASSERT(Options::useJIT());
    return ctiStub(vm, nativeConstructWithDebuggerHookGenerator).code();
}

CodePtr<JITThunkPtrTag> JITThunks::ctiNativeTailCall(VM&)
{
    ASSERT(Options::useJIT());
    return ctiStub(CommonJITThunkID::NativeTailCall).code();
}

CodePtr<JITThunkPtrTag> JITThunks::ctiNativeTailCallWithoutSavedTags(VM&)
{
    ASSERT(Options::useJIT());
    return ctiStub(CommonJITThunkID::NativeTailCallWithoutSavedTags).code();
}

CodePtr<JITThunkPtrTag> JITThunks::ctiInternalFunctionCall(VM&)
{
    ASSERT(Options::useJIT());
    return ctiStub(CommonJITThunkID::InternalFunctionCall).code();
}

CodePtr<JITThunkPtrTag> JITThunks::ctiInternalFunctionConstruct(VM&)
{
    ASSERT(Options::useJIT());
    return ctiStub(CommonJITThunkID::InternalFunctionConstruct).code();
}

template <typename GenerateThunk>
MacroAssemblerCodeRef<JITThunkPtrTag> JITThunks::ctiStubImpl(ThunkGenerator key, GenerateThunk generateThunk)
{
    Locker locker { m_lock };

    auto handleEntry = [&] (Entry& entry) {
        if (entry.needsCrossModifyingCodeFence && !isCompilationThread()) {
            // The main thread will issue a crossModifyingCodeFence before running
            // any code the compiler thread generates, including any thunks that they
            // generate. However, the main thread may grab the thunk a compiler thread
            // generated before we've issued that crossModifyingCodeFence. Hence, we
            // conservatively say that when the main thread grabs a thunk generated
            // from a compiler thread for the first time, it issues a crossModifyingCodeFence.
            WTF::crossModifyingCodeFence();
            entry.needsCrossModifyingCodeFence = false;
        }

        return MacroAssemblerCodeRef<JITThunkPtrTag>(*entry.handle);
    };

    {
        auto iter = m_ctiStubMap.find(key);
        if (iter != m_ctiStubMap.end())
            return handleEntry(iter->value);
    }

    // We do two lookups on first addition to the hash table because generateThunk may add to it.
    MacroAssemblerCodeRef<JITThunkPtrTag> codeRef = generateThunk();

    bool needsCrossModifyingCodeFence = isCompilationThread();
    auto addResult = m_ctiStubMap.add(key, Entry { PackedRefPtr<ExecutableMemoryHandle>(codeRef.executableMemory()), needsCrossModifyingCodeFence });
    RELEASE_ASSERT(addResult.isNewEntry); // Thunks aren't recursive, so anything we generated transitively shouldn't have generated 'key'.
    return handleEntry(addResult.iterator->value);
}

MacroAssemblerCodeRef<JITThunkPtrTag> JITThunks::ctiStub(VM& vm, ThunkGenerator generator)
{
    return ctiStubImpl(generator, [&] {
        return generator(vm);
    });
}

MacroAssemblerCodeRef<JITThunkPtrTag> JITThunks::ctiStub(CommonJITThunkID thunkID)
{
    auto result = m_commonThunks[static_cast<unsigned>(thunkID)];
    ASSERT(result);
    return result;
}

MacroAssemblerCodeRef<JITThunkPtrTag> JITThunks::ctiSlowPathFunctionStub(VM& vm, SlowPathFunction slowPathFunction)
{
    auto key = std::bit_cast<ThunkGenerator>(slowPathFunction);
    return ctiStubImpl(key, [&] {
        return JITSlowPathCall::generateThunk(vm, slowPathFunction);
    });
}

struct JITThunks::HostKeySearcher {
    static unsigned hash(const HostFunctionKey& key) { return WeakNativeExecutableHash::hash(key); }
    static bool equal(const Weak<NativeExecutable>& a, const HostFunctionKey& b) { return WeakNativeExecutableHash::equal(a, b); }
};

struct JITThunks::NativeExecutableTranslator {
    static unsigned hash(const NativeExecutable* key) { return WeakNativeExecutableHash::hash(key); }
    static bool equal(const Weak<NativeExecutable>& a, const NativeExecutable* b) { return WeakNativeExecutableHash::equal(a, b); }
    static void translate(Weak<NativeExecutable>& location, NativeExecutable* executable, unsigned)
    {
        location = Weak<NativeExecutable>(executable, executable->vm().jitStubs.get());
    }
};

void JITThunks::finalize(Handle<Unknown> handle, void*)
{
    auto* nativeExecutable = static_cast<NativeExecutable*>(handle.get().asCell());
    auto hostFunctionKey = std::make_tuple(nativeExecutable->function(), nativeExecutable->constructor(), nativeExecutable->implementationVisibility(), nativeExecutable->name());
    {
        AssertNoGC assertNoGC;
        auto iterator = m_nativeExecutableSet.find<HostKeySearcher>(hostFunctionKey);
        // Because this finalizer is called, this means that we still have dead Weak<> in m_nativeExecutableSet.
        ASSERT(iterator != m_nativeExecutableSet.end());
        ASSERT(iterator->unsafeImpl()->state() == WeakImpl::State::Finalized);
        m_nativeExecutableSet.remove(iterator);
    }
}

NativeExecutable* JITThunks::hostFunctionStub(VM& vm, TaggedNativeFunction function, TaggedNativeFunction constructor, ImplementationVisibility implementationVisibility, const String& name)
{
    return hostFunctionStub(vm, function, constructor, nullptr, implementationVisibility, NoIntrinsic, nullptr, name);
}

NativeExecutable* JITThunks::hostFunctionStub(VM& vm, TaggedNativeFunction function, TaggedNativeFunction constructor, ThunkGenerator generator, ImplementationVisibility implementationVisibility, Intrinsic intrinsic, const DOMJIT::Signature* signature, const String& name)
{
    ASSERT(!isCompilationThread());    
    ASSERT(Options::useJIT());

    auto hostFunctionKey = std::make_tuple(function, constructor, implementationVisibility, name);
    {
        AssertNoGC assertNoGC;
        auto iterator = m_nativeExecutableSet.find<HostKeySearcher>(hostFunctionKey);
        if (iterator != m_nativeExecutableSet.end()) {
            // It is possible that this returns Weak<> which is Dead, but not finalized.
            // We should not use this reference to store value created in the subsequent sequence, since allocating NativeExecutable can cause GC, which changes this Set.
            if (auto* executable = iterator->get())
                return executable;
        }
    }

    RefPtr<JSC::JITCode> forCall;
    if (generator) {
        MacroAssemblerCodeRef<JSEntryPtrTag> entry = generator(vm).retagged<JSEntryPtrTag>();
        forCall = adoptRef(new DirectJITCode(entry, entry.code(), JITType::HostCallThunk, intrinsic));
    } else if (signature)
        forCall = adoptRef(new NativeDOMJITCode(MacroAssemblerCodeRef<JSEntryPtrTag>::createSelfManagedCodeRef(ctiNativeCall(vm).retagged<JSEntryPtrTag>()), JITType::HostCallThunk, intrinsic, signature));
    else
        forCall = adoptRef(new NativeJITCode(MacroAssemblerCodeRef<JSEntryPtrTag>::createSelfManagedCodeRef(ctiNativeCall(vm).retagged<JSEntryPtrTag>()), JITType::HostCallThunk, intrinsic));
    
    Ref<JSC::JITCode> forConstruct = adoptRef(*new NativeJITCode(MacroAssemblerCodeRef<JSEntryPtrTag>::createSelfManagedCodeRef(ctiNativeConstruct(vm).retagged<JSEntryPtrTag>()), JITType::HostCallThunk, NoIntrinsic));
    
    NativeExecutable* nativeExecutable = NativeExecutable::create(vm, forCall.releaseNonNull(), function, WTFMove(forConstruct), constructor, implementationVisibility, name);
    {
        AssertNoGC assertNoGC;
        auto addResult = m_nativeExecutableSet.add<NativeExecutableTranslator>(nativeExecutable);
        if (!addResult.isNewEntry) {
            // Override the existing Weak<NativeExecutable> with the new one since it is dead.
            ASSERT(!*addResult.iterator);
            *addResult.iterator = Weak<NativeExecutable>(nativeExecutable, this);
            ASSERT(*addResult.iterator);
#if ASSERT_ENABLED
            auto iterator = m_nativeExecutableSet.find<HostKeySearcher>(hostFunctionKey);
            ASSERT(iterator != m_nativeExecutableSet.end());
            ASSERT(iterator->get() == nativeExecutable);
            ASSERT(iterator->unsafeImpl()->state() == WeakImpl::State::Live);
#endif
        }
    }
    return nativeExecutable;
}

NativeExecutable* JITThunks::hostFunctionStub(VM& vm, TaggedNativeFunction function, ThunkGenerator generator, ImplementationVisibility implementationVisibility, Intrinsic intrinsic, const String& name)
{
    return hostFunctionStub(vm, function, callHostFunctionAsConstructor, generator, implementationVisibility, intrinsic, nullptr, name);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(JIT)
