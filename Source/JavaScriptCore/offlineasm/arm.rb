# Copyright (C) 2011-2022 Apple Inc. All rights reserved.
# Copyright (C) 2013 University of Szeged. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

require "config"
require "ast"
require "opt"
require "risc"

# GPR conventions, to match the baseline JIT
#
#  x0 => t0, a0, r0
#  x1 => t1, a1, r1
#  x2 => t2, a2
#  x3 => t3, a3
#  x4 => t4                 (callee-save, PC)
#  x5 => t5                 (callee-save)
#  x6 => scratch            (callee-save)
#  x7 => cfr
#  x8 => t6                 (callee-save)
#  x9 => t7, also scratch!  (callee-save)
# x10 => csr0               (callee-save, metadataTable)
# x11 => csr1               (callee-save, PB)
# x12 => scratch            (callee-save)
#  lr => lr
#  sp => sp
#  pc => pc
#
# FPR conventions, to match the baseline JIT
#
#  d0 => ft0, fa0, fr
#  d1 => ft1, fa1
#  d2 => ft2
#  d3 => ft3
#  d4 => ft4
#  d5 => ft5
#  d6 => ft6
#  d7 => ft7
#  d8 => csfr0
#  d9 => csfr1
# d10 => csfr2
# d11 => csfr3
# d12 => csfr4
# d13 => csfr5
# d14 => scratch
# d15 => scratch

class Node
    def armSingle
        doubleOperand = armOperand
        raise "Bogus register name #{doubleOperand}" unless doubleOperand =~ /^d/
        "s" + ($~.post_match.to_i * 2).to_s
    end
    def armSingleHi
        doubleOperand = armOperand
        raise "Bogus register name #{doubleOperand}" unless doubleOperand =~ /^d/
        "s" + ($~.post_match.to_i * 2 + 1).to_s
    end
end

class SpecialRegister
    def armOperand
        @name
    end
end

# These are allocated from the end. Use the low order r6 first, ast it's often
# cheaper to encode. r12 and r9 are equivalent, but r9 conflicts with t7, so r9
# only as last resort.
ARM_EXTRA_GPRS = [SpecialRegister.new("r9"), SpecialRegister.new("r12"), SpecialRegister.new("r6")]
ARM_EXTRA_FPRS = [SpecialRegister.new("d7")]
ARM_SCRATCH_FPR = SpecialRegister.new("d15")
OS_DARWIN = ((RUBY_PLATFORM =~ /darwin/i) != nil)

def armMoveImmediate(value, register)
    # Currently we only handle the simple cases, and fall back to mov/movt for the complex ones.
    if value.is_a? String
      $asm.puts "mov #{register.armOperand}, (#{value})"
    elsif value >= 0 && value < 256
        $asm.puts "mov #{register.armOperand}, \##{value}"
    elsif (~value) >= 0 && (~value) < 256
        $asm.puts "mvn #{register.armOperand}, \##{~value}"
    else
        $asm.puts "movw #{register.armOperand}, \##{value & 0xffff}"
        if (value & 0xffff0000) != 0
            $asm.puts "movt #{register.armOperand}, \##{(value >> 16) & 0xffff}"
        end
    end
end

class RegisterID
    def armOperand
        case name
        when "t0", "a0", "wa0", "r0"
            "r0"
        when "t1", "a1", "wa1", "r1"
            "r1"
        when "t2", "a2", "wa2"
            "r2"
        when "t3", "a3", "wa3"
            "r3"
        when "t4" # LLInt PC
            "r4"
        when "t5", "ws0" # ws0 must be a non-argument/non-return GPR
            "r5"
        when "cfr"
            "r7"
        when "t6", "ws1" # ws1 must be a non-argument/non-return GPR
            "r8"
        when "t7"
            "r9" # r9 is also a scratch register, so use carefully!
        when "csr0"
            "r10"
        when "csr1"
            "r11"
        when "lr"
            "lr"
        when "sp"
            "sp"
        when "pc"
            "pc"
        else
            raise "Bad register #{name} for ARM at #{codeOriginString}"
        end
    end
end

class FPRegisterID
    def armOperand
        case name
        when "ft0", "fr", "fa0", "wfa0"
            "d0"
        when "ft1", "fa1", "wfa1"
            "d1"
        when "ft2", "wfa2"
            "d2"
        when "ft3", "wfa3"
            "d3"
        when "ft4", "wfa4"
            "d4"
        when "ft5", "wfa5"
            "d5"
        when "ft6", "wfa6"
            "d6"
        when "ft7", "wfa7"
            "d7"
        when "csfr0"
            "d8"
        when "csfr1"
            "d9"
        when "csfr2"
            "d10"
        when "csfr3"
            "d11"
        when "csfr4"
            "d12"
        when "csfr5"
            "d13"
        else
            raise "Bad register #{name} for ARM at #{codeOriginString}"
        end
    end
end

class Immediate
    def armOperand
        raise "Invalid immediate #{value} at #{codeOriginString}" if value < 0 or value > 255
        "\##{value}"
    end
end

class Address
    def armOperand
        raise "Bad offset at #{codeOriginString}" if offset.value < -0xff or offset.value > 0xfff
        if offset.value == 0
          "[#{base.armOperand}]"
        else
          "[#{base.armOperand}, \##{offset.value}]"
        end
    end
end

