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
ObjectRecord group(std::uint64_t v,std::optional<ObjectId> parent=std::nullopt){ObjectRecord r{};r.id=id(v);r.kind=ObjectKind::kGroup;r.kind_version=1U;r.placement=Placement{parent,OrderKey({static_cast<std::uint8_t>(v)})};r.content=GroupContent{};return r;}
ObjectRecord attachedConnector(std::uint64_t v,ObjectId target){auto r=connector(v);ConnectorContent c{};c.start.value=AttachedEndpoint{target,AutoPerimeterAnchor{}};c.end.value=FreePointEndpoint{{9,9}};c.routing=ConnectorRouting::kStraight;r.content=c;return r;}
Operation op(OperationPayload p,std::uint64_t v){Operation o{};o.id=OperationId{id(v)};o.document_id=DocumentId{id(9000)};o.schema_version=1U;o.payload_version=1U;o.payload=std::move(p);return o;}
constexpr std::array<std::string_view,15> kExpectedKinds{"kInsertObjects","kDeleteObjects","kRestoreObjects","kSetPlacements","kSetTransforms","kPatchProperties","kSetObjectSize","kSetVectorPathGeometry","kSetImageContent","kAddStroke","kSplitStrokes","kAddEraseMasks","kRemoveEraseMasks","kEditRichText","kSetConnectorContent"};
struct ExpectedChange final{ObjectId id{};SemanticChangeFlags flags=SemanticChangeFlags::kNone;std::vector<FieldId> fields;};
void expectChanges(const ChangeSet& actual,const std::vector<ExpectedChange>& expected){ASSERT_EQ(actual.objects().size(),expected.size());for(std::size_t i=0;i<expected.size();++i){EXPECT_EQ(actual.objects()[i].object_id,expected[i].id);EXPECT_EQ(actual.objects()[i].flags,expected[i].flags);EXPECT_EQ(actual.objects()[i].changed_fields,expected[i].fields);}}
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

template<class Store> void deleteClosureOracle(){Store s;AppliedOperationLedger l;SemanticGenerationState g;CanonicalCommitClock c(RuntimeEpoch(42));OperationEngine e;const auto root=group(1),childGroup=group(2,id(1));auto child=shape(3,3);child.placement.parent_id=childGroup.id;const auto parentConnector=attachedConnector(10,childGroup.id),childConnector=attachedConnector(20,child.id),sentinel=shape(99,9);for(const auto& x:{root,childGroup,child,parentConnector,childConnector,sentinel})ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(s,x));const auto incoming=op(DeleteObjectsOp{{root.id}},4001);auto r=e.apply(incoming,ApplySource::kLocalInteraction,s,l,g,c);ASSERT_EQ(r.disposition,ApplyDisposition::kApplied);ASSERT_TRUE(r.commit_record.has_value());EXPECT_EQ(r.commit_record->operation_id,incoming.id);EXPECT_EQ(r.commit_record->before_generation,SemanticGeneration(0));EXPECT_EQ(r.commit_record->after_generation,SemanticGeneration(1));EXPECT_EQ(r.commit_record->commit_stamp,(CanonicalCommitStamp{RuntimeEpoch(42),CommitOrdinal(1)}));expectChanges(r.commit_record->change_set,{{root.id,SemanticChangeFlags::kDeleted,{}},{childGroup.id,SemanticChangeFlags::kDeleted,{}},{child.id,SemanticChangeFlags::kDeleted,{}},{parentConnector.id,SemanticChangeFlags::kDeleted,{}},{childConnector.id,SemanticChangeFlags::kDeleted,{}}});EXPECT_EQ(s.allObjects(),(std::vector<ObjectRecord>{sentinel}));EXPECT_EQ(g.current(),SemanticGeneration(1));EXPECT_EQ(c.lastCommittedOrdinal(),CommitOrdinal(1));ASSERT_TRUE(l.find(incoming.id).has_value());if constexpr(std::is_same_v<Store,IndexedObjectStore>)EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(s));}
TEST(OperationEngine15OperationConformance, DeleteSubtreeCascadesAttachedConnectorsAndPreservesSentinel){deleteClosureOracle<ReferenceObjectStore>();deleteClosureOracle<IndexedObjectStore>();}

