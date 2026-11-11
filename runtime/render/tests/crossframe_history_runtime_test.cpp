// Real Input/OperationEngine/Scene publication and native GPU Render consumer.
#define main crossframeExistingMetalFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "../src/exact_history_path_observer.hpp"
#include <nlohmann/json.hpp>
#include <numeric>
#include <iostream>
using json=nlohmann::json;
namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {public:
 static void profile(interaction::CanvasEditingRuntime& r){render::RenderCacheOptions o;o.maxBytes=256U*1024U*1024U;o.sharedMaxBytes=512U*1024U*1024U;o.maxEntryBytes=64U*1024U*1024U;r.skiaRenderer_.setRenderCacheOptions(o);}
 static auto work(const interaction::CanvasEditingRuntime& r){return r.skiaRenderer_.renderCacheWork();}
 static auto scene(const interaction::CanvasEditingRuntime& r){return r.sceneCoordinator_->runtimeScene().publicationWork();}
 static const auto& records(const interaction::CanvasEditingRuntime& r){return r.sceneCoordinator_->runtimeScene();}
};}
namespace {
json rows=json::array(),events=json::array();std::string phase="setup";unsigned roundNumber=0;
std::vector<semantic::EraseMaskGeometry> geometries;std::size_t dropped=0;
void observe(const HistoryPathObservation& e){
 if(events.size()>=32768){++dropped;return;}
 std::size_t identity=0;if(e.geometry){const auto it=std::find(geometries.begin(),geometries.end(),*e.geometry);if(it==geometries.end()){geometries.push_back(*e.geometry);identity=geometries.size();}else identity=std::size_t(it-geometries.begin())+1;}
 if(e.event==HistoryPathEvent::kState)require(std::accumulate(e.categories.begin(),e.categories.end(),std::size_t{})==e.local,"resident categories fail reconciliation");
 events.push_back({{"sequence",events.size()},{"round",roundNumber},{"phase",phase},{"event",unsigned(e.event)},{"entry",reinterpret_cast<std::uintptr_t>(e.entry)},{"owner",reinterpret_cast<std::uintptr_t>(e.owner)},{"source",reinterpret_cast<std::uintptr_t>(e.source)},{"geometry_id",identity},{"local",e.local},{"estimate_or_category",e.estimate},{"actual",e.actual},{"cardinality",e.cardinality},{"historical",e.historical},{"categories",e.categories}});
}
class Target final:public SkiaSurfaceProvider {
 NativeMetalTarget target_;
 public:Target(std::shared_ptr<NativeMetalCompletion> c):target_(std::move(c)){}
 RenderTargetInfo describe()const noexcept override{auto i=target_.describe();return {i.profileId,i.kind,i.backend,i.format,{1431,1010,2862,2020,2,1},i.capabilities};}
 std::shared_ptr<GpuCompletionService> gpuCompletionService()const noexcept override{return target_.gpuCompletionService();}
 SkiaSurfaceAcquireResult acquire()noexcept override{return target_.acquire();}void release()noexcept override{target_.release();}
 BackendSubmissionResult present()noexcept override{return target_.present();}BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h)noexcept override{return target_.resize(w,h);}
 BackendSubmissionResult readbackRgba(std::span<std::uint8_t> out)noexcept override{return target_.readbackRgba(out);}
 std::uint64_t generation()const noexcept override{return target_.generation();}BackendSubmissionResult advanceGeneration()noexcept override{return target_.advanceGeneration();}
 std::uint64_t readbackCount()const noexcept override{return target_.readbackCount();}std::uint64_t cpuCopyCount()const noexcept override{return target_.cpuCopyCount();}std::uint64_t presentCount()const noexcept override{return target_.presentCount();}
};
json workDelta(const RenderCacheWork& a,const RenderCacheWork& b){return{{"history_builds",b.maskHistory.historyPathBuilds-a.maskHistory.historyPathBuilds},{"history_unions",b.maskHistory.historyUnionOps-a.maskHistory.historyUnionOps},{"query_builds",b.maskHistory.queryPathBuilds-a.maskHistory.queryPathBuilds},{"query_unions",b.maskHistory.queryUnionOps-a.maskHistory.queryUnionOps},{"flatten",b.maskHistory.flattenedPrimitives-a.maskHistory.flattenedPrimitives},{"value_records",b.maskHistory.valueRecords-a.maskHistory.valueRecords},{"value_primitives",b.maskHistory.valuePrimitives-a.maskHistory.valuePrimitives},{"local_bytes",b.maskHistory.retainedBytes},{"prepared_body_builds",b.preparedBodyBuilds-a.preparedBodyBuilds},{"prepared_compile",b.preparedCompileInputs-a.preparedCompileInputs},{"prepared_body_canonical_draws",b.preparedCanonicalDraws-a.preparedCanonicalDraws}};}
}
int main(int argc,char**argv){try{
 require(argc==4,"usage fixed40snapshot output.json rounds");const auto start=std::chrono::steady_clock::now();
 std::ifstream input(argv[1],std::ios::binary);std::vector<std::uint8_t>bytes{std::istreambuf_iterator<char>(input),{}};auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(decoded.ok(),"fixed40decode");const auto& s=*decoded.snapshot;require(s.objects.size()==40,"fixed40objects");
 runtime::RuntimeCompositionCore core;require(core.openDocument(s.document_id),"open");require(core.restoreSnapshotBeforeSurface(s,{s.document_id,s.document_revision,s.recovery_frontier,s.checkpoint_digest},1)==runtime::DocumentRestoreStatus::kRestored,"restore");
 ink_playground::InkPlaygroundHistoryTestAccess::profile(core.qualificationRuntime());
 auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"nativeMetalcontext");
 auto canonical=std::make_unique<Target>(completion),preview=std::make_unique<Target>(completion);require(canonical->resize(2862,2020).code==BackendSubmissionCode::kAccepted&&preview->resize(2862,2020).code==BackendSubmissionCode::kAccepted,"targets");
 require(core.registerSurfaceProvider("cfhcanonical",std::move(canonical))&&core.registerPreviewSurfaceProvider("cfhpreview",std::move(preview))&&core.bindSurface(SurfaceMetrics{1431,1010,2862,2020,2,1})&&core.registerSource(71)&&core.setPlatformPresentationDeferred(true),"bind");
 auto& runtime=core.qualificationRuntime();std::uint64_t seq=0,request=0;
 auto frame=[&]{runtime::RuntimeFrameOpportunity out;require(core.serviceFrameOpportunity(false,out),"frameopportunity");if(out.previewWork)require(core.finishFrameOpportunity({out.previewWork,true}),"previewack");if(out.canonicalWork)require(core.receivePlatformPresented({*out.canonicalWork,PresentOutcome::kPresented,PresentEvidenceKind::kPlatformQualified,{}}),"canonicalack");};
 auto settle=[&]{for(unsigned i=0;i<4096;++i){if(!core.canonicalPresentationDirty()&&!core.previewActive())return;frame();}throw std::runtime_error("native frame did not converge");};
 auto control=[&](runtime::CanvasControlPayload p){auto& facade=core.controlFacade();runtime::CanvasControlRequest q{};q.clientId=72;q.requestId=++request;q.target=facade.mountedCanvasTargets().front();q.payload=std::move(p);require(facade.submitCanvasControl(q).state==runtime::ProductControlState::kQueued&&core.ownerPump(),"control");};
 auto pointer=[&](input::PointerPhase p,float x,float y){input::PlatformPointerSample sample{};sample.source=71;sample.pointer=1;sample.sequence=++seq;sample.timestampNs=90000000000+seq*8000000;sample.x=x;sample.y=y;sample.pressure=.5;sample.phase=p;require(core.submit({{sample}}),"input rejected");};
 settle();runtime::CanvasControlPayload c{};c.kind=runtime::CanvasControlPayloadKind::kSelectTool;c.tool=runtime::CanvasToolKind::kEraser;control(c);c={};c.kind=runtime::CanvasControlPayloadKind::kEraserOptions;c.eraser.mode=runtime::OptionPatch<std::uint32_t>::set(2);c.eraser.diameterLogicalPx=runtime::OptionPatch<float>::set(32);control(c);
 historyPathObserver=observe;const unsigned rounds=unsigned(std::stoul(argv[3]));
 for(roundNumber=0;roundNumber<rounds;++roundNumber){float x=420-roundNumber*18,y=160;phase="mainMove";pointer(input::PointerPhase::kDown,x,y);
  for(unsigned i=1;i<=160;++i){x=float(420-roundNumber*18+7*std::sin(i*.17));y=float(160+170.*i/160);pointer(input::PointerPhase::kMove,x,y);if(i%8==0)frame();}
  auto before=ink_playground::InkPlaygroundHistoryTestAccess::work(runtime);auto began=std::chrono::steady_clock::now();phase="mainUp";pointer(input::PointerPhase::kUp,x,y);auto after=ink_playground::InkPlaygroundHistoryTestAccess::work(runtime);
  json row{{"round",roundNumber},{"main_ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count()},{"main",workDelta(before,after)},{"main_generation",runtime.semanticGeneration().value()}};
  phase="shortMove";pointer(input::PointerPhase::kDown,x+1,y+1);pointer(input::PointerPhase::kMove,x+2,y+4);
  before=ink_playground::InkPlaygroundHistoryTestAccess::work(runtime);began=std::chrono::steady_clock::now();phase="shortUp";pointer(input::PointerPhase::kUp,x+2,y+4);after=ink_playground::InkPlaygroundHistoryTestAccess::work(runtime);row["short_ms"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();row["short"]=workDelta(before,after);
  phase="committedDraw";before=after;frame();after=ink_playground::InkPlaygroundHistoryTestAccess::work(runtime);row["draw"]=workDelta(before,after);
  json owners=json::array();for(const auto&r:ink_playground::InkPlaygroundHistoryTestAccess::records(runtime).records())owners.push_back({{"object",r.objectId.bytes},{"body",reinterpret_cast<std::uintptr_t>(r.content.shared().get())},{"masks",reinterpret_cast<std::uintptr_t>(r.eraseMasks.storage().get())},{"mask_count",r.eraseMasks.size()}});row["published_sources"]=owners;rows.push_back(row);std::cout<<row.dump()<<std::endl;
 }
 phase="settle";settle();historyPathObserver=nullptr;
 json full=json::array();for(const auto&g:geometries){json v=json::array();std::visit([&](const auto&m){if constexpr(requires{m.segments;})for(const auto&s:m.segments)v.push_back({s.p0.position.x,s.p0.position.y,s.p0.radius,s.p1.position.x,s.p1.position.y,s.p1.radius,s.control1.x,s.control1.y,s.control2.x,s.control2.y});},g);full.push_back(v);}
 const auto final=ink_playground::InkPlaygroundHistoryTestAccess::work(runtime);json result{{"rows",rows},{"events",events},{"complete_geometry_values",full},{"dropped",dropped},{"elapsed_ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()},{"final_local_bytes",final.maskHistory.retainedBytes},{"generation",runtime.semanticGeneration().value()},{"production_repair",false},{"actual_native_gpu",true}};
 std::ofstream(argv[2])<<result.dump(2)<<'\n';require(core.close(),"close");
 if(std::getenv("CFH_VERIFY")){require(dropped==0,"bounded observation loss");require(rows.back()["main"]["history_builds"].get<unsigned>()==0,"OPTIONAL_DRAW_DISPLACED_EXACT_HISTORY");require(final.maskHistory.retainedBytes<=8388608,"resident quota");}
 return 0;
}catch(const std::exception&e){historyPathObserver=nullptr;std::cerr<<e.what()<<'\n';return 1;}}
