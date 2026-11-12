#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include "protobuf_codec_internal.hpp"
using namespace canvas::semantic;
TEST(CpuErasePreparation, StagesTargetsWithoutPublishingAndRejectsChangedBasis){
 IndexedObjectStore objects;AppliedOperationLedger ledger;SemanticGenerationState generation;CanonicalCommitClock clock{RuntimeEpoch{1}};OperationEngine engine;
 ObjectRecord object;object.id=ObjectId::fromUint64(7);object.kind=ObjectKind::kVectorStroke;object.kind_version=3;object.placement.order_key=OrderKey{{128}};CompactStrokeContent compact;compact.stroke.config=canvas::foundation::InkBrushConfig{};compact.stroke.knots={{1,1,32768},{4,4,32768}};object.content=compact;
 Operation insert{OperationId{ObjectId::fromUint64(8)},DocumentId{ObjectId::fromUint64(1)},1,3,AddStrokeOp{object}};
 ASSERT_EQ(engine.apply(insert,ApplySource::kLocalInteraction,objects,ledger,generation,clock).disposition,ApplyDisposition::kApplied);
 SweptCircleMask sweep;sweep.segments={{{{1,1},2},{{4,4},2},{2,2},{3,3}}};
 Operation erase{OperationId{ObjectId::fromUint64(9)},insert.document_id,1,1,AddEraseMasksOp{{{object.id,{{ObjectId::fromUint64(11),sweep}}}}}};
 auto prepared=engine.prepareErase(erase,objects,ledger,generation,clock);ASSERT_TRUE(prepared);
 while(!prepared->ready())ASSERT_TRUE(prepared->advance(1,8000));EXPECT_EQ(std::vector<std::uint8_t>(prepared->encoded().begin(),prepared->encoded().end()),SemanticCodec::encodeProtobufOperation(erase).bytes);EXPECT_TRUE(objects.find(object.id)->erase_masks.empty());
 auto applied=engine.commitErase(*prepared,ApplySource::kLocalInteraction,generation,clock);ASSERT_EQ(applied.disposition,ApplyDisposition::kApplied);EXPECT_EQ(objects.find(object.id)->erase_masks.size(),1);EXPECT_EQ(engine.commitErase(*prepared,ApplySource::kLocalInteraction,generation,clock).disposition,ApplyDisposition::kRejected);
 auto next=erase;next.id=OperationId{ObjectId::fromUint64(12)};std::get<AddEraseMasksOp>(next.payload).items[0].masks[0].id=ObjectId::fromUint64(13);auto stale=engine.prepareErase(next,objects,ledger,generation,clock);ASSERT_TRUE(stale);while(!stale->ready())ASSERT_TRUE(stale->advance(1,8000));
 Operation transform{OperationId{ObjectId::fromUint64(14)},insert.document_id,1,1,SetTransformsOp{{{object.id,{1,0,0,1,9,9}}}}};ASSERT_EQ(engine.apply(transform,ApplySource::kLocalInteraction,objects,ledger,generation,clock).disposition,ApplyDisposition::kApplied);EXPECT_EQ(engine.commitErase(*stale,ApplySource::kLocalInteraction,generation,clock).disposition,ApplyDisposition::kRejected);EXPECT_EQ(objects.find(object.id)->erase_masks.size(),1);
}

TEST(CpuErasePreparation, StagedWireMatchesCanonicalForSavedLongOperation){
 const auto saved=std::string(AXIOM_SOURCE_ROOT)+"/.aegis/results/G5-CPUW2-history-custody-r1/local-web/G5CWH-PAIRS/candidate7200/operation-0.axsnap";
 if(!std::ifstream(saved).good())GTEST_SKIP()<<"Historical long-operation recording was pruned; regenerate it for evidence reverification.";
 const std::string file=std::string(AXIOM_SOURCE_ROOT)+"/.aegis/results/G5-CPUW2-history-custody-r1/local-web/G5CWH-PAIRS/candidate7200/operation-0.axsnap";std::ifstream stream(file,std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream),{}};auto decoded=SemanticCodec::decodeProtobufOperation(bytes);ASSERT_TRUE(decoded.ok());std::vector<std::vector<std::uint8_t>> items;const auto& payload=std::get<AddEraseMasksOp>(decoded.operation.payload);for(std::size_t i=0;i<payload.items.size();++i){auto item=internal::encodePreparedEraseItem(decoded.operation,i);ASSERT_TRUE(item.ok());items.push_back(std::move(item.bytes));}auto encoded=internal::framePreparedEraseOperation(decoded.operation,items);ASSERT_TRUE(encoded.ok());EXPECT_EQ(encoded.bytes,bytes);
}
