#include "canvas/interaction/editor_history.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "object_store_mutator.hpp"
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstdio>
#include <new>
namespace { thread_local std::ptrdiff_t fail_after = -1; }
// Isolated executable: one real post-initialization C++ allocation fails.
void* operator new(std::size_t size) {
    if (fail_after == 0) { fail_after = -1; throw std::bad_alloc(); }
    if (fail_after > 0) --fail_after;
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
namespace {
using namespace canvas::semantic;
using namespace canvas::interaction;
struct Runtime : HistorySubmitPort {
    IndexedObjectStore objects; AppliedOperationLedger ledger; SemanticGenerationState generation;
    CanonicalCommitClock clock{RuntimeEpoch(9)}; OperationEngine engine; EditorHistory history{*this};
    std::uint64_t nonce = 0x8000000000000000ULL;
    OperationId allocateOperationId() override { return OperationId{ObjectId::fromUint64(nonce++)}; }
    SubmitResult submit(std::span<const Operation>, ApplySource) override { return SubmitResult::rejected(); }
    SubmitResult submitAdmitted(const Operation& op, ApplySource source, OperationAdmission& admission) override {
        const auto r = engine.apply(op,source,objects,ledger,generation,clock,&admission);
        return r.disposition == ApplyDisposition::kApplied || r.disposition == ApplyDisposition::kAlreadyApplied ? SubmitResult::acceptedResult() : SubmitResult::rejected();
    }
};
ObjectRecord section() {
    ObjectRecord r; r.id=ObjectId::fromUint64(2); r.kind=ObjectKind::kSection; r.kind_version=1;
    r.placement.order_key=OrderKey({2}); r.content=SectionContent{{0,0,100,80},"Section",4,16}; return r;
}
Operation operation(OperationPayload payload, std::uint64_t n=100) {
    return {OperationId{ObjectId::fromUint64(n)},DocumentId{ObjectId::fromUint64(200)},1,1,std::move(payload)};
}
Operation command(int scenario) {
    const auto r = section();
    if (scenario == 0 || scenario == 4) return operation(InsertObjectsOp{{r}});
    if (scenario == 1) {
        auto content=std::get<SectionContent>(r.content); auto next=content; next.title="renamed";
        return operation(SetSectionContentOp{{{r.id,1,content,next}},{}},101);
    }
    if (scenario == 2 || scenario == 5) return operation(DeleteObjectsOp{{r.id}},102);
    if (scenario == 6) { auto t=r.transform;t.tx=37;t.ty=-5;return operation(SetTransformsOp{{{r.id,t}}},104); }
    if (scenario == 7) return operation(PatchPropertiesOp{{{r.id,3U,PropertyPatchAction::kSet,PropertyValue{0.5F}}}},105);
    EditContainerTopologyOp op; op.effect=ReleaseWrappersEffect{{r}};
    op.guard.nodes={{r.id,r.kind,1,r.placement,r.transform}};
    op.guard.child_sets={{{},{{r.id,r.placement.order_key}}},{r.id,{}}};
    return operation(op,103);
}
void seed(Runtime& rt,int scenario) {
    if (scenario == 0 || scenario == 4) return;
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(rt.objects,section()));
    if (scenario == 5) {
        auto child=section();child.id=ObjectId::fromUint64(3);child.kind=ObjectKind::kShape;
        child.content=ShapeContent{1,20,20};child.placement.parent_id=section().id;
        ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(rt.objects,child));
        auto connector=child;connector.id=ObjectId::fromUint64(4);connector.kind=ObjectKind::kConnector;
        connector.placement.parent_id.reset();connector.placement.order_key=OrderKey({4});
        connector.content=ConnectorContent{ConnectorEndpoint{AttachedEndpoint{child.id,StablePortAnchor{1}}},ConnectorEndpoint{FreePointEndpoint{{200,100}}},ConnectorRouting::kStraight};
        ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(rt.objects,connector));
    }
}
void sweep(int scenario, int direction) {
    const auto f=command(scenario);
    ASSERT_TRUE(SemanticCodec::encodeProtobufOperation(f).ok());
    // Initialize SDK descriptors for both actual directions before the scoped
    // precommit sweep. Cold SDK call_once recovery is not qualified here.
    {
        Runtime warm;
        seed(warm,scenario);
        ASSERT_TRUE(warm.history.executeUndoable(f)); ASSERT_TRUE(warm.history.undo()); ASSERT_TRUE(warm.history.redo());
    }
    std::size_t rejected=0; bool succeeded=false;
    for (std::ptrdiff_t failure=0; failure<8192; ++failure) {
        Runtime rt;
        seed(rt,scenario);
        if (scenario == 4) {
            auto other=section(); other.id=ObjectId::fromUint64(3); other.placement.order_key=OrderKey({3});
            ASSERT_TRUE(rt.history.executeUndoable(operation(InsertObjectsOp{{other}},50)));
            ASSERT_TRUE(rt.history.undo());
        }
        if (direction) {
            ASSERT_TRUE(rt.history.executeUndoable(f));
            if (direction == 2) ASSERT_TRUE(rt.history.undo());
        }
        const auto before=rt.objects.allObjects(); const auto generation=rt.generation.current();
        const auto ordinal=rt.clock.lastCommittedOrdinal(); const auto historySize=rt.history.size();
        const bool canUndo=rt.history.canUndo(),canRedo=rt.history.canRedo();
        fail_after=failure;
        const bool accepted = direction==0 ? rt.history.executeUndoable(f) : direction==1 ? rt.history.undo() : rt.history.redo();
        fail_after=-1;
        if (accepted) { succeeded=true; EXPECT_EQ(rt.clock.lastCommittedOrdinal().value(),ordinal.value()+1); break; }
        ++rejected;
        ASSERT_EQ(rt.objects.allObjects(),before); EXPECT_EQ(rt.generation.current(),generation);
        EXPECT_EQ(rt.clock.lastCommittedOrdinal(),ordinal); EXPECT_EQ(rt.history.size(),historySize);
        EXPECT_EQ(rt.history.canUndo(),canUndo); EXPECT_EQ(rt.history.canRedo(),canRedo);
        EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(rt.objects));
        if (!direction) EXPECT_FALSE(rt.ledger.find(f.id));
    }
    std::fprintf(stdout,"C real allocation sweep scenario=%d direction=%d rejected=%zu\n",scenario,direction,rejected);
    EXPECT_TRUE(succeeded); EXPECT_GT(rejected,20U);
}
TEST(SectionHistoryAllocation,InsertForward) { sweep(0,0); }
TEST(SectionHistoryAllocation,InsertUndo) { sweep(0,1); }
TEST(SectionHistoryAllocation,InsertRedo) { sweep(0,2); }
TEST(SectionHistoryAllocation,ContentForward) { sweep(1,0); }
TEST(SectionHistoryAllocation,ContentUndo) { sweep(1,1); }
TEST(SectionHistoryAllocation,ContentRedo) { sweep(1,2); }
TEST(SectionHistoryAllocation,DeleteForward) { sweep(2,0); }
TEST(SectionHistoryAllocation,DeleteUndo) { sweep(2,1); }
TEST(SectionHistoryAllocation,DeleteRedo) { sweep(2,2); }
TEST(SectionHistoryAllocation,TopologyForward) { sweep(3,0); }
TEST(SectionHistoryAllocation,TopologyUndo) { sweep(3,1); }
TEST(SectionHistoryAllocation,RejectedForwardPreservesExistingRedo) { sweep(4,0); }
TEST(SectionHistoryAllocation,TopologyRedo) { sweep(3,2); }
TEST(SectionHistoryAllocation,DeleteCompoundForward) { sweep(5,0); }
TEST(SectionHistoryAllocation,DeleteCompoundUndo) { sweep(5,1); }
TEST(SectionHistoryAllocation,DeleteCompoundRedo) { sweep(5,2); }
TEST(SectionHistoryAllocation,MoveForward) { sweep(6,0); }
TEST(SectionHistoryAllocation,MoveUndo) { sweep(6,1); }
TEST(SectionHistoryAllocation,MoveRedo) { sweep(6,2); }
TEST(SectionHistoryAllocation,PropertyForward) { sweep(7,0); }
TEST(SectionHistoryAllocation,PropertyUndo) { sweep(7,1); }
TEST(SectionHistoryAllocation,PropertyRedo) { sweep(7,2); }

}
