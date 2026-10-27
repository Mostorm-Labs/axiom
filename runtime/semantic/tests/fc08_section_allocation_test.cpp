#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "object_store_mutator.hpp"
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstdio>
#include <new>
namespace {
thread_local std::ptrdiff_t fail_after=-1;
}
// This isolated test executable fails real allocations once. Qualification hooks
// are neither enabled nor modified, and no production failure adapter is used.
void* operator new(std::size_t size){
 if(fail_after==0){fail_after=-1;throw std::bad_alloc();}
 if(fail_after>0)--fail_after;
 if(void* p=std::malloc(size?size:1))return p;
 throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
namespace {
using namespace canvas::semantic;
ObjectRecord leaf(){ObjectRecord r;r.id=ObjectId::fromUint64(1);r.kind=ObjectKind::kShape;r.kind_version=1;r.placement.order_key=OrderKey({0x10});r.content=ShapeContent{1,20,20};return r;}
ObjectRecord wrapper(int scenario=0){auto r=leaf();r.id=ObjectId::fromUint64(2);r.kind=ObjectKind::kSection;r.placement.order_key=OrderKey({0x20});r.content=SectionContent{{0,0,100,80},"allocation",4,16};
 if(scenario==1)r.properties.entries={{0x100U,PropertyValue{FillStyleValue{SolidFill{{0.2F,0.3F,0.4F,1.0F}}}}}};
 if(scenario==2){r.kind=ObjectKind::kGroup;r.content=GroupContent{};r.properties.entries={{1U,PropertyValue{false}}};}
 return r;}
Operation command(int scenario=0){
 if(scenario==3){const auto r=wrapper();const auto before=std::get<SectionContent>(r.content);auto after=before;after.title="renamed";return {OperationId{ObjectId::fromUint64(100)},DocumentId{ObjectId::fromUint64(200)},1,1,SetSectionContentOp{{{r.id,1,before,after}},{}}};}
 EditContainerTopologyOp op;const auto a=leaf();op.guard.nodes={{a.id,a.kind,a.kind_version,a.placement,a.transform}};op.guard.child_sets={{{},{{a.id,a.placement.order_key}}}};op.relocations={{a.id,Placement{wrapper().id,OrderKey({0x10})},a.transform}};op.effect=IntroduceWrappersEffect{{wrapper(scenario)}};return {OperationId{ObjectId::fromUint64(100)},DocumentId{ObjectId::fromUint64(200)},1,1,op};}
template<class Store> void sweep(int scenario=0){
 // Runtime/descriptor initialization is a fixture prerequisite, outside this
 // precommit admission sweep. All command allocations follow the real path.
 ASSERT_TRUE(SemanticCodec::encodeProtobufOperation(command(scenario)).ok());
 std::size_t rejected=0;bool succeeded=false;
 for(std::ptrdiff_t failure=0;failure<4096;++failure){
  Store objects;ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(objects,scenario==3?wrapper():leaf()));
  AppliedOperationLedger ledger;SemanticGenerationState generation;CanonicalCommitClock clock(RuntimeEpoch(4));OperationEngine engine;const auto operation=command(scenario);const auto before=objects.allObjects();
  fail_after=failure;const auto result=engine.apply(operation,ApplySource::kLocalInteraction,objects,ledger,generation,clock);fail_after=-1;
  if(result.disposition==ApplyDisposition::kApplied){succeeded=true;EXPECT_EQ(objects.size(),scenario==3?1U:2U);EXPECT_TRUE(ledger.find(operation.id));EXPECT_EQ(generation.current().value(),1U);EXPECT_EQ(clock.lastCommittedOrdinal().value(),1U);break;}
  ++rejected;EXPECT_EQ(result.disposition,ApplyDisposition::kRejected);EXPECT_EQ(objects.allObjects(),before);EXPECT_FALSE(ledger.find(operation.id));EXPECT_EQ(generation.current().value(),0U);EXPECT_EQ(clock.lastCommittedOrdinal().value(),0U);EXPECT_FALSE(result.commit_record);
  if constexpr(std::is_same_v<Store,IndexedObjectStore>)EXPECT_TRUE(internal::ObjectStoreMutator::indexMatchesRebuild(objects));
 }
 std::fprintf(stdout,"post-initialization operator-new failures rejected: %zu\n",rejected);
 EXPECT_TRUE(succeeded);EXPECT_GT(rejected,20U);
}
TEST(SectionAllocation,ReferenceOwnerRejectsPostInitializationAllocationFailures){sweep<ReferenceObjectStore>();}
TEST(SectionAllocation,IndexedOwnerRejectsPostInitializationAllocationFailures){sweep<IndexedObjectStore>();}
TEST(SectionAllocation,ReferenceOwnerWithSectionFill){sweep<ReferenceObjectStore>(1);}
TEST(SectionAllocation,IndexedOwnerWithSectionFill){sweep<IndexedObjectStore>(1);}
TEST(SectionAllocation,ReferenceOwnerWithGroupWrapper){sweep<ReferenceObjectStore>(2);}
TEST(SectionAllocation,IndexedOwnerWithGroupWrapper){sweep<IndexedObjectStore>(2);}
TEST(SectionAllocation,ReferenceOwnerWithSetSectionContent){sweep<ReferenceObjectStore>(3);}
TEST(SectionAllocation,IndexedOwnerWithSetSectionContent){sweep<IndexedObjectStore>(3);}
} // namespace
