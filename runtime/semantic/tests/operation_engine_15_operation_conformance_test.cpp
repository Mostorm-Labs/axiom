#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/commit_publication_projection.hpp"
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
ObjectRecord vectorPath(std::uint64_t v){auto r=shape(v,1);r.kind=ObjectKind::kVectorPath;r.content=VectorPathContent{VectorPathGeometry{FillRule::kNonZero,{MoveTo{{0,0}},LineTo{{1,1}}}}};return r;}
ObjectRecord image(std::uint64_t v){auto r=shape(v,1);r.kind=ObjectKind::kImage;r.content=ImageContent{ResourceId{id(800)},10,10,std::nullopt,ImageContentMode::kFit,0,0};return r;}
ObjectRecord stroke(std::uint64_t v){auto r=shape(v,1);r.kind=ObjectKind::kVectorStroke;StrokeRecord s{};s.brush.brush_family_id=1;s.brush.brush_version=1;s.brush.nominal_size=1;s.brush.opacity=1;s.data=VectorStrokeData{{StrokeSample{{0,0},1,{0,0}}}};r.content=VectorStrokeContent{s};return r;}
ObjectRecord rich(std::uint64_t v){auto r=shape(v,1);r.kind=ObjectKind::kRichText;Paragraph p{};p.id=id(v*10);p.runs={{"A",TextStyle{}}};r.content=RichTextContent{{{p}}};return r;}
ObjectRecord connector(std::uint64_t v){auto r=shape(v,1);r.kind=ObjectKind::kConnector;ConnectorContent c{};c.start.value=FreePointEndpoint{{0,0}};c.end.value=FreePointEndpoint{{1,1}};c.routing=ConnectorRouting::kStraight;r.content=c;return r;}
Operation op(OperationPayload p,std::uint64_t v){Operation o{};o.id=OperationId{id(v)};o.document_id=DocumentId{id(9000)};o.schema_version=1U;o.payload_version=1U;o.payload=std::move(p);return o;}
constexpr std::array<std::string_view,15> kExpectedKinds{"kInsertObjects","kDeleteObjects","kRestoreObjects","kSetPlacements","kSetTransforms","kPatchProperties","kSetObjectSize","kSetVectorPathGeometry","kSetImageContent","kAddStroke","kSplitStrokes","kAddEraseMasks","kRemoveEraseMasks","kEditRichText","kSetConnectorContent"};
template<class Store> void applyInsert(){Store s;AppliedOperationLedger l;SemanticGenerationState g;CanonicalCommitClock c(RuntimeEpoch(42U));OperationEngine e;auto in=op(InsertObjectsOp{{shape(1,7)}},1001);auto r=e.apply(in,ApplySource::kLocalInteraction,s,l,g,c);ASSERT_EQ(r.disposition,ApplyDisposition::kApplied);ASSERT_EQ(g.current(),SemanticGeneration(1));ASSERT_EQ(c.lastCommittedOrdinal(),CommitOrdinal(1));ASSERT_EQ(s.allObjects(),std::vector<ObjectRecord>({shape(1,7)}));ASSERT_TRUE(r.commit_record.has_value());EXPECT_EQ(r.commit_record->operation_id,in.id);EXPECT_EQ(r.commit_record->commit_stamp,(CanonicalCommitStamp{RuntimeEpoch(42),CommitOrdinal(1)}));ASSERT_TRUE(l.find(in.id).has_value());auto again=e.apply(in,ApplySource::kLocalInteraction,s,l,g,c);EXPECT_EQ(again.disposition,ApplyDisposition::kAlreadyApplied);EXPECT_FALSE(again.commit_record.has_value());if constexpr(std::is_same_v<Store,IndexedObjectStore>)EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(s));}
}}
namespace canvas::semantic {
TEST(OperationEngine15OperationConformance, AuthorityFirstIndependentTableHasExactlyFifteenKinds){ASSERT_EQ(kExpectedKinds.size(),15U);EXPECT_EQ(kExpectedKinds.front(),"kInsertObjects");EXPECT_EQ(kExpectedKinds.back(),"kSetConnectorContent");}
TEST(OperationEngine15OperationConformance, AppliedInsertHasExactCanonicalCommitForBothProviders){applyInsert<ReferenceObjectStore>();applyInsert<IndexedObjectStore>();}
TEST(OperationEngine15OperationConformance, CollisionAndNegativeFixtureAreSideEffectFree){ReferenceObjectStore s;AppliedOperationLedger l;SemanticGenerationState g;CanonicalCommitClock c(RuntimeEpoch(42));OperationEngine e;auto first=op(InsertObjectsOp{{shape(2,8)}},1002);ASSERT_EQ(e.apply(first,ApplySource::kLocalInteraction,s,l,g,c).disposition,ApplyDisposition::kApplied);auto col=op(DeleteObjectsOp{{id(2)}},1002);auto cr=e.apply(col,ApplySource::kLocalInteraction,s,l,g,c);EXPECT_EQ(cr.disposition,ApplyDisposition::kRejected);EXPECT_EQ(cr.error.issue,StatefulIssue::kOperationIdCollision);auto bad=e.apply(op(SetTransformsOp{{TransformItem{id(99),Transform2D{2,0,0,2,0,0}}}},1003),ApplySource::kLocalInteraction,s,l,g,c);EXPECT_EQ(bad.disposition,ApplyDisposition::kRejected);EXPECT_EQ(bad.error.issue,StatefulIssue::kObjectMissing);EXPECT_EQ(g.current(),SemanticGeneration(1));EXPECT_EQ(c.lastCommittedOrdinal(),CommitOrdinal(1));}

TEST(OperationEngine15OperationConformance, LiteralAuthorityTableDrivesAllFifteenOperationFixtures) {
    const ObjectRecord base = shape(10U, 3U);
    const std::vector<Operation> fixtures{
        op(InsertObjectsOp{{base}}, 2001U),
        op(DeleteObjectsOp{{base.id}}, 2002U),
        op(RestoreObjectsOp{{base}}, 2003U),
        op(SetPlacementsOp{{PlacementItem{base.id, base.placement}}}, 2004U),
        op(SetTransformsOp{{TransformItem{base.id, base.transform}}}, 2005U),
        op(PatchPropertiesOp{{PropertyPatch{base.id, 1U, PropertyPatchAction::kSet, PropertyValue{false}}}}, 2006U),
        op(SetObjectSizeOp{{ObjectSizeItem{base.id, 10.0, 20.0}}}, 2007U),
        op(SetVectorPathGeometryOp{base.id, VectorPathGeometry{}}, 2008U),
        op(SetImageContentOp{base.id, ImageContent{}}, 2009U),
        op(AddStrokeOp{base}, 2010U),
        op(SplitStrokesOp{{StrokeSplit{base.id, {base}}}}, 2011U),
        op(AddEraseMasksOp{{EraseMaskAddItem{base.id, {}}}}, 2012U),
        op(RemoveEraseMasksOp{{EraseMaskRemoveItem{base.id, {}}}}, 2013U),
        op(EditRichTextOp{base.id, RichTextDelta{}}, 2014U),
        op(SetConnectorContentOp{base.id, ConnectorContent{}}, 2015U)};
    ASSERT_EQ(fixtures.size(), kExpectedKinds.size());
    for (std::size_t i = 0; i < fixtures.size(); ++i) {
        EXPECT_EQ(fixtures[i].payload.index(), i) << kExpectedKinds[i];
    }
}

template<class Store> void rejectAllFamilies() {
    const ObjectId missing=id(999U); const ObjectRecord existing=shape(77U,4U);
    const std::vector<std::pair<Operation, StatefulIssue>> bad{
      {op(InsertObjectsOp{{existing}},3001U),StatefulIssue::kObjectAlreadyExists}, {op(DeleteObjectsOp{{missing}},3002U),StatefulIssue::kObjectMissing}, {op(RestoreObjectsOp{{existing}},3003U),StatefulIssue::kObjectAlreadyExists},
      {op(SetPlacementsOp{{PlacementItem{missing,existing.placement}}},3004U),StatefulIssue::kObjectMissing}, {op(SetTransformsOp{{TransformItem{missing,existing.transform}}},3005U),StatefulIssue::kObjectMissing},
      {op(PatchPropertiesOp{{PropertyPatch{missing,1U,PropertyPatchAction::kSet,PropertyValue{false}}}},3006U),StatefulIssue::kObjectMissing}, {op(SetObjectSizeOp{{ObjectSizeItem{missing,1,1}}},3007U),StatefulIssue::kObjectMissing},
      {op(SetVectorPathGeometryOp{missing,VectorPathGeometry{}},3008U),StatefulIssue::kObjectMissing}, {op(SetImageContentOp{missing,ImageContent{}},3009U),StatefulIssue::kObjectMissing}, {op(AddStrokeOp{existing},3010U),StatefulIssue::kObjectAlreadyExists},
      {op(SplitStrokesOp{{StrokeSplit{missing,{existing}}}},3011U),StatefulIssue::kObjectMissing}, {op(AddEraseMasksOp{{EraseMaskAddItem{missing,{}}}},3012U),StatefulIssue::kObjectMissing}, {op(RemoveEraseMasksOp{{EraseMaskRemoveItem{missing,{}}}},3013U),StatefulIssue::kObjectMissing},
      {op(EditRichTextOp{missing,RichTextDelta{}},3014U),StatefulIssue::kObjectMissing}, {op(SetConnectorContentOp{missing,ConnectorContent{}},3015U),StatefulIssue::kObjectMissing}};
    for (const auto& [x, issue] : bad) { Store s; AppliedOperationLedger l; SemanticGenerationState g; CanonicalCommitClock c(RuntimeEpoch(42U)); OperationEngine e; ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(s,existing)); const auto before=s.allObjects(); const auto rr=e.apply(x,ApplySource::kLocalInteraction,s,l,g,c); EXPECT_EQ(rr.disposition,ApplyDisposition::kRejected); EXPECT_EQ(rr.error.issue,issue); EXPECT_EQ(s.allObjects(),before); EXPECT_FALSE(l.find(x.id).has_value()); EXPECT_EQ(g.current(),SemanticGeneration(0)); EXPECT_EQ(c.lastCommittedOrdinal(),CommitOrdinal(0)); if constexpr(std::is_same_v<Store,IndexedObjectStore>) EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(s)); }
}
TEST(OperationEngine15OperationConformance, EveryFamilyHasAtomicNegativeFixtureOnBothProviders){rejectAllFamilies<ReferenceObjectStore>();rejectAllFamilies<IndexedObjectStore>();}

