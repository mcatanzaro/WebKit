/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#include "DFGArgumentsEliminationPhase.h"

#if ENABLE(DFG_JIT)

#include "ArrayPrototype.h"
#include "ClonedArguments.h"
#include "DFGArgumentsUtilities.h"
#include "DFGBlockMapInlines.h"
#include "DFGClobberize.h"
#include "DFGForAllKills.h"
#include "DFGGraph.h"
#include "DFGInsertionSet.h"
#include "DFGLivenessAnalysisPhase.h"
#include "DFGOSRAvailabilityAnalysisPhase.h"
#include "DFGPhase.h"
#include "JSCInlines.h"
#include <wtf/HashSet.h>
#include <wtf/ListDump.h>
#include <wtf/RecursableLambda.h>

namespace JSC { namespace DFG {

namespace {

namespace DFGArgumentsEliminationPhaseInternal {
static constexpr bool verbose = false;
}

class ArgumentsEliminationPhase : public Phase {
public:
    ArgumentsEliminationPhase(Graph& graph)
        : Phase(graph, "arguments elimination"_s)
    {
    }
    
    bool run()
    {
        // For now this phase only works on SSA. This could be changed; we could have a block-local
        // version over LoadStore.
        DFG_ASSERT(m_graph, nullptr, m_graph.m_form == SSA);
        
        dataLogIf(DFGArgumentsEliminationPhaseInternal::verbose, "Graph before arguments elimination:\n", m_graph);
        
        identifyCandidates();
        if (m_candidates.isEmpty())
            return false;

        removeInvalidCandidates();
        if (m_candidates.isEmpty())
            return false;

        collectAvailability();

        eliminateCandidatesThatEscape();
        if (m_candidates.isEmpty())
            return false;
        
        eliminateCandidatesThatInterfere();
        if (m_candidates.isEmpty())
            return false;
        
        transform();
        
        return true;
    }

private:
    // Just finds nodes that we know how to work with.
    void identifyCandidates()
    {
        for (BasicBlock* block : m_graph.blocksInPreOrder()) {
            for (Node* node : *block) {
                switch (node->op()) {
                case CreateDirectArguments:
                case CreateClonedArguments:
                    m_candidates.add(node, AvailabilityMap { });
                    break;

                case CreateRest:
                    if (m_graph.isWatchingHavingABadTimeWatchpoint(node)) {
                        // If we're watching the HavingABadTime watchpoint it means that we will be invalidated
                        // when it fires (it may or may not have actually fired yet). We don't try to eliminate
                        // this allocation when we're not watching the watchpoint because it could entail calling
                        // indexed accessors (and probably more crazy things) on out of bound accesses to the
                        // rest parameter. It's also much easier to reason about this way.
                        m_candidates.add(node, AvailabilityMap { });
                    }
                    break;

                case Spread:
                    if (m_graph.isWatchingHavingABadTimeWatchpoint(node)) {
                        // We check ArrayUse here because ArrayUse indicates that the iterator
                        // protocol for Arrays is non-observable by user code (e.g, it hasn't
                        // been changed).
                        if (node->child1().useKind() == ArrayUse) {
                            if ((node->child1()->op() == CreateRest || node->child1()->op() == NewArrayBuffer) && m_candidates.contains(node->child1().node()))
                                m_candidates.add(node, AvailabilityMap { });
                        }
                    }
                    break;

                case NewArrayWithSpread: {
                    if (m_graph.isWatchingHavingABadTimeWatchpoint(node)) {
                        BitVector* bitVector = node->bitVector();
                        // We only allow for Spreads to be of CreateRest or NewArrayBuffer nodes for now.
                        bool isOK = true;
                        for (unsigned i = 0; i < node->numChildren(); i++) {
                            if (bitVector->get(i)) {
                                Node* child = m_graph.varArgChild(node, i).node();
                                isOK = child->op() == Spread && (child->child1()->op() == CreateRest || child->child1()->op() == NewArrayBuffer) && m_candidates.contains(child);
                                if (!isOK)
                                    break;
                            }
                        }

                        if (!isOK)
                            break;

                        m_candidates.add(node, AvailabilityMap { });
                    }
                    break;
                }

                case NewArrayBuffer: {
                    if (m_graph.isWatchingHavingABadTimeWatchpoint(node) && !hasAnyArrayStorage(node->indexingMode()))
                        m_candidates.add(node, AvailabilityMap { });
                    break;
                }
                    
                case CreateScopedArguments:
                    // FIXME: We could handle this if it wasn't for the fact that scoped arguments are
                    // always stored into the activation.
                    // https://bugs.webkit.org/show_bug.cgi?id=143072 and
                    // https://bugs.webkit.org/show_bug.cgi?id=143073
                    break;
                    
                default:
                    break;
                }
                if (node->isPseudoTerminal())
                    break;
            }
        }
        
        dataLogLnIf(DFGArgumentsEliminationPhaseInternal::verbose, "Candidates: ", listDump(m_candidates.keys()));
    }

    // Do this analysis after we found we have some candidates to avoid doing fixed-point analysis on FTL graph which does not have candidates.
    void collectAvailability()
    {
        performOSRAvailabilityAnalysis(m_graph);

        for (BasicBlock* block : m_graph.blocksInPreOrder()) {
            LocalOSRAvailabilityCalculator calculator(m_graph);
            calculator.beginBlock(block);
            for (Node* node : *block) {
                switch (node->op()) {
                case CreateDirectArguments:
                case CreateClonedArguments:
                case CreateRest: {
                    auto iterator = m_candidates.find(node);
                    if (iterator != m_candidates.end())
                        iterator->value = calculator.m_availability;
                    break;
                }
                default:
                    break;
                }
                if (node->isPseudoTerminal())
                    break;
                calculator.executeNode(node);
            }
        }
    }

    bool isStillValidCandidate(Node* candidate)
    {
        switch (candidate->op()) {
        case Spread:
            return m_candidates.contains(candidate->child1().node());

        case NewArrayWithSpread: {
            BitVector* bitVector = candidate->bitVector();
            for (unsigned i = 0; i < candidate->numChildren(); i++) {
                if (bitVector->get(i)) {
                    if (!m_candidates.contains(m_graph.varArgChild(candidate, i).node()))
                        return false;
                }
            }
            return true;
        }

        default:
            return true;
        }

        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }

    void removeInvalidCandidates()
    {
        bool changed;
        do {
            changed = false;
            Vector<Node*, 1> toRemove;

            for (auto& [candidate, availability] : m_candidates) {
                if (!isStillValidCandidate(candidate))
                    toRemove.append(candidate);
            }

            if (toRemove.size()) {
                changed = true;
                for (Node* node : toRemove)
                    m_candidates.remove(node);
            }

        } while (changed);
    }