template<class Store> void splitOracle(){ Store s; AppliedOperationLedger l; SemanticGenerationState g; CanonicalCommitClock c(RuntimeEpoch(42)); OperationEngine e; const auto src=stroke(200),a=stroke(201),b=stroke(202); ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(s,src));const auto incoming=op(SplitStrokesOp{{StrokeSplit{src.id,{b,a}}}},4100);auto r=e.apply(incoming,ApplySource::kLocalInteraction,s,l,g,c); ASSERT_EQ(r.disposition,ApplyDisposition::kApplied); ASSERT_TRUE(r.commit_record.has_value());EXPECT_EQ(r.commit_record->operation_id,incoming.id);EXPECT_EQ(r.commit_record->before_generation,SemanticGeneration(0));EXPECT_EQ(r.commit_record->after_generation,SemanticGeneration(1));EXPECT_EQ(r.commit_record->commit_stamp,(CanonicalCommitStamp{RuntimeEpoch(42),CommitOrdinal(1)}));expectChanges(r.commit_record->change_set,{{src.id,SemanticChangeFlags::kDeleted,{}},{a.id,SemanticChangeFlags::kCreated,{}},{b.id,SemanticChangeFlags::kCreated,{}}});EXPECT_EQ(s.allObjects(),(std::vector<ObjectRecord>{a,b}));EXPECT_EQ(g.current(),SemanticGeneration(1));EXPECT_EQ(c.lastCommittedOrdinal(),CommitOrdinal(1));ASSERT_TRUE(l.find(incoming.id).has_value());if constexpr(std::is_same_v<Store,IndexedObjectStore>)EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(s)); }
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
    std::vector<std::vector<ObjectRecord>> states; std::vector<CanonicalCommitRecord> records; std::vector<AppliedOperationEntry> ledger_entries;
    for (auto source:sources){ Store s; AppliedOperationLedger l; SemanticGenerationState g; CanonicalCommitClock c(RuntimeEpoch(77)); OperationEngine e; auto result=e.apply(incoming,source,s,l,g,c); ASSERT_EQ(result.disposition,ApplyDisposition::kApplied); ASSERT_TRUE(result.commit_record.has_value()); const auto ledger_entry=l.find(incoming.id); ASSERT_TRUE(ledger_entry.has_value()); states.push_back(s.allObjects()); records.push_back(*result.commit_record); ledger_entries.push_back(*ledger_entry); }
    EXPECT_EQ(states[0],states[1]); EXPECT_EQ(states[1],states[2]);
    for (std::size_t i=0;i<records.size();++i){ EXPECT_EQ(ledger_entries[i].canonical_operation.id,incoming.id); EXPECT_EQ(ledger_entries[i].canonical_operation.document_id,incoming.document_id); EXPECT_EQ(ledger_entries[i].canonical_operation.schema_version,incoming.schema_version); EXPECT_EQ(ledger_entries[i].canonical_operation.payload_version,incoming.payload_version); EXPECT_EQ(ledger_entries[i].canonical_operation.payload,incoming.payload); EXPECT_EQ(records[i].operation_id,incoming.id); EXPECT_EQ(records[i].before_generation,SemanticGeneration(0)); EXPECT_EQ(records[i].after_generation,SemanticGeneration(1)); EXPECT_EQ(records[i].commit_stamp,(CanonicalCommitStamp{RuntimeEpoch(77),CommitOrdinal(1)})); EXPECT_EQ(records[i].change_set.beforeGeneration(),records[0].change_set.beforeGeneration()); EXPECT_EQ(records[i].change_set.afterGeneration(),records[0].change_set.afterGeneration()); ASSERT_EQ(records[i].change_set.objects().size(),1U); EXPECT_EQ(records[i].change_set.objects()[0].object_id,id(400)); EXPECT_EQ(records[i].change_set.objects()[0].flags,SemanticChangeFlags::kCreated); EXPECT_TRUE(records[i].change_set.objects()[0].changed_fields.empty()); if(i>0){EXPECT_EQ(ledger_entries[i].canonical_operation.id,ledger_entries[0].canonical_operation.id);EXPECT_EQ(ledger_entries[i].canonical_operation.document_id,ledger_entries[0].canonical_operation.document_id);EXPECT_EQ(ledger_entries[i].canonical_operation.schema_version,ledger_entries[0].canonical_operation.schema_version);EXPECT_EQ(ledger_entries[i].canonical_operation.payload_version,ledger_entries[0].canonical_operation.payload_version);EXPECT_EQ(ledger_entries[i].canonical_operation.payload,ledger_entries[0].canonical_operation.payload);EXPECT_EQ(records[i].operation_id,records[0].operation_id);EXPECT_EQ(records[i].before_generation,records[0].before_generation);EXPECT_EQ(records[i].after_generation,records[0].after_generation);EXPECT_EQ(records[i].commit_stamp,records[0].commit_stamp);EXPECT_EQ(records[i].change_set.objects()[0].object_id,records[0].change_set.objects()[0].object_id);EXPECT_EQ(records[i].change_set.objects()[0].flags,records[0].change_set.objects()[0].flags);EXPECT_EQ(records[i].change_set.objects()[0].changed_fields,records[0].change_set.objects()[0].changed_fields);} }
    EXPECT_EQ(records[0].source,ApplySource::kLocalInteraction); EXPECT_EQ(records[1].source,ApplySource::kRestoreReplay); EXPECT_EQ(records[2].source,ApplySource::kRemoteSync); EXPECT_EQ(localBridgePublicationDisposition(records[0]),LocalBridgePublicationDisposition::kEligible); EXPECT_EQ(localBridgePublicationDisposition(records[1]),LocalBridgePublicationDisposition::kNoEcho); EXPECT_EQ(localBridgePublicationDisposition(records[2]),LocalBridgePublicationDisposition::kNoEcho);
}
TEST(OperationEngine15OperationConformance, ApplySourcesShareCanonicalResultsAndDifferOnlyInPublication){sourceParity<ReferenceObjectStore>();sourceParity<IndexedObjectStore>();}

