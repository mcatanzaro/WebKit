# Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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
require "set"

# Interesting invariant, which we take advantage of: branching instructions
# always begin with "b", and no non-branching instructions begin with "b".
# Terminal instructions are "jmp" and "ret".

MACRO_INSTRUCTIONS =
    [
     "emit",
     "addi",
     "andi",
     "andf",
     "andd",
     "lshifti",
     "lshiftp",
     "lshiftq",
     "muli",
     "negi",
     "negp",
     "negq",
     "noti",
     "ori",
     "orf",
     "ord",
     "orh",
     "rshifti",
     "urshifti",
     "rshiftp",
     "urshiftp",
     "rshiftq",
     "urshiftq",
     "lrotatei",
     "lrotateq",
     "rrotatei",
     "rrotateq",
     "subi",
     "xori",
     "load2ia",
     "loadi",
     "loadis",
     "loadb",
     "loadbsi",
     "loadbsq",
     "loadh",
     "loadhsi",
     "loadhsq",
     "store2ia",
     "storei",
     "storeh",
     "storeb",
     "transferi",
     "transferq",
     "transferp",
     "loadf",
     "loadd",
     "loadv",
     "moved",
     "storef",
     "stored",
     "storev",
     "addf",
     "addd",
     "divf",
     "divd",
     "subf",
     "subd",
     "mulf",
     "muld",
     "sqrtf",
     "sqrtd",
     "floorf",
     "floord",
     "roundf",
     "roundd",
     "truncatef",
     "truncated",
     "truncatef2i",
     "truncatef2q",
     "truncated2q",
     "truncated2i",
     "truncatef2is",
     "truncated2is",
     "truncatef2qs",
     "truncated2qs",
     "ci2d",
     "ci2ds",
     "ci2f",
     "ci2fs",
     "cq2f",
     "cq2fs",
     "cq2d",
     "cq2ds",
     "cd2f",
     "cf2d",
     "fii2d", # usage: fii2d <gpr with least significant bits>, <gpr with most significant bits>, <fpr>
     "fd2ii", # usage: fd2ii <fpr>, <gpr with least significant bits>, <gpr with most significant bits>
     "fq2d",
     "fd2q",
     "bdeq",
     "bdneq",
     "bdgt",
     "bdgteq",
     "bdlt",
     "bdlteq",
     "bdequn",
     "bdnequn",
     "bdgtun",
     "bdgtequn",
     "bdltun",
     "bdltequn",
     "bfeq",
     "bfgt",
     "bflt",
     "bfgtun",
     "bfgtequn",
     "bfltun",
     "bfltequn",
     "btd2i",
     "td2i",
     "bcd2i",
     "movdz",
     "pop",
     "popv",
     "push",
     "pushv",
     "move",
     "sxi2q",
     "zxi2q",
     "sxb2i",
     "sxh2i",
     "sxb2q",
     "sxh2q",
     "nop",
     "bieq",
     "bineq",
     "bia",
     "biaeq",
     "bib",
     "bibeq",
     "bigt",
     "bigteq",
     "bilt",
     "bilteq",
     "bbeq",
     "bbneq",
     "bba",
     "bbaeq",
     "bbb",
     "bbbeq",
     "bbgt",
     "bbgteq",
     "bblt",
     "bblteq",
     "btis",
     "btiz",
     "btinz",
     "btbs",
     "btbz",
     "btbnz",
     "jmp",
     "baddio",
     "baddis",
     "baddiz",
     "baddinz",
     "bsubio",
     "bsubis",
     "bsubiz",
     "bsubinz",
     "bmulio",
     "bmulis",
     "bmuliz",
     "bmulinz",
     "borio",
     "boris",
     "boriz",
     "borinz",
     "break",
     "call",
     "ret",
     "cbeq",
     "cbneq",
     "cba",
     "cbaeq",
     "cbb",
     "cbbeq",
     "cbgt",
     "cbgteq",
     "cblt",
     "cblteq",
     "cieq",
     "cineq",
     "cia",
     "ciaeq",
     "cib",
     "cibeq",
     "cigt",
     "cigteq",
     "cilt",
     "cilteq",
     "tis",
     "tiz",
     "tinz",
     "tbs",
     "tbz",
     "tbnz",
     "tps",
     "tpz",
     "tpnz",
     "peek",
     "poke",
     "bpeq",
     "bpneq",
     "bpa",
     "bpaeq",
     "bpb",
     "bpbeq",
     "bpgt",
     "bpgteq",
     "bplt",
     "bplteq",
     "addp",
     "mulp",
     "andp",
     "orp",
     "subp",
     "xorp",
     "loadp",
     "cpeq",
     "cpneq",
     "cpa",
     "cpaeq",
     "cpb",
     "cpbeq",
     "cpgt",
     "cpgteq",
     "cplt",
     "cplteq",
     "storep",
     "btps",
     "btpz",
     "btpnz",
     "baddpo",
     "baddps",
     "baddpz",
     "baddpnz",
     "tqs",
     "tqz",
     "tqnz",
     "bqeq",
     "bqneq",
     "bqa",
     "bqaeq",
     "bqb",
     "bqbeq",
     "bqgt",
     "bqgteq",
     "bqlt",
     "bqlteq",
     "addq",
     "mulq",
     "andq",
     "orq",
     "subq",
     "xorq",
     "loadq",
     "cqeq",
     "cqneq",
     "cqa",
     "cqaeq",
     "cqb",
     "cqbeq",
     "cqgt",
     "cqgteq",
     "cqlt",
     "cqlteq",
     "storeq",
     "btqs",
     "btqz",
     "btqnz",
     "baddqo",
     "baddqs",
     "baddqz",
     "baddqnz",
     "bo",
     "bs",
     "bz",
     "bnz",
     "leai",
     "leap",
     "memfence",
     "tagCodePtr",
     "tagReturnAddress",
     "untagReturnAddress",
     "removeCodePtrTag",
     "untagArrayPtr",
     "removeArrayPtrTag",
     "tzcnti",
     "tzcntq",
     "lzcnti",
     "lzcntq",
     "absf",
     "absd",
     "negf",
     "negd",
     "ceilf",
     "ceild",
     "cfeq",
     "cdeq",
     "cfneq",
     "cfnequn",
     "cdneq",
     "cdnequn",
     "cflt",
     "cdlt",
     "cflteq",
     "cdlteq",
     "cfgt",
     "cdgt",
     "cfgteq",
     "cdgteq",
     "fi2f",
     "ff2i",
     "tls_loadp",
     "tls_storep",
    ]

