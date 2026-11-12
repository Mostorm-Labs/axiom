#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "protobuf_codec_internal.hpp"
#include "auditoryworks/axiom/v1/snapshot.pb.h"
#include <gtest/gtest.h>
#include <fstream>
using namespace canvas::semantic;
namespace {
constexpr std::size_t limit=128U*1024U*1024U;
std::vector<std::uint8_t> read(const std::string& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
std::vector<std::uint8_t> boundary(std::size_t bytes){
 // document_id(20), schema_version(2), recovery_frontier tag+4byte length(5).
 std::vector<std::uint8_t> out{10,18,10,16};out.resize(20);out[19]=1;out.insert(out.end(),{16,1,50});
 std::size_t n=bytes-27;for(unsigned i=0;i<4;++i){out.push_back(std::uint8_t(n&127U)|(i==3?0:128));n>>=7;}out.resize(bytes,1);return out;
}
TEST(CpuSnapshotEnvelope, IndependentWholeSnapshotRangeAndCanonicalEncode){
 for(const auto n:{limit-1,limit,limit+1}){
  auto bytes=boundary(n);
  EXPECT_EQ(internal::preflightDocumentSnapshotV1(bytes),n<=limit?SemanticError::kNone:SemanticError::kLimitExceeded);
  auditoryworks::axiom::v1::DocumentSnapshot wire;wire.mutable_document_id()->set_value(std::string(15,'\0')+'\1');wire.set_schema_version(1);wire.set_recovery_frontier(std::string(n-27,'\1'));
  auto encoded=internal::canonicalEncodeDocumentSnapshot(wire);
  EXPECT_EQ(encoded.error,n<=limit?SemanticError::kNone:SemanticError::kLimitExceeded);
  if(n<=limit)EXPECT_EQ(encoded.bytes,bytes);else EXPECT_TRUE(encoded.bytes.empty());
 }
}
TEST(CpuSnapshotEnvelope, EncodeEnforcesCollectionLimitBeforeWritingOutput){
 auditoryworks::axiom::v1::DocumentSnapshot wire;wire.mutable_document_id()->set_value(std::string(16,'\1'));wire.set_schema_version(1);
 for(unsigned i=0;i<65536;++i)wire.add_objects();
 const auto encoded=internal::canonicalEncodeDocumentSnapshot(wire);
 EXPECT_EQ(encoded.error,SemanticError::kLimitExceeded);EXPECT_TRUE(encoded.bytes.empty());
}
TEST(CpuSnapshotEnvelope, OperationEnvelopeRemains32MiB){
 std::vector<std::uint8_t> op(32U*1024U*1024U);EXPECT_EQ(SemanticCodec::preflightOperationBytes(op),SemanticError::kNone);op.push_back(0);EXPECT_EQ(SemanticCodec::preflightOperationBytes(op),SemanticError::kLimitExceeded);
}
TEST(CpuSnapshotEnvelope, OversizedSourceRejectsBeforeDigestBindingAndPreservesCaller){
 SemanticSnapshot snapshot;snapshot.document_id=DocumentId{ObjectId::fromUint64(71)};snapshot.schema_version=3;snapshot.manifest_present=true;snapshot.manifest.schema_version=1;snapshot.recovery_frontier.resize(limit+1,1);snapshot.document_digest={7};snapshot.checkpoint_digest={9};
 EXPECT_FALSE(SnapshotCodec::bindDigests(snapshot,SnapshotCapability::kAllWithCompactStrokeV1));EXPECT_EQ(snapshot.document_digest,std::vector<std::uint8_t>{7});EXPECT_EQ(snapshot.checkpoint_digest,std::vector<std::uint8_t>{9});
 EXPECT_EQ(SnapshotCodec::encode(snapshot,SnapshotCapability::kAllWithCompactStrokeV1).error,SemanticError::kLimitExceeded);
}
TEST(CpuSnapshotEnvelope, OriginalLongEraseDocumentRoundTripsWithoutChangingWire){
 const std::string root=AXIOM_SOURCE_ROOT;
 const auto recorded=root+"/.aegis/results/G5-CPUW2-history-custody-r1/local-web/G5CWH-PAIRS/candidate7200/operation-0.axsnap";
 if(!std::ifstream(recorded).good())GTEST_SKIP()<<"Historical 7200-segment recording was pruned; regenerate it for evidence reverification.";
 auto decoded=SnapshotCodec::decode(read(root+"/.aegis/results/G5-web-up-attribution-and-witness-r1/local-web/G5WA1-BASELINE-TAIL/growth6/initial.axsnap"),SnapshotCapability::kAllWithCompactStrokeV1);ASSERT_TRUE(decoded.ok());auto snapshot=*decoded.snapshot;
 ReferenceObjectStore store;ResourceManifest manifest;SemanticGenerationState generation;auto state=DocumentRuntimeState::kLoading;
 SnapshotBootstrapper::CheckpointBinding binding{snapshot.document_id,snapshot.document_revision,snapshot.recovery_frontier,snapshot.checkpoint_digest};ASSERT_TRUE(SnapshotBootstrapper::restore(snapshot,SnapshotCapability::kAllWithCompactStrokeV1,state,store,manifest,generation,binding).restored);
 AppliedOperationLedger applied;CanonicalCommitClock clock{RuntimeEpoch{1}};OperationEngine engine;
 for(unsigned i=0;i<2;++i){auto op=SemanticCodec::decodeProtobufOperation(read(root+"/.aegis/results/G5-CPUW2-history-custody-r1/local-web/G5CWH-PAIRS/candidate7200/operation-"+std::to_string(i)+".axsnap"));ASSERT_TRUE(op.ok());ASSERT_EQ(engine.apply(op.operation,ApplySource::kRemoteSync,store,manifest,applied,generation,clock).disposition,ApplyDisposition::kApplied);}
 snapshot.objects=store.allObjects();snapshot.manifest=manifest;snapshot.document_revision+=2;ASSERT_TRUE(SnapshotCodec::populateRequirements(snapshot));ASSERT_TRUE(SnapshotCodec::bindDigests(snapshot,SnapshotCapability::kAllWithCompactStrokeV1));auto encoded=SnapshotCodec::encode(snapshot,SnapshotCapability::kAllWithCompactStrokeV1);ASSERT_TRUE(encoded.ok());EXPECT_EQ(encoded.bytes.size(),35456391U);
 auto round=SnapshotCodec::decode(encoded.bytes,SnapshotCapability::kAllWithCompactStrokeV1);ASSERT_TRUE(round.ok());EXPECT_EQ(round.snapshot->objects,snapshot.objects);EXPECT_EQ(round.snapshot->checkpoint_digest,snapshot.checkpoint_digest);EXPECT_EQ(SnapshotCodec::encode(*round.snapshot,SnapshotCapability::kAllWithCompactStrokeV1).bytes,encoded.bytes);
}
}