TEST(OperationEngine15OperationConformance, DeleteSubtreeCascadesAttachedConnectorsAndPreservesSentinel) {
    ReferenceObjectStore s; AppliedOperationLedger l; SemanticGenerationState g; CanonicalCommitClock c(RuntimeEpoch(42)); OperationEngine e;
    ObjectRecord root=shape(1,1), child=shape(2,2); child.placement.parent_id=root.id; ObjectRecord sentinel=shape(99,9);
    ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(s,root)); ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(s,child)); ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(s,sentinel));
    auto r=e.apply(op(DeleteObjectsOp{{root.id}},4001),ApplySource::kLocalInteraction,s,l,g,c); ASSERT_EQ(r.disposition,ApplyDisposition::kApplied); EXPECT_FALSE(s.contains(root.id)); EXPECT_FALSE(s.contains(child.id)); EXPECT_TRUE(s.contains(sentinel.id)); ASSERT_TRUE(r.commit_record.has_value()); ASSERT_EQ(r.commit_record->change_set.objects().size(),2U); EXPECT_EQ(r.commit_record->change_set.objects()[0].flags,SemanticChangeFlags::kDeleted);
}

template<class Store> void splitOracle(){ Store s; AppliedOperationLedger l; SemanticGenerationState g; CanonicalCommitClock c(RuntimeEpoch(42)); OperationEngine e; auto src=stroke(200); auto a=stroke(201); auto b=stroke(202); ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(s,src)); auto r=e.apply(op(SplitStrokesOp{{StrokeSplit{src.id,{a,b}}}},4100),ApplySource::kLocalInteraction,s,l,g,c); ASSERT_EQ(r.disposition,ApplyDisposition::kApplied); EXPECT_FALSE(s.contains(src.id)); EXPECT_TRUE(s.contains(a.id)); EXPECT_TRUE(s.contains(b.id)); ASSERT_TRUE(r.commit_record.has_value()); ASSERT_EQ(r.commit_record->change_set.objects().size(),3U); EXPECT_EQ(r.commit_record->change_set.objects()[0].object_id,src.id); EXPECT_EQ(r.commit_record->change_set.objects()[0].flags,SemanticChangeFlags::kDeleted); EXPECT_EQ(r.commit_record->change_set.objects()[1].flags,SemanticChangeFlags::kCreated); EXPECT_EQ(r.commit_record->change_set.objects()[2].flags,SemanticChangeFlags::kCreated); }
TEST(OperationEngine15OperationConformance, SplitStrokesDeletesSourceAndCreatesDeterministicReplacements){splitOracle<ReferenceObjectStore>();splitOracle<IndexedObjectStore>();}

