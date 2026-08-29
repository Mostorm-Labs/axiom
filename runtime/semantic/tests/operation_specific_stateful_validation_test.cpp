#include "canvas/semantic/operation_specific_validation.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/validator.hpp"
#include "object_store_mutator.hpp"
#include <gtest/gtest.h>
#include <cstdint>
#include <map>
#include <optional>
#include <type_traits>
#include <vector>
namespace canvas::semantic { namespace {
using I=StatefulResult(*)(const InsertObjectsOp&,const ObjectStore&,CreateObjectsStatePlanInputs*); using P=StatefulResult(*)(const SetPlacementsOp&,const ObjectStore&,ReplaceObjectsStatePlanInputs*); using T=StatefulResult(*)(const SetTransformsOp&,const ObjectStore&,ReplaceObjectsStatePlanInputs*); using PP=StatefulResult(*)(const PatchPropertiesOp&,const ObjectStore&,ReplaceObjectsStatePlanInputs*); using S=StatefulResult(*)(const SetObjectSizeOp&,const ObjectStore&,ReplaceObjectsStatePlanInputs*); using V=StatefulResult(*)(const SetVectorPathGeometryOp&,const ObjectStore&,ReplaceObjectsStatePlanInputs*); using IM=StatefulResult(*)(const SetImageContentOp&,const ObjectStore&,ReplaceObjectsStatePlanInputs*); using AS=StatefulResult(*)(const AddStrokeOp&,const ObjectStore&,CreateObjectsStatePlanInputs*); using SP=StatefulResult(*)(const SplitStrokesOp&,const ObjectStore&,SplitStrokesStatePlanInputs*); using AM=StatefulResult(*)(const AddEraseMasksOp&,const ObjectStore&,ReplaceObjectsStatePlanInputs*); using RM=StatefulResult(*)(const RemoveEraseMasksOp&,const ObjectStore&,ReplaceObjectsStatePlanInputs*); using R=StatefulResult(*)(const EditRichTextOp&,const ObjectStore&,ReplaceObjectsStatePlanInputs*); using C=StatefulResult(*)(const SetConnectorContentOp&,const ObjectStore&,ReplaceObjectsStatePlanInputs*);
static_assert(std::is_same_v<decltype(&validateInsertObjectsState),I>); static_assert(std::is_same_v<decltype(&validateSetPlacementsState),P>); static_assert(std::is_same_v<decltype(&validateSetTransformsState),T>); static_assert(std::is_same_v<decltype(&validatePatchPropertiesState),PP>); static_assert(std::is_same_v<decltype(&validateSetObjectSizeState),S>); static_assert(std::is_same_v<decltype(&validateSetVectorPathGeometryState),V>); static_assert(std::is_same_v<decltype(&validateSetImageContentState),IM>); static_assert(std::is_same_v<decltype(&validateAddStrokeState),AS>); static_assert(std::is_same_v<decltype(&validateSplitStrokesState),SP>); static_assert(std::is_same_v<decltype(&validateAddEraseMasksState),AM>); static_assert(std::is_same_v<decltype(&validateRemoveEraseMasksState),RM>); static_assert(std::is_same_v<decltype(&validateEditRichTextState),R>); static_assert(std::is_same_v<decltype(&validateSetConnectorContentState),C>);
ObjectId id(std::uint64_t n){return ObjectId::fromUint64(n);} Placement pl(std::uint64_t n,std::optional<ObjectId> p={}){return {p,OrderKey({static_cast<std::uint8_t>(n?n:1)})};}
ObjectRecord shape(std::uint64_t n,std::optional<ObjectId> p={}){ObjectRecord r{};r.id=id(n);r.kind=ObjectKind::kShape;r.kind_version=1;r.placement=pl(n,p);r.content=ShapeContent{1,10,20};return r;}
ObjectRecord group(std::uint64_t n,std::optional<ObjectId> p={}){ObjectRecord r{};r.id=id(n);r.kind=ObjectKind::kGroup;r.kind_version=1;r.placement=pl(n,p);r.content=GroupContent{};return r;}
ObjectRecord sticky(std::uint64_t n,std::optional<ObjectId> p={}){ObjectRecord r{};r.id=id(n);r.kind=ObjectKind::kSticky;r.kind_version=1;r.placement=pl(n,p);r.content=StickyContent{10,20};return r;}
ParagraphStyle ps(){return {ParagraphAlignment::kLeft,1,0,0};} TextStyle ts(float c=1){TextStyle s{};s.font_resource_id=ResourceId{id(90)};s.font_size=12;s.weight=400;s.color={c,c,c,1};return s;}
ObjectRecord rich(std::uint64_t n,std::optional<ObjectId> p={}){ObjectRecord r{};r.id=id(n);r.kind=ObjectKind::kRichText;r.kind_version=1;r.placement=pl(n,p);Paragraph q{};q.id=id(n*10);q.style=ps();q.runs={{"A",ts()}};r.content=RichTextContent{{{q}}};return r;}
StrokeRecord vs(){StrokeRecord s{};s.brush.brush_family_id=1;s.brush.brush_version=1;s.brush.color={0,0,0,1};s.brush.nominal_size=1;s.brush.opacity=1;s.data=VectorStrokeData{{StrokeSample{{1,2},1,{0,0}}}};return s;}
ObjectRecord vst(std::uint64_t n,std::optional<ObjectId> p={}){ObjectRecord r{};r.id=id(n);r.kind=ObjectKind::kVectorStroke;r.kind_version=1;r.placement=pl(n,p);r.content=VectorStrokeContent{vs()};return r;}
ObjectRecord dst(std::uint64_t n){auto r=vst(n);r.kind=ObjectKind::kDabStroke;StrokeRecord s{};s.brush.brush_family_id=3;s.brush.brush_version=1;s.brush.color={0,0,0,1};s.brush.nominal_size=1;s.brush.opacity=1;s.brush.texture_resource_id=ResourceId{id(8)};s.data=DabStrokeData{{DabInstance{{1,2},2,0,1}}};r.content=DabStrokeContent{s};return r;}
ImageContent image(){return {ResourceId{id(8)},100,80,std::nullopt,ImageContentMode::kFit,3,4};} ObjectRecord img(std::uint64_t n){ObjectRecord r{};r.id=id(n);r.kind=ObjectKind::kImage;r.kind_version=1;r.placement=pl(n);r.content=image();return r;}
VectorPathGeometry path(){return {FillRule::kNonZero,{MoveTo{{0,0}},LineTo{{1,1}}}};} ObjectRecord vp(std::uint64_t n){ObjectRecord r{};r.id=id(n);r.kind=ObjectKind::kVectorPath;r.kind_version=1;r.placement=pl(n);r.content=VectorPathContent{path()};return r;}
ConnectorContent freeCon(){return {ConnectorEndpoint{FreePointEndpoint{{0,0}}},ConnectorEndpoint{FreePointEndpoint{{1,1}}},ConnectorRouting::kStraight};} ObjectRecord con(std::uint64_t n,std::uint64_t t){ObjectRecord r{};r.id=id(n);r.kind=ObjectKind::kConnector;r.kind_version=1;r.placement=pl(n);r.content=ConnectorContent{ConnectorEndpoint{FreePointEndpoint{{0,0}}},ConnectorEndpoint{AttachedEndpoint{id(t),AutoPerimeterAnchor{}}},ConnectorRouting::kStraight};return r;}
EraseMaskRecord mask(std::uint64_t n){const EraseCubicSegment s{EraseKnot{{0,0},1},EraseKnot{{1,1},1},{.5,.5},{.5,.5}};return {id(n),SweptCircleMask{{s}}};}
template<class Store> void seed(Store& s,const std::vector<ObjectRecord>& v){for(const auto& r:v)ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(s,r));}
Operation structuralOperation(OperationPayload payload){Operation o{};o.id=OperationId{id(7001)};o.document_id=DocumentId{id(7002)};o.schema_version=1;o.payload_version=1;o.payload=std::move(payload);return o;}
template<class Payload> void expectPayloadStructurallyValid(const Payload& payload){ASSERT_TRUE(validatePayloadStructure(structuralOperation(OperationPayload{payload})).ok());}
void expectRecordsStructurallyValid(const std::vector<ObjectRecord>& records){if(records.empty())return;auto canonical=records;std::sort(canonical.begin(),canonical.end(),[](const auto&a,const auto&b){return a.id<b.id;});expectPayloadStructurallyValid(InsertObjectsOp{canonical});}
enum class StructuralExpectation { kCanonical, kIntentionalDefenseInvalid };
template<class Payload,class F> void expectCreateCase(const std::vector<ObjectRecord>& init,const Payload& payload,F&& f,StatefulIssue issue,const CreateObjectsStatePlanInputs& expected,const CreateObjectsStatePlanInputs& sent,StructuralExpectation mode=StructuralExpectation::kCanonical){ASSERT_FALSE(sent.creates.empty());if(mode==StructuralExpectation::kCanonical){expectRecordsStructurallyValid(init);expectPayloadStructurallyValid(payload);}ReferenceObjectStore a;IndexedObjectStore b;seed(a,init);seed(b,init);auto ab=a.allObjects(),bb=b.allObjects();auto x=sent,y=sent;auto ar=f(payload,a,&x),br=f(payload,b,&y);EXPECT_EQ(ar.issue,issue);EXPECT_EQ(br.issue,issue);EXPECT_EQ(ar.issue,br.issue);if(issue==StatefulIssue::kNone){EXPECT_EQ(x.creates,expected.creates);EXPECT_EQ(y.creates,expected.creates);}else{EXPECT_EQ(x.creates,sent.creates);EXPECT_EQ(y.creates,sent.creates);}EXPECT_EQ(a.allObjects(),ab);EXPECT_EQ(b.allObjects(),bb);EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(b));}
template<class Payload,class F> void expectReplaceCase(const std::vector<ObjectRecord>& init,const Payload& payload,F&& f,StatefulIssue issue,const ReplaceObjectsStatePlanInputs& expected,const ReplaceObjectsStatePlanInputs& sent,StructuralExpectation mode=StructuralExpectation::kCanonical){ASSERT_FALSE(sent.replacements.empty());if(mode==StructuralExpectation::kCanonical){expectRecordsStructurallyValid(init);expectPayloadStructurallyValid(payload);}ReferenceObjectStore a;IndexedObjectStore b;seed(a,init);seed(b,init);auto ab=a.allObjects(),bb=b.allObjects();auto x=sent,y=sent;auto ar=f(payload,a,&x),br=f(payload,b,&y);EXPECT_EQ(ar.issue,issue);EXPECT_EQ(br.issue,issue);EXPECT_EQ(ar.issue,br.issue);if(issue==StatefulIssue::kNone){EXPECT_EQ(x.replacements,expected.replacements);EXPECT_EQ(y.replacements,expected.replacements);}else{EXPECT_EQ(x.replacements,sent.replacements);EXPECT_EQ(y.replacements,sent.replacements);}EXPECT_EQ(a.allObjects(),ab);EXPECT_EQ(b.allObjects(),bb);EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(b));}
template<class Payload,class F> void expectSplitCase(const std::vector<ObjectRecord>& init,const Payload& payload,F&& f,StatefulIssue issue,const SplitStrokesStatePlanInputs& expected,const SplitStrokesStatePlanInputs& sent,StructuralExpectation mode=StructuralExpectation::kCanonical){ASSERT_FALSE(sent.source_delete_ids.empty());ASSERT_FALSE(sent.replacement_creates.empty());if(mode==StructuralExpectation::kCanonical){expectRecordsStructurallyValid(init);expectPayloadStructurallyValid(payload);}ReferenceObjectStore a;IndexedObjectStore b;seed(a,init);seed(b,init);auto ab=a.allObjects(),bb=b.allObjects();auto x=sent,y=sent;auto ar=f(payload,a,&x),br=f(payload,b,&y);EXPECT_EQ(ar.issue,issue);EXPECT_EQ(br.issue,issue);EXPECT_EQ(ar.issue,br.issue);if(issue==StatefulIssue::kNone){EXPECT_EQ(x.source_delete_ids,expected.source_delete_ids);EXPECT_EQ(x.replacement_creates,expected.replacement_creates);EXPECT_EQ(y.source_delete_ids,expected.source_delete_ids);EXPECT_EQ(y.replacement_creates,expected.replacement_creates);}else{EXPECT_EQ(x.source_delete_ids,sent.source_delete_ids);EXPECT_EQ(x.replacement_creates,sent.replacement_creates);EXPECT_EQ(y.source_delete_ids,sent.source_delete_ids);EXPECT_EQ(y.replacement_creates,sent.replacement_creates);}EXPECT_EQ(a.allObjects(),ab);EXPECT_EQ(b.allObjects(),bb);EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(b));}

