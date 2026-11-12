#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "cpu_pending_erase_state.hpp"
#include "partial_erase_geometry.hpp"
#include "pending_scene_preparation.hpp"
#include <algorithm>
#include <cmath>
namespace canvas::interaction {
namespace {
constexpr std::size_t traceLimit=8193,traceCharge=traceLimit*sizeof(foundation::WorldPoint)+65536U;
constexpr std::size_t candidateBytes=sizeof(RuntimeSceneRecord)+sizeof(foundation::ObjectId)+sizeof(semantic::EraseMaskAddItem)+256U;
bool eligible(const RuntimeSceneRecord& record){
 if(record.kind!=semantic::ObjectKind::kVectorStroke&&record.kind!=semantic::ObjectKind::kDabStroke)return true;
 const auto* compact=std::get_if<semantic::CompactStrokeContent>(&record.content.get());
 if(!compact||record.kind!=semantic::ObjectKind::kVectorStroke||compact->stroke.config.algorithmVersion!=4||
    (compact->stroke.config.family!=foundation::InkFamily::kRound&&compact->stroke.config.family!=foundation::InkFamily::kMarker))return false;
 const auto& t=record.transform;const auto x=std::hypot(t.a,t.b),y=std::hypot(t.c,t.d),scale=std::max(x,y);
 return std::isfinite(scale)&&scale>0&&std::abs(x-y)<=1e-12*scale&&
  std::abs((t.a/scale)*(t.c/scale)+(t.b/scale)*(t.d/scale))<=1e-12;
}
const char* phaseName(CpuPendingEraseState::Phase p){switch(p){
 case CpuPendingEraseState::Phase::Active:return "ActiveContact";
 case CpuPendingEraseState::Phase::Frozen:return "FrozenAwaitingPreparation";
 case CpuPendingEraseState::Phase::Preparing:return "PreparingLocalQuery";
 case CpuPendingEraseState::Phase::Waiting:return "WaitingWorker";
 case CpuPendingEraseState::Phase::Confirming:return "OwnerExactConfirmation";
 case CpuPendingEraseState::Phase::ExactReady:return "OwnerExactReady";
 case CpuPendingEraseState::Phase::Ready:return "ReadyToApply";
 case CpuPendingEraseState::Phase::CommitPreparing:return "PreparingAtomicCommit";
 }return "Invalid";}
}
bool CanvasEditingRuntime::bindCpuPendingExecutor(std::array<std::uint64_t,4> basis,std::uint64_t executor)noexcept{
#if defined(AXIOM_CPU_PENDING_ERASE)
 if(closed_||publicationCallbackActive_||std::this_thread::get_id()!=ownerThread_||!executor||!basis[0]||!basis[1]||(!basis[2]&&!basis[3]))return false;
 std::array<std::uint64_t,2> doc{};for(unsigned i=0;i<16;++i)doc[i/8]|=std::uint64_t(documentId_.value().bytes[i])<<((i%8)*8);
 if(doc[0]!=basis[2]||doc[1]!=basis[3]||displayOwner_!=basis[0])return false;
 if(cpuPending_){
  if(basis!=cpuPending_->basis||executor<=cpuPending_->executor)return false;
  // An ended executor can be rebound only between owner requests; accepted
  // queued intents retain their own original trace/payload lease identities.
  if(cpuPending_->requestOwner())return false;
  if(const auto* head=cpuPending_->head();head&&head->phase==CpuPendingEraseState::Phase::Confirming)return false;
 }
 try{if(!cpuPending_)cpuPending_=std::make_shared<CpuPendingEraseState>();if(!cpuPending_->enabled){auto metadata=skiaRenderer_.reserveCpuPendingMetadata(65536);if(!metadata)return false;cpuPending_->metadata=std::move(*metadata);cpuPending_->terminals.reserve(64);cpuPending_->jobLedger.reserve(128);}cpuPending_->basis=basis;cpuPending_->executor=executor;cpuPending_->enabled=true;cpuPending_->executorSuspended=false;skiaRenderer_.setCpuPendingPreparation(true);return true;}catch(...){return false;}
#else
 (void)basis;(void)executor;return false;
#endif
}
bool CanvasEditingRuntime::cpuPendingBusy()const noexcept{return cpuPending_&&cpuPending_->head();}
bool CanvasEditingRuntime::competingCanonicalAdmission()const noexcept{return canonicalWriteAdmission()&&(!cpuPendingBusy()||(cpuPending_&&cpuPending_->headApplying));}
CpuPendingEraseFacts CanvasEditingRuntime::cpuPendingFacts()const noexcept{
 CpuPendingEraseFacts f;
#if defined(AXIOM_CPU_PENDING_ERASE)
 f.capability=true;
#endif
 if(!cpuPending_)return f;
 const auto& s=*cpuPending_;f.enabled=s.enabled;f.suspended=s.executorSuspended;const auto work=skiaRenderer_.renderCacheWork().maskHistory;f.newQueryUnions=work.queryUnionOps;f.historyUnions=work.historyUnionOps;f.retainedPathBytes=work.retainedBytes;f.finishedImports=work.finishedImports;f.finishedHits=work.finishedHits;f.admitted=s.admitted;f.committed=s.committed;f.failed=s.failed;f.noops=s.noops;f.error=s.error;f.targetPrepareNs=s.targetPrepareNs;f.sceneStampPrepareNs=s.sceneStampPrepareNs;f.historyPrepareNs=s.historyPrepareNs;f.publicationPrepareNs=s.publicationPrepareNs;f.publishNs=s.publishNs;f.maximumCommitStageNs=s.maximumCommitStageNs;f.allUnionSteps=s.allUnionSteps;f.preparedBeforeUp=s.preparedBeforeUp;f.suffixAfterUp=s.suffixAfterUp;f.checkpointBytes=s.checkpointBytes;f.jobs=s.jobs;f.geometryReuse=s.geometryReuse;f.exactCalls=s.exactCalls;f.opportunities=s.opportunities;f.maxOwnerNs=s.maxOwnerNs;f.maxExactNs=s.maxExactNs;f.prepareNs=s.prepareNs;f.importNs=s.importNs;f.exactNs=s.exactNs;f.commitNs=s.commitNs;f.workerWaitNs=s.workerWaitNs;f.terminals=s.terminals;f.jobLedger=s.jobLedger;
 for(const auto& p:s.slots)if(p){++f.reserved;if(p->serial)++f.pending;}
 if(const auto* p=s.requestOwner()?s.requestOwner():s.head()){f.head=p->serial;f.historyJob=p->historyJob;f.job=p->identity[5];f.token=p->identity;f.state=phaseName(p->phase);f.requestReady=p->phase==CpuPendingEraseState::Phase::Waiting&&!p->requestTaken;}
 return f;
}
void CanvasEditingRuntime::suspendCpuPendingExecutor()noexcept{if(cpuPending_)cpuPending_->executorSuspended=true;}
void CanvasEditingRuntime::failCpuPendingExecutor()noexcept{
 if(cpuPending_){cpuPending_->unavailable=true;cpuPending_->executorSuspended=false;cpuPending_->error="CPU_PENDING_EXECUTOR_UNAVAILABLE";}
}
bool CanvasEditingRuntime::reserveCpuPendingContact(std::uint64_t session)noexcept{
 if(!cpuPending_||!cpuPending_->enabled||toolMode_!=ToolMode::kPartialEraser)return true;
 if(pendingHistoryBarrier())return false;
 if(cpuPending_->unavailable)return false;
 cpuPending_->error.clear();
 // Freeze eligibility before admission, across the entire current document;
 // a later move cannot newly enter an unnoticed unsupported existing object.
 if(!sceneCoordinator_)return false;
 const auto sceneStamp=sceneRevision();
 if(cpuPending_->eligibilityScene!=sceneStamp){
  cpuPending_->eligibleScene=true;
  for(const auto& record:sceneCoordinator_->runtimeScene().records())if(!eligible(record)){cpuPending_->eligibleScene=false;break;}
  cpuPending_->eligibilityScene=sceneStamp;
 }
 if(!cpuPending_->eligibleScene){cpuPending_->error="LEGACY_ROUTE_INELIGIBLE_DOCUMENT";return !cpuPendingBusy();}
 if(cpuPending_->nextActive==UINT64_MAX||cpuPending_->nextSerial>=(std::uint64_t{1}<<63)-1)return false;
 auto slot=std::find_if(cpuPending_->slots.begin(),cpuPending_->slots.end(),[](const auto& p){return !p;});
 if(slot==cpuPending_->slots.end())return false;
 const auto candidateCapacity=std::min<std::size_t>(4096,sceneCoordinator_->runtimeScene().records().size()+8);
 const auto metadata=candidateCapacity*candidateBytes;
 const auto grant=reserveCpuWorkerGrant(cpuPending_->executor,traceCharge+metadata+3U*1048576U);if(!grant)return false;
 try{
  auto p=std::make_unique<CpuPendingEraseState::Intent>();p->activeSerial=++cpuPending_->nextActive;p->session=session;p->key=displayInputKey_;p->radius=eraserDiameterLogicalPx_*.5;p->traceGrant=grant;p->grantExecutor=cpuPending_->executor;p->candidateCapacity=candidateCapacity;
  eraserTraces_.at(session).reserve(traceLimit);*slot=std::move(p);return true;
 }catch(...){(void)releaseCpuWorkerGrant(cpuPending_->executor,grant);return false;}
}
void CanvasEditingRuntime::cancelCpuPendingContact(std::uint64_t session)noexcept{
 if(!cpuPending_)return;
 for(auto& p:cpuPending_->slots)if(p&&p->session==session&&!p->serial){
  if(p->activeJob){p->activeCancelled=true;return;}
  const auto token=p->traceGrant,executor=p->grantExecutor;p.reset();(void)releaseCpuWorkerGrant(executor,token);return;
 }
}
bool CanvasEditingRuntime::freezeCpuPendingContact(std::uint64_t session)noexcept{
 if(!cpuPending_)return false;
 for(auto& p:cpuPending_->slots)if(p&&p->session==session&&!p->serial){
  const auto it=eraserTraces_.find(session);if(it==eraserTraces_.end()||it->second.empty()||cpuPending_->nextSerial==UINT64_MAX)return false;
  p->trace=std::move(it->second);p->serial=++cpuPending_->nextSerial;++cpuPending_->revision;cpuPending_->lastAdmitted=p->serial;if(!p->activeJob)p->phase=CpuPendingEraseState::Phase::Frozen;++cpuPending_->admitted;
  eraserTraces_.erase(it);eraserPreviewRevisions_.erase(session);transientOverlayDirty_=true;canonicalPresentationDirty_=true;return true;
 }
 return false;
}
std::vector<std::uint8_t> CanvasEditingRuntime::takeCpuPendingRequest()noexcept{
 if(!cpuPending_||closed_||std::this_thread::get_id()!=ownerThread_||publicationCallbackActive_)return {};
 auto* p=cpuPending_->requestOwner();if(!p||p->phase!=CpuPendingEraseState::Phase::Waiting||p->requestTaken)return {};
 p->requestTaken=true;p->requestedAt=std::chrono::steady_clock::now();return std::move(p->request);
}
bool CanvasEditingRuntime::queueCpuPendingCompletion(std::array<std::uint64_t,8> token,std::span<const std::uint8_t> bytes,bool success)noexcept{
 if(!cpuPending_||closed_||std::this_thread::get_id()!=ownerThread_||publicationCallbackActive_)return false;
 auto* p=cpuPending_->requestOwner();if(!p||p->phase!=CpuPendingEraseState::Phase::Waiting||!p->requestTaken||p->completion||p->identity!=token||bytes.size()>1048576)return false;
 try{p->response.assign(bytes.begin(),bytes.end());p->success=success;p->completion=true;
  for(auto& j:cpuPending_->jobLedger)if(j.token==token){j.returnedNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();j.outputBytes=bytes.size();j.status=success?"Returned":"Failed";cpuPending_->workerWaitNs+=j.returnedNs-j.issuedNs;break;}
  return true;}catch(...){p->success=false;p->completion=true;return false;}
}
void CanvasEditingRuntime::serviceCpuActivePrefixes(std::chrono::steady_clock::time_point began)noexcept{
 // Active input is retained, never eagerly expanded to sixteen complete union
 // paths. The frozen decision job validates and requests only relevant ranges.
 // Preserve the legacy completion drain below for an already issued job.
 if(cpuPending_&&!cpuPending_->requestOwner()){
  // Prepare only the immutable input/index while contact is active. No global
  // union job is launched ahead of a target decision.
  return;
 }
 if(!cpuPending_||!cpuPending_->enabled||!canonicalWriteAdmission()||!sceneCoordinator_)return;
 auto& state=*cpuPending_;
 try{
  for(auto& slot:state.slots)if(slot&&slot->activeJob){
   auto& p=*slot;
   if(!p.completion&&p.requestTaken&&std::chrono::steady_clock::now()-p.requestedAt>std::chrono::seconds(30)){p.completion=true;p.success=false;}
   if(!p.completion)return;
   if(p.activeCancelled){const auto trace=p.traceGrant,transport=p.transportGrant,executor=p.grantExecutor,transportExecutor=p.transportExecutor;slot.reset();(void)releaseCpuWorkerGrant(executor,trace);if(transport)(void)releaseCpuWorkerGrant(transportExecutor,transport);continue;}
   const auto pin=p.success?skiaRenderer_.importCpuPendingPath(p.jobGeometry,p.response,p.identity):render::SkiaRenderer::CpuPathPin{};
   for(auto& j:state.jobLedger)if(j.token==p.identity){j.status=pin?"Ready":"Rejected";break;}
   if(pin&&p.activeTarget<p.prefixes.size()){
    auto& prefix=p.prefixes[p.activeTarget];prefix.geometry=std::move(p.jobGeometry);prefix.pin=pin;
    const auto count=std::get<semantic::SweptCircleMask>(prefix.geometry).segments.size();
    if(!p.serial)state.preparedBeforeUp+=count-p.jobPrefix;
   }
   std::vector<std::uint8_t>{}.swap(p.response);p.completion=false;p.activeJob=false;p.phase=p.serial?CpuPendingEraseState::Phase::Frozen:CpuPendingEraseState::Phase::Active;
   (void)releaseCpuWorkerGrant(p.transportExecutor,p.transportGrant);p.transportGrant=0;
   if(!pin){state.error="ACTIVE_PREFIX_FAILED";return;}
   return;
  }
  if(state.unavailable||state.executorSuspended||state.requestOwner()||state.head()||std::chrono::steady_clock::now()-began>=std::chrono::microseconds(8000))return;
  for(auto& slot:state.slots)if(slot&&!slot->serial&&!slot->activeJob&&slot->kind==CpuPendingEraseState::Kind::Erase){
   auto& p=*slot;const auto trace=eraserTraces_.find(p.session);if(trace==eraserTraces_.end()||trace->second.size()<2)continue;
   if(!p.prefixMetadata){auto lease=skiaRenderer_.reserveCpuPendingMetadata(16U*(2U*8192U*sizeof(semantic::EraseCubicSegment)+65536U));if(!lease)return;p.prefixMetadata=std::move(*lease);p.prefixes.reserve(16);}
   if(p.prefixes.empty()){
    // Query only the active trace envelope; newly exposed candidates join later.
    double l=trace->second.front().x,r=l,t=trace->second.front().y,b=t;
    for(const auto& v:trace->second){l=std::min(l,double(v.x));r=std::max(r,double(v.x));t=std::min(t,double(v.y));b=std::max(b,double(v.y));}
    auto found=runtimeSceneHost_->query(canvas::SceneQuery{{float(l-p.radius),float(t-p.radius),float(r+p.radius),float(b+p.radius)}});if(!found)return;
    for(auto id:found.value().backToFront){const auto* record=sceneCoordinator_->runtimeScene().find(id);if(!record||!eligible(*record)||!std::holds_alternative<semantic::CompactStrokeContent>(record->content.get()))continue;if(p.prefixes.size()==16)break;CpuPendingEraseState::Intent::Prefix prefix;prefix.object=id;prefix.transform=record->transform;p.prefixes.push_back(std::move(prefix));}
   }
   for(std::size_t n=0;n<p.prefixes.size();++n){
    if(std::chrono::steady_clock::now()-began>=std::chrono::microseconds(8000))return;
    const auto index=p.nextPrefix++%p.prefixes.size();auto& prefix=p.prefixes[index];const auto* record=sceneCoordinator_->runtimeScene().find(prefix.object);if(!record||record->transform!=prefix.transform)continue;
    auto& segments=prefix.input.segments;
    for(unsigned steps=0;segments.size()+1<trace->second.size()&&steps<4096;++steps){
     if(std::chrono::steady_clock::now()-began>=std::chrono::microseconds(8000))return;
     const auto at=segments.size();auto segment=partialEraseSimilaritySegment(trace->second[at],trace->second[at+1],prefix.transform,p.radius);if(!segment)return;segments.push_back(*segment);
    }
    semantic::EraseMaskGeometry geometry=prefix.input;
    if(prefix.pin&&prefix.geometry==geometry)continue;
    if(auto ready=skiaRenderer_.lookupCpuPendingPath(geometry)){prefix.geometry=std::move(geometry);prefix.pin=std::move(ready);continue;}
    if(state.nextJob==UINT64_MAX)return;
    p.identity={state.basis[0],state.basis[1],state.basis[2],state.basis[3],state.executor,++state.nextJob,1,p.session};
    p.transportExecutor=state.executor; // Native envelope is already reserved at Down.
    p.jobGeometry=std::move(geometry);p.request=skiaRenderer_.encodeCpuPendingContinuation(p.jobGeometry,p.identity,prefix.geometry,prefix.pin,&p.jobPrefix);
    if(p.request.empty()){(void)releaseCpuWorkerGrant(p.transportExecutor,p.transportGrant);p.transportGrant=0;return;}
    state.checkpointBytes+=p.request.size();state.allUnionSteps+=std::get<semantic::SweptCircleMask>(p.jobGeometry).segments.size()-p.jobPrefix;p.activeTarget=index;p.activeJob=true;p.requestTaken=false;p.historyJob=false;p.phase=CpuPendingEraseState::Phase::Waiting;
    if(state.jobLedger.size()==128)state.jobLedger.erase(state.jobLedger.begin());state.jobLedger.push_back({p.identity,false,p.request.size(),0,std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()),0,0,"ActivePrefix"});++state.jobs;++p.jobCount;return;
   }
  }
 }catch(...){state.error="ACTIVE_PREFIX_PREPARATION_FAILED";}
}
void CanvasEditingRuntime::serviceCpuPendingErases()noexcept{
 if(cpuPending_&&cpuPending_->enabled)skiaRenderer_.beginCpuPendingOpportunity();
 const auto began=std::chrono::steady_clock::now();
 struct OwnerTurn {std::shared_ptr<CpuPendingEraseState> state;std::chrono::steady_clock::time_point began;
 ~OwnerTurn(){if(state)state->maxOwnerNs=std::max(state->maxOwnerNs,std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-began).count()));}} turn{cpuPending_,began};
 servicePendingRegionSelection();
 serviceCpuActivePrefixes(began);
 if(cpuPending_&&cpuPending_->requestOwner()&&cpuPending_->requestOwner()->activeJob)return;
 if(servicePendingLocalHead())return;
 // Cheap state transitions andready exactdecisions share ONE owner allowance.
 // A genuineWorker wait, suspendedexecutor or unchangedstate ends the turn.
 for(unsigned step=0;step<4096&&cpuPendingBusy();++step){
  if(std::chrono::steady_clock::now()-began>=std::chrono::microseconds(8000))break;
  auto* before=cpuPending_->head();const auto serial=before->serial,job=before->identity[5];
  const auto phase=before->phase;const auto next=before->next,reuse=before->reuseScan;
  if(servicePendingLocalHead())return;
  serviceCpuPendingEraseStep(began);
  const auto* after=cpuPending_->head();
  if(after&&after->serial==serial&&after->identity[5]==job&&after->phase==phase&&after->next==next&&after->reuseScan==reuse)break;
  if(after&&after->phase==CpuPendingEraseState::Phase::Waiting&&!after->completion)break;
 }
}
void CanvasEditingRuntime::serviceCpuPendingEraseStep(std::chrono::steady_clock::time_point began)noexcept{
 if(!cpuPending_||!cpuPendingBusy()||!canonicalWriteAdmission()||!recoverHistoryScene())return;
 auto& state=*cpuPending_;auto* p=state.head();
 constexpr auto allowance=std::chrono::microseconds(8000);
 ++state.opportunities;
 struct Timed {CpuPendingEraseState& s;CpuPendingEraseState::Phase phase;std::chrono::steady_clock::time_point start;
 ~Timed(){const auto ns=std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count());if(phase==CpuPendingEraseState::Phase::Frozen||phase==CpuPendingEraseState::Phase::Preparing)s.prepareNs+=ns;else if(phase==CpuPendingEraseState::Phase::CommitPreparing)s.commitNs+=ns;}} timing{state,p->phase,std::chrono::steady_clock::now()};
 const auto exact=[&](const RuntimeSceneRecord& record,const semantic::EraseMaskGeometry& geometry){
  const auto at=std::chrono::steady_clock::now();++state.exactCalls;
  const auto hit=skiaRenderer_.confirmCpuPendingErase(record,geometry,p->currentPath);
  const auto ns=std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-at).count());state.exactNs+=ns;state.maxExactNs=std::max(state.maxExactNs,ns);return hit;
 };
 const auto retire=[&](bool failure){
  if(p->publicationPrepared&&localOperationSink_)localOperationSink_->abortPreparedPublication();
  if(failure){failPendingDependents(p->serial);++state.failed;cancelDisplay(p->session,DisplayKind::kErase);}
  if(state.terminals.size()==64)state.terminals.erase(state.terminals.begin());
  const auto stamp=canonicalCommitFence();
  ++state.revision;
  state.terminals.push_back({p->serial,p->session,p->jobCount,
      !failure&&!p->proofs.empty()?stamp.ordinal.value():0,p->identity,
      failure?"Failed":p->proofs.empty()?"NoOp":"Committed",semanticGeneration_.current().value()});
  const auto traceToken=p->traceGrant,payloadToken=p->payloadGrant,transportToken=p->transportGrant;
  const auto traceExecutor=p->grantExecutor,payloadExecutor=p->payloadExecutor,transportExecutor=p->transportExecutor;
  // Destroy every task-local allocation/pin before returning its grant.
  for(auto& slot:state.slots)if(slot.get()==p){slot.reset();break;}
  (void)releaseCpuWorkerGrant(traceExecutor,traceToken);
  if(payloadToken)(void)releaseCpuWorkerGrant(payloadExecutor,payloadToken);
  if(transportToken)(void)releaseCpuWorkerGrant(transportExecutor,transportToken);
  transientOverlayDirty_=true;canonicalPresentationDirty_=true;
 };
 try{
  if(state.unavailable){state.error="CPU_PENDING_EXECUTOR_UNAVAILABLE";retire(true);return;}
  if(p->phase==CpuPendingEraseState::Phase::Frozen){
   double l=p->trace.front().x,r=l,t=p->trace.front().y,b=t;
   for(const auto& v:p->trace){l=std::min(l,double(v.x));r=std::max(r,double(v.x));t=std::min(t,double(v.y));b=std::max(b,double(v.y));}
   // Bounded broad-phase/result/proof metadata is charged before query can
   // allocate. The larger canonical operation payload is separately reserved
   // once the exact candidate cardinality is known, before geometry building.
   // Query metadata and one native transport envelope were admitted at Down.
   // Allocate according to the bounded document candidate count, not 4096
   // heavy proof records for every short gesture. Never discard an accepted
   // intent merely because disposable raster work temporarily occupies quota.
   if(p->candidates.empty()){
    const auto required=std::min<std::size_t>(4096,sceneCoordinator_->runtimeScene().records().size());
    if(required>p->candidateCapacity){auto extra=skiaRenderer_.reserveCpuPendingMetadata((required-p->candidateCapacity)*candidateBytes);if(!extra)return;p->candidateMetadata=std::move(*extra);p->candidateCapacity=required;}
    const auto found=runtimeSceneHost_->query(canvas::SceneQuery{{float(l-p->radius),float(t-p->radius),float(r+p->radius),float(b+p->radius)}});
    if(!found){state.error="PENDING_SCENE_QUERY_FAILED";retire(true);return;}
    p->candidates=found.value().backToFront;
   }
   if(p->candidates.size()>4096){state.error="PENDING_TARGET_CAPACITY";retire(true);return;}
   const auto segments=std::max<std::size_t>(1,p->trace.size()-1);
   const auto perTarget=segments*sizeof(semantic::EraseCubicSegment)+sizeof(RuntimeSceneRecord)+4096U;
   if(p->candidates.size()>(80U*1024U*1024U)/(perTarget*3U)){
    state.error="PENDING_ATOMIC_PAYLOAD_CAPACITY";retire(true);return;
   }
   const auto payload=p->candidates.size()*perTarget*3U;
   if(payload&&!p->payloadGrant){p->payloadExecutor=state.executor;p->payloadGrant=reserveCpuWorkerGrant(state.executor,payload);if(!p->payloadGrant)return;p->payloadBytes=payload;}
   // Canonical operation payloads remain canonical owner allocations; prepare
   // their existing History/publication transaction only after exact decisions.
   p->hits.reserve(p->candidates.size());p->proofs.reserve(p->candidates.size());
   p->processed.assign(p->candidates.size(),0);p->hitOrder.reserve(p->candidates.size());p->pathPins.reserve(512);p->phase=CpuPendingEraseState::Phase::Preparing;
   return;
  }
  if(p->phase==CpuPendingEraseState::Phase::Waiting){
   if(!p->completion){
    if(p->requestTaken&&std::chrono::steady_clock::now()-p->requestedAt>std::chrono::seconds(30)){
     state.error="CPU_PENDING_WORKER_TIMEOUT";retire(true);
    }
    return;
   }
   p->phase=CpuPendingEraseState::Phase::Confirming;
  }
  if(p->phase==CpuPendingEraseState::Phase::Confirming){
   if(!p->success){state.error="CPU_PENDING_WORKER_FAILED";retire(true);return;}
   const auto importAt=std::chrono::steady_clock::now();
   if(p->decisionRequest){
    bool pending=false;const bool accepted=skiaRenderer_.importCpuLocalExact(p->decision,p->response,p->identity,&pending);
    if(pending)return;
    for(auto& j:state.jobLedger)if(j.token==p->identity){j.importNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-importAt).count();j.status=accepted?"ReadyPiece":"RejectedPiece";state.importNs+=j.importNs;break;}
    if(!accepted){state.error="CPU_PENDING_PIECE_IMPORT_FAILED";retire(true);return;}
    std::vector<std::uint8_t>{}.swap(p->response);p->completion=false;p->decisionRequest=false;
    p->phase=CpuPendingEraseState::Phase::ExactReady;return;
   }
   bool resourcePending=false;const auto pin=skiaRenderer_.importCpuPendingPath(p->jobGeometry,p->response,p->identity,&resourcePending);if(resourcePending)return;
   for(auto& j:state.jobLedger)if(j.token==p->identity){j.importNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-importAt).count();j.status=pin?"Ready":"Rejected";state.importNs+=j.importNs;break;}
   if(!pin){state.error="CPU_PENDING_IMPORT_FAILED";retire(true);return;}
   if(std::find(p->pathPins.begin(),p->pathPins.end(),pin)==p->pathPins.end()){
    if(p->pathPins.size()>=512){state.error="PENDING_PIN_CAPACITY";retire(true);return;}p->pathPins.push_back(pin);
   }
   if(!p->historyJob)p->currentPath=pin;
   std::vector<std::uint8_t>{}.swap(p->response);p->completion=false;
   if(p->transportGrant){(void)releaseCpuWorkerGrant(p->transportExecutor,p->transportGrant);p->transportGrant=0;}
   p->phase=CpuPendingEraseState::Phase::ExactReady;return;
  }
  const auto request=[&](const semantic::EraseMaskGeometry& geometry,bool historical){
   if(state.executorSuspended)return false;
   if(state.nextJob==UINT64_MAX)return false;
   p->identity={state.basis[0],state.basis[1],state.basis[2],state.basis[3],state.executor,++state.nextJob,1,p->serial};
   p->transportExecutor=state.executor; // Native request/result envelope remains in Down admission lease.
   p->jobGeometry=geometry;p->historyJob=historical;
   p->jobPrefix=0;
   if(!historical)for(const auto& prefix:p->prefixes)if(prefix.pin){p->request=skiaRenderer_.encodeCpuPendingContinuation(p->jobGeometry,p->identity,prefix.geometry,prefix.pin,&p->jobPrefix);if(p->jobPrefix)break;}
   if(p->request.empty())p->request=skiaRenderer_.encodeCpuPendingRequest(p->jobGeometry,p->identity);
   if(const auto* sweep=std::get_if<semantic::SweptCircleMask>(&p->jobGeometry)){state.allUnionSteps+=sweep->segments.size()-p->jobPrefix;if(!historical)state.suffixAfterUp+=sweep->segments.size()-p->jobPrefix;}
   if(p->request.empty())return false;
   if(state.jobLedger.size()==128)state.jobLedger.erase(state.jobLedger.begin());
   state.jobLedger.push_back({p->identity,historical,p->request.size(),0,std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()),0,0,"Issued"});
   ++state.jobs;++p->jobCount;p->requestTaken=false;p->phase=CpuPendingEraseState::Phase::Waiting;return true;
  };
  if(p->phase==CpuPendingEraseState::Phase::ExactReady){
   if(state.executorSuspended)return;
   const auto* current=p->target?sceneCoordinator_->runtimeScene().find(p->target->objectId):nullptr;
   if(!current||current->content.shared()!=p->target->content.shared()||current->transform!=p->target->transform||current->eraseMasks.storage()!=p->target->eraseMasks.storage()||!textWritable(current->objectId)){
    state.error="PENDING_TARGET_BASIS_STALE";retire(true);return;
   }
   if(!p->fullFallback){
    if(!p->decision)p->decision=skiaRenderer_.requestCpuLocalExact(*current,p->geometry);
    if(!p->decision)return;
    const auto at=std::chrono::steady_clock::now();const auto status=skiaRenderer_.advanceCpuLocalExact(p->decision);
    state.exactNs+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-at).count();
    using S=render::internal::LocalExactStatus;
    if(status==S::kPending)return;
    if(status==S::kNeedPiece){
     if(state.nextJob==UINT64_MAX){state.error="CPU_PENDING_JOB_EXHAUSTED";retire(true);return;}
     p->identity={state.basis[0],state.basis[1],state.basis[2],state.basis[3],state.executor,++state.nextJob,1,p->serial};
     p->request=skiaRenderer_.encodeCpuLocalExact(p->decision,p->identity);if(p->request.empty()){state.error="CPU_PENDING_PIECE_ENCODE_FAILED";retire(true);return;}
     const auto count=(p->request.size()-128)/80;state.allUnionSteps+=count;state.suffixAfterUp+=count;
     p->historyJob=false;p->decisionRequest=true;p->requestTaken=false;p->phase=CpuPendingEraseState::Phase::Waiting;
     if(state.jobLedger.size()==128)state.jobLedger.erase(state.jobLedger.begin());
     state.jobLedger.push_back({p->identity,false,p->request.size(),0,std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()),0,0,"IssuedPiece"});++state.jobs;++p->jobCount;return;
    }
    if(status==S::kInvalid){state.error="CPU_PENDING_LOCAL_EXACT_INVALID";retire(true);return;}
    if(status==S::kNeedFull){p->fullFallback=true;p->decision.reset();}
    else{
     ++state.exactCalls;
     if(status==S::kHit){p->proofs.push_back(*current);p->hitOrder.push_back(p->next);p->hits.push_back({current->objectId,{{{},p->geometry}}});}
     p->processed[p->next]=1;p->target.reset();p->decision.reset();p->completion=false;
     if(!p->reuseTarget){p->retainingResult=true;p->reuseScan=0;}
     p->reuseTarget=false;p->phase=CpuPendingEraseState::Phase::Preparing;return;
    }
   }
   if(!p->currentPath){
    p->currentPath=skiaRenderer_.lookupCpuPendingPath(p->geometry);
    if(!p->currentPath){if(!request(p->geometry,false)){state.error="PENDING_QUERY_PREPARATION_FAILED";retire(true);}return;}
   }
   if(std::find(p->pathPins.begin(),p->pathPins.end(),p->currentPath)==p->pathPins.end()){
    if(p->pathPins.size()>=512){state.error="PENDING_PIN_CAPACITY";retire(true);return;}p->pathPins.push_back(p->currentPath);
   }
   for(const auto& mask:current->eraseMasks){
    auto pin=skiaRenderer_.lookupCpuPendingPath(mask.geometry);
    if(!pin){if(!request(mask.geometry,true)){state.error="PENDING_HISTORY_PREPARATION_FAILED";retire(true);}return;}
    if(std::find(p->pathPins.begin(),p->pathPins.end(),pin)==p->pathPins.end()){
     if(p->pathPins.size()>=512){state.error="PENDING_PIN_CAPACITY";retire(true);return;}p->pathPins.push_back(std::move(pin));
    }
   }
   const auto hit=exact(*current,p->geometry);
   if(hit==render::SelectionGeometryHit::kInvalid||hit==render::SelectionGeometryHit::kUnsupported){state.error="CPU_PENDING_RESULT_OR_EXACT_INVALID";retire(true);return;}
   if(hit==render::SelectionGeometryHit::kHit){p->proofs.push_back(*current);p->hitOrder.push_back(p->next);p->hits.push_back({current->objectId,{{{},p->geometry}}});}
   p->processed[p->next]=1;p->target.reset();p->completion=false;
   if(!p->reuseTarget){p->retainingResult=true;p->reuseScan=0;}
   p->reuseTarget=false;p->phase=CpuPendingEraseState::Phase::Preparing;return;
  }
  if(p->phase==CpuPendingEraseState::Phase::Preparing){
   if(state.executorSuspended)return;
   // Retain just this one complete result while visiting all equal local
   // geometries. Every comparison is full value equality, not a hash/ID.
   // Decisions stay per-object and final payload is restored to original order.
   if(p->retainingResult){
    while(p->reuseScan<p->candidates.size()){
     if(std::chrono::steady_clock::now()-began>=allowance)return;
     const auto index=p->reuseScan++;if(p->processed[index])continue;
     const auto* candidate=sceneCoordinator_->runtimeScene().find(p->candidates[index]);
     if(!candidate){state.error="PENDING_TARGET_REMOVED";retire(true);return;}
     if((candidate->kind!=semantic::ObjectKind::kVectorStroke&&candidate->kind!=semantic::ObjectKind::kDabStroke)||!textWritable(candidate->objectId)){p->processed[index]=1;continue;}
     if(!eligible(*candidate)){state.error="PENDING_UNSUPPORTED_TARGET";retire(true);return;}
     auto local=partialEraseGeometry(p->trace,candidate->transform,p->radius);
     if(!local){state.error="PENDING_LOCAL_GEOMETRY_INVALID";retire(true);return;}
     if(*local!=p->geometry)continue;
     ++state.geometryReuse;
     p->target=*candidate;p->next=index;p->reuseTarget=true;p->decision.reset();p->fullFallback=false;p->phase=CpuPendingEraseState::Phase::ExactReady;return;
    }
    p->retainingResult=false;p->currentPath.reset();std::vector<std::uint8_t>{}.swap(p->response);
    if(p->transportGrant){(void)releaseCpuWorkerGrant(p->transportExecutor,p->transportGrant);p->transportGrant=0;}
    p->next=0;
   }
   while(p->next<p->candidates.size()){
    if(p->processed[p->next]){++p->next;continue;}
    if(std::chrono::steady_clock::now()-began>=allowance)return;
    const auto* current=sceneCoordinator_->runtimeScene().find(p->candidates[p->next]);
    if(!current){state.error="PENDING_TARGET_REMOVED";retire(true);return;}
    if((current->kind!=semantic::ObjectKind::kVectorStroke&&current->kind!=semantic::ObjectKind::kDabStroke)||!textWritable(current->objectId)){++p->next;continue;}
    if(!eligible(*current)){state.error="PENDING_UNSUPPORTED_TARGET";retire(true);return;}
    auto local=partialEraseGeometry(p->trace,current->transform,p->radius);
    if(!local){state.error="PENDING_LOCAL_GEOMETRY_INVALID";retire(true);return;}
    p->target=*current;p->geometry=std::move(*local);
    p->decision.reset();p->fullFallback=false;
    p->currentPath=skiaRenderer_.lookupCpuPendingPath(p->geometry);
    p->phase=CpuPendingEraseState::Phase::ExactReady;return;
   }
   p->phase=CpuPendingEraseState::Phase::Ready;return;
  }
  if(p->phase==CpuPendingEraseState::Phase::Ready){
   for(const auto& proof:p->proofs){const auto* current=sceneCoordinator_->runtimeScene().find(proof.objectId);
    if(!current||current->content.shared()!=proof.content.shared()||current->transform!=proof.transform||current->eraseMasks.storage()!=proof.eraseMasks.storage()||!textWritable(proof.objectId)){state.error="PENDING_COMMIT_BASIS_STALE";retire(true);return;}}
   if(p->hits.empty()){++state.noops;cancelDisplay(p->session,DisplayKind::kErase);retire(false);return;}
   // Full equality grouping changes preparation order only. Restore the
   // canonical candidate ordering before allocating mask IDs/History bytes.
   for(std::size_t i=0;i<p->hits.size();++i)for(std::size_t j=i+1;j<p->hits.size();++j)if(p->hitOrder[j]<p->hitOrder[i]){std::swap(p->hitOrder[i],p->hitOrder[j]);std::swap(p->hits[i],p->hits[j]);}
   if(!skiaRenderer_.retainCpuPendingHandoff(p->session,p->pathPins)){state.error="PENDING_HANDOFF_PIN_QUOTA";retire(true);return;}
   semantic::Operation operation{localOperationIdentity(localOperationOrdinal()),documentId_,1,1,semantic::AddEraseMasksOp{std::move(p->hits)}};
   semantic::RemoveEraseMasksOp remove;
   for(auto& item:std::get<semantic::AddEraseMasksOp>(operation.payload).items){semantic::EraseMaskRemoveItem inverse;inverse.object_id=item.object_id;for(auto& mask:item.masks){mask.id=allocateTextIdentity();if(mask.id.isZero()){retire(true);return;}inverse.mask_ids.push_back(mask.id);}remove.items.push_back(std::move(inverse));}
   p->preparedInverse={allocateOperationId(),documentId_,1,1,std::move(remove)};
   p->preparedErase=operationEngine_.prepareErase(std::move(operation),semanticObjects_,appliedOperations_,semanticGeneration_,canonicalCommitClock_);
   if(!p->preparedErase){state.error="PENDING_ATOMIC_PREPARE_FAILED";retire(true);return;}p->phase=CpuPendingEraseState::Phase::CommitPreparing;return;
  }
  if(p->phase==CpuPendingEraseState::Phase::CommitPreparing){
   const auto commitAt=std::chrono::steady_clock::now();
   const unsigned stage=!p->preparedErase->ready()?0:p->sceneStamp.target<std::get<semantic::AddEraseMasksOp>(p->preparedErase->operation().payload).items.size()?4:!p->preparedHistory?1:!p->publicationPrepared?2:3;
   struct StageCost{CpuPendingEraseState& s;unsigned stage;std::chrono::steady_clock::time_point began;~StageCost(){const auto ns=std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-began).count());s.maximumCommitStageNs=std::max(s.maximumCommitStageNs,ns);if(stage==4)s.sceneStampPrepareNs+=ns;else if(stage==0)s.targetPrepareNs+=ns;else if(stage==1)s.historyPrepareNs+=ns;else if(stage==2)s.publicationPrepareNs+=ns;else s.publishNs+=ns;}} stageCost{state,stage,commitAt};
   if(!p->preparedErase->ready()){
    if(!p->preparedErase->advance(1,8000)){state.error="PENDING_ATOMIC_TARGET_FAILED";retire(true);}return;
   }
   if(p->sceneStamp.target<std::get<semantic::AddEraseMasksOp>(p->preparedErase->operation().payload).items.size()){
    const auto remaining=std::chrono::duration_cast<std::chrono::microseconds>(allowance-(std::chrono::steady_clock::now()-began)).count();
    if(remaining>0)(void)preparePendingMaskStamps(*sceneCompiler_,p->proofs,{p->preparedErase,&p->preparedErase->operation()},p->sceneStamp,4096,remaining);
    if(p->sceneStamp.failed){state.error="PENDING_SCENE_STAMP_FAILED";retire(true);}return;
   }
   if(!p->preparedHistory){p->preparedHistory=history_.prepareRecord(p->preparedErase->operation(),{p->preparedInverse},{},{});if(!p->preparedHistory){state.error="PENDING_HISTORY_FAILED";retire(true);}return;}
   if(!p->publicationPrepared){if(localOperationSink_&&!localOperationSink_->prepareEncodedPublication(p->preparedErase->operation(),p->preparedErase->encoded())){state.error="PENDING_PUBLICATION_FAILED";retire(true);return;}p->publicationPrepared=true;return;}
   state.headApplying=true;auto applied=operationEngine_.commitErase(*p->preparedErase,semantic::ApplySource::kLocalInteraction,semanticGeneration_,canonicalCommitClock_);state.headApplying=false;
   const bool ok=applied.disposition==semantic::ApplyDisposition::kApplied&&applied.commit_record;
   if(ok){history_.commitPrepared(std::move(*p->preparedHistory));if(auto* display=findDisplay(p->session,DisplayKind::kErase))sealDisplay(*display,*applied.commit_record);observeApplied(p->preparedErase->operation(),std::move(applied));++submittedOperationCount_;if(sceneCoordinator_&&sceneCompiler_&&lastAppliedCommit_){semantic::SemanticReadView view(semanticObjects_,semanticGeneration_.current());canvas::SceneCommitInput input(lastAppliedCommit_->before_generation,lastAppliedCommit_->after_generation,view,&lastAppliedCommit_->change_set);try{(void)sceneCoordinator_->apply(*sceneCompiler_,input);}catch(const std::bad_alloc&){/* Canonical is committed; derived Scene recovers at the next owner opportunity. */}}}
   (void)commitAt;

   if(!ok){skiaRenderer_.retireCpuPendingHandoff(p->session);state.error="PENDING_COMMIT_REJECTED";retire(true);return;}
   // Truecanonicalvalues have now been applied bytheoriginalowner. These
   // derivedpins remain until its qualifieddisplayroster replacement.
   ++state.committed;retire(false);
  }
 }catch(...){state.headApplying=false;state.error="CPU_PENDING_ALLOCATION_FAILURE";retire(true);}
}
}