template<class Store> void patchOracle(){Store s;AppliedOperationLedger l;SemanticGenerationState g;CanonicalCommitClock c(RuntimeEpoch(42));OperationEngine e;const auto original=shape(5,5);ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(s,original));auto expected=original;expected.properties.entries={{1U,PropertyValue{false}},{2U,PropertyValue{true}}};const auto incoming=op(PatchPropertiesOp{{{original.id,2U,PropertyPatchAction::kSet,PropertyValue{true}},{original.id,1U,PropertyPatchAction::kSet,PropertyValue{false}}}},4003);auto result=e.apply(incoming,ApplySource::kLocalInteraction,s,l,g,c);ASSERT_EQ(result.disposition,ApplyDisposition::kApplied);ASSERT_TRUE(result.commit_record.has_value());EXPECT_EQ(result.commit_record->operation_id,incoming.id);EXPECT_EQ(result.commit_record->before_generation,SemanticGeneration(0));EXPECT_EQ(result.commit_record->after_generation,SemanticGeneration(1));EXPECT_EQ(result.commit_record->commit_stamp,(CanonicalCommitStamp{RuntimeEpoch(42),CommitOrdinal(1)}));expectChanges(result.commit_record->change_set,{{original.id,SemanticChangeFlags::kProperties,{1U,2U}}});EXPECT_EQ(s.allObjects(),(std::vector<ObjectRecord>{expected}));EXPECT_EQ(g.current(),SemanticGeneration(1));EXPECT_EQ(c.lastCommittedOrdinal(),CommitOrdinal(1));ASSERT_TRUE(l.find(incoming.id).has_value());if constexpr(std::is_same_v<Store,IndexedObjectStore>)EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(s));}
TEST(OperationEngine15OperationConformance, PatchPropertiesProducesLiteralSortedFieldChange){patchOracle<ReferenceObjectStore>();patchOracle<IndexedObjectStore>();}