// Compatibility overloads retain the established behavioral call sites while
// canonical fixture/payload validity is exercised by the structural matrix
// below. New cases should use the payload-taking overloads above.
template<class F> void expectCreateCase(const std::vector<ObjectRecord>& init,F&& f,StatefulIssue issue,const CreateObjectsStatePlanInputs& expected,const CreateObjectsStatePlanInputs& sent){ASSERT_FALSE(sent.creates.empty());expectRecordsStructurallyValid(init);ReferenceObjectStore a;IndexedObjectStore b;seed(a,init);seed(b,init);auto ab=a.allObjects(),bb=b.allObjects();auto x=sent,y=sent;auto ar=f(a,&x),br=f(b,&y);EXPECT_EQ(ar.issue,issue);EXPECT_EQ(br.issue,issue);EXPECT_EQ(ar.issue,br.issue);if(issue==StatefulIssue::kNone){EXPECT_EQ(x.creates,expected.creates);EXPECT_EQ(y.creates,expected.creates);}else{EXPECT_EQ(x.creates,sent.creates);EXPECT_EQ(y.creates,sent.creates);}EXPECT_EQ(a.allObjects(),ab);EXPECT_EQ(b.allObjects(),bb);EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(b));}
template<class F> void expectReplaceCase(const std::vector<ObjectRecord>& init,F&& f,StatefulIssue issue,const ReplaceObjectsStatePlanInputs& expected,const ReplaceObjectsStatePlanInputs& sent){ASSERT_FALSE(sent.replacements.empty());expectRecordsStructurallyValid(init);ReferenceObjectStore a;IndexedObjectStore b;seed(a,init);seed(b,init);auto ab=a.allObjects(),bb=b.allObjects();auto x=sent,y=sent;auto ar=f(a,&x),br=f(b,&y);EXPECT_EQ(ar.issue,issue);EXPECT_EQ(br.issue,issue);EXPECT_EQ(ar.issue,br.issue);if(issue==StatefulIssue::kNone){EXPECT_EQ(x.replacements,expected.replacements);EXPECT_EQ(y.replacements,expected.replacements);}else{EXPECT_EQ(x.replacements,sent.replacements);EXPECT_EQ(y.replacements,sent.replacements);}EXPECT_EQ(a.allObjects(),ab);EXPECT_EQ(b.allObjects(),bb);EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(b));}
template<class F> void expectSplitCase(const std::vector<ObjectRecord>& init,F&& f,StatefulIssue issue,const SplitStrokesStatePlanInputs& expected,const SplitStrokesStatePlanInputs& sent){ASSERT_FALSE(sent.source_delete_ids.empty());ASSERT_FALSE(sent.replacement_creates.empty());expectRecordsStructurallyValid(init);ReferenceObjectStore a;IndexedObjectStore b;seed(a,init);seed(b,init);auto ab=a.allObjects(),bb=b.allObjects();auto x=sent,y=sent;auto ar=f(a,&x),br=f(b,&y);EXPECT_EQ(ar.issue,issue);EXPECT_EQ(br.issue,issue);EXPECT_EQ(ar.issue,br.issue);if(issue==StatefulIssue::kNone){EXPECT_EQ(x.source_delete_ids,expected.source_delete_ids);EXPECT_EQ(x.replacement_creates,expected.replacement_creates);EXPECT_EQ(y.source_delete_ids,expected.source_delete_ids);EXPECT_EQ(y.replacement_creates,expected.replacement_creates);}else{EXPECT_EQ(x.source_delete_ids,sent.source_delete_ids);EXPECT_EQ(x.replacement_creates,sent.replacement_creates);EXPECT_EQ(y.source_delete_ids,sent.source_delete_ids);EXPECT_EQ(y.replacement_creates,sent.replacement_creates);}EXPECT_EQ(a.allObjects(),ab);EXPECT_EQ(b.allObjects(),bb);EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(b));}
CreateObjectsStatePlanInputs cs(){return {{shape(900)}};} ReplaceObjectsStatePlanInputs rs(){return {{shape(901)}};} SplitStrokesStatePlanInputs ss(){return {{id(902)},{vst(903)}};}
TEST(OperationSpecificStatefulValidation,FailureSentinelsAreNonEmpty){EXPECT_FALSE(cs().creates.empty());EXPECT_FALSE(rs().replacements.empty());EXPECT_FALSE(ss().source_delete_ids.empty());EXPECT_FALSE(ss().replacement_creates.empty());}
TEST(OperationSpecificStatefulValidation,StructuralPreconditionsCoverCanonicalFixtures){Operation o{};o.id=OperationId{id(7001)};o.document_id=DocumentId{id(7002)};o.schema_version=1;o.payload_version=1;o.payload=InsertObjectsOp{{shape(1),group(2),sticky(3),rich(4),vp(5),img(6),vst(7),dst(8)}};EXPECT_TRUE(validatePayloadStructure(o).ok());RichTextDelta d{1,{InsertTextStep{id(40),1,"X",ts()}}};o.payload=EditRichTextOp{id(4),d};EXPECT_TRUE(validatePayloadStructure(o).ok());o.payload=SetConnectorContentOp{id(9),freeCon()};EXPECT_TRUE(validatePayloadStructure(o).ok());}
TEST(OperationSpecificStatefulValidation,StructuralCaseMatrixUsesExactCanonicalPayloads){
    const auto validPlacement=SetPlacementsOp{{{id(2),pl(2,id(1))}}};
    const auto validTransform=SetTransformsOp{{{id(2),Transform2D{2,0,0,2,4,5}}}};
    const auto validPatch=PatchPropertiesOp{{{id(2),1,PropertyPatchAction::kSet,PropertyValue{false}}}};
    const auto validSize=SetObjectSizeOp{{{id(2),2,3}}};
    const auto validPath=SetVectorPathGeometryOp{id(4),path()};
    const auto validImage=SetImageContentOp{id(3),image()};
    const auto validStroke=AddStrokeOp{vst(10)};
    const auto validSplit=SplitStrokesOp{{{id(5),{vst(11)}}}};
    const auto validMaskAdd=AddEraseMasksOp{{{id(5),{mask(12)}}}};
    const auto validMaskRemove=RemoveEraseMasksOp{{{id(5),{id(13)}}}};
    const auto validRich=EditRichTextOp{id(6),RichTextDelta{1,{InsertTextStep{id(60),0,"X",ts()}}}};
    const auto validConnector=SetConnectorContentOp{id(7),freeCon()};
    expectPayloadStructurallyValid(InsertObjectsOp{{group(1),shape(2,id(1))}});
    expectPayloadStructurallyValid(validPlacement); expectPayloadStructurallyValid(validTransform);
    expectPayloadStructurallyValid(validPatch); expectPayloadStructurallyValid(validSize);
    expectPayloadStructurallyValid(validPath); expectPayloadStructurallyValid(validImage);
    expectPayloadStructurallyValid(validStroke); expectPayloadStructurallyValid(validSplit);
    expectPayloadStructurallyValid(validMaskAdd); expectPayloadStructurallyValid(validMaskRemove);
    expectPayloadStructurallyValid(validRich); expectPayloadStructurallyValid(validConnector);
}
TEST(OperationSpecificStatefulValidation,INS_B01_GroupAndShapeStagedTogetherSucceeds){auto payload = InsertObjectsOp{{{group(1),shape(2,id(1))}}};
    expectCreateCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateInsertObjectsState(payload, s, o); }, StatefulIssue::kNone, {{group(1),shape(2,id(1))}}, cs());}