    void transitivelyRemoveCandidate(Node* node, Node* source = nullptr)
    {
        bool removed = m_candidates.remove(node);
        if (removed && source)
            dataLogLnIf(DFGArgumentsEliminationPhaseInternal::verbose, "eliminating candidate: ", node, " because it escapes from: ", source);

        if (removed)
            removeInvalidCandidates();
    }
    
    // Look for escaping sites, and remove from the candidates set if we see an escape.
    void eliminateCandidatesThatEscape()
    {
        auto escape = [&] (Edge edge, Node* source) {
            if (!edge)
                return;
            transitivelyRemoveCandidate(edge.node(), source);
        };
        
        auto escapeBasedOnArrayMode = [&] (ArrayMode mode, Edge edge, Node* source) {
            switch (mode.type()) {
            case Array::DirectArguments: {
                if (edge->op() != CreateDirectArguments) {
                    escape(edge, source);
                    break;
                }

                // Everything is fine if we're doing an in-bounds access.
                if (mode.isInBounds())
                    break;

                // If we're out-of-bounds then we proceed only if the prototype chain
                // for the allocation is sane (i.e. doesn't have indexed properties).
                if (m_graph.isWatchingObjectPrototypeChainIsSaneWatchpoint(edge.node()))
                    break;
                escape(edge, source);
                break;
            }
            
            case Array::Contiguous: {
                if (edge->op() != CreateClonedArguments && edge->op() != CreateRest) {
                    escape(edge, source);
                    break;
                }
            
                // Everything is fine if we're doing an in-bounds access.
                if (mode.isInBounds())
                    break;
                
                // If we're out-of-bounds then we proceed only if the prototype chain
                // for the allocation is sane (i.e. doesn't have indexed properties).
                if (edge->op() == CreateRest) {
                    if (m_graph.isWatchingArrayPrototypeChainIsSaneWatchpoint(edge.node()))
                        break;
                } else {
                    if (m_graph.isWatchingObjectPrototypeChainIsSaneWatchpoint(edge.node()))
                        break;
                }
                escape(edge, source);
                break;
            }
            
            case Array::ForceExit:
                break;
            
            default:
                escape(edge, source);
                break;
            }
        };

        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            for (Node* node : *block) {
                switch (node->op()) {
                case GetFromArguments:
                    break;
                    
                case GetByVal:
                    if (m_graph.hasExitSite(node, NegativeIndex))
                        escape(m_graph.varArgChild(node, 0), node);
                    else
                        escapeBasedOnArrayMode(node->arrayMode(), m_graph.varArgChild(node, 0), node);
                    escape(m_graph.varArgChild(node, 1), node);
                    escape(m_graph.varArgChild(node, 2), node);
                    break;

                case GetArrayLength:
                    escape(node->child2(), node);
                    // Computing the length of a NewArrayWithSpread can require some additions.
                    // These additions can overflow if the array is sufficiently enormous, and in that case we will need to exit.
                    if ((node->child1()->op() == NewArrayWithSpread) && !node->origin.exitOK)
                        escape(node->child1(), node);
                    break;

                case GetUndetachedTypeArrayLength:
                case GetTypedArrayLengthAsInt52:
                    // This node is only used for TypedArrays, so should not be relevant for arguments elimination
                    escape(node->child2(), node);
                    break;

                case NewArrayWithSpread: {
                    BitVector* bitVector = node->bitVector();
                    bool isWatchingHavingABadTimeWatchpoint = m_graph.isWatchingHavingABadTimeWatchpoint(node); 
                    for (unsigned i = 0; i < node->numChildren(); i++) {
                        Edge child = m_graph.varArgChild(node, i);
                        bool dontEscape;
                        if (bitVector->get(i)) {
                            dontEscape = child->op() == Spread
                                && child->child1().useKind() == ArrayUse
                                && (child->child1()->op() == CreateRest || child->child1()->op() == NewArrayBuffer)
                                && isWatchingHavingABadTimeWatchpoint;
                        } else
                            dontEscape = false;

                        if (!dontEscape)
                            escape(child, node);
                    }

                    break;
                }

                case Spread: {
                    bool isOK = node->child1().useKind() == ArrayUse && (node->child1()->op() == CreateRest || node->child1()->op() == NewArrayBuffer);
                    if (!isOK)
                        escape(node->child1(), node);
                    break;
                }

                case NewArrayBuffer:
                    break;
                    
                case VarargsLength:
                    break;

                case LoadVarargs:
                    if (node->loadVarargsData()->offset && (node->argumentsChild()->op() == NewArrayWithSpread || node->argumentsChild()->op() == Spread || node->argumentsChild()->op() == NewArrayBuffer))
                        escape(node->argumentsChild(), node);
                    break;
                    
                case CallVarargs:
                case ConstructVarargs:
                case TailCallVarargs:
                case TailCallVarargsInlinedCaller:
                    escape(node->child1(), node);
                    escape(node->child2(), node);
                    if (node->callVarargsData()->firstVarArgOffset && (node->child3()->op() == NewArrayWithSpread || node->child3()->op() == Spread || node->child3()->op() == NewArrayBuffer))
                        escape(node->child3(), node);
                    break;

                case Check:
                case CheckVarargs:
                    m_graph.doToChildren(
                        node,
                        [&] (Edge edge) {
                            if (edge.willNotHaveCheck())
                                return;
                            
                            if (alreadyChecked(edge.useKind(), SpecObject))
                                return;
                            
                            escape(edge, node);
                        });
                    break;
                    
                case MovHint:
                case PutHint:
                    break;
                    
                case GetButterfly:
                    // This barely works. The danger is that the GetButterfly is used by something that
                    // does something escaping to a candidate. Fortunately, the only butterfly-using ops
                    // that we exempt here also use the candidate directly. If there ever was a
                    // butterfly-using op that we wanted to exempt, then we'd have to look at the
                    // butterfly's child and check if it's a candidate.
                    break;
                    
                case FilterGetByStatus:
                case FilterPutByStatus:
                case FilterCallLinkStatus:
                case FilterInByStatus:
                case FilterDeleteByStatus:
                case FilterCheckPrivateBrandStatus:
                case FilterSetPrivateBrandStatus:
                    break;

                case CheckArrayOrEmpty:
                case CheckArray:
                    escapeBasedOnArrayMode(node->arrayMode(), node->child1(), node);
                    break;

                case CheckStructureOrEmpty:
                case CheckStructure: {
                    Node* target = node->child1().node();
                    if (!m_candidates.contains(target))
                        break;

                    Structure* structure = nullptr;
                    JSGlobalObject* globalObject = m_graph.globalObjectFor(target->origin.semantic);
                    switch (target->op()) {
                    case CreateDirectArguments:
                        structure = globalObject->directArgumentsStructure();
                        break;
                    case CreateClonedArguments:
                        structure = globalObject->clonedArgumentsStructure();
                        break;
                    case CreateRest:
                        ASSERT(m_graph.isWatchingHavingABadTimeWatchpoint(target));
                        structure = globalObject->restParameterStructure();
                        break;
                    case NewArrayWithSpread:
                        ASSERT(m_graph.isWatchingHavingABadTimeWatchpoint(target));
                        structure = globalObject->originalArrayStructureForIndexingType(ArrayWithContiguous);
                        break;
                    case NewArrayBuffer: {
                        ASSERT(m_graph.isWatchingHavingABadTimeWatchpoint(target));
                        IndexingType indexingMode = target->indexingMode();
                        ASSERT(!hasAnyArrayStorage(indexingMode));
                        structure = globalObject->originalArrayStructureForIndexingType(indexingMode);
                        break;
                    }
                    default:
                        RELEASE_ASSERT_NOT_REACHED();
                    }
                    ASSERT(structure);

                    if (!node->structureSet().contains(m_graph.registerStructure(structure)))
                        escape(node->child1(), node);
                    break;
                }
                    
                // FIXME: We should be able to handle GetById/GetByOffset on callee.
                // https://bugs.webkit.org/show_bug.cgi?id=143075

                case GetByOffset:
                    if (node->child2()->op() == CreateClonedArguments && node->storageAccessData().offset == clonedArgumentsLengthPropertyOffset)
                        break;
                    [[fallthrough]];
                default:
                    m_graph.doToChildren(node, [&] (Edge edge) { return escape(edge, node); });
                    break;
                }
                if (node->isPseudoTerminal())
                    break;
            }
        }

