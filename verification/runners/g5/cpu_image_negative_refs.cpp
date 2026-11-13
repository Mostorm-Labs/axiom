// Verification-only canonical wrong-state references via original Operations.
#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/codec.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace canvas::semantic;
int main(int argc,char** argv)try{
 if(argc!=3)return 2;std::ifstream f(argv[1],std::ios::binary);std::vector<std::uint8_t>b{std::istreambuf_iterator<char>(f),{}};auto decoded=SnapshotCodec::decode(b,SnapshotCapability::kAllWithCompactStrokeV1);if(!decoded.ok())return 3;const auto original=*decoded.snapshot;nlohmann::json results=nlohmann::json::array();
 for(const std::string name:{"pose1px","crop1texel","opacity"}){
  IndexedObjectStore store;ResourceManifest manifest;SemanticGenerationState generation;auto phase=DocumentRuntimeState::kLoading;if(!SnapshotBootstrapper::restore(original,SnapshotCapability::kAllWithCompactStrokeV1,phase,store,manifest,generation,{original.document_id,original.document_revision,original.recovery_frontier,original.checkpoint_digest}).restored)return 4;
  const auto object=std::find_if(original.objects.begin(),original.objects.end(),[](const auto& o){return o.kind==ObjectKind::kImage;});if(object==original.objects.end())return 5;
  Operation operation{OperationId{ObjectId::fromUint64(0x7fffffb1)},original.document_id,1,1,SetTransformsOp{}};
  if(name=="pose1px"){auto pose=object->transform;pose.tx+=1;operation.payload=SetTransformsOp{{{object->id,pose}}};}
  else if(name=="crop1texel"){auto image=std::get<ImageContent>(object->content);image.source_rect=NormalizedRect{1./880,0,879./880,1};operation.payload=SetImageContentOp{object->id,image};}
  else operation.payload=PatchPropertiesOp{{{object->id,3,PropertyPatchAction::kSet,PropertyValue{.5F}}}};
  AppliedOperationLedger ledger;CanonicalCommitClock clock{RuntimeEpoch{1}};const auto applied=OperationEngine{}.apply(operation,ApplySource::kRemoteSync,store,manifest,ledger,generation,clock);if(applied.disposition!=ApplyDisposition::kApplied)return 6;
  auto saved=original;saved.objects=store.allObjects();saved.document_revision+=1;if(!SnapshotCodec::bindDigests(saved,SnapshotCapability::kAllWithCompactStrokeV1))return 7;const auto bytes=SnapshotCodec::encode(saved,SnapshotCapability::kAllWithCompactStrokeV1),op=SemanticCodec::encodeProtobufOperation(operation);if(!bytes.ok()||!op.ok())return 8;
  const auto write=[](const std::string&p,const auto& bytes){std::ofstream out(p,std::ios::binary);out.write((const char*)bytes.data(),bytes.size());};write(std::string(argv[2])+"-"+name+".axsnap",bytes.bytes);write(std::string(argv[2])+"-"+name+".op",op.bytes);results.push_back({{"name",name},{"generation",generation.current().value()},{"bytes",bytes.bytes.size()},{"object_count",store.size()}});
 }
 std::cout<<results.dump(2)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