TEST(OperationSpecificStatefulValidation,INS_B02_ExistingCandidateIdentityRejects){auto payload = InsertObjectsOp{{{shape(1)}}};
    expectCreateCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateInsertObjectsState(payload, s, o); }, StatefulIssue::kObjectAlreadyExists, {}, cs());}
TEST(OperationSpecificStatefulValidation,INS_B03_ConnectorAndStagedTargetSucceeds){auto payload = InsertObjectsOp{{{con(1,2),shape(2)}}};
    expectCreateCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateInsertObjectsState(payload, s, o); }, StatefulIssue::kNone, {{con(1,2),shape(2)}}, cs());}
TEST(OperationSpecificStatefulValidation,INS_B04_ConnectorMissingAttachedTargetRejects){auto payload = InsertObjectsOp{{{con(1,2)}}};
    expectCreateCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateInsertObjectsState(payload, s, o); }, StatefulIssue::kInvalidReference, {}, cs());}
TEST(OperationSpecificStatefulValidation,INS_B05_StickyShapeChildRejects){auto payload = InsertObjectsOp{{{sticky(1),shape(2,id(1))}}};
    expectCreateCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateInsertObjectsState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, cs());}
TEST(OperationSpecificStatefulValidation,INS_B06_StickyTwoRichTextChildrenRejects){auto payload = InsertObjectsOp{{{sticky(1),rich(2,id(1)),rich(3,id(1))}}};
    expectCreateCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateInsertObjectsState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, cs());}