X86_INSTRUCTIONS =
    [
     "cdqi",
     "idivi",
     "udivi",
     "cqoq",
     "idivq",
     "udivq",
     "notq",
     "atomicxchgaddb",
     "atomicxchgaddh",
     "atomicxchgaddi",
     "atomicxchgaddq",
     "atomicxchgsubb",
     "atomicxchgsubh",
     "atomicxchgsubi",
     "atomicxchgsubq",
     "atomicxchgb",
     "atomicxchgh",
     "atomicxchgi",
     "atomicxchgq",
     "batomicweakcasb",
     "batomicweakcash",
     "batomicweakcasi",
     "batomicweakcasq",
     "atomicweakcasb",
     "atomicweakcash",
     "atomicweakcasi",
     "atomicweakcasq",
     "atomicloadb",
     "atomicloadh",
     "atomicloadi",
     "atomicloadq",
     "fence",
    ]

X86_SIMD_INSTRUCTIONS =
    [
    ]

ARM_INSTRUCTIONS =
    [
     "adci",
     "bcs",
     "clrbp",
     "mvlbl",
     "globaladdr",
     "sbci",
     "moveii",
     "loadlinkb",
     "loadlinkh",
     "loadlinki",
     "loadlink2i",
     "storecondb",
     "storecondh",
     "storecondi",
     "storecond2i",
     "writefence",
    ]