        dataLogLnIf(DFGArgumentsEliminationPhaseInternal::verbose, "After escape analysis: ", listDump(m_candidates.keys()));
    }

    // Anywhere that a candidate is live (in bytecode or in DFG), check if there is a chance of
    // interference between the stack area that the arguments object copies from and the arguments
    // object's payload. Conservatively this means that the stack region doesn't get stored to.
    void eliminateCandidatesThatInterfere()
    {
        performGraphPackingAndLivenessAnalysis(m_graph);
        m_graph.initializeNodeOwners();
        CombinedLiveness combinedLiveness(m_graph);

        BlockMap<Operands<bool>> clobberedByBlock(m_graph);
        for (BasicBlock* block : m_graph.blocksInNaturalOrder()) {
            Operands<bool>& clobberedByThisBlock = clobberedByBlock[block];
            clobberedByThisBlock = Operands<bool>(OperandsLike, m_graph.block(0)->variablesAtHead);
            for (Node* node : *block) {
                clobberize(
                    m_graph, node, NoOpClobberize(),
                    [&] (AbstractHeap heap) {
                        if (heap.kind() != Stack) {
                            ASSERT(!heap.overlaps(Stack));
                            return;
                        }
                        ASSERT(!heap.payload().isTop());
                        Operand operand = heap.operand();
                        // The register may not point to an argument or local, for example if we are looking at SetArgumentCountIncludingThis.
                        if (!operand.isHeader())
                            clobberedByThisBlock.operand(operand) = true;
                    },
                    NoOpClobberize());
            }
        }

        auto forEachDependentNode = recursableLambda([&](auto self, Node* node, const auto& functor) -> IterationStatus {
            if (functor(node) == IterationStatus::Done)
                return IterationStatus::Done;

            if (node->op() == Spread)
                return self(node->child1().node(), functor);

            if (node->op() == NewArrayWithSpread) {
                BitVector* bitVector = node->bitVector();
                for (unsigned i = node->numChildren(); i--; ) {
                    if (bitVector->get(i)) {
                        if (self(m_graph.varArgChild(node, i).node(), functor) == IterationStatus::Done)
                            return IterationStatus::Done;
                    }
                }
            }

            return IterationStatus::Continue;
        });

        using InlineCallFrames = UncheckedKeyHashSet<InlineCallFrame*, WTF::DefaultHash<InlineCallFrame*>, WTF::NullableHashTraits<InlineCallFrame*>>;
        using InlineCallFramesForCanditates = UncheckedKeyHashMap<Node*, InlineCallFrames>;
        InlineCallFramesForCanditates inlineCallFramesForCandidate;
        for (auto& [candidate, availability] : m_candidates) {
            auto& set = inlineCallFramesForCandidate.add(candidate, InlineCallFrames()).iterator->value;
            forEachDependentNode(candidate, [&](Node* dependent) {
                if (dependent->op() != CreateRest && dependent->op() != CreateDirectArguments && dependent->op() != CreateClonedArguments)
                    return IterationStatus::Continue;
                set.add(dependent->origin.semantic.inlineCallFrame());
                return IterationStatus::Continue;
            });
        }

        auto computeInterference = [&](Node* candidate, const AvailabilityMap& availability) {
            bool interfere = false;
            forEachDependentNode(candidate, [&](Node* dependent) -> IterationStatus {
                if (dependent->op() != CreateRest && dependent->op() != CreateDirectArguments && dependent->op() != CreateClonedArguments)
                    return IterationStatus::Continue;
                auto iterator = m_candidates.find(dependent);
                if (iterator == m_candidates.end())
                    return IterationStatus::Continue;
                InlineCallFrame* inlineCallFrame = dependent->origin.semantic.inlineCallFrame();
                if (inlineCallFrame) {
                    if (inlineCallFrame->isVarargs()) {
                        VirtualRegister reg(inlineCallFrame->stackOffset + CallFrameSlot::argumentCountIncludingThis);
                        if (availability.m_locals.operand(reg) != iterator->value.m_locals.operand(reg)) {
                            interfere = true;
                            return IterationStatus::Done;
                        }
                    }
                    if (inlineCallFrame->isClosureCall) {
                        VirtualRegister reg(inlineCallFrame->stackOffset + CallFrameSlot::callee);
                        if (availability.m_locals.operand(reg) != iterator->value.m_locals.operand(reg)) {
                            interfere = true;
                            return IterationStatus::Done;
                        }
                    }
                    for (unsigned i = 0; i < static_cast<unsigned>(inlineCallFrame->argumentCountIncludingThis - 1); ++i) {
                        VirtualRegister reg = VirtualRegister(inlineCallFrame->stackOffset) + CallFrame::argumentOffset(i);
                        if (availability.m_locals.operand(reg) != iterator->value.m_locals.operand(reg)) {
                            interfere = true;
                            return IterationStatus::Done;
                        }
                    }
                } else {
                    // We don't include the ArgumentCount or Callee in this case because we can be
                    // damn sure that this won't be clobbered.
                    for (unsigned i = 1; i < static_cast<unsigned>(codeBlock()->numParameters()); ++i) {
                        if (availability.m_locals.argument(i) != iterator->value.m_locals.argument(i)) {
                            interfere = true;
                            return IterationStatus::Done;
                        }
                    }
                }
                return IterationStatus::Continue;
            });
            return interfere;
        };

        auto removeViaKill = [&](BasicBlock* block, unsigned nodeIndex, Node* candidate) {
            if (!m_candidates.contains(candidate))
                return;

            for (InlineCallFrame* inlineCallFrame : inlineCallFramesForCandidate.get(candidate)) {
                // Check if this block has any clobbers that affect this candidate. This is a fairly
                // fast check.
                bool isClobberedByBlock = false;
                Operands<bool>& clobberedByThisBlock = clobberedByBlock[block];

                if (inlineCallFrame) {
                    if (inlineCallFrame->isVarargs()) {
                        isClobberedByBlock |= clobberedByThisBlock.operand(
                            VirtualRegister(inlineCallFrame->stackOffset + CallFrameSlot::argumentCountIncludingThis));
                    }

                    if (!isClobberedByBlock || inlineCallFrame->isClosureCall) {
                        isClobberedByBlock |= clobberedByThisBlock.operand(
                            VirtualRegister(inlineCallFrame->stackOffset + CallFrameSlot::callee));
                    }

                    if (!isClobberedByBlock) {
                        for (unsigned i = 0; i < static_cast<unsigned>(inlineCallFrame->argumentCountIncludingThis - 1); ++i) {
                            VirtualRegister reg =
                                VirtualRegister(inlineCallFrame->stackOffset) +
                                CallFrame::argumentOffset(i);
                            if (clobberedByThisBlock.operand(reg)) {
                                isClobberedByBlock = true;
                                break;
                            }
                        }
                    }
                } else {
                    // We don't include the ArgumentCount or Callee in this case because we can be
                    // damn sure that this won't be clobbered.
                    for (unsigned i = 1; i < static_cast<unsigned>(codeBlock()->numParameters()); ++i) {
                        if (clobberedByThisBlock.argument(i)) {
                            isClobberedByBlock = true;
                            break;
                        }
                    }
                }

                if (!isClobberedByBlock)
                    continue;

                // Check if we can immediately eliminate this candidate. If the block has a clobber
                // for this arguments allocation, and we'd have to examine every node in the block,
                // then we can just eliminate the candidate.
                if (nodeIndex == block->size() && candidate->owner != block) {
                    dataLogLnIf(DFGArgumentsEliminationPhaseInternal::verbose, "eliminating candidate: ", candidate, " because it is clobbered by block", block);
                    transitivelyRemoveCandidate(candidate);
                    return;
                }

                // This loop considers all nodes up to the nodeIndex, excluding the nodeIndex.
                //
                // Note: nodeIndex here has a double meaning. Before entering this
                // while loop, it refers to the remaining number of nodes that have
                // yet to be processed. Inside the loop, it refers to the index
                // of the current node to process (after we decrement it).
                //
                // If the remaining number of nodes is 0, we should not decrement nodeIndex.
                // Hence, we must only decrement nodeIndex inside the while loop instead of
                // in its condition statement. Note that this while loop is embedded in an
                // outer for loop. If we decrement nodeIndex in the condition statement, a
                // nodeIndex of 0 will become UINT_MAX, and the outer loop will wrongly
                // treat this as there being UINT_MAX remaining nodes to process.
                while (nodeIndex) {
                    --nodeIndex;
                    Node* node = block->at(nodeIndex);
                    if (node == candidate)
                        break;

                    bool found = false;
                    clobberize(
                        m_graph, node, NoOpClobberize(),
                        [&] (AbstractHeap heap) {
                            if (heap.kind() == Stack && !heap.payload().isTop()) {
                                if (argumentsInvolveStackSlot(inlineCallFrame, heap.operand()))
                                    found = true;
                                return;
                            }
                            if (heap.overlaps(Stack))
                                found = true;
                        },
                        NoOpClobberize());

                    if (found) {
                        dataLogLnIf(DFGArgumentsEliminationPhaseInternal::verbose, "eliminating candidate: ", candidate, " because it is clobbered by ", block->at(nodeIndex));
                        transitivelyRemoveCandidate(candidate);
                        return;
                    }
                }
            }
        };

        for (BasicBlock* block : m_graph.blocksInPreOrder()) {
            // Stop if we've already removed all candidates.
            if (m_candidates.isEmpty())
                return;

            LocalOSRAvailabilityCalculator calculator(m_graph);
            calculator.beginBlock(block);

            bool clobberStack = false;
            for (unsigned i = clobberedByBlock[block].size(); i--;) {
                if (clobberedByBlock[block][i])
                    clobberStack = true;
            }

            for (Node* candidate : combinedLiveness.liveAtHead[block]) {
                if (!m_candidates.contains(candidate))
                    continue;
                if (computeInterference(candidate, calculator.m_availability)) {
                    dataLogLnIf(DFGArgumentsEliminationPhaseInternal::verbose, "eliminating candidate: ", candidate, " because it is clobbered at block ", block);
                    transitivelyRemoveCandidate(candidate);
                    continue;
                }
            }

            if (clobberStack) {
                for (Node* node : combinedLiveness.liveAtTail[block])
                    removeViaKill(block, block->size(), node);

                for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                    Node* node = block->at(nodeIndex);
                    if (nodeIndex) {
                        forAllKilledNodesAtNodeIndex(
                            m_graph, calculator.m_availability, block, nodeIndex,
                            [&] (Node* node) {
                                removeViaKill(block, nodeIndex, node);
                            });
                    }

                    calculator.executeNode(node);
                }
            }
        }

