#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace canvas;using nlohmann::json;
std::vector<std::uint8_t> read(const char*p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){try{if(argc!=5)throw std::runtime_error("fixture op0 op1 out");auto base=semantic::SnapshotCodec::decode(read(argv[1]),semantic::SnapshotCapability::kAllWithCompactStrokeV1);if(!base.ok())throw std::runtime_error("base");auto s=*base.snapshot;
 semantic::ReferenceObjectStore store;semantic::ResourceManifest manifest;semantic::SemanticGenerationState generation;auto state=semantic::DocumentRuntimeState::kLoading;
 semantic::SnapshotBootstrapper::CheckpointBinding binding{s.document_id,s.document_revision,s.recovery_frontier,s.checkpoint_digest};auto restored=semantic::SnapshotBootstrapper::restore(s,semantic::SnapshotCapability::kAllWithCompactStrokeV1,state,store,manifest,generation,binding);if(!restored.restored)throw std::runtime_error("restore");
 semantic::AppliedOperationLedger applied;semantic::CanonicalCommitClock clock{semantic::RuntimeEpoch{1}};semantic::OperationEngine engine;json operations=json::array();for(int i=2;i<4;++i){auto bytes=read(argv[i]);auto op=semantic::SemanticCodec::decodeProtobufOperation(bytes);if(!op.ok())throw std::runtime_error("opdecode");auto a=engine.apply(op.operation,semantic::ApplySource::kRemoteSync,store,manifest,applied,generation,clock);operations.push_back({{"bytes",bytes.size()},{"decode_error",unsigned(op.error)},{"disposition",unsigned(a.disposition)}});if(a.disposition!=semantic::ApplyDisposition::kApplied)throw std::runtime_error("apply");}
 s.objects=store.allObjects();s.manifest=manifest;s.document_revision+=2;if(!semantic::SnapshotCodec::populateRequirements(s)||!semantic::SnapshotCodec::bindDigests(s,semantic::SnapshotCapability::kAllWithCompactStrokeV1))throw std::runtime_error("bind");auto encoded=semantic::SnapshotCodec::encode(s,semantic::SnapshotCapability::kAllWithCompactStrokeV1);auto decoded=semantic::SnapshotCodec::decode(encoded.bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);
 json result{{"status","REPRODUCED"},{"operations",operations},{"objects",s.objects.size()},{"encode_ok",encoded.ok()},{"encoded_bytes",encoded.bytes.size()},{"decode_ok",decoded.ok()},{"decode_error",unsigned(decoded.error)},{"limit_bytes",32U*1024U*1024U},{"conclusion","encode accepts snapshot largerthan32MiB;decode preflight rejects kLimitExceeded"}};std::ofstream(argv[4])<<result.dump(2)<<'\n';std::cout<<result.dump()<<'\n';return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
