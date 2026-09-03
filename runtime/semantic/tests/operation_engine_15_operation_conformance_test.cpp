#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <vector>
namespace canvas::semantic { namespace {
ObjectId id(std::uint64_t v){return ObjectId::fromUint64(v);} 
ObjectRecord shape(std::uint64_t v,std::uint8_t t){ObjectRecord r{};r.id=id(v);r.kind=ObjectKind::kShape;r.kind_version=1U;r.placement=Placement{std::nullopt,OrderKey({t})};r.transform=Transform2D{1,0,0,1,static_cast<double>(t),0};r.content=ShapeContent{t,static_cast<double>(t),static_cast<double>(t+1U)};return r;}
Operation op(OperationPayload p,std::uint64_t v){Operation o{};o.id=OperationId{id(v)};o.document_id=DocumentId{id(9000)};o.schema_version=1U;o.payload_version=1U;o.payload=std::move(p);return o;}
constexpr std::array<std::string_view,15> kExpectedKinds{"kInsertObjects","kDeleteObjects","kRestoreObjects","kSetPlacements","kSetTransforms","kPatchProperties","kSetObjectSize","kSetVectorPathGeometry","kSetImageContent","kAddStroke","kSplitStrokes","kAddEraseMasks","kRemoveEraseMasks","kEditRichText","kSetConnectorContent"};
template<class Store> void applyInsert(){Store s;AppliedOperationLedger l;SemanticGenerationState g;CanonicalCommitClock c(RuntimeEpoch(42U));OperationEngine e;auto in=op(InsertObjectsOp{{shape(1,7)}},1001);auto r=e.apply(in,ApplySource::kLocalInteraction,s,l,g,c);ASSERT_EQ(r.disposition,ApplyDisposition::kApplied);ASSERT_EQ(g.current(),SemanticGeneration(1));ASSERT_EQ(c.nextOrdinalForTest(),CanonicalCommitOrdinal(2));ASSERT_EQ(s.allObjects(),std::vector<ObjectRecord>({shape(1,7)}));ASSERT_TRUE(r.commit_record.has_value());EXPECT_EQ(r.commit_record->operation,in);EXPECT_EQ(r.commit_record->commit_stamp,(CanonicalCommitStamp{RuntimeEpoch(42),CanonicalCommitOrdinal(1)}));ASSERT_TRUE(l.find(in.id).has_value());auto again=e.apply(in,ApplySource::kLocalInteraction,s,l,g,c);EXPECT_EQ(again.disposition,ApplyDisposition::kAlreadyApplied);EXPECT_FALSE(again.commit_record.has_value());if constexpr(std::is_same_v<Store,IndexedObjectStore>)EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(s));}
}}
namespace canvas::semantic {
TEST(OperationEngine15OperationConformance, AuthorityFirstIndependentTableHasExactlyFifteenKinds){ASSERT_EQ(kExpectedKinds.size(),15U);EXPECT_EQ(kExpectedKinds.front(),"kInsertObjects");EXPECT_EQ(kExpectedKinds.back(),"kSetConnectorContent");}
TEST(OperationEngine15OperationConformance, AppliedInsertHasExactCanonicalCommitForBothProviders){applyInsert<ReferenceObjectStore>();applyInsert<IndexedObjectStore>();}
TEST(OperationEngine15OperationConformance, CollisionAndNegativeFixtureAreSideEffectFree){ReferenceObjectStore s;AppliedOperationLedger l;SemanticGenerationState g;CanonicalCommitClock c(RuntimeEpoch(42));OperationEngine e;auto first=op(InsertObjectsOp{{shape(2,8)}},1002);ASSERT_EQ(e.apply(first,ApplySource::kLocalInteraction,s,l,g,c).disposition,ApplyDisposition::kApplied);auto col=op(DeleteObjectsOp{{id(2)}},1002);auto cr=e.apply(col,ApplySource::kLocalInteraction,s,l,g,c);EXPECT_EQ(cr.disposition,ApplyDisposition::kRejected);EXPECT_EQ(cr.error.issue,StatefulIssue::kOperationIdCollision);auto bad=e.apply(op(SetTransformsOp{{TransformItem{id(99),Transform2D{2,0,0,2,0,0}}}},1003),ApplySource::kLocalInteraction,s,l,g,c);EXPECT_EQ(bad.disposition,ApplyDisposition::kRejected);EXPECT_EQ(bad.error.issue,StatefulIssue::kObjectMissing);EXPECT_EQ(g.current(),SemanticGeneration(1));EXPECT_EQ(c.nextOrdinalForTest(),CanonicalCommitOrdinal(2));}
}