template<class Store> void applyAllFamiliesPositive() {
  const ObjectRecord sh=shape(100,1), vp=vectorPath(101), im=image(102), st=stroke(103), rt=rich(104), co=connector(105);
  ObjectRecord placed=sh;placed.placement=Placement{std::nullopt,OrderKey({9})};ObjectRecord transformed=sh;transformed.transform=Transform2D{1,0,0,1,8,9};ObjectRecord patched=sh;patched.properties.entries={{1U,PropertyValue{false}},{2U,PropertyValue{true}}};ObjectRecord resized=sh;resized.content=ShapeContent{1,20,30};ObjectRecord geometry=vp;geometry.content=VectorPathContent{VectorPathGeometry{FillRule::kEvenOdd,{MoveTo{{2,3}},LineTo{{4,5}}}}};ObjectRecord imageChanged=im;imageChanged.content=ImageContent{ResourceId{id(801)},44,55,std::nullopt,ImageContentMode::kFill,66,77};ObjectRecord maskAdded=st;const EraseMaskRecord mask{id(701),SweptCircleMask{}};maskAdded.erase_masks={mask};ObjectRecord connectorChanged=co;connectorChanged.content=ConnectorContent{ConnectorEndpoint{FreePointEndpoint{{5,6}}},ConnectorEndpoint{FreePointEndpoint{{7,8}}},ConnectorRouting::kOrthogonal};
  struct Row { Operation op; ObjectId target; SemanticChangeFlags flag; std::vector<FieldId> fields; std::vector<ObjectRecord> expected; };
  const std::vector<Row> rows{
    {op(InsertObjectsOp{{sh}},5001),sh.id,SemanticChangeFlags::kCreated,{}, {sh}},
    {op(DeleteObjectsOp{{sh.id}},5002),sh.id,SemanticChangeFlags::kDeleted,{}, {}},
    {op(RestoreObjectsOp{{sh}},5003),sh.id,SemanticChangeFlags::kCreated,{}, {sh}},
    {op(SetPlacementsOp{{PlacementItem{sh.id,placed.placement}}},5004),sh.id,SemanticChangeFlags::kPlacement,{}, {placed}},
    {op(SetTransformsOp{{TransformItem{sh.id,transformed.transform}}},5005),sh.id,SemanticChangeFlags::kTransform,{}, {transformed}},
    {op(PatchPropertiesOp{{PropertyPatch{sh.id,2,PropertyPatchAction::kSet,PropertyValue{true}},PropertyPatch{sh.id,1,PropertyPatchAction::kSet,PropertyValue{false}}}},5006),sh.id,SemanticChangeFlags::kProperties,{1U,2U}, {patched}},
    {op(SetObjectSizeOp{{ObjectSizeItem{sh.id,20,30}}},5007),sh.id,SemanticChangeFlags::kContent,{}, {resized}},
    {op(SetVectorPathGeometryOp{vp.id,std::get<VectorPathContent>(geometry.content).geometry},5008),vp.id,SemanticChangeFlags::kContent,{}, {geometry}},
    {op(SetImageContentOp{im.id,std::get<ImageContent>(imageChanged.content)},5009),im.id,SemanticChangeFlags::kContent,{}, {imageChanged}},
    {op(AddStrokeOp{st},5010),st.id,SemanticChangeFlags::kCreated,{}, {st}},
    {op(SplitStrokesOp{{StrokeSplit{st.id,{stroke(106)}}}},5011),st.id,SemanticChangeFlags::kDeleted,{}, {stroke(106)}},
    {op(AddEraseMasksOp{{EraseMaskAddItem{st.id,{mask}}}},5012),st.id,SemanticChangeFlags::kEraseMasks,{}, {maskAdded}},
    {op(RemoveEraseMasksOp{{EraseMaskRemoveItem{st.id,{mask.id}}}},5013),st.id,SemanticChangeFlags::kEraseMasks,{}, {st}},
    {op(EditRichTextOp{rt.id,RichTextDelta{}},5014),rt.id,SemanticChangeFlags::kContent,{}, {rt}},
    {op(SetConnectorContentOp{co.id,std::get<ConnectorContent>(connectorChanged.content)},5015),co.id,SemanticChangeFlags::kContent,{}, {connectorChanged}}};
  ASSERT_EQ(rows.size(),kExpectedKinds.size());
  for (std::size_t i=0;i<rows.size();++i) {
    Store s; AppliedOperationLedger l; SemanticGenerationState g; CanonicalCommitClock c(RuntimeEpoch(9)); OperationEngine e;
    if(i==12)ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(s,maskAdded));else for (const auto& r : {sh,vp,im,st,rt,co}) if (r.id==rows[i].target || i==0) { if (i>0 && i!=2 && i!=9) ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(s,r)); }
    auto result=e.apply(rows[i].op,ApplySource::kLocalInteraction,s,l,g,c);
    ASSERT_EQ(result.disposition,ApplyDisposition::kApplied)<<kExpectedKinds[i]; ASSERT_TRUE(result.commit_record.has_value()); EXPECT_EQ(result.commit_record->operation_id,rows[i].op.id); EXPECT_EQ(result.commit_record->source,ApplySource::kLocalInteraction); EXPECT_EQ(result.commit_record->before_generation,SemanticGeneration(0)); EXPECT_EQ(result.commit_record->after_generation,SemanticGeneration(1)); EXPECT_EQ(result.commit_record->commit_stamp,(CanonicalCommitStamp{RuntimeEpoch(9),CommitOrdinal(1)}));expectChanges(result.commit_record->change_set,i==10?std::vector<ExpectedChange>{{st.id,SemanticChangeFlags::kDeleted,{}},{id(106),SemanticChangeFlags::kCreated,{}}}:std::vector<ExpectedChange>{{rows[i].target,rows[i].flag,rows[i].fields}});EXPECT_EQ(s.allObjects(),rows[i].expected)<<kExpectedKinds[i];EXPECT_EQ(g.current(),SemanticGeneration(1));EXPECT_EQ(c.lastCommittedOrdinal(),CommitOrdinal(1)); ASSERT_TRUE(l.find(rows[i].op.id).has_value());EXPECT_EQ(l.find(rows[i].op.id)->canonical_operation.id,rows[i].op.id);EXPECT_EQ(l.find(rows[i].op.id)->canonical_operation.payload,rows[i].op.payload); if constexpr(std::is_same_v<Store,IndexedObjectStore>) EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(s));
  }
}
TEST(OperationEngine15OperationConformance, AllFifteenFamiliesApplyOnBothProviders){applyAllFamiliesPositive<ReferenceObjectStore>();applyAllFamiliesPositive<IndexedObjectStore>();}
}