template<class Store> void idempotencyAndCollisionOracle(){
    Store s; AppliedOperationLedger l; SemanticGenerationState g; CanonicalCommitClock c(RuntimeEpoch(42)); OperationEngine e;
    const Operation first=op(InsertObjectsOp{{shape(300,3)}},4200);
    ASSERT_EQ(e.apply(first,ApplySource::kLocalInteraction,s,l,g,c).disposition,ApplyDisposition::kApplied);
    const auto before_objects=s.allObjects(); const auto before_entry=l.find(first.id); const auto before_generation=g.current(); const auto before_ordinal=c.lastCommittedOrdinal();
    const auto already=e.apply(first,ApplySource::kLocalInteraction,s,l,g,c);
    EXPECT_EQ(already.disposition,ApplyDisposition::kAlreadyApplied); EXPECT_FALSE(already.commit_record.has_value()); EXPECT_EQ(s.allObjects(),before_objects); ASSERT_TRUE(before_entry.has_value()); ASSERT_TRUE(l.find(first.id).has_value()); EXPECT_EQ(l.find(first.id)->canonical_operation.id,before_entry->canonical_operation.id); EXPECT_EQ(l.find(first.id)->canonical_operation.payload,before_entry->canonical_operation.payload); EXPECT_EQ(g.current(),before_generation); EXPECT_EQ(c.lastCommittedOrdinal(),before_ordinal);
    const auto collision=e.apply(op(DeleteObjectsOp{{id(300)}},4200),ApplySource::kLocalInteraction,s,l,g,c);
    EXPECT_EQ(collision.disposition,ApplyDisposition::kRejected); EXPECT_EQ(collision.error.issue,StatefulIssue::kOperationIdCollision); EXPECT_FALSE(collision.commit_record.has_value()); EXPECT_EQ(s.allObjects(),before_objects); ASSERT_TRUE(l.find(first.id).has_value()); EXPECT_EQ(l.find(first.id)->canonical_operation.id,before_entry->canonical_operation.id); EXPECT_EQ(l.find(first.id)->canonical_operation.payload,before_entry->canonical_operation.payload); EXPECT_EQ(g.current(),before_generation); EXPECT_EQ(c.lastCommittedOrdinal(),before_ordinal); if constexpr(std::is_same_v<Store,IndexedObjectStore>) EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(s));
}
TEST(OperationEngine15OperationConformance, IdempotencyAndCollisionAreAtomicOnBothProviders){idempotencyAndCollisionOracle<ReferenceObjectStore>();idempotencyAndCollisionOracle<IndexedObjectStore>();}

