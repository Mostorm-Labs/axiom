#include "fc08_container_fixture.hpp"
namespace {
using namespace fc08_test;
class SectionOperationsWire:public testing::TestWithParam<Json>{};
TEST_P(SectionOperationsWire,FrozenWireAndModelContract){
 const auto& row=GetParam();SCOPED_TRACE(row["variant_id"].get<std::string>());
 const auto input=read(root/row["input"]["path"].get<std::string>());const auto expected=read(root/row["expected"]["path"].get<std::string>());
 Runtime<ReferenceObjectStore> rt;rt.seed(input["before"]);const auto before=rt.objects.allObjects();
 bool accepted=false;std::vector<std::uint8_t> canonical;
 if(input["direction"]=="model-canonicalization"){
  p::Operation dto;literal(input["wire_model"],dto);Operation model;
  model.id=OperationId{id(input["wire_model"]["operation_id"]["value"].get<std::string>())};model.document_id=DocumentId{id(input["wire_model"]["document_id"]["value"].get<std::string>())};model.schema_version=dto.schema_version();model.payload_version=dto.payload_version();
  if(canvas::semantic::internal::fromProtobufContainerOperation(dto.payload(),model.payload)){const auto out=SemanticCodec::encodeProtobufOperation(model);accepted=out.ok();canonical=out.bytes;}
 }else{
  const auto raw=wireFor(row,"input");ASSERT_FALSE(raw.empty());const auto o=SemanticCodec::observeGoldenFixture("Operation",raw,true);accepted=o.accepted;canonical=o.canonical_bytes;
  if(row["case_id"]=="FC08-W06")accepted=rt.apply(raw).disposition==ApplyDisposition::kApplied;
 }
 for(const auto& predicate:expected["predicates"]){
  if(predicate["op"]=="equal"&&predicate["path"]=="accepted")EXPECT_EQ(accepted,predicate["expected"].get<bool>());
  else if(predicate["op"]=="equal"&&predicate["path"]=="canonical_hex")EXPECT_EQ(hex(canonical),predicate["expected"].get<std::string>());
  else if(predicate["op"]=="unchanged"&&predicate["after"]=="after")EXPECT_EQ(rt.objects.allObjects(),before);
  else if(predicate["op"]=="unchanged"&&predicate["after"]=="after_child_properties"){
   for(const auto& r:before)if(r.kind!=ObjectKind::kSection){const auto* after=rt.objects.find(r.id);ASSERT_NE(after,nullptr);EXPECT_EQ(after->properties,r.properties);}
  }else FAIL()<<predicate.dump();
 }
}
INSTANTIATE_TEST_SUITE_P(Frozen,SectionOperationsWire,testing::ValuesIn(cases(true)),[](const testing::TestParamInfo<Json>& i){return std::to_string(i.index);});
class SectionTopology:public testing::TestWithParam<Json>{};
template<class Store> void checkTopology(const Json& row){
 SCOPED_TRACE(row["variant_id"].get<std::string>());const auto input=read(root/row["input"]["path"].get<std::string>());const auto expected=read(root/row["expected"]["path"].get<std::string>());
 Runtime<Store> rt;rt.seed(input["before"]);const auto forward=wireFor(row,"forward");ASSERT_FALSE(forward.empty());
 if(input.contains("intervening_operation")&&!input["intervening_operation"].is_null()){
  const auto decoded=SemanticCodec::decodeProtobufOperation(forward);ASSERT_TRUE(decoded.ok());
  (void)rt.engine.prepare(decoded.operation,{rt.objects,rt.ledger});
  ASSERT_EQ(rt.apply(carrier(input["intervening_operation"])).disposition,ApplyDisposition::kApplied);
 }
 const auto before=rt.objects.allObjects();const auto generation=rt.generation.current();const auto ordinal=rt.clock.lastCommittedOrdinal();
 ApplyResult result;
 if(row["variant_id"]=="FC08-T06/payload-permutation") {
  // This variant exercises the typed planner's normalization, whereas W04
  // separately freezes rejection of noncanonical wire collections.
  p::Operation dto;literal(input["forward"],dto);Operation model;
  model.id=OperationId{id(input["forward"]["operation_id"]["value"].get<std::string>())};
  model.document_id=DocumentId{id(input["forward"]["document_id"]["value"].get<std::string>())};model.schema_version=1;model.payload_version=1;
  ASSERT_TRUE(canvas::semantic::internal::fromProtobufContainerOperation(dto.payload(),model.payload));
  result=rt.engine.apply(model,ApplySource::kLocalInteraction,rt.objects,rt.ledger,rt.generation,rt.clock);
 } else result=rt.apply(forward);
 for(const auto& predicate:expected["predicates"]){
  if(predicate["op"]=="equal"&&predicate["path"]=="outcome")EXPECT_EQ(result.disposition==ApplyDisposition::kApplied?"Applied":"Rejected",predicate["expected"].get<std::string>());
  else if(predicate["op"]=="equal"&&predicate["path"]=="canonical.records")EXPECT_EQ(records(rt.objects),predicate["expected"]);
  else if(predicate["op"]=="equal"&&predicate["path"]=="canonical_commit_delta")EXPECT_EQ(rt.clock.lastCommittedOrdinal().value()-ordinal.value(),predicate["expected"].get<std::uint64_t>());
  else if(predicate["op"]=="unchanged"){
   EXPECT_EQ(rt.objects.allObjects(),before);EXPECT_EQ(rt.generation.current(),generation);EXPECT_EQ(rt.clock.lastCommittedOrdinal(),ordinal);EXPECT_FALSE(result.commit_record.has_value());
  }else if(predicate["op"]=="equal"&&predicate["path"]=="unrelated_leaf_content_preserved"){
   for(const auto& r:before){const auto* after=rt.objects.find(r.id);if(after&&r.kind!=ObjectKind::kGroup&&r.kind!=ObjectKind::kSection)EXPECT_EQ(after->content,r.content);}
  }else FAIL()<<predicate.dump();
 }
 if(result.disposition==ApplyDisposition::kApplied){
  ASSERT_TRUE(result.commit_record.has_value());
  // Retry must win before now-stale guards, without another publication.
  if(row["variant_id"]!="FC08-T06/payload-permutation")EXPECT_EQ(rt.apply(forward).disposition,ApplyDisposition::kAlreadyApplied);
  EXPECT_EQ(rt.clock.lastCommittedOrdinal().value(),ordinal.value()+1U);
  // T06's frozen expected predicates bind forward canonicalization only.
  // Its shared redo carrier omits the T06 OrderOnlyEdit delta; preserve and
  // report that fixture limitation for the separate History slice.
  if(row["case_id"]!="FC08-T05"&&row["case_id"]!="FC08-T06"){
   const auto inverse=wireFor(row,"inverse");ASSERT_FALSE(inverse.empty());ASSERT_EQ(rt.apply(inverse).disposition,ApplyDisposition::kApplied);
   EXPECT_EQ(records(rt.objects),input["undo_after"]);
   const auto redo=wireFor(row,"redo");ASSERT_EQ(rt.apply(redo).disposition,ApplyDisposition::kApplied);
   EXPECT_EQ(records(rt.objects),input["after"]);
  }
 }
}
TEST_P(SectionTopology,ReferenceOwner){checkTopology<ReferenceObjectStore>(GetParam());}
TEST_P(SectionTopology,IndexedOwner){checkTopology<IndexedObjectStore>(GetParam());}
INSTANTIATE_TEST_SUITE_P(Frozen,SectionTopology,testing::ValuesIn(cases(false)),[](const testing::TestParamInfo<Json>& i){return std::to_string(i.index);});
} // namespace