        // Q: How do we handle OSR exit with a live PhantomArguments at a point where the inline call
        // frame is dead?  A: Naively we could say that PhantomArguments must escape the stack slots. But
        // that would break PutStack sinking, which in turn would break object allocation sinking, in
        // cases where we have a varargs call to an otherwise pure method. So, we need something smarter.
        // For the outermost arguments, we just have a PhantomArguments that magically knows that it
        // should load the arguments from the call frame. For the inline arguments, we have the heap map
        // in the availabiltiy map track each possible inline argument as a promoted heap location. If the
        // PutStacks for those arguments aren't sunk, those heap locations will map to very trivial
        // availabilities (they will be flush availabilities). But if sinking happens then those
        // availabilities may become whatever. OSR exit should be able to handle this quite naturally,
        // since those availabilities speak of the stack before the optimizing compiler stack frame is
        // torn down.

        dataLogLnIf(DFGArgumentsEliminationPhaseInternal::verbose, "After interference analysis: ", listDump(m_candidates.keys()));
    }

    void transform()
    {
        bool modifiedCFG = false;
        
        InsertionSet insertionSet(m_graph);

        for (BasicBlock* block : m_graph.blocksInPreOrder()) {
            Node* pseudoTerminal = nullptr;
            for (unsigned nodeIndex = 0; nodeIndex < block->size(); ++nodeIndex) {
                Node* node = block->at(nodeIndex);
                
                auto getArrayLength = [&] (Node* candidate) -> Node* {
                    return emitCodeToGetArgumentsArrayLength(
                        insertionSet, candidate, nodeIndex, node->origin);
                };

                auto isEliminatedAllocation = [&] (Node* candidate) -> bool {
                    if (!m_candidates.contains(candidate))
                        return false;
                    // We traverse in such a way that we are guaranteed to see a def before a use.
                    // Therefore, we should have already transformed the allocation before the use
                    // of an allocation.
                    ASSERT(candidate->op() == PhantomCreateRest || candidate->op() == PhantomDirectArguments || candidate->op() == PhantomClonedArguments
                        || candidate->op() == PhantomSpread || candidate->op() == PhantomNewArrayWithSpread || candidate->op() == PhantomNewArrayBuffer);
                    return true;
                };

                switch (node->op()) {
                case CreateDirectArguments:
                    if (!m_candidates.contains(node))
                        break;
                    
                    node->setOpAndDefaultFlags(PhantomDirectArguments);
                    break;

                case CreateRest:
                    if (!m_candidates.contains(node))
                        break;

                    ASSERT(node->origin.exitOK);
                    ASSERT(node->child1().useKind() == Int32Use);
                    insertionSet.insertNode(
                        nodeIndex, SpecNone, Check, node->origin,
                        node->child1()); 

                    node->setOpAndDefaultFlags(PhantomCreateRest);
                    // We don't need this parameter for OSR exit, we can find out all the information
                    // we need via the static parameter count and the dynamic argument count.
                    node->child1() = Edge(); 
                    break;
                    
                case CreateClonedArguments:
                    if (!m_candidates.contains(node))
                        break;
                    
                    node->setOpAndDefaultFlags(PhantomClonedArguments);
                    break;

                case Spread:
                    if (!m_candidates.contains(node))
                        break;
                    
                    node->setOpAndDefaultFlags(PhantomSpread);
                    break;

                case NewArrayWithSpread:
                    if (!m_candidates.contains(node))
                        break;
                    
                    node->setOpAndDefaultFlags(PhantomNewArrayWithSpread);
                    break;

                case NewArrayBuffer:
                    if (!m_candidates.contains(node))
                        break;

                    node->setOpAndDefaultFlags(PhantomNewArrayBuffer);
                    node->child1() = Edge(insertionSet.insertConstant(nodeIndex, node->origin, node->cellOperand()));
                    break;
                    
                case GetFromArguments: {
                    Node* candidate = node->child1().node();
                    if (!isEliminatedAllocation(candidate))
                        break;
                    
                    DFG_ASSERT(
                        m_graph, node, node->child1()->op() == PhantomDirectArguments, node->child1()->op());
                    VirtualRegister reg =
                        virtualRegisterForArgumentIncludingThis(node->capturedArgumentsOffset().offset() + 1) +
                        node->origin.semantic.stackOffset();
                    StackAccessData* data = m_graph.m_stackAccessData.add(reg, FlushedJSValue);
                    node->convertToGetStack(data);
                    break;
                }

                case GetByOffset: {
                    Node* candidate = node->child2().node();
                    if (!isEliminatedAllocation(candidate))
                        break;

                    ASSERT(candidate->op() == PhantomClonedArguments);
                    ASSERT(node->storageAccessData().offset == clonedArgumentsLengthPropertyOffset);

                    // Meh, this is kind of hackish - we use an Identity so that we can reuse the
                    // getArrayLength() helper.
                    node->convertToIdentityOn(getArrayLength(candidate));
                    break;
                }
                    
                case GetArrayLength: {
                    Node* candidate = node->child1().node();
                    if (!isEliminatedAllocation(candidate))
                        break;
                    
                    node->convertToIdentityOn(getArrayLength(candidate));
                    break;
                }

                case GetByVal: {
                    // FIXME: For ClonedArguments, we would have already done a separate bounds check.
                    // This code will cause us to have two bounds checks - the original one that we
                    // already factored out in SSALoweringPhase, and the new one we insert here, which is
                    // often implicitly part of GetMyArgumentByVal. B3 will probably eliminate the
                    // second bounds check, but still - that's just silly.
                    // https://bugs.webkit.org/show_bug.cgi?id=143076
                    
                    Node* candidate = m_graph.varArgChild(node, 0).node();
                    if (!isEliminatedAllocation(candidate))
                        break;

                    unsigned numberOfArgumentsToSkip = 0;
                    if (candidate->op() == PhantomCreateRest)
                        numberOfArgumentsToSkip = candidate->numberOfArgumentsToSkip();
                    
                    Node* result = nullptr;
                    if (m_graph.varArgChild(node, 1)->isInt32Constant()) {
                        unsigned index = m_graph.varArgChild(node, 1)->asUInt32();
                        InlineCallFrame* inlineCallFrame = candidate->origin.semantic.inlineCallFrame();
                        index += numberOfArgumentsToSkip;
                        
                        bool safeToGetStack = index >= numberOfArgumentsToSkip;
                        if (inlineCallFrame)
                            safeToGetStack &= index < static_cast<unsigned>(inlineCallFrame->argumentCountIncludingThis - 1);
                        else {
                            safeToGetStack &=
                                index < static_cast<unsigned>(codeBlock()->numParameters()) - 1;
                        }
                        if (safeToGetStack) {
                            StackAccessData* data;
                            VirtualRegister arg = virtualRegisterForArgumentIncludingThis(index + 1);
                            if (inlineCallFrame)
                                arg += inlineCallFrame->stackOffset;
                            data = m_graph.m_stackAccessData.add(arg, FlushedJSValue);
                            
                            Node* check = nullptr;
                            if (!inlineCallFrame || inlineCallFrame->isVarargs()) {
                                check = insertionSet.insertNode(
                                    nodeIndex, SpecNone, CheckInBounds, node->origin,
                                    m_graph.varArgChild(node, 1), Edge(getArrayLength(candidate), Int32Use));
                            }
                            
                            result = insertionSet.insertNode(
                                nodeIndex, node->prediction(), GetStack, node->origin, OpInfo(data), Edge(check, UntypedUse));
                        }
                    }
                    
                    if (!result) {
                        NodeType op;
                        if (node->arrayMode().isInBounds())
                            op = GetMyArgumentByVal;
                        else
                            op = GetMyArgumentByValOutOfBounds;
                        result = insertionSet.insertNode(
                            nodeIndex, node->prediction(), op, node->origin, OpInfo(numberOfArgumentsToSkip),
                            m_graph.varArgChild(node, 0), m_graph.varArgChild(node, 1));
                    }

                    // Need to do this because we may have a data format conversion here.
                    node->convertToIdentityOn(result);
                    break;
                }
                
                case VarargsLength: {
                    Node* candidate = node->argumentsChild().node();
                    if (!isEliminatedAllocation(candidate))
                        break;

                    // VarargsLength can exit, so it better be exitOK.
                    DFG_ASSERT(m_graph, node, node->origin.exitOK);
                    NodeOrigin origin = node->origin.withExitOK(true);


                    node->convertToIdentityOn(emitCodeToGetArgumentsArrayLength(insertionSet, candidate, nodeIndex, origin, /* addThis = */ true));
                    break;
                }

                case LoadVarargs: {
                    Node* candidate = node->argumentsChild().node();
                    if (!isEliminatedAllocation(candidate))
                        break;
                    
                    // LoadVarargs can exit, so it better be exitOK.
                    DFG_ASSERT(m_graph, node, node->origin.exitOK);
                    bool canExit = true;
                    LoadVarargsData* varargsData = node->loadVarargsData();

                    auto storeArgumentCountIncludingThis = [&] (unsigned argumentCountIncludingThis) {
                        Node* argumentCountIncludingThisNode = insertionSet.insertConstant(
                            nodeIndex, node->origin.withExitOK(canExit),
                            jsNumber(argumentCountIncludingThis));
                        insertionSet.insertNode(
                            nodeIndex, SpecNone, KillStack, node->origin.takeValidExit(canExit),
                            OpInfo(varargsData->count));
                        insertionSet.insertNode(
                            nodeIndex, SpecNone, MovHint, node->origin.takeValidExit(canExit),
                            OpInfo(varargsData->count), Edge(argumentCountIncludingThisNode));
                        insertionSet.insertNode(
                            nodeIndex, SpecNone, PutStack, node->origin.withExitOK(canExit),
                            OpInfo(m_graph.m_stackAccessData.add(varargsData->count, FlushedInt32)),
                            Edge(argumentCountIncludingThisNode, KnownInt32Use));
                    };

                    auto storeValue = [&] (Node* value, unsigned storeIndex) {
                        VirtualRegister reg = varargsData->start + storeIndex;
                        ASSERT(reg.isLocal());
                        StackAccessData* data =
                            m_graph.m_stackAccessData.add(reg, FlushedJSValue);
                        
                        insertionSet.insertNode(
                            nodeIndex, SpecNone, KillStack, node->origin.takeValidExit(canExit), OpInfo(reg));
                        insertionSet.insertNode(
                            nodeIndex, SpecNone, MovHint, node->origin.takeValidExit(canExit),
                            OpInfo(reg), Edge(value));
                        insertionSet.insertNode(
                            nodeIndex, SpecNone, PutStack, node->origin.withExitOK(canExit),
                            OpInfo(data), Edge(value));
                    };

                    if (candidate->op() == PhantomNewArrayWithSpread || candidate->op() == PhantomNewArrayBuffer || candidate->op() == PhantomSpread) {
                        auto canConvertToStaticLoadStores = recursableLambda([&] (auto self, Node* candidate) -> bool {
                            if (candidate->op() == PhantomSpread)
                                return self(candidate->child1().node());

                            if (candidate->op() == PhantomNewArrayWithSpread) {
                                BitVector* bitVector = candidate->bitVector();
                                for (unsigned i = 0; i < candidate->numChildren(); i++) {
                                    if (bitVector->get(i)) {
                                        if (!self(m_graph.varArgChild(candidate, i).node()))
                                            return false;
                                    }
                                }
                                return true;
                            }

                            // PhantomNewArrayBuffer only contains constants. It can always convert LoadVarargs to static load stores.
                            if (candidate->op() == PhantomNewArrayBuffer)
                                return true;

                            ASSERT(candidate->op() == PhantomCreateRest);
                            InlineCallFrame* inlineCallFrame = candidate->origin.semantic.inlineCallFrame();
                            return inlineCallFrame && !inlineCallFrame->isVarargs();
                        });

                        if (canConvertToStaticLoadStores(candidate)) {
                            auto countNumberOfArguments = recursableLambda([&](auto self, Node* candidate) -> unsigned {
                                if (candidate->op() == PhantomSpread)
                                    return self(candidate->child1().node());

                                if (candidate->op() == PhantomNewArrayWithSpread) {
                                    BitVector* bitVector = candidate->bitVector();
                                    unsigned result = 0;
                                    for (unsigned i = 0; i < candidate->numChildren(); i++) {
                                        if (bitVector->get(i))
                                            result += self(m_graph.varArgChild(candidate, i).node());
                                        else
                                            ++result;
                                    }
                                    return result;
                                }

                                if (candidate->op() == PhantomNewArrayBuffer)
                                    return candidate->castOperand<JSImmutableButterfly*>()->length();

                                ASSERT(candidate->op() == PhantomCreateRest);
                                unsigned numberOfArgumentsToSkip = candidate->numberOfArgumentsToSkip();
                                InlineCallFrame* inlineCallFrame = candidate->origin.semantic.inlineCallFrame();
                                unsigned frameArgumentCount = static_cast<unsigned>(inlineCallFrame->argumentCountIncludingThis - 1);
                                if (frameArgumentCount >= numberOfArgumentsToSkip)
                                    return frameArgumentCount - numberOfArgumentsToSkip;
                                return 0;
                            });

                            unsigned argumentCountIncludingThis = 1 + countNumberOfArguments(candidate); // |this|
                            if (argumentCountIncludingThis <= varargsData->limit) {
                                DFG_ASSERT(m_graph, node, varargsData->limit - 1 >= varargsData->mandatoryMinimum, varargsData->limit, varargsData->mandatoryMinimum);
                                // Define our limit to exclude "this", since that's a bit easier to reason about.
                                unsigned limit = varargsData->limit - 1;

                                auto forwardNode = recursableLambda([&](auto self, Node* candidate, unsigned storeIndex) -> unsigned {
                                    if (candidate->op() == PhantomSpread)
                                        return self(candidate->child1().node(), storeIndex);

                                    if (candidate->op() == PhantomNewArrayWithSpread) {
                                        BitVector* bitVector = candidate->bitVector();
                                        for (unsigned i = 0; i < candidate->numChildren(); i++) {
                                            if (bitVector->get(i))
                                                storeIndex = self(m_graph.varArgChild(candidate, i).node(), storeIndex);
                                            else {
                                                Node* value = m_graph.varArgChild(candidate, i).node();
                                                storeValue(value, storeIndex++);
                                            }
                                        }
                                        return storeIndex;
                                    }

                                    if (candidate->op() == PhantomNewArrayBuffer) {
                                        auto* array = candidate->castOperand<JSImmutableButterfly*>();
                                        for (unsigned index = 0; index < array->length(); ++index) {
                                            JSValue constant;
                                            if (candidate->indexingType() == ArrayWithDouble)
                                                constant = jsDoubleNumber(array->get(index).asNumber());
                                            else
                                                constant = array->get(index);
                                            Node* value = insertionSet.insertConstant(nodeIndex, node->origin.withExitOK(canExit), constant);
                                            storeValue(value, storeIndex++);
                                        }
                                        return storeIndex;
                                    }

                                    ASSERT(candidate->op() == PhantomCreateRest);
                                    unsigned numberOfArgumentsToSkip = candidate->numberOfArgumentsToSkip();
                                    InlineCallFrame* inlineCallFrame = candidate->origin.semantic.inlineCallFrame();
                                    unsigned frameArgumentCount = static_cast<unsigned>(inlineCallFrame->argumentCountIncludingThis - 1);
                                    for (unsigned loadIndex = numberOfArgumentsToSkip; loadIndex < frameArgumentCount; ++loadIndex) {
                                        VirtualRegister reg = virtualRegisterForArgumentIncludingThis(loadIndex + 1) + inlineCallFrame->stackOffset;
                                        StackAccessData* data = m_graph.m_stackAccessData.add(reg, FlushedJSValue);
                                        Node* value = insertionSet.insertNode(
                                            nodeIndex, SpecNone, GetStack, node->origin.withExitOK(canExit),
                                            OpInfo(data));
                                        storeValue(value, storeIndex++);
                                    }
                                    return storeIndex;
                                });

                                unsigned storeIndex = forwardNode(candidate, 0);
                                RELEASE_ASSERT(storeIndex <= limit);
                                Node* undefined = nullptr;
                                for (; storeIndex < limit; ++storeIndex) {
                                    if (!undefined) {
                                        undefined = insertionSet.insertConstant(
                                            nodeIndex, node->origin.withExitOK(canExit), jsUndefined());
                                    }
                                    storeValue(undefined, storeIndex);
                                }
                                storeArgumentCountIncludingThis(argumentCountIncludingThis);
                                
                                node->remove(m_graph);
                                node->origin.exitOK = canExit;
                                break;
                            }
                        }
                    } else {
                        unsigned numberOfArgumentsToSkip = 0;
                        if (candidate->op() == PhantomCreateRest)
                            numberOfArgumentsToSkip = candidate->numberOfArgumentsToSkip();
                        varargsData->offset += numberOfArgumentsToSkip;

                        InlineCallFrame* inlineCallFrame = candidate->origin.semantic.inlineCallFrame();

                        if (inlineCallFrame && !inlineCallFrame->isVarargs()) {
                            unsigned argumentCountIncludingThis = inlineCallFrame->argumentCountIncludingThis;
                            if (argumentCountIncludingThis > varargsData->offset)
                                argumentCountIncludingThis -= varargsData->offset;
                            else
                                argumentCountIncludingThis = 1;
                            RELEASE_ASSERT(argumentCountIncludingThis >= 1);

                            if (argumentCountIncludingThis <= varargsData->limit) {
                                DFG_ASSERT(m_graph, node, varargsData->limit - 1 >= varargsData->mandatoryMinimum, varargsData->limit, varargsData->mandatoryMinimum);
                                // Define our limit to exclude "this", since that's a bit easier to reason about.
                                unsigned limit = varargsData->limit - 1;
                                Node* undefined = nullptr;
                                for (unsigned storeIndex = 0; storeIndex < limit; ++storeIndex) {
                                    // First determine if we have an element we can load, and load it if
                                    // possible.
                                    
                                    Node* value = nullptr;
                                    unsigned loadIndex = storeIndex + varargsData->offset;

                                    if (loadIndex + 1 < inlineCallFrame->argumentCountIncludingThis) {
                                        VirtualRegister reg = virtualRegisterForArgumentIncludingThis(loadIndex + 1) + inlineCallFrame->stackOffset;
                                        StackAccessData* data = m_graph.m_stackAccessData.add(
                                            reg, FlushedJSValue);
                                        
                                        value = insertionSet.insertNode(
                                            nodeIndex, SpecNone, GetStack, node->origin.withExitOK(canExit),
                                            OpInfo(data));
                                    } else {
                                        // FIXME: We shouldn't have to store anything if
                                        // storeIndex >= varargsData->mandatoryMinimum, but we will still
                                        // have GetStacks in that range. So if we don't do the stores, we'll
                                        // have degenerate IR: we'll have GetStacks of something that didn't
                                        // have PutStacks.
                                        // https://bugs.webkit.org/show_bug.cgi?id=147434
                                        
                                        if (!undefined) {
                                            undefined = insertionSet.insertConstant(
                                                nodeIndex, node->origin.withExitOK(canExit), jsUndefined());
                                        }
                                        value = undefined;
                                    }
                                    
                                    // Now that we have a value, store it.
                                    storeValue(value, storeIndex);
                                }
                                storeArgumentCountIncludingThis(argumentCountIncludingThis);
                                
                                node->remove(m_graph);
                                node->origin.exitOK = canExit;
                                break;
                            }
                        }
                    }

                    node->setOpAndDefaultFlags(ForwardVarargs);
                    break;
                }
                    
                case CallVarargs:
                case ConstructVarargs:
                case TailCallVarargs:
                case TailCallVarargsInlinedCaller: {
                    Node* candidate = node->child3().node();
                    if (!isEliminatedAllocation(candidate))
                        break;

                    auto convertToStaticArgumentCountCall = [&] (const Vector<Node*>& arguments) {
                        unsigned firstChild = m_graph.m_varArgChildren.size();
                        m_graph.m_varArgChildren.append(node->child1());
                        m_graph.m_varArgChildren.append(node->child2());
                        for (Node* argument : arguments)
                            m_graph.m_varArgChildren.append(Edge(argument));
                        switch (node->op()) {
                        case CallVarargs:
                            node->setOpAndDefaultFlags(Call);
                            break;
                        case ConstructVarargs:
                            node->setOpAndDefaultFlags(Construct);
                            break;
                        case TailCallVarargs:
                            node->setOpAndDefaultFlags(TailCall);
                            break;
                        case TailCallVarargsInlinedCaller:
                            node->setOpAndDefaultFlags(TailCallInlinedCaller);
                            break;
                        default:
                            RELEASE_ASSERT_NOT_REACHED();
                        }
                        node->children = AdjacencyList(
                            AdjacencyList::Variable,
                            firstChild, m_graph.m_varArgChildren.size() - firstChild);
                    };

                    auto convertToForwardsCall = [&] () {
                        switch (node->op()) {
                        case CallVarargs:
                            node->setOpAndDefaultFlags(CallForwardVarargs);
                            break;
                        case ConstructVarargs:
                            node->setOpAndDefaultFlags(ConstructForwardVarargs);
                            break;
                        case TailCallVarargs:
                            node->setOpAndDefaultFlags(TailCallForwardVarargs);
                            break;
                        case TailCallVarargsInlinedCaller:
                            node->setOpAndDefaultFlags(TailCallForwardVarargsInlinedCaller);
                            break;
                        default:
                            RELEASE_ASSERT_NOT_REACHED();
                        }
                    };
                    
                    if (candidate->op() == PhantomNewArrayWithSpread || candidate->op() == PhantomNewArrayBuffer || candidate->op() == PhantomSpread) {
                        auto canTransformToStaticArgumentCountCall = recursableLambda([&](auto self, Node* candidate) -> bool {
                            if (candidate->op() == PhantomSpread)
                                return self(candidate->child1().node());

                            if (candidate->op() == PhantomNewArrayWithSpread) {
                                BitVector* bitVector = candidate->bitVector();
                                for (unsigned i = 0; i < candidate->numChildren(); i++) {
                                    if (bitVector->get(i)) {
                                        Node* spread = m_graph.varArgChild(candidate, i).node();
                                        if (!self(spread))
                                            return false;
                                    }
                                }
                                return true;
                            }

                            // PhantomNewArrayBuffer only contains constants. It can always convert LoadVarargs to static load stores.
                            if (candidate->op() == PhantomNewArrayBuffer)
                                return true;

                            ASSERT(candidate->op() == PhantomCreateRest);
                            InlineCallFrame* inlineCallFrame = candidate->origin.semantic.inlineCallFrame();
                            return inlineCallFrame && !inlineCallFrame->isVarargs();
                        });

                        if (canTransformToStaticArgumentCountCall(candidate)) {
                            Vector<Node*> arguments;
                            auto appendNode = recursableLambda([&](auto self, Node* candidate) -> void {
                                if (candidate->op() == PhantomSpread) {
                                    self(candidate->child1().node());
                                    return;
                                }

                                if (candidate->op() == PhantomNewArrayWithSpread) {
                                    BitVector* bitVector = candidate->bitVector();
                                    for (unsigned i = 0; i < candidate->numChildren(); i++) {
                                        Node* child = m_graph.varArgChild(candidate, i).node();
                                        if (bitVector->get(i))
                                            self(child);
                                        else
                                            arguments.append(child);
                                    }
                                    return;
                                }

                                if (candidate->op() == PhantomNewArrayBuffer) {
                                    bool canExit = true;
                                    auto* array = candidate->castOperand<JSImmutableButterfly*>();
                                    for (unsigned index = 0; index < array->length(); ++index) {
                                        JSValue constant;
                                        if (candidate->indexingType() == ArrayWithDouble)
                                            constant = jsDoubleNumber(array->get(index).asNumber());
                                        else
                                            constant = array->get(index);
                                        arguments.append(insertionSet.insertConstant(nodeIndex, node->origin.withExitOK(canExit), constant));
                                    }
                                    return;
                                }

                                ASSERT(candidate->op() == PhantomCreateRest);
                                InlineCallFrame* inlineCallFrame = candidate->origin.semantic.inlineCallFrame();
                                unsigned numberOfArgumentsToSkip = candidate->numberOfArgumentsToSkip();
                                for (unsigned i = 1 + numberOfArgumentsToSkip; i < inlineCallFrame->argumentCountIncludingThis; ++i) {
                                    StackAccessData* data = m_graph.m_stackAccessData.add(
                                        virtualRegisterForArgumentIncludingThis(i) + inlineCallFrame->stackOffset,
                                        FlushedJSValue);

                                    Node* value = insertionSet.insertNode(
                                        nodeIndex, SpecNone, GetStack, node->origin, OpInfo(data));

                                    arguments.append(value);
                                }
                            });

                            appendNode(candidate);
                            convertToStaticArgumentCountCall(arguments);
                        } else
                            convertToForwardsCall();
                    } else {
                        unsigned numberOfArgumentsToSkip = 0;
                        if (candidate->op() == PhantomCreateRest)
                            numberOfArgumentsToSkip = candidate->numberOfArgumentsToSkip();
                        CallVarargsData* varargsData = node->callVarargsData();
                        varargsData->firstVarArgOffset += numberOfArgumentsToSkip;

                        InlineCallFrame* inlineCallFrame = candidate->origin.semantic.inlineCallFrame();
                        if (inlineCallFrame && !inlineCallFrame->isVarargs()) {
                            Vector<Node*> arguments;
                            for (unsigned i = 1 + varargsData->firstVarArgOffset; i < inlineCallFrame->argumentCountIncludingThis; ++i) {
                                StackAccessData* data = m_graph.m_stackAccessData.add(
                                    virtualRegisterForArgumentIncludingThis(i) + inlineCallFrame->stackOffset,
                                    FlushedJSValue);
                                
                                Node* value = insertionSet.insertNode(
                                    nodeIndex, SpecNone, GetStack, node->origin, OpInfo(data));
                                
                                arguments.append(value);
                            }
                            
                            convertToStaticArgumentCountCall(arguments);
                        } else
                            convertToForwardsCall();
                    }

                    break;
                }
                    
                case CheckArrayOrEmpty:
                case CheckArray:
                case GetButterfly:
                case FilterGetByStatus:
                case FilterPutByStatus:
                case FilterCallLinkStatus:
                case FilterInByStatus:
                case FilterDeleteByStatus:
                case FilterCheckPrivateBrandStatus:
                case FilterSetPrivateBrandStatus: {
                    if (!isEliminatedAllocation(node->child1().node()))
                        break;
                    node->remove(m_graph);
                    break;
                }

                case CheckStructureOrEmpty:
                case CheckStructure:
                    if (!isEliminatedAllocation(node->child1().node()))
                        break;
                    node->child1() = Edge(); // Remove the cell check since we've proven it's not needed and FTL lowering might botch this.
                    node->remove(m_graph);
                    break;
                    
                default:
                    break;
                }

                if (node->isPseudoTerminal()) {
                    pseudoTerminal = node;
                    break;
                }
            }

            insertionSet.execute(block);

            if (pseudoTerminal) {
                for (unsigned i = 0; i < block->size(); ++i) {
                    Node* node = block->at(i);
                    if (node != pseudoTerminal)
                        continue;
                    block->resize(i + 1);
                    block->append(m_graph.addNode(SpecNone, Unreachable, node->origin));
                    modifiedCFG = true;
                    break;
                }
            }
        }

        if (modifiedCFG) {
            m_graph.invalidateCFG();
            m_graph.resetReachability();
            m_graph.killUnreachableBlocks();
        }
    }
    
    UncheckedKeyHashMap<Node*, AvailabilityMap> m_candidates;
};

} // anonymous namespace

bool performArgumentsElimination(Graph& graph)
{
    return runPhase<ArgumentsEliminationPhase>(graph);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

