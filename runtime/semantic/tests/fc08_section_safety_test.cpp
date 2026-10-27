#include "fc08_container_fixture.hpp"
#include "canvas/semantic/validator.hpp"
namespace {
using namespace fc08_test;
ObjectId oid(std::uint64_t v){return ObjectId::fromUint64(v);}
ObjectRecord shape(std::uint64_t n,std::uint8_t key){ObjectRecord r;r.id=oid(n);r.kind=ObjectKind::kShape;r.kind_version=1;r.placement.order_key=OrderKey({key});r.content=ShapeContent{1,20,20};return r;}
ObjectRecord section(std::uint64_t n,std::uint8_t key){auto r=shape(n,key);r.kind=ObjectKind::kSection;r.content=SectionContent{{0,0,120,80},"original",4,16};return r;}
ContainerStructuralExpectation node(const ObjectRecord& r){return {r.id,r.kind,r.kind_version,r.placement,r.transform};}
Operation operation(OperationPayload p,std::uint64_t n=100){return {OperationId{oid(n)},DocumentId{oid(200)},1,1,std::move(p)};}
template<class Store> ApplyResult apply(Runtime<Store>& rt,OperationPayload p,std::uint64_t n=100){return rt.engine.apply(operation(std::move(p),n),ApplySource::kLocalInteraction,rt.objects,rt.ledger,rt.generation,rt.clock);}
TEST(SectionSafety,BoundaryEditRequiresExactCompleteGeometryBasis){
 Runtime<IndexedObjectStore> rt;auto parent=shape(1,0x10);parent.kind=ObjectKind::kGroup;parent.content=GroupContent{};parent.transform={0,2,-2,0,30,50};
 auto s=section(2,0x10);s.placement.parent_id=parent.id;auto child=shape(3,0x10);child.placement.parent_id=s.id;child.transform.tx=30;child.transform.ty=20;
 ASSERT_EQ(apply(rt,InsertObjectsOp{{parent,s,child}},90).disposition,ApplyDisposition::kApplied);
 auto expected=std::get<SectionContent>(s.content);auto replacement=expected;replacement.boundary={-20,0,140,80};
 SetSectionContentOp edit{{{s.id,1,expected,replacement}}, {node(s)}};
 const auto before=rt.objects.allObjects();EXPECT_EQ(apply(rt,edit).disposition,ApplyDisposition::kRejected);EXPECT_EQ(rt.objects.allObjects(),before);
 edit.geometry_basis.push_back(node(parent));const auto result=apply(rt,edit);ASSERT_EQ(result.disposition,ApplyDisposition::kApplied);
 EXPECT_EQ(*rt.objects.find(child.id),child);EXPECT_EQ(rt.objects.find(s.id)->transform,s.transform);EXPECT_EQ(rt.objects.find(s.id)->placement,s.placement);
 ASSERT_TRUE(result.commit_record);ASSERT_EQ(result.commit_record->change_set.objects().size(),1U);EXPECT_EQ(result.commit_record->change_set.objects()[0].flags,SemanticChangeFlags::kContent);
 // Reusing an id with a changed canonical payload is collision, not a rebase.
 edit.items[0].replacement_content.title="collision";EXPECT_EQ(apply(rt,edit).error.issue,StatefulIssue::kOperationIdCollision);
}
TEST(SectionSafety,TitleEditDoesNotDemandUnrelatedAncestorBasis){
 Runtime<ReferenceObjectStore> rt;const auto s=section(1,0x10);ASSERT_EQ(apply(rt,InsertObjectsOp{{s}},90).disposition,ApplyDisposition::kApplied);
 auto expected=std::get<SectionContent>(s.content);auto next=expected;next.title="renamed";SetSectionContentOp edit{{{s.id,1,expected,next}}, {}};
 EXPECT_EQ(apply(rt,edit).disposition,ApplyDisposition::kApplied);EXPECT_EQ(std::get<SectionContent>(rt.objects.find(s.id)->content).title,"renamed");
 edit.items[0].replacement_content.title="lost update";EXPECT_EQ(apply(rt,edit,101).disposition,ApplyDisposition::kRejected);
 EXPECT_EQ(std::get<SectionContent>(rt.objects.find(s.id)->content).title,"renamed");
}
TEST(SectionSafety,FinalGraphRejectsCycleAndOrderCollisionWithoutPublishing){
 Runtime<IndexedObjectStore> rt;auto a=section(1,0x10),b=section(2,0x20);ASSERT_EQ(apply(rt,InsertObjectsOp{{a,b}},90).disposition,ApplyDisposition::kApplied);
 EditContainerTopologyOp move;move.effect=RelocateObjectsEffect{};move.guard.nodes={node(a),node(b)};move.guard.child_sets={{{},{{a.id,a.placement.order_key},{b.id,b.placement.order_key}}},{a.id,{}},{b.id,{}}};
 move.relocations={{a.id,{b.id,OrderKey({0x10})},a.transform},{b.id,{a.id,OrderKey({0x10})},b.transform}};
 const auto before=rt.objects.allObjects();EXPECT_EQ(apply(rt,move).disposition,ApplyDisposition::kRejected);EXPECT_EQ(rt.objects.allObjects(),before);
 move.relocations={{a.id,{{},b.placement.order_key},a.transform}};EXPECT_EQ(apply(rt,move).disposition,ApplyDisposition::kRejected);EXPECT_EQ(rt.objects.allObjects(),before);
 EXPECT_EQ(rt.clock.lastCommittedOrdinal().value(),1U);EXPECT_FALSE(rt.ledger.find(operation(move).id));
}
TEST(SectionSafety,Operation17EnforcesKeyedBatchLimit){
 const auto c=std::get<SectionContent>(section(1,0x10).content);SetSectionContentOp value;
 value.items.reserve(65536);for(std::uint64_t n=1;n<=65535;++n)value.items.push_back({oid(n),1,c,c});
 auto normalized=normalizeOperation(operation(value));ASSERT_TRUE(normalized.ok());EXPECT_TRUE(validatePayloadStructure(normalized.value).ok());
 value.items.push_back({oid(65536),1,c,c});EXPECT_FALSE(normalizeOperation(operation(std::move(value))).ok());
}
TEST(SectionSafety,FullEnvelopeOver32MiBRejectsBeforeAnyPublication){
 auto c=std::get<SectionContent>(section(1,0x10).content);c.title.assign(512U*1024U,'a');SetSectionContentOp payload;
 for(std::uint64_t n=1;n<=32;++n)payload.items.push_back({oid(n),1,c,c});
 const auto op=operation(payload);EXPECT_EQ(SemanticCodec::encodeProtobufOperation(op).error,SemanticError::kLimitExceeded);
 Runtime<ReferenceObjectStore> rt;EXPECT_EQ(rt.engine.apply(op,ApplySource::kLocalInteraction,rt.objects,rt.ledger,rt.generation,rt.clock).disposition,ApplyDisposition::kRejected);
 EXPECT_EQ(rt.objects.size(),0U);EXPECT_EQ(rt.generation.current().value(),0U);EXPECT_EQ(rt.clock.lastCommittedOrdinal().value(),0U);EXPECT_FALSE(rt.ledger.find(op.id));
}
TEST(SectionSafety,UnknownEnvelopeVersionAndMissingEffectReject){
 auto op=operation(EditContainerTopologyOp{});op.payload_version=2;Runtime<ReferenceObjectStore> rt;
 EXPECT_EQ(rt.engine.apply(op,ApplySource::kLocalInteraction,rt.objects,rt.ledger,rt.generation,rt.clock).disposition,ApplyDisposition::kRejected);
 op.payload_version=1;EXPECT_EQ(rt.engine.apply(op,ApplySource::kLocalInteraction,rt.objects,rt.ledger,rt.generation,rt.clock).disposition,ApplyDisposition::kRejected);
 EXPECT_EQ(rt.objects.size(),0U);EXPECT_FALSE(isKnownOperationKind(static_cast<OperationKind>(19)));
}
TEST(SectionSafety,OrderOnlyEditDoesNotPublishTransformImpact){
 Runtime<ReferenceObjectStore> rt;auto a=shape(1,0x10),b=shape(2,0x20),c=shape(3,0x30);
 ASSERT_EQ(apply(rt,InsertObjectsOp{{a,b,c}},90).disposition,ApplyDisposition::kApplied);
 EditContainerTopologyOp op;op.effect=RelocateObjectsEffect{};op.guard.nodes={node(a),node(b)};
 op.guard.child_sets={{{},{{a.id,a.placement.order_key},{b.id,b.placement.order_key},{c.id,c.placement.order_key}}}};
 auto moved=a.transform;moved.tx=25;op.relocations={{a.id,a.placement,moved}};op.order_only_edits={{b.id,OrderKey({0x25})}};
 const auto result=apply(rt,op);ASSERT_EQ(result.disposition,ApplyDisposition::kApplied);ASSERT_TRUE(result.commit_record);
 const auto changes=result.commit_record->change_set.objects();ASSERT_EQ(changes.size(),2U);
 EXPECT_EQ(changes[0].flags,SemanticChangeFlags::kPlacement|SemanticChangeFlags::kTransform);
 EXPECT_EQ(changes[1].flags,SemanticChangeFlags::kPlacement);
 EXPECT_EQ(rt.objects.find(b.id)->transform,b.transform);
}
} // namespace