class BaseIndex
    def armOperand
        raise "Bad offset at #{codeOriginString}" if offset.value != 0
        "[#{base.armOperand}, #{index.armOperand}, lsl \##{scaleShift}]"
    end
end

class AbsoluteAddress
    def armOperand
        raise "Unconverted absolute address at #{codeOriginString}"
    end
end

#
# Lea support.
#

class Address
    def armEmitLea(destination)
        if destination == base
            $asm.puts "adds #{destination.armOperand}, \##{offset.value}"
        else
            $asm.puts "adds #{destination.armOperand}, #{base.armOperand}, \##{offset.value}"
        end
    end
end

class BaseIndex
    def armEmitLea(destination)
        raise "Malformed BaseIndex, offset should be zero at #{codeOriginString}" unless offset.value == 0
        $asm.puts "add #{destination.armOperand}, #{base.armOperand}, #{index.armOperand}, lsl \##{scaleShift}"
    end
end

# FIXME: we could support AbsoluteAddress for lea, but we don't.

#
# Actual lowering code follows.
#

def armOpcodeReversedOperands(opcode)
    m = /\Ab[ipb]/.match(opcode)

    operation =
        case m.post_match
        when "eq" then "eq"
        when "neq" then "neq"
        when "a" then "b"
        when "aeq" then "beq"
        when "b" then "a"
        when "beq" then "aeq"
        when "gt" then "lt"
        when "gteq" then "lteq"
        when "lt" then "gt"
        when "lteq" then "gteq"
        else
            raise "unknown operation #{m.post_match}"
        end

    "#{m[0]}#{operation}"
end