TEST(OperationSpecificStatefulValidation,INS_B07_EmptyStickySucceeds){auto payload = InsertObjectsOp{{{sticky(1)}}};
    expectCreateCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateInsertObjectsState(payload, s, o); }, StatefulIssue::kNone, {{sticky(1)}}, cs());}
TEST(OperationSpecificStatefulValidation,INS_B08_BadRecordRejectsWholeOutput){auto bad=shape(2);bad.kind_version=9;auto payload = InsertObjectsOp{{{shape(1),bad}}};
    expectCreateCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateInsertObjectsState(payload, s, o); }, StatefulIssue::kInvalidKindVersion, {}, cs(), StructuralExpectation::kIntentionalDefenseInvalid);}
TEST(OperationSpecificStatefulValidation,PLC_B01_MoveShapeUnderGroupSucceeds){auto e=shape(2,id(1));auto payload = SetPlacementsOp{{{{id(2),pl(2,id(1))}}}};
    expectReplaceCase({group(1),shape(2)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetPlacementsState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,PLC_B02_MissingTargetRejects){auto payload = SetPlacementsOp{{{{id(2),pl(2)}}}};
    expectReplaceCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetPlacementsState(payload, s, o); }, StatefulIssue::kObjectMissing, {}, rs());}
TEST(OperationSpecificStatefulValidation,PLC_B03_MissingParentRejectsFromB3){auto payload = SetPlacementsOp{{{{id(2),pl(2,id(9))}}}};
    expectReplaceCase({shape(2)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetPlacementsState(payload, s, o); }, StatefulIssue::kInvalidReference, {}, rs());}
TEST(OperationSpecificStatefulValidation,PLC_B04_BatchCycleRejectsFromB3){auto payload = SetPlacementsOp{{{{id(1),pl(1,id(2))},{id(2),pl(2,id(1))}}}};
    expectReplaceCase({shape(1),shape(2)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetPlacementsState(payload, s, o); }, StatefulIssue::kHierarchyCycle, {}, rs());}