ARM64_INSTRUCTIONS =
    [
     "bfiq", # Bit field insert <source reg> <last bit written> <width immediate> <dest reg>
     "pcrtoaddr",   # Address from PC relative offset - adr instruction
     "globaladdr",
     "notq",
     "loadqinc",
     "loadlinkacqb",
     "loadlinkacqh",
     "loadlinkacqi",
     "loadlinkacqq",
     "storecondrelb",
     "storecondrelh",
     "storecondreli",
     "storecondrelq",
     "fence",
     # They are available only if Atomic LSE is supported.
     "atomicxchgaddb",
     "atomicxchgaddh",
     "atomicxchgaddi",
     "atomicxchgaddq",
     "atomicxchgclearb",
     "atomicxchgclearh",
     "atomicxchgcleari",
     "atomicxchgclearq",
     "atomicxchgorb",
     "atomicxchgorh",
     "atomicxchgori",
     "atomicxchgorq",
     "atomicxchgxorb",
     "atomicxchgxorh",
     "atomicxchgxori",
     "atomicxchgxorq",
     "atomicxchgb",
     "atomicxchgh",
     "atomicxchgi",
     "atomicxchgq",
     "atomicweakcasb",
     "atomicweakcash",
     "atomicweakcasi",
     "atomicweakcasq",
     "atomicloadb",
     "atomicloadh",
     "atomicloadi",
     "atomicloadq",
     "loadpairq",
     "loadpairi",
     "storepairq",
     "storepairi",
     "loadpaird",
     "storepaird",
    ]

ARM64_SIMD_INSTRUCTIONS =
    [
     "umovb",
     "umovh",
     "umovi",
     "umovq",
    ]

RISC_INSTRUCTIONS =
    [
     "smulli",  # Multiply two 32-bit words and produce a 64-bit word
     "umulli",  # Multiply two 32-bit words and produce a 64-bit word
     "addis",   # Add integers and set a flag.
     "subis",   # Same, but for subtraction.
     "oris",    # Same, but for bitwise or.
     "addps",   # addis but for pointers.
     "divi",
     "divis",
     "divq",
     "divqs",
     "remi",
     "remis",
     "remq",
     "remqs"
    ]

MIPS_INSTRUCTIONS =
    [
    "la",
    "movz",
    "movn",
    "setcallreg",
    "slt",
    "sltu",
    "pichdr"
    ]

CXX_INSTRUCTIONS =
    [
     "cloopCrash",              # no operands
     "cloopCallJSFunction",     # operands: callee
     "cloopCallNative",         # operands: callee
     "cloopCallSlowPath",       # operands: callTarget, currentFrame, currentPC
     "cloopCallSlowPathVoid",   # operands: callTarget, currentFrame, currentPC
     "cloopCallSlowPath3",      # operands: callTarget, a0, a1, a2
     "cloopCallSlowPath4",      # operands: callTarget, a0, a1, a2, a3

     # For debugging only:
     # Takes no operands but simply emits whatever follows in // comments as
     # a line of C++ code in the generated LLIntAssembly.h file. This can be
     # used to insert instrumentation into the interpreter loop to inspect
     # variables of interest. Do not leave these instructions in production
     # code.
     "cloopDo",              # no operands
    ]

INSTRUCTIONS = MACRO_INSTRUCTIONS + X86_INSTRUCTIONS + X86_SIMD_INSTRUCTIONS + ARM_INSTRUCTIONS + ARM64_INSTRUCTIONS + ARM64_SIMD_INSTRUCTIONS + RISC_INSTRUCTIONS + MIPS_INSTRUCTIONS + CXX_INSTRUCTIONS

INSTRUCTION_SET = INSTRUCTIONS.to_set

def isBranch(instruction)
    instruction =~ /^b/
end

def hasFallThrough(instruction)
    instruction != "ret" and instruction != "jmp"
end

def isPowerOfTwo(value)
    return false if value <= 0
    (value & (value - 1)).zero?
end