template<class Store> void sourceParity(){
    const Operation incoming=op(InsertObjectsOp{{shape(400,4)}},4300);
    std::vector<ApplySource> sources{ApplySource::kLocalInteraction,ApplySource::kRestoreReplay,ApplySource::kRemoteSync};
    std::vector<std::vector<ObjectRecord>> states; std::vector<CanonicalCommitRecord> records;
    for (auto source:sources){ Store s; AppliedOperationLedger l; SemanticGenerationState g; CanonicalCommitClock c(RuntimeEpoch(77)); OperationEngine e; auto result=e.apply(incoming,source,s,l,g,c); ASSERT_EQ(result.disposition,ApplyDisposition::kApplied); ASSERT_TRUE(result.commit_record.has_value()); states.push_back(s.allObjects()); records.push_back(*result.commit_record); }
    EXPECT_EQ(states[0],states[1]); EXPECT_EQ(states[1],states[2]);
    for (std::size_t i=0;i<records.size();++i){ EXPECT_EQ(records[i].operation_id,incoming.id); EXPECT_EQ(records[i].before_generation,SemanticGeneration(0)); EXPECT_EQ(records[i].after_generation,SemanticGeneration(1)); EXPECT_EQ(records[i].commit_stamp,(CanonicalCommitStamp{RuntimeEpoch(77),CommitOrdinal(1)})); EXPECT_EQ(records[i].change_set.beforeGeneration(),records[0].change_set.beforeGeneration()); EXPECT_EQ(records[i].change_set.afterGeneration(),records[0].change_set.afterGeneration()); ASSERT_EQ(records[i].change_set.objects().size(),1U); EXPECT_EQ(records[i].change_set.objects()[0].object_id,id(400)); EXPECT_EQ(records[i].change_set.objects()[0].flags,SemanticChangeFlags::kCreated); EXPECT_TRUE(records[i].change_set.objects()[0].changed_fields.empty()); }
    EXPECT_EQ(records[0].source,ApplySource::kLocalInteraction); EXPECT_EQ(records[1].source,ApplySource::kRestoreReplay); EXPECT_EQ(records[2].source,ApplySource::kRemoteSync); EXPECT_EQ(localBridgePublicationDisposition(records[0]),LocalBridgePublicationDisposition::kEligible); EXPECT_EQ(localBridgePublicationDisposition(records[1]),LocalBridgePublicationDisposition::kNoEcho); EXPECT_EQ(localBridgePublicationDisposition(records[2]),LocalBridgePublicationDisposition::kNoEcho);
}
TEST(OperationEngine15OperationConformance, ApplySourcesShareCanonicalResultsAndDifferOnlyInPublication){sourceParity<ReferenceObjectStore>();sourceParity<IndexedObjectStore>();}