def armLowerStackPointerInComparison(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when "bieq", "bpeq", "bbeq",
                "bineq", "bpneq", "bbneq",
                "bia", "bpa", "bba",
                "biaeq", "bpaeq", "bbaeq",
                "bib", "bpb", "bbb",
                "bibeq", "bpbeq", "bbbeq",
                "bigt", "bpgt", "bbgt",
                "bigteq", "bpgteq", "bbgteq",
                "bilt", "bplt", "bblt",
                "bilteq", "bplteq", "bblteq"
                if node.operands[1].is_a?(RegisterID) && node.operands[1].name == "sp"
                    newList << Instruction.new(codeOrigin, armOpcodeReversedOperands(node.opcode), [node.operands[1], node.operands[0]] + node.operands[2..-1])
                else
                    newList << node
                end
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

def armLowerLabelReferences(list)
    newList = []
    list.each {
        | node |
        if node.is_a? Instruction
            case node.opcode
            when "leai", "leap", "leaq"
                labelRef = node.operands[0]
                if labelRef.is_a? LabelReference
                    tmp = Tmp.new(node.codeOrigin, :gpr)
                    newList << Instruction.new(codeOrigin, "globaladdr", [LabelReference.new(node.codeOrigin, labelRef.label), node.operands[1], tmp])
                    # FIXME: This check against 255 is just the simplest check we can do. ARM is capable of encoding some larger constants using
                    # rotation (subject to some special rules). Perhaps we can add the more comprehensive encoding check here.
                    if labelRef.offset > 255
                        newList << Instruction.new(codeOrigin, "move", [Immediate.new(node.codeOrigin, labelRef.offset), tmp])
                        newList << Instruction.new(codeOrigin, "addp", [tmp, node.operands[1]])
                    elsif labelRef.offset > 0
                        newList << Instruction.new(codeOrigin, "addp", [Immediate.new(node.codeOrigin, labelRef.offset), node.operands[1]])
                    end
                else
                    newList << node
                end
            else
                newList << node
            end
        else
            newList << node
        end
    }
    newList
end

class Sequence
    def getModifiedListARMv7
        raise unless $activeBackend == "ARMv7"
        getModifiedListARMCommon
    end

    def getModifiedListARMCommon
        result = @list
        result = riscDropTags(result)
        result = riscLowerSimpleBranchOps(result)
        result = riscLowerHardBranchOps(result)
        result = riscLowerShiftOps(result)
        result = armLowerLabelReferences(result)
        result = riscLowerMalformedAddresses(result) {
            | node, address |
            case node.opcode
            when "load2ia", "store2ia"
                if address.is_a? Address
                    (address.offset.value % 4 == 0) && ((-1023..1023).include? address.offset.value)
                else
                    false
                end
            else
                if address.is_a? BaseIndex
                    address.offset.value == 0 && node.opcode
                elsif address.is_a? Address
                    (-0xff..0xfff).include? address.offset.value
                else
                    false
                end
            end
        }
        result = riscLowerMalformedAddressesDouble(result)
        result = riscLowerMisplacedImmediates(result, ["storeb", "storeh", "storei", "storep", "storeq", "store2ia"])
        result = riscLowerMalformedImmediates(result, 0..0xff, 0..0x0ff)
        result = riscLowerMisplacedAddresses(result)
        result = riscLowerRegisterReuse(result)
        result = assignRegistersToTemporaries(result, :gpr, ARM_EXTRA_GPRS)
        result = assignRegistersToTemporaries(result, :fpr, ARM_EXTRA_FPRS)
        result = armLowerStackPointerInComparison(result)
        return result
    end
end

def armOperands(operands)
    operands.map{|v| v.armOperand}.join(", ")
end

def armFlippedOperands(operands)
    armOperands([operands[-1]] + operands[0..-2])
end

def armFlippedOperandsPair(operands)
    armOperands([operands[-2], operands[-1]] + operands[0..-3])
end

def emitArmCompact(opcode2, opcode3, operands)
    if operands.size == 3
        $asm.puts "#{opcode3} #{armFlippedOperands(operands)}"
    else
        raise unless operands.size == 2
        raise unless operands[1].register?
        if operands[0].immediate?
            $asm.puts "#{opcode3} #{operands[1].armOperand}, #{operands[1].armOperand}, #{operands[0].armOperand}"
        else
            $asm.puts "#{opcode2} #{armFlippedOperands(operands)}"
        end
    end
end

def emitArm(opcode, operands)
    if operands.size == 3
        $asm.puts "#{opcode} #{armFlippedOperands(operands)}"
    else
        raise unless operands.size == 2
        $asm.puts "#{opcode} #{operands[1].armOperand}, #{operands[1].armOperand}, #{operands[0].armOperand}"
    end
end

def emitArmDoubleBranch(branchOpcode, operands)
    $asm.puts "vcmpe.f64 #{armOperands(operands[0..1])}"
    $asm.puts "vmrs apsr_nzcv, fpscr"
    $asm.puts "#{branchOpcode} #{operands[2].asmLabel}"
end

def emitArmSingleBranch(branchOpcode, operands)
    $asm.puts "vcmpe.f32 #{operands[0].armSingle}, #{operands[1].armSingle}"
    $asm.puts "vmrs apsr_nzcv, fpscr"
    $asm.puts "#{branchOpcode} #{operands[2].asmLabel}"
end

def emitArmTest(operands)
    value = operands[0]
    case operands.size
    when 2
        mask = Immediate.new(codeOrigin, -1)
    when 3
        mask = operands[1]
    else
        raise "Expected 2 or 3 operands but got #{operands.size} at #{codeOriginString}"
    end
    
    if mask.immediate? and mask.value == -1
        $asm.puts "tst #{value.armOperand}, #{value.armOperand}"
    else
        $asm.puts "tst #{value.armOperand}, #{mask.armOperand}"
    end
end

def emitArmDoubleCompare(operands, code)
    $asm.puts "mov #{operands[2].armOperand}, \#0"
    $asm.puts "vcmp.f64 #{armOperands(operands[0..1])}"
    $asm.puts "vmrs APSR_nzcv, FPSCR"
    $asm.puts "it #{code}"
    $asm.puts "mov#{code} #{operands[2].armOperand}, \#1"
end

def emitArmFloatCompare(operands, code)
    $asm.puts "mov #{operands[2].armOperand}, \#0"
    $asm.puts "vcmp.f32 #{operands[0].armSingle}, #{operands[1].armSingle}"
    $asm.puts "vmrs APSR_nzcv, FPSCR"
    $asm.puts "it #{code}"
    $asm.puts "mov#{code} #{operands[2].armOperand}, \#1"
end

def emitArmCompare(operands, code)
    $asm.puts "movs #{operands[2].armOperand}, \#0"
    $asm.puts "cmp #{operands[0].armOperand}, #{operands[1].armOperand}"
    $asm.puts "it #{code}"
    $asm.puts "mov#{code} #{operands[2].armOperand}, \#1"
end

def emitArmTestSet(operands, code)
    $asm.puts "movs #{operands[-1].armOperand}, \#0"
    emitArmTest(operands)
    $asm.puts "it #{code}"
    $asm.puts "mov#{code} #{operands[-1].armOperand}, \#1"
end

def rawInstruction(codeOrigin, text)
    return Instruction.new(codeOrigin, "emit", [StringLiteral.new(codeOrigin, "\"#{text}\"")])
end

def emitArmTruncateFloat(codeOrigin, to, from, operands)
    tmp = ARM_EXTRA_FPRS[-1]
    src = from == "f32" ? operands[0].armSingle : operands[0].armOperand
    Sequence.new(codeOrigin, [
        rawInstruction(codeOrigin, "vcvt.#{to}.#{from} #{tmp.armSingle}, #{src}"),
        rawInstruction(codeOrigin, "vmov #{operands[1].armOperand}, #{tmp.armSingle}")
    ]).lower($activeBackend)
end

class Instruction
    def lowerARMv7
        raise unless $activeBackend == "ARMv7"
        lowerARMCommon
    end

    def lowerARMCommon
        case opcode
        when "addi", "addp", "addis", "addps"
            if opcode == "addis" or opcode == "addps"
                suffix = "s"
            else
                suffix = ""
            end
            if operands.size == 3 and operands[0].immediate?
                raise unless operands[1].register?
                raise unless operands[2].register?
                if operands[0].value == 0 and suffix.empty?
                    unless operands[1] == operands[2]
                        $asm.puts "mov #{operands[2].armOperand}, #{operands[1].armOperand}"
                    end
                else
                    $asm.puts "adds #{operands[2].armOperand}, #{operands[1].armOperand}, #{operands[0].armOperand}"
                end
            elsif operands.size == 3 and operands[0].register?
                raise unless operands[1].register?
                raise unless operands[2].register?
                $asm.puts "adds #{armFlippedOperands(operands)}"
            else
                if operands[0].immediate?
                    unless Immediate.new(nil, 0) == operands[0]
                        $asm.puts "adds #{armFlippedOperands(operands)}"
                    end
                else
                    $asm.puts "add#{suffix} #{armFlippedOperands(operands)}"
                end
            end
        when "adci"
            emitArm("adc", operands)
        when "absd"
          $asm.puts "vabs.f64 #{armFlippedOperands(operands)}"
        when "andi", "andp"
            emitArmCompact("ands", "and", operands)
        when "ori", "orp", "orh"
            emitArmCompact("orrs", "orr", operands)
        when "oris"
            emitArmCompact("orrs", "orrs", operands)
        when "xori", "xorp"
            emitArmCompact("eors", "eor", operands)
        when "lshifti", "lshiftp"
            emitArmCompact("lsls", "lsls", operands)
        when "rshifti", "rshiftp"
            emitArmCompact("asrs", "asrs", operands)
        when "urshifti", "urshiftp"
            emitArmCompact("lsrs", "lsrs", operands)
        when "muli", "mulp"
            emitArm("mul", operands)
        when "subi", "subp", "subis"
            emitArmCompact("subs", "subs", operands)
        when "sbci"
            emitArm("sbc", operands)
        when "negi", "negp"
            $asm.puts "rsbs #{operands[0].armOperand}, #{operands[0].armOperand}, \#0"
        when "noti"
            $asm.puts "mvns #{operands[0].armOperand}, #{operands[0].armOperand}"
        when "lrotatei"
            tmp = Tmp.new(codeOrigin, :gpr)
            Sequence.new(codeOrigin, [
                Instruction.new(codeOrigin, "move", [operands[0], tmp]),
                Instruction.new(codeOrigin, "negi", [tmp]),
                Instruction.new(codeOrigin, "rrotatei", [tmp, operands[1]]),
            ]).lower($activeBackend)
        when "rrotatei"
            $asm.puts "ror #{armFlippedOperands(operands)}"
        when "tzcnti"
            $asm.puts "rbit #{armFlippedOperands(operands)}"
            $asm.puts "clz #{operands[1].armOperand}, #{operands[1].armOperand}"
        when "lzcnti"
            $asm.puts "clz #{armFlippedOperands(operands)}"
        when "loadi", "loadis", "loadp"
            $asm.puts "ldr #{armFlippedOperands(operands)}"
        when "storei", "storep"
            $asm.puts "str #{armOperands(operands)}"
        when "load2ia"
            $asm.puts "ldrd #{armFlippedOperandsPair(operands)}"
        when "store2ia"
            $asm.puts "strd #{armOperands(operands)}"
        when "loadb"
            $asm.puts "ldrb #{armFlippedOperands(operands)}"
        when "loadbsi"
            $asm.puts "ldrsb.w #{armFlippedOperands(operands)}"
        when "storeb"
            $asm.puts "strb #{armOperands(operands)}"
        when "loadh"
            $asm.puts "ldrh #{armFlippedOperands(operands)}"
        when "loadhsi"
            $asm.puts "ldrsh.w #{armFlippedOperands(operands)}"
        when "storeh"
            $asm.puts "strh #{armOperands(operands)}"
        when "transferi", "transferp"
            tmp = ARM_EXTRA_GPRS[-1]
            $asm.puts "ldr #{tmp.armOperand}, #{operands[0].armOperand}"
            $asm.puts "str #{tmp.armOperand}, #{operands[1].armOperand}"
        when "transferq"
            tmp1, tmp2 = ARM_EXTRA_GPRS[-2..-1]
            $asm.puts "ldp #{tmp1.armOperand}, #{tmp2.armOperand}, #{operands[0].armOperand}"
            $asm.puts "stp #{tmp1.armOperand}, #{tmp2.armOperand}, #{operands[1].armOperand}"
        when "loadlinkb"
            $asm.puts "ldrexb #{armFlippedOperands(operands)}"
        when "loadlinkh"
            $asm.puts "ldrexh #{armFlippedOperands(operands)}"
        when "loadlinki"
            $asm.puts "ldrex #{armFlippedOperands(operands)}"
        when "loadlink2i"
            $asm.puts "ldrexd #{armFlippedOperandsPair(operands)}"
        when "storecondb"
            $asm.puts "strexb #{armOperands(operands)}"
        when "storecondh"
            $asm.puts "strexh #{armOperands(operands)}"
        when "storecondi"
            $asm.puts "strex #{armOperands(operands)}"
        when "storecond2i"
            $asm.puts "strexd #{armOperands(operands)}"
        when "loadd"
            $asm.puts "vldr.64 #{armFlippedOperands(operands)}"
        when "stored"
            $asm.puts "vstr.64 #{armOperands(operands)}"
        when "loadf"
            $asm.puts "vldr.32 #{operands[-1].armSingle}, #{armOperands(operands[0..-2])}"
        when "storef"
            $asm.puts "vstr.32 #{operands[0].armSingle}, #{armOperands(operands[1..-1])}"
        when "addf"
            $asm.puts "vadd.f32 #{operands[1].armSingle}, #{operands[1].armSingle}, #{operands[0].armSingle}"
        when "subf"
            $asm.puts "vsub.f32 #{operands[1].armSingle}, #{operands[1].armSingle}, #{operands[0].armSingle}"
        when "mulf"
            $asm.puts "vmul.f32 #{operands[1].armSingle}, #{operands[1].armSingle}, #{operands[0].armSingle}"
        when "divf"
            $asm.puts "vdiv.f32 #{operands[1].armSingle}, #{operands[1].armSingle}, #{operands[0].armSingle}"
        when "sqrtf"
            $asm.puts "vsqrt.f32 #{operands[1].armSingle}, #{operands[0].armSingle}"
        when "absf"
            $asm.puts "vabs.f32 #{operands[1].armSingle}, #{operands[0].armSingle}"
        when "negf"
            $asm.puts "vneg.f32 #{operands[1].armSingle}, #{operands[0].armSingle}"
        when "bflt"
            emitArmSingleBranch("bmi", operands)
        when "bfltun"
            emitArmSingleBranch("blt", operands)
        when "bfltequn"
            emitArmSingleBranch("ble", operands)
        when "bfeq"
            emitArmSingleBranch("beq", operands)
        when "bfgt"
            emitArmSingleBranch("bgt", operands)
        when "bfgtequn"
            emitArmSingleBranch("bpl", operands)
        when "bdltun"
            emitArmDoubleBranch("blt", operands)
        when "addd"
            emitArm("vadd.f64", operands)
        when "subd"
            emitArm("vsub.f64", operands)
        when "muld"
            emitArm("vmul.f64", operands)
        when "divd"
            emitArm("vdiv.f64", operands)
        when "sqrtd"
            $asm.puts "vsqrt.f64 #{armFlippedOperands(operands)}"
        when "negd"
            $asm.puts "vneg.f64 #{armFlippedOperands(operands)}"
        when "orf"
            (tmpLhs, tmpRhs) = ARM_EXTRA_GPRS[-2..-1]
            Sequence.new(codeOrigin, [
              rawInstruction(codeOrigin,  "vmov #{tmpLhs.armOperand}, #{operands[0].armSingle}"),
              rawInstruction(codeOrigin,  "vmov #{tmpRhs.armOperand}, #{operands[1].armSingle}"),
              Instruction.new(codeOrigin, "ori", [tmpLhs, tmpRhs]),
              rawInstruction(codeOrigin,  "vmov #{operands[1].armSingle}, #{tmpRhs.armOperand}")
            ]).lower($activeBackend)
        when "andf"
            (tmpLhs, tmpRhs) = ARM_EXTRA_GPRS[-2..-1]
            Sequence.new(codeOrigin, [
              rawInstruction(codeOrigin,  "vmov #{tmpLhs.armOperand}, #{operands[0].armSingle}"),
              rawInstruction(codeOrigin,  "vmov #{tmpRhs.armOperand}, #{operands[1].armSingle}"),
              Instruction.new(codeOrigin, "andi", [tmpLhs, tmpRhs]),
              rawInstruction(codeOrigin,  "vmov #{operands[1].armSingle}, #{tmpRhs.armOperand}")
            ]).lower($activeBackend)
        when "ord"
            (tmpLhs, tmpRhs) = ARM_EXTRA_GPRS[-2..-1]
            Sequence.new(codeOrigin, [
              rawInstruction(codeOrigin,  "vmov #{tmpLhs.armOperand}, #{operands[0].armSingle}"),
              rawInstruction(codeOrigin,  "vmov #{tmpRhs.armOperand}, #{operands[1].armSingle}"),
              Instruction.new(codeOrigin, "ori", [tmpLhs, tmpRhs]),
              rawInstruction(codeOrigin,  "vmov #{operands[1].armSingle}, #{tmpRhs.armOperand}"),
              rawInstruction(codeOrigin,  "vmov #{tmpLhs.armOperand}, #{operands[0].armSingleHi}"),
              rawInstruction(codeOrigin,  "vmov #{tmpRhs.armOperand}, #{operands[1].armSingleHi}"),
              Instruction.new(codeOrigin, "ori", [tmpLhs, tmpRhs]),
              rawInstruction(codeOrigin,  "vmov #{operands[1].armSingleHi}, #{tmpRhs.armOperand}")
            ]).lower($activeBackend)
        when "andd"
            (tmpLhs, tmpRhs) = ARM_EXTRA_GPRS[-2..-1]
            Sequence.new(codeOrigin, [
              rawInstruction(codeOrigin,  "vmov #{tmpLhs.armOperand}, #{operands[0].armSingle}"),
              rawInstruction(codeOrigin,  "vmov #{tmpRhs.armOperand}, #{operands[1].armSingle}"),
              Instruction.new(codeOrigin, "andi", [tmpLhs, tmpRhs]),
              rawInstruction(codeOrigin,  "vmov #{operands[1].armSingle}, #{tmpRhs.armOperand}"),
              rawInstruction(codeOrigin,  "vmov #{tmpLhs.armOperand}, #{operands[0].armSingleHi}"),
              rawInstruction(codeOrigin,  "vmov #{tmpRhs.armOperand}, #{operands[1].armSingleHi}"),
              Instruction.new(codeOrigin, "andi", [tmpLhs, tmpRhs]),
              rawInstruction(codeOrigin,  "vmov #{operands[1].armSingleHi}, #{tmpRhs.armOperand}")
            ]).lower($activeBackend)
        when "ci2ds"
            $asm.puts "vmov #{operands[1].armSingle}, #{operands[0].armOperand}"
            $asm.puts "vcvt.f64.s32 #{operands[1].armOperand}, #{operands[1].armSingle}"
        when "bdeq"
            emitArmDoubleBranch("beq", operands)
        when "bdneq"
            $asm.puts "vcmpe.f64 #{armOperands(operands[0..1])}"
            $asm.puts "vmrs apsr_nzcv, fpscr"
            isUnordered = LocalLabel.unique(codeOrigin, "bdneq")
            $asm.puts "bvs #{LocalLabelReference.new(codeOrigin, isUnordered).asmLabel}"
            $asm.puts "bne #{operands[2].asmLabel}"
            isUnordered.lower("ARM")
        when "bdgt"
            emitArmDoubleBranch("bgt", operands)
        when "bdgteq"
            emitArmDoubleBranch("bge", operands)
        when "bdlt"
            emitArmDoubleBranch("bmi", operands)
        when "bdlteq"
            emitArmDoubleBranch("bls", operands)
        when "bdequn"
            $asm.puts "vcmpe.f64 #{armOperands(operands[0..1])}"
            $asm.puts "vmrs apsr_nzcv, fpscr"
            $asm.puts "bvs #{operands[2].asmLabel}"
            $asm.puts "beq #{operands[2].asmLabel}"
        when "bdnequn"
            emitArmDoubleBranch("bne", operands)
        when "bdgtun"
            emitArmDoubleBranch("bhi", operands)
        when "bdgtequn"
            emitArmDoubleBranch("bpl", operands)
        when "bdltun"
            emitArmDoubleBranch("blt", operands)
        when "bdltequn"
            emitArmDoubleBranch("ble", operands)
        when "btd2i"
            # FIXME: may be a good idea to just get rid of this instruction, since the interpreter
            # currently does not use it.
            raise "ARM does not support this opcode yet, #{codeOrigin}"
        when "td2i"
            $asm.puts "vcvt.s32.f64 #{ARM_SCRATCH_FPR.armSingle}, #{operands[0].armOperand}"
            $asm.puts "vmov #{operands[1].armOperand}, #{ARM_SCRATCH_FPR.armSingle}"
        when "bcd2i"
            $asm.puts "vcvt.s32.f64 #{ARM_SCRATCH_FPR.armSingle}, #{operands[0].armOperand}"
            $asm.puts "vmov #{operands[1].armOperand}, #{ARM_SCRATCH_FPR.armSingle}"
            $asm.puts "vcvt.f64.s32 #{ARM_SCRATCH_FPR.armOperand}, #{ARM_SCRATCH_FPR.armSingle}"
            emitArmDoubleBranch("bne", [ARM_SCRATCH_FPR, operands[0], operands[2]])
            $asm.puts "tst #{operands[1].armOperand}, #{operands[1].armOperand}"
            $asm.puts "beq #{operands[2].asmLabel}"
        when "movdz"
            # FIXME: either support this or remove it.
            raise "ARM does not support this opcode yet, #{codeOrigin}"
        when "moved"
            $asm.puts "vmov.f64 #{armFlippedOperands(operands)}"
        when "pop"
            operands.each {
                | op |
                $asm.puts "pop { #{op.armOperand} }"
            }
        when "push"
            operands.each {
                | op |
                $asm.puts "push { #{op.armOperand} }"
            }
        when "move"
            if operands[0].immediate?
                armMoveImmediate(operands[0].value, operands[1])
            else
                $asm.puts "mov #{armFlippedOperands(operands)}"
            end
        when "moveii"
            raise "First operand of moveii must be an immediate" unless operands[0].immediate?
            armMoveImmediate(operands[0].value >> 32, operands[1])
            armMoveImmediate(operands[0].value & 0xffffffff, operands[2])
        when "mvlbl"
                $asm.puts "movw #{operands[1].armOperand}, \#:lower16:#{operands[0].value}"
                $asm.puts "movt #{operands[1].armOperand}, \#:upper16:#{operands[0].value}"
        when "sxb2i"
            $asm.puts "sxtb #{armFlippedOperands(operands)}"
        when "sxh2i"
            $asm.puts "sxth #{armFlippedOperands(operands)}"
        when "nop"
            $asm.puts "nop"
        when "bieq", "bpeq", "bbeq"
            if Immediate.new(nil, 0) == operands[0]
                $asm.puts "tst #{operands[1].armOperand}, #{operands[1].armOperand}"
            elsif Immediate.new(nil, 0) == operands[1]
                $asm.puts "tst #{operands[0].armOperand}, #{operands[0].armOperand}"
            else
                $asm.puts "cmp #{armOperands(operands[0..1])}"
            end
            $asm.puts "beq #{operands[2].asmLabel}"
        when "bineq", "bpneq", "bbneq"
            if Immediate.new(nil, 0) == operands[0]
                $asm.puts "tst #{operands[1].armOperand}, #{operands[1].armOperand}"
            elsif Immediate.new(nil, 0) == operands[1]
                $asm.puts "tst #{operands[0].armOperand}, #{operands[0].armOperand}"
            else
                $asm.puts "cmp #{armOperands(operands[0..1])}"
            end
            $asm.puts "bne #{operands[2].asmLabel}"
        when "bia", "bpa", "bba"
            $asm.puts "cmp #{armOperands(operands[0..1])}"
            $asm.puts "bhi #{operands[2].asmLabel}"
        when "biaeq", "bpaeq", "bbaeq"
            $asm.puts "cmp #{armOperands(operands[0..1])}"
            $asm.puts "bhs #{operands[2].asmLabel}"
        when "bib", "bpb", "bbb"
            $asm.puts "cmp #{armOperands(operands[0..1])}"
            $asm.puts "blo #{operands[2].asmLabel}"
        when "bibeq", "bpbeq", "bbbeq"
            $asm.puts "cmp #{armOperands(operands[0..1])}"
            $asm.puts "bls #{operands[2].asmLabel}"
        when "bigt", "bpgt", "bbgt"
            $asm.puts "cmp #{armOperands(operands[0..1])}"
            $asm.puts "bgt #{operands[2].asmLabel}"
        when "bigteq", "bpgteq", "bbgteq"
            $asm.puts "cmp #{armOperands(operands[0..1])}"
            $asm.puts "bge #{operands[2].asmLabel}"
        when "bilt", "bplt", "bblt"
            $asm.puts "cmp #{armOperands(operands[0..1])}"
            $asm.puts "blt #{operands[2].asmLabel}"
        when "bilteq", "bplteq", "bblteq"
            $asm.puts "cmp #{armOperands(operands[0..1])}"
            $asm.puts "ble #{operands[2].asmLabel}"
        when "btiz", "btpz", "btbz"
            emitArmTest(operands)
            $asm.puts "beq #{operands[-1].asmLabel}"
        when "btinz", "btpnz", "btbnz"
            emitArmTest(operands)
            $asm.puts "bne #{operands[-1].asmLabel}"
        when "btis", "btps", "btbs"
            emitArmTest(operands)
            $asm.puts "bmi #{operands[-1].asmLabel}"
        when "jmp"
            if operands[0].label?
                $asm.puts "b #{operands[0].asmLabel}"
            else
                $asm.puts "mov pc, #{operands[0].armOperand}"
            end
        when "call"
            if operands[0].label?
                if OS_DARWIN
                    $asm.puts "blx #{operands[0].asmLabel}"
                else
                    $asm.puts "bl #{operands[0].asmLabel}"
                end
            else
                $asm.puts "blx #{operands[0].armOperand}"
            end
        when "break"
            $asm.puts "udf #0"
        when "ret"
            $asm.puts "bx lr"
        when "cieq", "cpeq", "cbeq"
            emitArmCompare(operands, "eq")
        when "cineq", "cpneq", "cbneq"
            emitArmCompare(operands, "ne")
        when "cia", "cpa", "cba"
            emitArmCompare(operands, "hi")
        when "ciaeq", "cpaeq", "cbaeq"
            emitArmCompare(operands, "hs")
        when "cib", "cpb", "cbb"
            emitArmCompare(operands, "lo")
        when "cibeq", "cpbeq", "cbbeq"
            emitArmCompare(operands, "ls")
        when "cigt", "cpgt", "cbgt"
            emitArmCompare(operands, "gt")
        when "cigteq", "cpgteq", "cbgteq"
            emitArmCompare(operands, "ge")
        when "cilt", "cplt", "cblt"
            emitArmCompare(operands, "lt")
        when "cilteq", "cplteq", "cblteq"
            emitArmCompare(operands, "le")
        when "cfeq"
            emitArmFloatCompare(operands, "eq")
        when "cfnequn"
            emitArmFloatCompare(operands, "ne")
        when "cflt"
            emitArmFloatCompare(operands, "mi")
        when "cflteq"
            emitArmFloatCompare(operands, "ls")
        when "cfgt"
            emitArmFloatCompare(operands, "gt")
        when "cfgteq"
            emitArmFloatCompare(operands, "ge")
        when "cdeq"
            emitArmDoubleCompare(operands, "eq")
        when "cdnequn"
            emitArmDoubleCompare(operands, "ne")
        when "cdlt"
            emitArmDoubleCompare(operands, "mi")
        when "cdlteq"
            emitArmDoubleCompare(operands, "ls")
        when "cdgt"
            emitArmDoubleCompare(operands, "gt")
        when "cdgteq"
            emitArmDoubleCompare(operands, "ge")
        when "cf2d"
            $asm.puts "vcvt.f64.f32 #{operands[1].armOperand}, #{operands[0].armSingle}"
        when "cd2f"
            $asm.puts "vcvt.f32.f64 #{operands[1].armSingle}, #{operands[0].armOperand}"
        when "ci2f"
            $asm.puts "vmov #{operands[1].armSingle}, #{operands[0].armOperand}"
            $asm.puts "vcvt.f32.u32 #{operands[1].armSingle}, #{operands[1].armSingle}"
        when "ci2fs"
            $asm.puts "vmov #{operands[1].armSingle}, #{operands[0].armOperand}"
            $asm.puts "vcvt.f32.s32 #{operands[1].armSingle}, #{operands[1].armSingle}"
        when "ci2d"
            $asm.puts "vmov #{operands[1].armSingle}, #{operands[0].armOperand}"
            $asm.puts "vcvt.f64.u32 #{operands[1].armOperand}, #{operands[1].armSingle}"
        when "ci2ds"
            $asm.puts "vmov #{operands[1].armSingle}, #{operands[0].armOperand}"
            $asm.puts "vcvt.f64.s32 #{operands[1].armOperand}, #{operands[1].armSingle}"
        when "tis", "tbs", "tps"
            emitArmTestSet(operands, "mi")
        when "tiz", "tbz", "tpz"
            emitArmTestSet(operands, "eq")
        when "tinz", "tbnz", "tpnz"
            emitArmTestSet(operands, "ne")
        when "peek"
            $asm.puts "ldr #{operands[1].armOperand}, [sp, \##{operands[0].value * 4}]"
        when "poke"
            $asm.puts "str #{operands[1].armOperand}, [sp, \##{operands[0].value * 4}]"
        when "fi2f"
            $asm.puts "vmov #{operands[1].armSingle}, #{operands[0].armOperand}"
        when "ff2i"
            $asm.puts "vmov #{operands[1].armOperand}, #{operands[0].armSingle}"
        when "fii2d"
            $asm.puts "vmov #{operands[2].armOperand}, #{operands[0].armOperand}, #{operands[1].armOperand}"
        when "fd2ii"
            $asm.puts "vmov #{operands[1].armOperand}, #{operands[2].armOperand}, #{operands[0].armOperand}"
        when "truncatef2i"
            emitArmTruncateFloat(codeOrigin, "u32", "f32", operands)
        when "truncatef2is"
            emitArmTruncateFloat(codeOrigin, "s32", "f32", operands)
        when "truncated2i"
            emitArmTruncateFloat(codeOrigin, "u32", "f64", operands)
        when "truncated2is"
            emitArmTruncateFloat(codeOrigin, "s32", "f64", operands)
        when "bo"
            $asm.puts "bvs #{operands[0].asmLabel}"
        when "bs"
            $asm.puts "bmi #{operands[0].asmLabel}"
        when "bz"
            $asm.puts "beq #{operands[0].asmLabel}"
        when "bnz"
            $asm.puts "bne #{operands[0].asmLabel}"
        when "bcs"
            $asm.puts "bcs #{operands[0].asmLabel}"
        when "leai", "leap"
            operands[0].armEmitLea(operands[1])
        when "smulli", "umulli"
            raise "Wrong number of arguments to smull in #{self.inspect} at #{codeOriginString}" unless operands.length == 4
            $asm.puts "#{opcode[0..-2]} #{operands[2].armOperand}, #{operands[3].armOperand}, #{operands[0].armOperand}, #{operands[1].armOperand}"
        when "memfence"
            $asm.puts "dmb sy"
        when "fence"
            $asm.puts "dmb ish"
        when "writefence"
            $asm.puts "dmb ishst"
        when "clrbp"
            $asm.puts "bic #{operands[2].armOperand}, #{operands[0].armOperand}, #{operands[1].armOperand}"
        when "globaladdr"
            labelRef = operands[0]
            dest = operands[1]
            temp = operands[2]
            raise "Destination interferes with scratch in #{self.inspect} at #{codeOriginString}" unless dest.armOperand != temp.armOperand

            uid = $asm.newUID

            $asm.putStr("#if OS(DARWIN)")
            $asm.puts "movw #{operands[1].armOperand}, :lower16:(L#{operands[0].asmLabel}_#{uid}$non_lazy_ptr-(Ljsc_llint_#{uid}+4))"
            $asm.puts "movt #{operands[1].armOperand}, :upper16:(L#{operands[0].asmLabel}_#{uid}$non_lazy_ptr-(Ljsc_llint_#{uid}+4))"
            $asm.puts "Ljsc_llint_#{uid}:"
            $asm.puts "add #{operands[1].armOperand}, pc"
            $asm.puts "ldr #{operands[1].armOperand}, [#{operands[1].armOperand}]"

            # On Linux, use ELF GOT relocation specifiers.
            $asm.putStr("#elif OS(LINUX)")
            gotLabel = Assembler.localLabelReference("jsc_llint_arm_got_#{uid}")
            offsetLabel = Assembler.localLabelReference("jsc_llint_arm_got_offset_#{uid}")

            $asm.puts "ldr #{dest.armOperand}, #{gotLabel}"
            $asm.puts "ldr #{temp.armOperand}, #{gotLabel}+4"
            $asm.puts "#{offsetLabel}:"
            $asm.puts "add #{dest.armOperand}, pc, #{dest.armOperand}"
            $asm.puts "ldr #{dest.armOperand}, [#{dest.armOperand}, #{temp.armOperand}]"

            # Throw a compiler error everywhere else.
            $asm.putStr("#else")
            $asm.putStr("#error Missing globaladdr implementation")
            $asm.putStr("#endif")

            offset = 4

            $asm.deferNextLabelAction {
                $asm.putStr("#if OS(DARWIN)")
                $asm.puts ".section __DATA,__nl_symbol_ptr,non_lazy_symbol_pointers"
                $asm.puts ".p2align 2"

                $asm.puts "L#{operands[0].asmLabel}_#{uid}$non_lazy_ptr:"
                $asm.puts ".indirect_symbol #{operands[0].asmLabel}"
                $asm.puts ".long 0"
                
                $asm.puts "OFFLINE_ASM_TEXT_SECTION"
                $asm.puts ".align 4"

                $asm.putStr("#elif OS(LINUX)")
                $asm.puts "#{gotLabel}:"
                $asm.puts ".word _GLOBAL_OFFSET_TABLE_-(#{offsetLabel}+#{offset})"
                $asm.puts ".word #{labelRef.asmLabel}(GOT)"

                $asm.putStr("#endif")
            }
        else
            lowerDefault
        end
    end
end

