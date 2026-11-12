#pragma once
#include "canvas/interaction/internal/cpu_pending_erase.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/scene/scene_types.hpp"
#include "canvas/semantic/operation.hpp"
#include <array>
#include "pending_scene_preparation.hpp"
namespace canvas::interaction {
struct CpuPendingEraseState final {
 enum class Kind { Erase,Create,Transform,Brush,Undo,Redo,Image };
 enum class Phase { Active,Frozen,Preparing,Waiting,Confirming,ExactReady,Ready,CommitPreparing };
 struct Intent {
  Kind kind=Kind::Erase;
  std::uint64_t activeSerial=0;
  std::optional<resources::ResourceLease> prefixMetadata;
  std::vector<semantic::ObjectRecord> creates;
  semantic::SetTransformsOp transforms;
  std::vector<std::pair<foundation::ObjectId,semantic::Transform2D>> initialPoses;
  std::vector<RuntimeSceneRecord> transformBasis;
  std::shared_ptr<semantic::PreparedEraseOperation> preparedErase;
  std::optional<EditorHistory::PreparedEntry> preparedHistory;
  semantic::Operation preparedInverse;
  bool publicationPrepared=false;
  PendingSceneStampPreparation sceneStamp;
  std::optional<semantic::Operation> brush;
  std::optional<semantic::Operation> brushInverse;
  std::optional<runtime::ImageImportRequest> image;
  std::vector<std::uint64_t> dependencies;
  std::uint64_t focusRevision=0;
  bool editText=false;
  std::vector<std::pair<foundation::ObjectId,semantic::Transform2D>> erasePoses;
  struct Prefix {
   foundation::ObjectId object{};semantic::Transform2D transform{};
   semantic::EraseMaskGeometry geometry;render::SkiaRenderer::CpuPathPin pin;
   semantic::SweptCircleMask input;
  };
  std::vector<Prefix> prefixes;
  std::size_t nextPrefix=0,activeTarget=0,jobPrefix=0;
  bool activeJob=false,activeCancelled=false;
  Phase phase=Phase::Active;
  std::uint64_t session=0,serial=0,traceGrant=0,payloadGrant=0,grantExecutor=0,payloadExecutor=0;
  std::size_t payloadBytes=0,candidateCapacity=0;
  std::optional<resources::ResourceLease> candidateMetadata;
  input::PointerKey key{};double radius=0;
  std::vector<foundation::WorldPoint> trace;
  std::vector<foundation::ObjectId> candidates;
  std::size_t next=0;
  std::vector<semantic::EraseMaskAddItem> hits;
  std::vector<std::size_t> processed,hitOrder;
  std::size_t reuseScan=0;bool retainingResult=false;
  std::vector<RuntimeSceneRecord> proofs;
  semantic::EraseMaskGeometry geometry,jobGeometry;
  bool historyJob=false,reuseTarget=false;
  render::SkiaRenderer::CpuPathPin currentPath;
  std::vector<render::SkiaRenderer::CpuPathPin> pathPins;
  std::optional<RuntimeSceneRecord> target;
  std::array<std::uint64_t,8> identity{};
  std::uint64_t transportGrant=0,transportExecutor=0,jobCount=0;
  std::vector<std::uint8_t> request,response;
  bool requestTaken=false,completion=false,success=false;
  std::chrono::steady_clock::time_point requestedAt{};
 };
 std::array<std::unique_ptr<Intent>,8> slots;
 std::array<std::uint64_t,4> basis{};
 std::uint64_t executor=0,nextSerial=0,nextJob=0,admitted=0,committed=0,noops=0,failed=0;
 std::uint64_t revision=1,lastAdmitted=0,nextActive=(std::uint64_t{1}<<63);
 std::string error;bool enabled=false,headApplying=false;
 std::uint64_t prepareNs=0,importNs=0,exactNs=0,commitNs=0,workerWaitNs=0;
 std::uint64_t targetPrepareNs=0,sceneStampPrepareNs=0,historyPrepareNs=0,publicationPrepareNs=0,publishNs=0,maximumCommitStageNs=0;
 std::uint64_t jobs=0,geometryReuse=0,exactCalls=0,opportunities=0,maxOwnerNs=0,maxExactNs=0;
 std::uint64_t preparedBeforeUp=0,suffixAfterUp=0,checkpointBytes=0,allUnionSteps=0;
 std::vector<CpuPendingEraseTerminal> terminals;
 std::vector<CpuPendingJobFact> jobLedger;resources::ResourceLease metadata;
 std::uint64_t eligibilityScene=UINT64_MAX;bool eligibleScene=false;bool unavailable=false,executorSuspended=false;
 Intent* head()const noexcept {
  Intent* out=nullptr;for(const auto& p:slots)if(p&&p->serial&&(!out||p->serial<out->serial))out=p.get();return out;
 }
 Intent* requestOwner()const noexcept {
  for(const auto& p:slots)if(p&&p->phase==Phase::Waiting)return p.get();return nullptr;
 }
};
}