TEST(OperationSpecificStatefulValidation,PLC_B05_MoveRichTextIntoEmptyStickySucceeds){auto e=rich(2,id(1));auto payload = SetPlacementsOp{{{{id(2),pl(2,id(1))}}}};
    expectReplaceCase({sticky(1),rich(2)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetPlacementsState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,PLC_B06_SecondRichTextIntoOccupiedStickyRejects){auto payload = SetPlacementsOp{{{{id(3),pl(3,id(1))}}}};
    expectReplaceCase({sticky(1),rich(2,id(1)),rich(3)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetPlacementsState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, rs());}
TEST(OperationSpecificStatefulValidation,PLC_B07_MoveShapeIntoStickyRejects){auto payload = SetPlacementsOp{{{{id(2),pl(2,id(1))}}}};
    expectReplaceCase({sticky(1),shape(2)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetPlacementsState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, rs());}
TEST(OperationSpecificStatefulValidation,PLC_B08_MoveSoleRichTextOutLeavesEmptySticky){auto e=rich(2,id(3));auto payload = SetPlacementsOp{{{{id(2),pl(2,id(3))}}}};
    expectReplaceCase({sticky(1),rich(2,id(1)),group(3)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetPlacementsState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,PLC_B09_ValidFinalStateBatch){auto a=rich(2,id(1)),b=rich(3,id(1));auto e1=a;e1.placement=pl(2,id(1));auto e2=b;e2.placement=pl(3,id(4));auto payload = SetPlacementsOp{{{{id(2),pl(2,id(1))},{id(3),pl(3,id(4))}}}};
    expectReplaceCase({sticky(1),a,b,group(4)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetPlacementsState(payload, s, o); }, StatefulIssue::kNone, {{e1,e2}}, rs());}
TEST(OperationSpecificStatefulValidation,TRN_B01_ValidTargetProducesReplacement){auto e=shape(1);e.transform={2,0,0,2,4,5};auto payload = SetTransformsOp{{{{id(1),{2,0,0,2,4,5}}}}};
    expectReplaceCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetTransformsState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,TRN_B02_MissingTargetRejects){auto payload = SetTransformsOp{{{{id(1),{}}}}};
    expectReplaceCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetTransformsState(payload, s, o); }, StatefulIssue::kObjectMissing, {}, rs());}
TEST(OperationSpecificStatefulValidation,TRN_B03_LateMissingTargetLeavesWholeOutputUnchanged){auto payload = SetTransformsOp{{{{id(1),{}},{id(2),{}}}}};
    expectReplaceCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetTransformsState(payload, s, o); }, StatefulIssue::kObjectMissing, {}, rs());}
TEST(OperationSpecificStatefulValidation,SIZE_B01_ShapeSucceeds){auto e=shape(1);std::get<ShapeContent>(e.content).width=3;std::get<ShapeContent>(e.content).height=4;auto payload = SetObjectSizeOp{{{{id(1),3,4}}}};
    expectReplaceCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetObjectSizeState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,SIZE_B02_ImageSucceedsAndPreservesNonSizeContent){auto e=img(1);std::get<ImageContent>(e.content).width=3;std::get<ImageContent>(e.content).height=4;auto payload = SetObjectSizeOp{{{{id(1),3,4}}}};
    expectReplaceCase({img(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetObjectSizeState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,SIZE_B03_StickySucceeds){auto e=sticky(1);std::get<StickyContent>(e.content).width=3;std::get<StickyContent>(e.content).height=4;auto payload = SetObjectSizeOp{{{{id(1),3,4}}}};
    expectReplaceCase({sticky(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetObjectSizeState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,SIZE_B04_GroupRejects){auto payload = SetObjectSizeOp{{{{id(1),3,4}}}};
    expectReplaceCase({group(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetObjectSizeState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, rs());}
TEST(OperationSpecificStatefulValidation,SIZE_B05_MissingTargetRejects){auto payload = SetObjectSizeOp{{{{id(1),3,4}}}};
    expectReplaceCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetObjectSizeState(payload, s, o); }, StatefulIssue::kObjectMissing, {}, rs());}
TEST(OperationSpecificStatefulValidation,PATH_B01_VectorPathSucceeds){auto e=vp(1);auto payload = SetVectorPathGeometryOp{id(1),path()};
    expectReplaceCase({vp(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetVectorPathGeometryState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,PATH_B02_WrongKindRejects){auto payload = SetVectorPathGeometryOp{id(1),path()};
    expectReplaceCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetVectorPathGeometryState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, rs());}
TEST(OperationSpecificStatefulValidation,PATH_B03_MissingTargetRejects){auto payload = SetVectorPathGeometryOp{id(1),path()};
    expectReplaceCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetVectorPathGeometryState(payload, s, o); }, StatefulIssue::kObjectMissing, {}, rs());}
TEST(OperationSpecificStatefulValidation,IMG_B01_CompleteImageContentReplacementSucceeds){auto e=img(1);std::get<ImageContent>(e.content)=image();auto payload = SetImageContentOp{id(1),image()};
    expectReplaceCase({img(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetImageContentState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,IMG_B02_WrongKindRejects){auto payload = SetImageContentOp{id(1),image()};
    expectReplaceCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetImageContentState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, rs());}
TEST(OperationSpecificStatefulValidation,IMG_B03_MissingTargetRejects){auto payload = SetImageContentOp{id(1),image()};
    expectReplaceCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetImageContentState(payload, s, o); }, StatefulIssue::kObjectMissing, {}, rs());}
TEST(OperationSpecificStatefulValidation,STR_B01_VectorStrokeCreateSucceeds){auto payload = AddStrokeOp{{vst(1)}};
    expectCreateCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateAddStrokeState(payload, s, o); }, StatefulIssue::kNone, {{vst(1)}}, cs());}
TEST(OperationSpecificStatefulValidation,STR_B02_DabStrokeCreateSucceeds){auto payload = AddStrokeOp{{dst(1)}};
    expectCreateCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateAddStrokeState(payload, s, o); }, StatefulIssue::kNone, {{dst(1)}}, cs());}
TEST(OperationSpecificStatefulValidation,STR_B03_ExistingIdRejects){auto payload = AddStrokeOp{{vst(1)}};
    expectCreateCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateAddStrokeState(payload, s, o); }, StatefulIssue::kObjectAlreadyExists, {}, cs());}
TEST(OperationSpecificStatefulValidation,STR_B04_NonStrokeRecordRejects){auto payload = AddStrokeOp{{shape(1)}};
    expectCreateCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateAddStrokeState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, cs());}
TEST(OperationSpecificStatefulValidation,STR_B05_StrokeUnderGroupSucceeds){auto payload = AddStrokeOp{{vst(2,id(1))}};
    expectCreateCase({group(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateAddStrokeState(payload, s, o); }, StatefulIssue::kNone, {{vst(2,id(1))}}, cs());}
TEST(OperationSpecificStatefulValidation,STR_B06_StrokeUnderStickyRejects){auto payload = AddStrokeOp{{vst(2,id(1))}};
    expectCreateCase({sticky(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateAddStrokeState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, cs());}
TEST(OperationSpecificStatefulValidation,SPL_B01_SourceToTwoAbsentReplacementsSucceeds){SplitStrokesOp op{{{id(1),{vst(2),vst(3)}}}};auto payload = SplitStrokesOp{op};
    expectSplitCase({vst(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSplitStrokesState(payload, s, o); }, StatefulIssue::kNone, {{id(1)},{vst(2),vst(3)}}, ss());}
TEST(OperationSpecificStatefulValidation,SPL_B02_MissingSourceRejects){auto payload = SplitStrokesOp{{{{id(1),{vst(2)}}}}};
    expectSplitCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSplitStrokesState(payload, s, o); }, StatefulIssue::kObjectMissing, {}, ss());}
TEST(OperationSpecificStatefulValidation,SPL_B03_NonStrokeSourceRejects){auto payload = SplitStrokesOp{{{{id(1),{vst(2)}}}}};
    expectSplitCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSplitStrokesState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, ss());}
TEST(OperationSpecificStatefulValidation,SPL_B04_ReplacementCollidesWithUnrelatedExistingId){auto payload = SplitStrokesOp{{{{id(1),{vst(2)}}}}};
    expectSplitCase({vst(1),shape(2)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSplitStrokesState(payload, s, o); }, StatefulIssue::kObjectAlreadyExists, {}, ss());}
TEST(OperationSpecificStatefulValidation,SPL_B05_ReplacementReusesSourceIdRejects){auto payload = SplitStrokesOp{{{{id(1),{vst(1)}}}}};
    expectSplitCase({vst(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSplitStrokesState(payload, s, o); }, StatefulIssue::kObjectAlreadyExists, {}, ss());}
TEST(OperationSpecificStatefulValidation,SPL_B06_NonStrokeReplacementRejects){auto payload = SplitStrokesOp{{{{id(1),{shape(2)}}}}};
    expectSplitCase({vst(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSplitStrokesState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, ss());}
TEST(OperationSpecificStatefulValidation,SPL_B07_InvalidReplacementParentRejects){auto payload = SplitStrokesOp{{{{id(1),{vst(2,id(9))}}}}};
    expectSplitCase({vst(1),sticky(9)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSplitStrokesState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, ss());}
TEST(OperationSpecificStatefulValidation,SPL_B08_OneFailingSplitLeavesAllFragmentsUnchanged){auto payload = SplitStrokesOp{{{{id(1),{vst(3)}},{id(2),{shape(4)}}}}};
    expectSplitCase({vst(1),vst(2)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSplitStrokesState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, ss());}
TEST(OperationSpecificStatefulValidation,MASK_ADD_B01_VectorStrokeSucceeds){auto e=vst(1);e.erase_masks={mask(2)};auto payload = AddEraseMasksOp{{{{id(1),{mask(2)}}}}};
    expectReplaceCase({vst(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateAddEraseMasksState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,MASK_ADD_B02_DabStrokeSucceeds){auto e=dst(1);e.erase_masks={mask(2)};auto payload = AddEraseMasksOp{{{{id(1),{mask(2)}}}}};
    expectReplaceCase({dst(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateAddEraseMasksState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,MASK_ADD_B03_WrongKindRejects){auto payload = AddEraseMasksOp{{{{id(1),{mask(2)}}}}};
    expectReplaceCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateAddEraseMasksState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, rs());}
TEST(OperationSpecificStatefulValidation,MASK_ADD_B04_MissingTargetRejects){auto payload = AddEraseMasksOp{{{{id(1),{mask(2)}}}}};
    expectReplaceCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateAddEraseMasksState(payload, s, o); }, StatefulIssue::kObjectMissing, {}, rs());}
TEST(OperationSpecificStatefulValidation,MASK_ADD_B05_ExistingMaskIdRejects){auto s=vst(1);s.erase_masks={mask(2)};auto payload = AddEraseMasksOp{{{{id(1),{mask(2)}}}}};
    expectReplaceCase({s}, payload, [&](const auto& payload, auto& s, auto* o) { return validateAddEraseMasksState(payload, s, o); }, StatefulIssue::kMaskStateInvalid, {}, rs());}
TEST(OperationSpecificStatefulValidation,MASK_ADD_B06_LateFailureLeavesWholeOutputUnchanged){auto payload = AddEraseMasksOp{{{{id(1),{mask(2)}},{id(9),{mask(3)}}}}};
    expectReplaceCase({vst(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateAddEraseMasksState(payload, s, o); }, StatefulIssue::kObjectMissing, {}, rs());}
TEST(OperationSpecificStatefulValidation,MASK_REM_B01_AllRequestedMasksExistSucceeds){auto s=vst(1);s.erase_masks={mask(2),mask(3)};auto e=s;e.erase_masks.clear();auto payload = RemoveEraseMasksOp{{{{id(1),{id(2),id(3)}}}}};
    expectReplaceCase({s}, payload, [&](const auto& payload, auto& s, auto* o) { return validateRemoveEraseMasksState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,MASK_REM_B02_OneMissingMaskRejectsWholeOperation){auto s=vst(1);s.erase_masks={mask(2)};auto payload = RemoveEraseMasksOp{{{{id(1),{id(2),id(9)}}}}};
    expectReplaceCase({s}, payload, [&](const auto& payload, auto& s, auto* o) { return validateRemoveEraseMasksState(payload, s, o); }, StatefulIssue::kMaskStateInvalid, {}, rs());}
TEST(OperationSpecificStatefulValidation,MASK_REM_B03_WrongKindRejects){auto payload = RemoveEraseMasksOp{{{{id(1),{id(2)}}}}};
    expectReplaceCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateRemoveEraseMasksState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, rs());}
TEST(OperationSpecificStatefulValidation,MASK_REM_B04_MissingTargetRejects){auto payload = RemoveEraseMasksOp{{{{id(1),{id(2)}}}}};
    expectReplaceCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateRemoveEraseMasksState(payload, s, o); }, StatefulIssue::kObjectMissing, {}, rs());}
TEST(OperationSpecificStatefulValidation,MASK_REM_B05_LateFailureLeavesOutputUnchanged){auto s=vst(1);s.erase_masks={mask(2)};auto payload = RemoveEraseMasksOp{{{{id(1),{id(2)}},{id(9),{id(2)}}}}};
    expectReplaceCase({s}, payload, [&](const auto& payload, auto& s, auto* o) { return validateRemoveEraseMasksState(payload, s, o); }, StatefulIssue::kObjectMissing, {}, rs());}
TEST(OperationSpecificStatefulValidation,PROP_B01_SetApplicableFieldProducesExactReplacement){auto e=shape(1);e.properties.entries={{1,false}};auto payload = PatchPropertiesOp{{{{id(1),1,PropertyPatchAction::kSet,PropertyValue{false}}}}};
    expectReplaceCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validatePatchPropertiesState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,PROP_B02_WrongKindApplicabilityRejects){auto payload = PatchPropertiesOp{{{{id(1),0x100,PropertyPatchAction::kClear,{}}}}};
    expectReplaceCase({group(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validatePatchPropertiesState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, rs());}
TEST(OperationSpecificStatefulValidation,PROP_B03_ClearRemovesExplicitEntry){auto s=shape(1);s.properties.entries={{1,false}};auto e=shape(1);auto payload = PatchPropertiesOp{{{{id(1),1,PropertyPatchAction::kClear,{}}}}};
    expectReplaceCase({s}, payload, [&](const auto& payload, auto& s, auto* o) { return validatePatchPropertiesState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,PROP_B04_SetRegistryDefaultCanonicalizesToAbsence){auto payload = PatchPropertiesOp{{{{id(1),1,PropertyPatchAction::kSet,PropertyValue{true}}}}};
    expectReplaceCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validatePatchPropertiesState(payload, s, o); }, StatefulIssue::kNone, {{shape(1)}}, rs());}
TEST(OperationSpecificStatefulValidation,PROP_B05_TwoFieldsOneObjectProduceOneSortedReplacement){auto e=shape(1);e.properties.entries={{1,false},{2,true}};auto payload = PatchPropertiesOp{{{{id(1),1,PropertyPatchAction::kSet,PropertyValue{false}},{id(1),2,PropertyPatchAction::kSet,PropertyValue{true}}}}};
    expectReplaceCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validatePatchPropertiesState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,PROP_B06_MissingTargetRejects){auto payload = PatchPropertiesOp{{{{id(1),1,PropertyPatchAction::kClear,{}}}}};
    expectReplaceCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validatePatchPropertiesState(payload, s, o); }, StatefulIssue::kObjectMissing, {}, rs());}
TEST(OperationSpecificStatefulValidation,PROP_B07_LateFailureLeavesOutputUnchanged){auto payload = PatchPropertiesOp{{{{id(1),1,PropertyPatchAction::kSet,PropertyValue{false}},{id(2),1,PropertyPatchAction::kClear,{}}}}};
    expectReplaceCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validatePatchPropertiesState(payload, s, o); }, StatefulIssue::kObjectMissing, {}, rs());}
TEST(OperationSpecificStatefulValidation,EDIT_B01_UnicodeScalarDeltaProducesReplacement){auto r=rich(1);auto e=r;auto& p=std::get<RichTextContent>(e.content).document.paragraphs[0];p.runs={{"A",ts()},{"😀X",ts(.5f)}};RichTextDelta d{1,{InsertTextStep{id(10),1,"😀X",ts(.5f)}}};auto payload = EditRichTextOp{id(1),d};
    expectReplaceCase({r}, payload, [&](const auto& payload, auto& s, auto* o) { return validateEditRichTextState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,EDIT_B02_MissingObjectRejects){auto payload = EditRichTextOp{id(1),RichTextDelta{1,{InsertTextStep{id(10),0,"X",ts()}}}};
    expectReplaceCase({}, payload, [&](const auto& payload, auto& s, auto* o) { return validateEditRichTextState(payload, s, o); }, StatefulIssue::kObjectMissing, {}, rs());}
TEST(OperationSpecificStatefulValidation,EDIT_B03_WrongKindRejects){auto payload = EditRichTextOp{id(1),RichTextDelta{1,{InsertTextStep{id(10),0,"X",ts()}}}};
    expectReplaceCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateEditRichTextState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, rs());}
TEST(OperationSpecificStatefulValidation,EDIT_B04_InvalidStagedDeltaLeavesOutputUnchanged){auto r=rich(1);RichTextDelta d{1,{InsertTextStep{id(10),0,"X",ts()},DeleteTextStep{id(99),0,1}}};auto payload = EditRichTextOp{id(1),d};
    expectReplaceCase({r}, payload, [&](const auto& payload, auto& s, auto* o) { return validateEditRichTextState(payload, s, o); }, StatefulIssue::kTextStateInvalid, {}, rs());}
TEST(OperationSpecificStatefulValidation,CON_B01_ValidConnectorExistingConnectableTargetSucceeds){auto c=con(1,2);auto payload = SetConnectorContentOp{id(1),std::get<ConnectorContent>(c.content)};
    expectReplaceCase({shape(2),c}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetConnectorContentState(payload, s, o); }, StatefulIssue::kNone, {{c}}, rs());}
TEST(OperationSpecificStatefulValidation,CON_B02_NonConnectorTargetObjectRejects){auto payload = SetConnectorContentOp{id(1),freeCon()};
    expectReplaceCase({shape(1)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetConnectorContentState(payload, s, o); }, StatefulIssue::kInvalidApplicability, {}, rs());}
TEST(OperationSpecificStatefulValidation,CON_B03_MissingAttachedTargetRejects){auto c=con(1,9);auto payload = SetConnectorContentOp{id(1),std::get<ConnectorContent>(c.content)};
    expectReplaceCase({c}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetConnectorContentState(payload, s, o); }, StatefulIssue::kInvalidReference, {}, rs());}
TEST(OperationSpecificStatefulValidation,CON_B04_NonConnectableAttachedTargetRejects){auto c=con(1,2);auto payload = SetConnectorContentOp{id(1),std::get<ConnectorContent>(c.content)};
    expectReplaceCase({c,group(2)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetConnectorContentState(payload, s, o); }, StatefulIssue::kConnectorInvalid, {}, rs());}
TEST(OperationSpecificStatefulValidation,CON_B05_ValidStablePortForConnectableTargetSucceeds){auto c=con(1,2);auto content=std::get<ConnectorContent>(c.content);content.end.value=AttachedEndpoint{id(2),StablePortAnchor{1}};auto e=c;e.content=content;auto payload = SetConnectorContentOp{id(1),content};
    expectReplaceCase({shape(2),c}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetConnectorContentState(payload, s, o); }, StatefulIssue::kNone, {{e}}, rs());}
TEST(OperationSpecificStatefulValidation,CON_B06_InvalidStablePortForNonConnectableTargetRejects){auto c=con(1,2);auto content=std::get<ConnectorContent>(c.content);content.end.value=AttachedEndpoint{id(2),StablePortAnchor{1}};auto payload = SetConnectorContentOp{id(1),content};
    expectReplaceCase({c,group(2)}, payload, [&](const auto& payload, auto& s, auto* o) { return validateSetConnectorContentState(payload, s, o); }, StatefulIssue::kConnectorInvalid, {}, rs());}
class CountingStore final:public ObjectStore{public:explicit CountingStore(const ObjectStore& d):d_(d){}std::size_t size()const noexcept override{return d_.size();}bool contains(const ObjectId&i)const noexcept override{++contains_calls;return d_.contains(i);}const ObjectRecord*find(const ObjectId&i)const noexcept override{++find_calls;return d_.find(i);}std::vector<ObjectRecord>allObjects()const override{++all_objects_calls;return d_.allObjects();}std::vector<ObjectRecord>children(const std::optional<ObjectId>&p)const override{++children_calls;return d_.children(p);}mutable std::size_t contains_calls=0,find_calls=0,all_objects_calls=0,children_calls=0;private:const ObjectStore&d_;};
TEST(OperationSpecificStatefulValidation,CountingStore_CoversAllThirteenFamiliesWithoutAllObjectsScan){
    auto stroke=vst(5); stroke.erase_masks={mask(13)}; const std::vector<ObjectRecord> seeded={group(1),shape(2),img(3),vp(4),stroke,rich(6),con(7,2)}; ReferenceObjectStore b; seed(b,seeded);
    expectRecordsStructurallyValid(seeded);
    auto check=[&](const auto& payload, auto fn){expectPayloadStructurallyValid(payload);CountingStore c(b);EXPECT_TRUE(fn(payload,c).ok());EXPECT_EQ(c.all_objects_calls,0U);};
    check(InsertObjectsOp{{sticky(10)}},[](const auto& p,auto& c){CreateObjectsStatePlanInputs o; return validateInsertObjectsState(p,c,&o);});
    check(SetPlacementsOp{{{id(2),pl(2,id(1))}}},[](const auto& p,auto& c){ReplaceObjectsStatePlanInputs o; return validateSetPlacementsState(p,c,&o);});
    check(SetTransformsOp{{{id(2),{}}}},[](const auto& p,auto& c){ReplaceObjectsStatePlanInputs o; return validateSetTransformsState(p,c,&o);});
    check(PatchPropertiesOp{{{id(2),1,PropertyPatchAction::kSet,PropertyValue{false}}}},[](const auto& p,auto& c){ReplaceObjectsStatePlanInputs o; return validatePatchPropertiesState(p,c,&o);});
    check(SetObjectSizeOp{{{id(2),2,3}}},[](const auto& p,auto& c){ReplaceObjectsStatePlanInputs o; return validateSetObjectSizeState(p,c,&o);});
    check(SetVectorPathGeometryOp{id(4),path()},[](const auto& p,auto& c){ReplaceObjectsStatePlanInputs o; return validateSetVectorPathGeometryState(p,c,&o);});
    check(SetImageContentOp{id(3),image()},[](const auto& p,auto& c){ReplaceObjectsStatePlanInputs o; return validateSetImageContentState(p,c,&o);});
    check(AddStrokeOp{vst(10)},[](const auto& p,auto& c){CreateObjectsStatePlanInputs o; return validateAddStrokeState(p,c,&o);});
    check(SplitStrokesOp{{{id(5),{vst(11)}}}},[](const auto& p,auto& c){SplitStrokesStatePlanInputs o; return validateSplitStrokesState(p,c,&o);});
    check(AddEraseMasksOp{{{id(5),{mask(12)}}}},[](const auto& p,auto& c){ReplaceObjectsStatePlanInputs o; return validateAddEraseMasksState(p,c,&o);});
    check(RemoveEraseMasksOp{{{id(5),{id(13)}}}},[](const auto& p,auto& c){ReplaceObjectsStatePlanInputs o; return validateRemoveEraseMasksState(p,c,&o);});
    check(EditRichTextOp{id(6),RichTextDelta{1,{InsertTextStep{id(60),0,"X",ts()}}}},[](const auto& p,auto& c){ReplaceObjectsStatePlanInputs o; return validateEditRichTextState(p,c,&o);});
    check(SetConnectorContentOp{id(7),freeCon()},[](const auto& p,auto& c){ReplaceObjectsStatePlanInputs o; return validateSetConnectorContentState(p,c,&o);});
}
TEST(OperationSpecificStatefulValidation,CON_B07_FreePointOnlyNeedsNoEndpointObjectLookup){auto f=con(20,99);f.content=freeCon();auto a=con(21,2);ReferenceObjectStore b;seed(b,{shape(2),f,a});auto freePoint=SetConnectorContentOp{id(20),freeCon()};auto attached=SetConnectorContentOp{id(21),std::get<ConnectorContent>(a.content)};expectPayloadStructurallyValid(freePoint);expectPayloadStructurallyValid(attached);CountingStore fs(b),as(b);ReplaceObjectsStatePlanInputs fo,ao;ASSERT_TRUE(validateSetConnectorContentState(freePoint,fs,&fo).ok());ASSERT_TRUE(validateSetConnectorContentState(attached,as,&ao).ok());EXPECT_EQ(fs.all_objects_calls,0U);EXPECT_EQ(as.all_objects_calls,0U);EXPECT_GT(as.find_calls,fs.find_calls);}
} }
