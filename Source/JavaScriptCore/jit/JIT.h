/*
 * Copyright (C) 2008-2022 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(JIT)

#define ASSERT_JIT_OFFSET(actual, expected) ASSERT_WITH_MESSAGE(actual == expected, "JIT Offset \"%s\" should be %d, not %d.\n", #expected, static_cast<int>(expected), static_cast<int>(actual));

#include "BaselineJITCode.h"
#include "CodeBlock.h"
#include "CommonSlowPaths.h"
#include "JITDisassembler.h"
#include "JITInlineCacheGenerator.h"
#include "JITMathIC.h"
#include "JITRightShiftGenerator.h"
#include "JSInterfaceJIT.h"
#include "LLIntData.h"
#include "PCToCodeOriginMap.h"
#include <wtf/SequesteredMalloc.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

    enum OpcodeID : unsigned;

    class ArrayAllocationProfile;
    class BaselineJITPlan;
    class CallLinkInfo;
    class CodeBlock;
    class FunctionExecutable;
    class JIT;
    class Identifier;
    class BlockDirectory;
    class Register;
    class StructureChain;
    class StructureStubInfo;

    template<typename> struct BaseInstruction;
    struct JSOpcodeTraits;
    using JSInstruction = BaseInstruction<JSOpcodeTraits>;

    struct OperandTypes;
    struct SimpleJumpTable;
    struct StringJumpTable;

    struct OpPutByVal;
    struct OpPutByValDirect;
    struct OpPutPrivateName;
    struct OpPutToScope;

    template<PtrTag tag>
    struct CallRecord {
        MacroAssembler::Call from;
        CodePtr<tag> callee;

        CallRecord()
        {
        }

        CallRecord(MacroAssembler::Call from, CodePtr<tag> callee)
            : from(from)
            , callee(callee)
        {
        }
    };

    using FarCallRecord = CallRecord<OperationPtrTag>;
    using NearCallRecord = CallRecord<JSInternalPtrTag>;

    struct NearJumpRecord {
        MacroAssembler::Jump from;
        CodeLocationLabel<JITThunkPtrTag> target;

        NearJumpRecord() = default;
        NearJumpRecord(MacroAssembler::Jump from, CodeLocationLabel<JITThunkPtrTag> target)
            : from(from)
            , target(target)
        { }
    };

    struct JumpTable {
        MacroAssembler::Jump from;
        unsigned toBytecodeOffset;

        JumpTable(MacroAssembler::Jump f, unsigned t)
            : from(f)
            , toBytecodeOffset(t)
        {
        }
    };

    struct SlowCaseEntry {
        MacroAssembler::Jump from;
        BytecodeIndex to;
        
        SlowCaseEntry(MacroAssembler::Jump f, BytecodeIndex t)
            : from(f)
            , to(t)
        {
        }
    };

    struct SwitchRecord {
        enum Type {
            Immediate,
            Character,
            String
        };

        Type type;

        BytecodeIndex bytecodeIndex;
        unsigned defaultOffset;
        unsigned tableIndex;

        SwitchRecord(unsigned tableIndex, BytecodeIndex bytecodeIndex, unsigned defaultOffset, Type type)
            : type(type)
            , bytecodeIndex(bytecodeIndex)
            , defaultOffset(defaultOffset)
            , tableIndex(tableIndex)
        {
        }
    };

    struct CallCompilationInfo {
        MacroAssembler::Label doneLocation;
        BaselineUnlinkedCallLinkInfo* unlinkedCallLinkInfo;
    };

    class JIT final : public JSInterfaceJIT {
        WTF_MAKE_TZONE_NON_HEAP_ALLOCATABLE(JIT);

        friend class JITSlowPathCall;
        friend class JITStubCall;
        friend class JITThunks;

        using MacroAssembler::Jump;
        using MacroAssembler::JumpList;
        using MacroAssembler::Label;

        using Base = JSInterfaceJIT;

    public:
        JIT(VM&, BaselineJITPlan&, CodeBlock*);
        ~JIT();

        VM& vm() { return *JSInterfaceJIT::vm(); }

        RefPtr<BaselineJITCode> compileAndLinkWithoutFinalizing(JITCompilationEffort);
        static CompilationResult finalizeOnMainThread(CodeBlock*, BaselineJITPlan&, RefPtr<BaselineJITCode>);

        static void doMainThreadPreparationBeforeCompile(VM&);
        
        static CompilationResult compileSync(VM&, CodeBlock*, JITCompilationEffort);

        static unsigned frameRegisterCountFor(UnlinkedCodeBlock*);
        static unsigned frameRegisterCountFor(CodeBlock*);
        static int stackPointerOffsetFor(UnlinkedCodeBlock*);
        static int stackPointerOffsetFor(CodeBlock*);

        JS_EXPORT_PRIVATE static UncheckedKeyHashMap<CString, Seconds> compileTimeStats();
        JS_EXPORT_PRIVATE static Seconds totalCompileTime();

    private:
        void privateCompileMainPass();
        void privateCompileLinkPass();
        void privateCompileSlowCases();
        RefPtr<BaselineJITCode> link(LinkBuffer&);

        // Add a call out from JIT code, without an exception check.
        Call appendCall(const CodePtr<CFunctionPtrTag> function)
        {
            Call functionCall = call(OperationPtrTag);
            m_farCalls.append(FarCallRecord(functionCall, function.retagged<OperationPtrTag>()));
            return functionCall;
        }

        void appendCall(Address function)
        {
            call(function, OperationPtrTag);
        }

        template<size_t minAlign, typename Bytecode>
        Address computeBaseAddressForMetadata(const Bytecode&, GPRReg);

        template <typename Bytecode>
        void loadPtrFromMetadata(const Bytecode&, size_t offset, GPRReg);

        template <typename Bytecode>
        void load32FromMetadata(const Bytecode&, size_t offset, GPRReg);

        template <typename Bytecode>
        void load8FromMetadata(const Bytecode&, size_t offset, GPRReg);

        template <typename ValueType, typename Bytecode>
        void store8ToMetadata(ValueType, const Bytecode&, size_t offset);

        template <typename Bytecode>
        void store32ToMetadata(GPRReg, const Bytecode&, size_t offset);

        template <typename Bytecode>
        void storePtrToMetadata(GPRReg, const Bytecode&, size_t offset);

        template <typename Bytecode>
        void materializePointerIntoMetadata(const Bytecode&, size_t offset, GPRReg);

    public:
        void loadConstant(unsigned constantIndex, GPRReg);
        void loadStructureStubInfo(StructureStubInfoIndex, GPRReg);
        static void emitMaterializeMetadataAndConstantPoolRegisters(CCallHelpers&);
    private:
        void loadGlobalObject(GPRReg);

        // Assuming GPRInfo::jitDataRegister is available.
        static void loadGlobalObject(CCallHelpers&, GPRReg);
        static void loadConstant(CCallHelpers&, unsigned constantIndex, GPRReg);
        static void loadStructureStubInfo(CCallHelpers&, StructureStubInfoIndex, GPRReg);

        void loadCodeBlockConstant(VirtualRegister, JSValueRegs);
        void loadCodeBlockConstantPayload(VirtualRegister, RegisterID);
#if USE(JSVALUE32_64)
        void loadCodeBlockConstantTag(VirtualRegister, RegisterID);
#endif

        void exceptionCheck(Jump jumpToHandler);
        void exceptionCheck();

        void advanceToNextCheckpoint();
        void emitJumpSlowToHotForCheckpoint(Jump);
        void setFastPathResumePoint();
        Label fastPathResumePoint() const;

        void addSlowCase(Jump);
        void addSlowCase(const JumpList&);
        void addSlowCase();
        void addJump(Jump, int);
        void addJump(const JumpList&, int);
        void emitJumpSlowToHot(Jump, int);

        template<typename Op>
        void compileOpCall(const JSInstruction*);

        template<typename Op>
        void compileSetupFrame(const Op&);

        template<typename Op>
        bool compileTailCall(const Op&, BaselineUnlinkedCallLinkInfo*, unsigned callLinkInfoIndex);
        template<typename Op>
        void compileCallDirectEval(const Op&);
        void compileCallDirectEvalSlowCase(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        template<typename Op>
        void emitPutCallResult(const Op&);

#if USE(JSVALUE64)
        template<typename Op> void compileOpStrictEq(const JSInstruction*);
        template<typename Op> void compileOpStrictEqJump(const JSInstruction*);
#elif USE(JSVALUE32_64)
        void compileOpEqCommon(VirtualRegister src1, VirtualRegister src2);
        void compileOpEqSlowCommon(Vector<SlowCaseEntry>::iterator&);
        void compileOpStrictEqCommon(VirtualRegister src1,  VirtualRegister src2);
#endif

        enum WriteBarrierMode { UnconditionalWriteBarrier, ShouldFilterBase, ShouldFilterValue, ShouldFilterBaseAndValue };
        // value register in write barrier is used before any scratch registers
        // so may safely be the same as either of the scratch registers.
        void emitWriteBarrier(VirtualRegister owner, WriteBarrierMode);
        void emitWriteBarrier(VirtualRegister owner, VirtualRegister value, WriteBarrierMode);
        void emitWriteBarrier(JSCell* owner);
        void emitWriteBarrier(GPRReg owner);

        template<typename Bytecode> void emitValueProfilingSite(const Bytecode&, JSValueRegs);
        template<typename Bytecode> void emitValueProfilingSite(const Bytecode&, BytecodeIndex, JSValueRegs);

        template<typename Op>
        static inline constexpr bool isProfiledOp = std::is_same_v<decltype(Op::Metadata::m_profile), ValueProfile>;
        template<typename Op>
        void emitValueProfilingSiteIfProfiledOpcode(Op bytecode)
        {
            // This assumes that the value to profile is in jsRegT10.
            if constexpr (isProfiledOp<Op>)
                emitValueProfilingSite(bytecode, jsRegT10);
            else
                UNUSED_PARAM(bytecode);
        }

        template <typename Bytecode>
        void emitArrayProfilingSiteWithCell(const Bytecode&, RegisterID cellGPR, RegisterID scratchGPR);
        template <typename Bytecode>
        void emitArrayProfilingSiteWithCell(const Bytecode&, ptrdiff_t, RegisterID cellGPR, RegisterID scratchGPR);

        void emitArrayProfilingSiteWithCellAndProfile(RegisterID cellGPR, RegisterID profileGPR, RegisterID scratchGPR);

        template<typename Op>
        ECMAMode ecmaMode(Op);

        void emitGetVirtualRegister(VirtualRegister src, JSValueRegs dst);
        void emitGetVirtualRegisterPayload(VirtualRegister src, RegisterID dst);
        void emitPutVirtualRegister(VirtualRegister dst, JSValueRegs src);

#if USE(JSVALUE32_64)
        void emitGetVirtualRegisterTag(VirtualRegister src, RegisterID dst);
#elif USE(JSVALUE64)
        // Machine register variants purely for convenience
        void emitGetVirtualRegister(VirtualRegister src, RegisterID dst);
        void emitPutVirtualRegister(VirtualRegister dst, RegisterID from);

        Jump emitJumpIfNotInt(RegisterID, RegisterID, RegisterID scratch);
        void emitJumpSlowCaseIfNotInt(RegisterID, RegisterID, RegisterID scratch);
        void emitJumpSlowCaseIfNotInt(RegisterID);
#endif

        void emitJumpSlowCaseIfNotInt(JSValueRegs);

        void emitJumpSlowCaseIfNotJSCell(JSValueRegs);
        void emitJumpSlowCaseIfNotJSCell(JSValueRegs, VirtualRegister);

        template<typename Op>
        void emit_compare(const JSInstruction*, RelationalCondition);
        template <typename EmitCompareFunctor>
        void emit_compareImpl(VirtualRegister op1, VirtualRegister op2, RelationalCondition, const EmitCompareFunctor&);
        template<typename Op>
        void emit_compareAndJump(const JSInstruction*, RelationalCondition);
        template<typename Op>
        void emit_compareUnsigned(const JSInstruction*, RelationalCondition);
        void emit_compareUnsignedImpl(VirtualRegister dst, VirtualRegister op1, VirtualRegister op2, RelationalCondition);
        template<typename Op>
        void emit_compareUnsignedAndJump(const JSInstruction*, RelationalCondition);
        void emit_compareUnsignedAndJumpImpl(VirtualRegister op1, VirtualRegister op2, unsigned target, RelationalCondition);
        template<typename Op, typename SlowOperation>
        void emit_compareSlow(const JSInstruction*, DoubleCondition, SlowOperation, Vector<SlowCaseEntry>::iterator&);
        template<typename SlowOperation, typename HanldeReturnValueGPRFunctor, typename EmitDoubleCompareFunctor>
        void emit_compareSlowImpl(VirtualRegister op1, VirtualRegister op2, size_t instructionSize, SlowOperation, Vector<SlowCaseEntry>::iterator&, const HanldeReturnValueGPRFunctor&, const EmitDoubleCompareFunctor&);
        template<typename Op, typename SlowOperation>
        void emit_compareAndJumpSlow(const JSInstruction*, DoubleCondition, SlowOperation, bool invert, Vector<SlowCaseEntry>::iterator&);

        void emit_op_add(const JSInstruction*);
        void emit_op_bitand(const JSInstruction*);
        void emit_op_bitor(const JSInstruction*);
        void emit_op_bitxor(const JSInstruction*);
        void emit_op_bitnot(const JSInstruction*);
        void emit_op_call(const JSInstruction*);
        void emit_op_call_ignore_result(const JSInstruction*);
        void emit_op_tail_call(const JSInstruction*);
        void emit_op_call_direct_eval(const JSInstruction*);
        void emit_op_call_varargs(const JSInstruction*);
        void emit_op_tail_call_varargs(const JSInstruction*);
        void emit_op_tail_call_forward_arguments(const JSInstruction*);
        void emit_op_construct_varargs(const JSInstruction*);
        void emit_op_super_construct_varargs(const JSInstruction*);
        void emit_op_catch(const JSInstruction*);
        void emit_op_construct(const JSInstruction*);
        void emit_op_super_construct(const JSInstruction*);
        void emit_op_create_this(const JSInstruction*);
        void emit_op_to_this(const JSInstruction*);
        void emit_op_get_argument(const JSInstruction*);
        void emit_op_argument_count(const JSInstruction*);
        void emit_op_get_rest_length(const JSInstruction*);
        void emit_op_check_tdz(const JSInstruction*);
        void emit_op_identity_with_profile(const JSInstruction*);
        void emit_op_debug(const JSInstruction*);
        void emit_op_del_by_id(const JSInstruction*);
        void emitSlow_op_del_by_id(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emit_op_del_by_val(const JSInstruction*);
        void emitSlow_op_del_by_val(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emit_op_div(const JSInstruction*);
        void emit_op_end(const JSInstruction*);
        void emit_op_enter(const JSInstruction*);
        void emit_op_get_scope(const JSInstruction*);
        void emit_op_eq(const JSInstruction*);
        void emit_op_eq_null(const JSInstruction*);
        void emit_op_below(const JSInstruction*);
        void emit_op_beloweq(const JSInstruction*);
        void emit_op_try_get_by_id(const JSInstruction*);
        void emit_op_get_by_id(const JSInstruction*);
        void emit_op_get_length(const JSInstruction*);
        void emit_op_get_by_id_with_this(const JSInstruction*);
        void emit_op_get_by_id_direct(const JSInstruction*);
        void emit_op_get_by_val(const JSInstruction*);
        void emit_op_get_by_val_with_this(const JSInstruction*);
        void emit_op_get_private_name(const JSInstruction*);
        void emit_op_set_private_brand(const JSInstruction*);
        void emit_op_check_private_brand(const JSInstruction*);
        void emit_op_get_argument_by_val(const JSInstruction*);
        void emit_op_get_prototype_of(const JSInstruction*);
        void emit_op_in_by_id(const JSInstruction*);
        void emit_op_in_by_val(const JSInstruction*);
        void emit_op_has_private_name(const JSInstruction*);
        void emit_op_has_private_brand(const JSInstruction*);
        void emit_op_init_lazy_reg(const JSInstruction*);
        void emit_op_overrides_has_instance(const JSInstruction*);
        void emit_op_instanceof(const JSInstruction*);
        void emit_op_is_empty(const JSInstruction*);
        void emit_op_typeof_is_undefined(const JSInstruction*);
        void emit_op_typeof_is_function(const JSInstruction*);
        void emit_op_is_undefined_or_null(const JSInstruction*);
        void emit_op_is_boolean(const JSInstruction*);
        void emit_op_is_number(const JSInstruction*);
#if USE(BIGINT32)
        void emit_op_is_big_int(const JSInstruction*);
#else
        [[noreturn]] void emit_op_is_big_int(const JSInstruction*);
#endif
        void emit_op_is_object(const JSInstruction*);
        void emit_op_is_cell_with_type(const JSInstruction*);
        void emit_op_has_structure_with_flags(const JSInstruction*);
        void emit_op_jeq_null(const JSInstruction*);
        void emit_op_jfalse(const JSInstruction*);
        void emit_op_jmp(const JSInstruction*);
        void emit_op_jneq_null(const JSInstruction*);
        void emit_op_jundefined_or_null(const JSInstruction*);
        void emit_op_jnundefined_or_null(const JSInstruction*);
        void emit_op_jeq_ptr(const JSInstruction*);
        void emit_op_jneq_ptr(const JSInstruction*);
        void emit_op_less(const JSInstruction*);
        void emit_op_lesseq(const JSInstruction*);
        void emit_op_greater(const JSInstruction*);
        void emit_op_greatereq(const JSInstruction*);
        void emit_op_jless(const JSInstruction*);
        void emit_op_jlesseq(const JSInstruction*);
        void emit_op_jgreater(const JSInstruction*);
        void emit_op_jgreatereq(const JSInstruction*);
        void emit_op_jnless(const JSInstruction*);
        void emit_op_jnlesseq(const JSInstruction*);
        void emit_op_jngreater(const JSInstruction*);
        void emit_op_jngreatereq(const JSInstruction*);
        void emit_op_jeq(const JSInstruction*);
        void emit_op_jneq(const JSInstruction*);
        void emit_op_jstricteq(const JSInstruction*);
        void emit_op_jnstricteq(const JSInstruction*);
        void emit_op_jbelow(const JSInstruction*);
        void emit_op_jbeloweq(const JSInstruction*);
        void emit_op_jtrue(const JSInstruction*);
        void emit_op_loop_hint(const JSInstruction*);
        void emit_op_check_traps(const JSInstruction*);
        void emit_op_nop(const JSInstruction*);
        void emit_op_super_sampler_begin(const JSInstruction*);
        void emit_op_super_sampler_end(const JSInstruction*);
        void emit_op_lshift(const JSInstruction*);
        void emit_op_mod(const JSInstruction*);
        void emit_op_pow(const JSInstruction*);
        void emit_op_mov(const JSInstruction*);
        void emit_op_mul(const JSInstruction*);
        void emit_op_negate(const JSInstruction*);
        void emit_op_neq(const JSInstruction*);
        void emit_op_neq_null(const JSInstruction*);
        void emit_op_new_array(const JSInstruction*);
        void emit_op_new_array_with_size(const JSInstruction*);
        void emit_op_new_func(const JSInstruction*);
        void emit_op_new_func_exp(const JSInstruction*);
        void emit_op_new_generator_func(const JSInstruction*);
        void emit_op_new_generator_func_exp(const JSInstruction*);
        void emit_op_new_async_func(const JSInstruction*);
        void emit_op_new_async_func_exp(const JSInstruction*);
        void emit_op_new_async_generator_func(const JSInstruction*);
        void emit_op_new_async_generator_func_exp(const JSInstruction*);
        void emit_op_new_object(const JSInstruction*);
        void emit_op_new_reg_exp(const JSInstruction*);
        void emit_op_create_lexical_environment(const JSInstruction*);
        void emit_op_create_direct_arguments(const JSInstruction*);
        void emit_op_create_scoped_arguments(const JSInstruction*);
        void emit_op_create_cloned_arguments(const JSInstruction*);
        void emit_op_not(const JSInstruction*);
        void emit_op_nstricteq(const JSInstruction*);
        void emit_op_dec(const JSInstruction*);
        void emit_op_inc(const JSInstruction*);
        void emit_op_profile_type(const JSInstruction*);
        void emit_op_profile_control_flow(const JSInstruction*);
        void emit_op_get_parent_scope(const JSInstruction*);
        void emit_op_put_by_id(const JSInstruction*);
        template<typename Op = OpPutByVal>
        void emit_op_put_by_val(const JSInstruction*);
        void emit_op_put_by_val_direct(const JSInstruction*);
        void emit_op_put_private_name(const JSInstruction*);
        void emit_op_put_getter_by_id(const JSInstruction*);
        void emit_op_put_setter_by_id(const JSInstruction*);
        void emit_op_put_getter_setter_by_id(const JSInstruction*);
        void emit_op_put_getter_by_val(const JSInstruction*);
        void emit_op_put_setter_by_val(const JSInstruction*);
        void emit_op_ret(const JSInstruction*);
        void emit_op_rshift(const JSInstruction*);
        void emit_op_set_function_name(const JSInstruction*);
        void emit_op_stricteq(const JSInstruction*);
        void emit_op_sub(const JSInstruction*);
        void emit_op_switch_char(const JSInstruction*);
        void emit_op_switch_imm(const JSInstruction*);
        void emit_op_switch_string(const JSInstruction*);
        void emit_op_tear_off_arguments(const JSInstruction*);
        void emit_op_throw(const JSInstruction*);
        void emit_op_to_number(const JSInstruction*);
        void emit_op_to_numeric(const JSInstruction*);
        void emit_op_to_string(const JSInstruction*);
        void emit_op_to_object(const JSInstruction*);
        void emit_op_to_primitive(const JSInstruction*);
        void emit_op_unexpected_load(const JSInstruction*);
        void emit_op_unsigned(const JSInstruction*);
        void emit_op_urshift(const JSInstruction*);
        void emit_op_get_internal_field(const JSInstruction*);
        void emit_op_put_internal_field(const JSInstruction*);
        void emit_op_log_shadow_chicken_prologue(const JSInstruction*);
        void emit_op_log_shadow_chicken_tail(const JSInstruction*);
        void emit_op_to_property_key(const JSInstruction*);
        void emit_op_to_property_key_or_number(const JSInstruction*);

        template<typename OpcodeType>
        void generateGetByValSlowCase(const OpcodeType&, Vector<SlowCaseEntry>::iterator&);

        void emit_op_get_property_enumerator(const JSInstruction*);
        void emit_op_enumerator_next(const JSInstruction*);
        void emit_op_enumerator_get_by_val(const JSInstruction*);
        void emitSlow_op_enumerator_get_by_val(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);

        template<typename OpcodeType, typename SlowPathFunctionType>
        void emit_enumerator_has_propertyImpl(const OpcodeType&, SlowPathFunctionType);
        void emit_op_enumerator_in_by_val(const JSInstruction*);
        void emit_op_enumerator_has_own_property(const JSInstruction*);

        template<typename OpcodeType>
        void generatePutByValSlowCase(const OpcodeType&, Vector<SlowCaseEntry>::iterator&);
        void emit_op_enumerator_put_by_val(const JSInstruction*);
        void emitSlow_op_enumerator_put_by_val(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);

        void emitSlow_op_add(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_call_direct_eval(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_eq(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_callee(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_try_get_by_id(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_id(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_length(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_id_with_this(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_id_direct(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_val(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_by_val_with_this(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_private_name(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_set_private_brand(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_check_private_brand(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_get_argument_by_val(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_in_by_id(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_in_by_val(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_has_private_name(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_has_private_brand(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_instanceof(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_less(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_lesseq(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_greater(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_greatereq(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jless(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jlesseq(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jgreater(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jgreatereq(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jnless(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jnlesseq(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jngreater(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jngreatereq(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jeq(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jneq(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jstricteq(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_jnstricteq(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_loop_hint(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_enter(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_check_traps(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_mod(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_pow(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_mul(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_negate(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_neq(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_new_object(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_by_id(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_by_val(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_by_val_direct(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_put_private_name(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emitSlow_op_sub(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);

        void emit_op_resolve_scope(const JSInstruction*);
        void emitSlow_op_resolve_scope(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emit_op_get_from_scope(const JSInstruction*);
        void emitSlow_op_get_from_scope(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emit_op_put_to_scope(const JSInstruction*);
        void emit_op_get_from_arguments(const JSInstruction*);
        void emit_op_put_to_arguments(const JSInstruction*);
        void emitSlow_op_put_to_scope(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);

        void emitSlowCaseCall(Vector<SlowCaseEntry>::iterator&, SlowPathFunction);

        void emit_op_iterator_open(const JSInstruction*);
        void emitSlow_op_iterator_open(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);
        void emit_op_iterator_next(const JSInstruction*);
        void emitSlow_op_iterator_next(const JSInstruction*, Vector<SlowCaseEntry>::iterator&);

        void emitHasPrivate(VirtualRegister dst, VirtualRegister base, VirtualRegister propertyOrBrand, AccessType);
        void emitHasPrivateSlow(AccessType, Vector<SlowCaseEntry>::iterator&);

        template<typename Op>
        void emitNewFuncCommon(const JSInstruction*);
        template<typename Op>
        void emitNewFuncExprCommon(const JSInstruction*);
        void emitVarInjectionCheck(bool needsVarInjectionChecks, GPRReg);
        void emitVarReadOnlyCheck(ResolveType, GPRReg scratchGPR);
        void emitNotifyWriteWatchpoint(GPRReg pointerToSet);
        void emitGetScope(VirtualRegister destination);
        void emitCheckTraps();

        bool isKnownCell(VirtualRegister);

        JSValue getConstantOperand(VirtualRegister);

#if USE(JSVALUE64)
        bool isOperandConstantDouble(VirtualRegister);
        double getOperandConstantDouble(VirtualRegister src);
#endif
        bool isOperandConstantInt(VirtualRegister);
        int32_t getOperandConstantInt(VirtualRegister src);
        bool isOperandConstantChar(VirtualRegister);

        template <typename Op, typename Generator, typename ProfiledFunction, typename NonProfiledFunction>
        void emitMathICFast(JITUnaryMathIC<Generator>*, const JSInstruction*, ProfiledFunction, NonProfiledFunction);
        template <typename Op, typename Generator, typename ProfiledFunction, typename NonProfiledFunction>
        void emitMathICFast(JITBinaryMathIC<Generator>*, const JSInstruction*, ProfiledFunction, NonProfiledFunction);

        template <typename Op, typename Generator, typename ProfiledRepatchFunction, typename ProfiledFunction, typename RepatchFunction>
        void emitMathICSlow(JITBinaryMathIC<Generator>*, const JSInstruction*, ProfiledRepatchFunction, ProfiledFunction, RepatchFunction);
        template <typename Op, typename Generator, typename ProfiledRepatchFunction, typename ProfiledFunction, typename RepatchFunction>
        void emitMathICSlow(JITUnaryMathIC<Generator>*, const JSInstruction*, ProfiledRepatchFunction, ProfiledFunction, RepatchFunction);

    private:
        static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_put_to_scopeGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> op_throw_handlerGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> op_check_traps_handlerGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_get_from_scopeGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> slow_op_resolve_scopeGenerator(VM&);
        template <ResolveType>
        static MacroAssemblerCodeRef<JITThunkPtrTag> generateOpGetFromScopeThunk(VM&);
        template <ResolveType>
        static MacroAssemblerCodeRef<JITThunkPtrTag> generateOpResolveScopeThunk(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> op_enter_handlerGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> valueIsTruthyGenerator(VM&);
        static MacroAssemblerCodeRef<JITThunkPtrTag> valueIsFalseyGenerator(VM&);

        Jump getSlowCase(Vector<SlowCaseEntry>::iterator& iter)
        {
            return iter++->from;
        }
        void linkSlowCase(Vector<SlowCaseEntry>::iterator& iter)
        {
            if (iter->from.isSet())
                iter->from.link(this);
            ++iter;
        }
        void linkAllSlowCasesForBytecodeIndex(Vector<SlowCaseEntry>& slowCases,
            Vector<SlowCaseEntry>::iterator&, BytecodeIndex bytecodeOffset);

        void linkAllSlowCases(Vector<SlowCaseEntry>::iterator& iter)
        {
            linkAllSlowCasesForBytecodeIndex(m_slowCases, iter, m_bytecodeIndex);
        }

        bool hasAnySlowCases(Vector<SlowCaseEntry>& slowCases, Vector<SlowCaseEntry>::iterator&, BytecodeIndex bytecodeOffset);
        bool hasAnySlowCases(Vector<SlowCaseEntry>::iterator& iter)
        {
            return hasAnySlowCases(m_slowCases, iter, m_bytecodeIndex);
        }

        template<typename OperationType>
        MacroAssembler::Call appendCallWithExceptionCheck(const CodePtr<CFunctionPtrTag>);
        template<typename OperationType>
        void appendCallWithExceptionCheck(Address);
        template<typename OperationType>
        MacroAssembler::Call appendCallWithExceptionCheckSetJSValueResult(const CodePtr<CFunctionPtrTag>, VirtualRegister result);
        template<typename OperationType>
        void appendCallWithExceptionCheckSetJSValueResult(Address, VirtualRegister result);
        template<typename OperationType>
        MacroAssembler::Call appendCallSetJSValueResult(const CodePtr<CFunctionPtrTag>, VirtualRegister result);
        template<typename OperationType>
        void appendCallSetJSValueResult(Address, VirtualRegister result);
        template<typename OperationType, typename Bytecode>
        MacroAssembler::Call appendCallWithExceptionCheckSetJSValueResultWithProfile(const Bytecode&, const CodePtr<CFunctionPtrTag>, VirtualRegister result);
        template<typename OperationType, typename Bytecode>
        void appendCallWithExceptionCheckSetJSValueResultWithProfile(const Bytecode&, Address, VirtualRegister result);
        
        template<typename OperationType, typename... Args>
        requires OperationHasResult<OperationType>
        MacroAssembler::Call callOperation(OperationType operation, VirtualRegister result, Args... args)
        {
            setupArguments<OperationType>(args...);
            return appendCallWithExceptionCheckSetJSValueResult<OperationType>(operation, result);
        }

        template<typename OperationType, typename... Args>
        requires OperationHasResult<OperationType>
        void callOperation(Address target, VirtualRegister result, Args... args)
        {
            setupArgumentsForIndirectCall<OperationType>(target, args...);
            return appendCallWithExceptionCheckSetJSValueResult<OperationType>(Address(GPRInfo::nonArgGPR0, target.offset), result);
        }

        template<typename OperationType, typename... Args>
        requires OperationHasResult<OperationType>
        MacroAssembler::Call callOperationNoExceptionCheck(OperationType operation, VirtualRegister result, Args... args)
        {
            setupArguments<OperationType>(args...);
            return appendCallSetJSValueResult<OperationType>(operation, result);
        }

        template<typename OperationType, typename... Args>
        requires OperationHasResult<OperationType>
        void callOperationNoExceptionCheck(Address target, VirtualRegister result, Args... args)
        {
            setupArgumentsForIndirectCall<OperationType>(target, args...);
            return appendCallSetJSValueResult<OperationType>(Address(GPRInfo::nonArgGPR0, target.offset), result);
        }

        template<typename OperationType, typename... Args>
        MacroAssembler::Call callOperation(OperationType operation, Args... args)
        {
            setupArguments<OperationType>(args...);
            return appendCallWithExceptionCheck<OperationType>(operation);
        }

        template<typename OperationType, typename... Args>
        void callOperation(Address target, Args... args)
        {
            setupArgumentsForIndirectCall<OperationType>(target, args...);
            appendCallWithExceptionCheck<OperationType>(Address(GPRInfo::nonArgGPR0, target.offset));
        }

        template<typename Bytecode, typename OperationType, typename... Args>
        requires OperationHasResult<OperationType>
        MacroAssembler::Call callOperationWithProfile(const Bytecode& bytecode, OperationType operation, VirtualRegister result, Args... args)
        {
            setupArguments<OperationType>(args...);
            return appendCallWithExceptionCheckSetJSValueResultWithProfile<OperationType>(bytecode, operation, result);
        }

        template<typename OperationType, typename Bytecode, typename... Args>
        requires OperationHasResult<OperationType>
        void callOperationWithProfile(const Bytecode& bytecode, Address target, VirtualRegister result, Args... args)
        {
            setupArgumentsForIndirectCall<OperationType>(target, args...);
            return appendCallWithExceptionCheckSetJSValueResultWithProfile<OperationType>(bytecode, Address(GPRInfo::nonArgGPR0, target.offset), result);
        }

        template<typename OperationType, typename... Args>
        requires OperationHasResult<OperationType>
        MacroAssembler::Call callOperationWithResult(OperationType operation, JSValueRegs resultRegs, Args... args)
        {
            setupArguments<OperationType>(args...);
            auto result = appendCallWithExceptionCheck<OperationType>(operation);
            setupResults(resultRegs);
            return result;
        }

        template<typename OperationType, typename... Args>
        MacroAssembler::Call callOperationNoExceptionCheck(OperationType operation, Args... args)
        {
            setupArguments<OperationType>(args...);
            updateTopCallFrame();
            return appendCall(operation);
        }

        enum class ProfilingPolicy {
            ShouldEmitProfiling,
            NoProfiling
        };

        template<typename Op, typename SnippetGenerator>
        void emitBitBinaryOpFastPath(const JSInstruction* currentInstruction);

        template<typename Op>
        void emitRightShiftFastPath(const JSInstruction* currentInstruction, JITRightShiftGenerator::ShiftType);

        void updateTopCallFrame();

        // Loads the character value of a single character string into dst.
        void emitLoadCharacterString(RegisterID src, RegisterID dst, JumpList& failures);

        int jumpTarget(const JSInstruction*, int target);

#ifndef NDEBUG
        void printBytecodeOperandTypes(VirtualRegister src1, VirtualRegister src2);
#endif

#if ENABLE(SAMPLING_FLAGS)
        void setSamplingFlag(int32_t);
        void clearSamplingFlag(int32_t);
#endif

#if ENABLE(SAMPLING_COUNTERS)
        void emitCount(AbstractSamplingCounter&, int32_t = 1);
#endif

#if ENABLE(DFG_JIT)
        bool canBeOptimized() { return m_canBeOptimized; }
        bool shouldEmitProfiling() { return m_shouldEmitProfiling; }
#else
        bool canBeOptimized() { return false; }
        // Enables use of value profiler with tiered compilation turned off,
        // in which case all code gets profiled.
        bool shouldEmitProfiling() { return false; }
#endif

        void emitMaterializeMetadataAndConstantPoolRegisters();

        void emitSaveCalleeSaves();
        void emitRestoreCalleeSaves();

#if ASSERT_ENABLED
        static MacroAssemblerCodeRef<JITThunkPtrTag> consistencyCheckGenerator(VM&);
        void emitConsistencyCheck();
#endif

        static bool reportCompileTimes();
        static bool computeCompileTimes();

        void resetSP();

        JITConstantPool::Constant addToConstantPool(JITConstantPool::Type, void* payload = nullptr);
        std::tuple<BaselineUnlinkedStructureStubInfo*, StructureStubInfoIndex> addUnlinkedStructureStubInfo();
        BaselineUnlinkedCallLinkInfo* addUnlinkedCallLinkInfo();

        BaselineJITPlan& m_plan;
        Vector<FarCallRecord> m_farCalls;
        Vector<Label> m_labels;
        UncheckedKeyHashMap<BytecodeIndex, Label> m_checkpointLabels;
        UncheckedKeyHashMap<BytecodeIndex, Label> m_fastPathResumeLabels;
        Vector<JITGetByIdGenerator> m_getByIds;
        Vector<JITGetByValGenerator> m_getByVals;
        Vector<JITGetByIdWithThisGenerator> m_getByIdsWithThis;
        Vector<JITGetByValWithThisGenerator> m_getByValsWithThis;
        Vector<JITPutByIdGenerator> m_putByIds;
        Vector<JITPutByValGenerator> m_putByVals;
        Vector<JITInByIdGenerator> m_inByIds;
        Vector<JITInByValGenerator> m_inByVals;
        Vector<JITDelByIdGenerator> m_delByIds;
        Vector<JITDelByValGenerator> m_delByVals;
        Vector<JITInstanceOfGenerator> m_instanceOfs;
        Vector<JITPrivateBrandAccessGenerator> m_privateBrandAccesses;
        Vector<CallCompilationInfo> m_callCompilationInfo;
        Vector<JumpTable> m_jmpTable;

        BytecodeIndex m_bytecodeIndex;
        Vector<SlowCaseEntry> m_slowCases;
        Vector<SwitchRecord> m_switches;

#if ASSERT_ENABLED
        Label m_consistencyCheckLabel;
        Vector<Call> m_consistencyCheckCalls;
#endif

        unsigned m_getByIdIndex { UINT_MAX };
        unsigned m_getByValIndex { UINT_MAX };
        unsigned m_getByIdWithThisIndex { UINT_MAX };
        unsigned m_getByValWithThisIndex { UINT_MAX };
        unsigned m_putByIdIndex { UINT_MAX };
        unsigned m_putByValIndex { UINT_MAX };
        unsigned m_inByIdIndex { UINT_MAX };
        unsigned m_inByValIndex { UINT_MAX };
        unsigned m_delByValIndex { UINT_MAX };
        unsigned m_delByIdIndex { UINT_MAX };
        unsigned m_instanceOfIndex { UINT_MAX };
        unsigned m_privateBrandAccessIndex { UINT_MAX };
        unsigned m_bytecodeCountHavingSlowCase { 0 };
        
        Label m_arityCheck;

        std::unique_ptr<JITDisassembler> m_disassembler;
        RefPtr<Profiler::Compilation> m_compilation;

        PCToCodeOriginMapBuilder m_pcToCodeOriginMapBuilder;

        UncheckedKeyHashMap<const JSInstruction*, void*> m_instructionToMathIC;
        UncheckedKeyHashMap<const JSInstruction*, UniqueRef<MathICGenerationState>> m_instructionToMathICGenerationState;

        bool m_canBeOptimized;
        bool m_shouldEmitProfiling;

        CodeBlock* const m_profiledCodeBlock { nullptr };
        UnlinkedCodeBlock* const m_unlinkedCodeBlock { nullptr };

        MathICHolder m_mathICs;

        Vector<JITConstantPool::Value> m_constantPool;
        SaSegmentedVector<BaselineUnlinkedCallLinkInfo> m_unlinkedCalls;
        SaSegmentedVector<BaselineUnlinkedStructureStubInfo> m_unlinkedStubInfos;
        FixedVector<SimpleJumpTable> m_switchJumpTables;
        FixedVector<StringJumpTable> m_stringSwitchJumpTables;

        struct NotACodeBlock { } m_codeBlock;

        bool m_isShareable { true };
    };

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(JIT)