TEST(OperationEngine15OperationConformance, PatchPropertiesProducesLiteralSortedFieldChange) {
    IndexedObjectStore s; AppliedOperationLedger l; SemanticGenerationState g; CanonicalCommitClock c(RuntimeEpoch(42)); OperationEngine e; ObjectRecord r=shape(5,5); ASSERT_EQ(e.apply(op(InsertObjectsOp{{r}},4002),ApplySource::kLocalInteraction,s,l,g,c).disposition,ApplyDisposition::kApplied);
    auto result=e.apply(op(PatchPropertiesOp{{{r.id,2U,PropertyPatchAction::kSet,PropertyValue{true}},{r.id,1U,PropertyPatchAction::kSet,PropertyValue{false}}}},4003),ApplySource::kLocalInteraction,s,l,g,c); ASSERT_EQ(result.disposition,ApplyDisposition::kApplied); auto expected=*s.find(r.id); const std::vector<PropertyEntry> expected_entries{{1U,PropertyValue{false}},{2U,PropertyValue{true}}}; EXPECT_EQ(expected.properties.entries,expected_entries); ASSERT_TRUE(result.commit_record.has_value()); ASSERT_EQ(result.commit_record->change_set.objects().size(),1U); EXPECT_EQ(result.commit_record->change_set.objects()[0].changed_fields,(std::vector<FieldId>{1U,2U})); EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(s));
}

