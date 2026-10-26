// Semantic-only test host. No Interaction, Composition, History or test friend.
#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/replay.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {
using namespace canvas::semantic;
std::string hex(const std::vector<std::uint8_t>& bytes) {
  constexpr char digits[]="0123456789abcdef";
  std::string out;
  for (auto b:bytes) {out+=digits[b>>4];out+=digits[b&15];}
  return out;
}
}
int main(int argc,char** argv) {
  using namespace canvas::semantic;
  if(argc==2 && std::string(argv[1])=="--discover") {
    std::cout<<"{\"roots\":[\"Replay\",\"LegacyImport\",\"Golden\"],\"History_owner\":false}\n";return 0;
  }
  if(argc!=4) return 2;
  const std::string mode=argv[1],root=argv[2];
  std::ifstream file(argv[3],std::ios::binary);
  if(!file)return 3;
  const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
  ReferenceObjectStore objects;
  ResourceManifest manifest;
  AppliedOperationLedger ledger;
  SemanticGenerationState generation;
  CanonicalCommitClock clock(RuntimeEpoch(17));
  DocumentRuntimeState state=DocumentRuntimeState::kLoading;
  bool ok=false;
  std::cout<<"{\"mode\":\""<<mode<<"\",\"History_owner\":false";
  if(mode=="Replay") {
    const auto decoded=SemanticCodec::decodeProtobufOperation(bytes);
    std::cout<<",\"decoded\":"<<(decoded.ok()?"true":"false");
    if(decoded.ok()) {
      const std::vector<Operation> sequence{decoded.operation,decoded.operation};
      const auto replay=ReplayCoordinator::replayAndFinalize(sequence,state,objects,ledger,generation,clock);
      ok=replay.ready;
      std::cout<<",\"applied\":"<<replay.applied<<",\"already_applied\":"<<replay.already_applied
               <<",\"ledger_entry_present\":"<<(ledger.find(decoded.operation.id)?"true":"false");
    }
  } else if(mode=="LegacyImport") {
    const auto decoded=SnapshotCodec::decode(bytes);
    std::cout<<",\"decoded\":"<<(decoded.ok()?"true":"false");
    if(decoded.ok()) {
      const auto& s=*decoded.snapshot;
      const auto restored=s.schema_version<3?
        SnapshotBootstrapper::restore(s,state,objects,generation):
        SnapshotBootstrapper::restore(s,SnapshotCapability::kLegacyV1,state,objects,manifest,generation,
          {s.document_id,s.document_revision,s.recovery_frontier,s.checkpoint_digest});
      // Import explicitly restores a checkpoint; empty continuation is the
      // production replay coordinator's Ready transition.
      if(restored.restored)ok=ReplayCoordinator::replayAndFinalize({},state,objects,ledger,generation,clock).ready;
      std::cout<<",\"restored\":"<<(restored.restored?"true":"false")<<",\"schema\":"<<s.schema_version;
    }
  } else if(mode=="Golden" || mode=="Ingress") {
    const auto observed=SemanticCodec::observeGoldenFixture(root,bytes,true);
    ok=observed.accepted;
    std::cout<<",\"category\":\""<<observed.category<<"\",\"input_bytes\":"<<bytes.size();
    if(mode=="Golden")std::cout<<",\"canonical_hex\":\""<<hex(observed.canonical_bytes)<<"\"";
    if(root=="Operation")std::cout<<",\"normal_ingress_ok\":"<<(SemanticCodec::decodeProtobufOperation(bytes).ok()?"true":"false");
    if(root=="DocumentSnapshot")std::cout<<",\"normal_ingress_ok\":"<<(SnapshotCodec::decode(bytes).ok()?"true":"false");
  } else return 2;
  std::cout<<",\"ok\":"<<(ok?"true":"false")<<",\"state\":"<<static_cast<unsigned>(state)
           <<",\"objects\":"<<objects.size()<<",\"generation\":"<<generation.current().value()
           <<",\"ordinal\":"<<clock.lastCommittedOrdinal().value()<<",\"records\":[";
  bool comma=false;
  for(const auto& object:objects.allObjects()) {
    if(comma)std::cout<<',';
    comma=true;
    // Observe records by the canonical insert carrier, never expected bytes.
    const Operation carrier{OperationId{ObjectId::fromUint64(9000)},DocumentId{ObjectId::fromUint64(100)},
      1,1,InsertObjectsOp{{object}}};
    std::cout<<'"'<<hex(SemanticCodec::encodeProtobufOperation(carrier).bytes)<<'"';
  }
  std::cout<<"]}\n";
  return 0;
}
