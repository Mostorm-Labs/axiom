#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "object_store_mutator.hpp"
#include "protobuf_codec_internal.hpp"
#include <gtest/gtest.h>
#include <cstdlib>
#include <new>
namespace {thread_local std::ptrdiff_t failAfter=-1;}
void* operator new(std::size_t n){if(failAfter==0){failAfter=-1;throw std::bad_alloc();}if(failAfter>0)--failAfter;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using namespace canvas::semantic;
TEST(CpuEraseAtomicAllocation, EveryPrepareAllocationFailurePreservesCanonicalBeforeAtomicPublish){
 bool success=false;unsigned failures=0;
 for(std::ptrdiff_t fault=0;fault<2048;++fault){
  IndexedObjectStore objects;AppliedOperationLedger ledger;SemanticGenerationState generation;CanonicalCommitClock clock{RuntimeEpoch{1}};OperationEngine engine;
  ObjectRecord object;object.id=ObjectId::fromUint64(7);object.kind=ObjectKind::kVectorStroke;object.kind_version=3;object.placement.order_key=OrderKey{{128}};CompactStrokeContent compact;compact.stroke.config=canvas::foundation::InkBrushConfig{};compact.stroke.knots={{1,1,32768},{4,4,32768}};object.content=compact;
  ASSERT_TRUE(internal::ObjectStoreMutator::insertFresh(objects,object));const auto before=objects.allObjects();SweptCircleMask sweep;sweep.segments={{{{1,1},2},{{4,4},2},{2,2},{3,3}}};
  Operation operation{OperationId{ObjectId::fromUint64(9)},DocumentId{ObjectId::fromUint64(1)},1,1,AddEraseMasksOp{{{object.id,{{ObjectId::fromUint64(11),sweep}}}}}};
  if(fault==0){ASSERT_TRUE(SemanticCodec::encodeProtobufOperation(operation).ok());auto item=internal::encodePreparedEraseItem(operation,0);ASSERT_TRUE(item.ok());std::vector<std::vector<std::uint8_t>> items;items.push_back(std::move(item.bytes));ASSERT_TRUE(internal::framePreparedEraseOperation(operation,items).ok());}
  failAfter=fault;auto prepared=engine.prepareErase(std::move(operation),objects,ledger,generation,clock);bool valid=bool(prepared);unsigned turns=0;while(valid&&!prepared->ready()&&turns++<32)valid=prepared->advance(1,8000);failAfter=-1;
  EXPECT_EQ(objects.allObjects(),before);EXPECT_FALSE(ledger.find(OperationId{ObjectId::fromUint64(9)}));EXPECT_EQ(generation.current().value(),0U);EXPECT_EQ(clock.lastCommittedOrdinal().value(),0U);
  if(valid&&prepared->ready()){const auto applied=engine.commitErase(*prepared,ApplySource::kLocalInteraction,generation,clock);ASSERT_EQ(applied.disposition,ApplyDisposition::kApplied);EXPECT_EQ(objects.find(object.id)->erase_masks.size(),1U);EXPECT_EQ(generation.current().value(),1U);success=true;break;}++failures;
 }
 EXPECT_TRUE(success);EXPECT_GT(failures,20U);
}