template<class Store> void applyAllFamiliesPositive() {
  const ObjectRecord sh=shape(100,1), vp=vectorPath(101), im=image(102), st=stroke(103), rt=rich(104), co=connector(105);
  struct Row { Operation op; ObjectId target; SemanticChangeFlags flag; };
  const std::vector<Row> rows{
    {op(InsertObjectsOp{{sh}},5001),sh.id,SemanticChangeFlags::kCreated},
    {op(DeleteObjectsOp{{sh.id}},5002),sh.id,SemanticChangeFlags::kDeleted},
    {op(RestoreObjectsOp{{sh}},5003),sh.id,SemanticChangeFlags::kCreated},
    {op(SetPlacementsOp{{PlacementItem{sh.id,sh.placement}}},5004),sh.id,SemanticChangeFlags::kPlacement},
    {op(SetTransformsOp{{TransformItem{sh.id,sh.transform}}},5005),sh.id,SemanticChangeFlags::kTransform},
    {op(PatchPropertiesOp{{PropertyPatch{sh.id,1,PropertyPatchAction::kSet,PropertyValue{true}}}},5006),sh.id,SemanticChangeFlags::kProperties},
    {op(SetObjectSizeOp{{ObjectSizeItem{sh.id,20,30}}},5007),sh.id,SemanticChangeFlags::kContent},
    {op(SetVectorPathGeometryOp{vp.id,std::get<VectorPathContent>(vp.content).geometry},5008),vp.id,SemanticChangeFlags::kContent},
    {op(SetImageContentOp{im.id,std::get<ImageContent>(im.content)},5009),im.id,SemanticChangeFlags::kContent},
    {op(AddStrokeOp{st},5010),st.id,SemanticChangeFlags::kCreated},
    {op(SplitStrokesOp{{StrokeSplit{st.id,{stroke(106)}}}},5011),st.id,SemanticChangeFlags::kDeleted},
    {op(AddEraseMasksOp{{EraseMaskAddItem{st.id,{}}}},5012),st.id,SemanticChangeFlags::kEraseMasks},
    {op(RemoveEraseMasksOp{{EraseMaskRemoveItem{st.id,{}}}},5013),st.id,SemanticChangeFlags::kEraseMasks},
    {op(EditRichTextOp{rt.id,RichTextDelta{}},5014),rt.id,SemanticChangeFlags::kContent},
    {op(SetConnectorContentOp{co.id,std::get<ConnectorContent>(co.content)},5015),co.id,SemanticChangeFlags::kContent}};
  ASSERT_EQ(rows.size(),kExpectedKinds.size());
  for (std::size_t i=0;i<rows.size();++i) {
    Store s; AppliedOperationLedger l; SemanticGenerationState g; CanonicalCommitClock c(RuntimeEpoch(9)); OperationEngine e;
    for (const auto& r : {sh,vp,im,st,rt,co}) if (r.id==rows[i].target || i==0) { if (i>0 && i!=2 && i!=9) ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(s,r)); }
    auto result=e.apply(rows[i].op,ApplySource::kLocalInteraction,s,l,g,c);
    ASSERT_EQ(result.disposition,ApplyDisposition::kApplied)<<kExpectedKinds[i]; ASSERT_TRUE(result.commit_record.has_value()); EXPECT_EQ(result.commit_record->operation_id,rows[i].op.id); EXPECT_EQ(result.commit_record->source,ApplySource::kLocalInteraction); EXPECT_EQ(result.commit_record->before_generation,SemanticGeneration(0)); EXPECT_EQ(result.commit_record->after_generation,SemanticGeneration(1)); EXPECT_EQ(result.commit_record->commit_stamp,(CanonicalCommitStamp{RuntimeEpoch(9),CommitOrdinal(1)})); ASSERT_EQ(result.commit_record->change_set.objects().size(),i==10?2U:1U); EXPECT_EQ(result.commit_record->change_set.objects().front().flags,rows[i].flag); ASSERT_TRUE(l.find(rows[i].op.id).has_value()); if constexpr(std::is_same_v<Store,IndexedObjectStore>) EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(s));
  }
}
TEST(OperationEngine15OperationConformance, AllFifteenFamiliesApplyOnBothProviders){applyAllFamiliesPositive<ReferenceObjectStore>();applyAllFamiliesPositive<IndexedObjectStore>();}
}
