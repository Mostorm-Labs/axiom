#define main previousCanonicalMain
#include "../../render/tests/canonical_prepared_body_test.cpp"
#undef main
#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/semantic/snapshot.hpp"
#include <thread>
#include "../../render/tests/navigation-sharing-serialize.hpp"
#include "../../render/src/navigation_sharing_observation.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using nlohmann::json;
#if defined(__APPLE__)
class WholeRuntimeTarget final:public SkiaSurfaceProvider {
 NativeMetalTarget inner_;unsigned width_=1431,height_=1010;std::uint64_t presents_=0;
 public:explicit WholeRuntimeTarget(std::shared_ptr<NativeMetalCompletion>c):inner_(c){}
 RenderTargetInfo describe()const noexcept override{auto d=inner_.describe();return RenderTargetInfo{.profileId=d.profileId,.kind=d.kind,.backend=d.backend,.format=d.format,.metrics={1431,1010,width_,height_,float(width_/1431.),1},.capabilities=d.capabilities};}
 std::shared_ptr<GpuCompletionService>gpuCompletionService()const noexcept override{return inner_.gpuCompletionService();}
 SkiaSurfaceAcquireResult acquire()noexcept override{return inner_.acquire();}void release()noexcept override{inner_.release();}
 BackendSubmissionResult present()noexcept override{++presents_;return inner_.present();}BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h)noexcept override{width_=w;height_=h;return inner_.resize(w,h);}
 BackendSubmissionResult readbackRgba(std::span<std::uint8_t>b)noexcept override{return inner_.readbackRgba(b);}
 std::uint64_t readbackCount()const noexcept override{return inner_.readbackCount();}std::uint64_t cpuCopyCount()const noexcept override{return inner_.cpuCopyCount();}std::uint64_t presentCount()const noexcept override{return presents_;}
 std::uint64_t generation()const noexcept override{return inner_.generation();}BackendSubmissionResult advanceGeneration()noexcept override{return inner_.advanceGeneration();}
};
std::vector<std::uint8_t>wholeRead(const char*p){std::ifstream f(p,std::ios::binary);require(bool(f),"wholefile");return{std::istreambuf_iterator<char>(f),{}};}
namespace canvas::ink_playground {class InkPlaygroundHistoryTestAccess {public:static auto geometry(const interaction::CanvasEditingRuntime&r){return r.skiaRenderer_.navigationWork();}};}
json wholeHandoff(std::shared_ptr<NativeMetalCompletion> c,const char*snapshotPath,const char*imagePath){
 auto decoded=semantic::SnapshotCodec::decode(wholeRead(snapshotPath),semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(decoded.ok(),"wholeSnapshot");auto snapshot=*decoded.snapshot;auto projected=projectRuntimeScene(snapshot.objects,semantic::SemanticGeneration{1});json bounds=json::array();for(const auto&r:projected.records)bounds.push_back({{"kind",unsigned(r.kind)},{"bounds",{r.worldBounds.left,r.worldBounds.top,r.worldBounds.right,r.worldBounds.bottom}}});std::ofstream("/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/G5-post-navigation-settle-r1/correctness/mixed-bounds.json")<<bounds.dump(2);
json sourceFacts=json::array();for(const auto&o:snapshot.objects)if(const auto*c=std::get_if<semantic::CompactStrokeContent>(&o.content))sourceFacts.push_back({{"algorithm",c->stroke.config.algorithmVersion},{"family",c->stroke.config.family},{"knots",c->stroke.knots.size()},{"seed",c->stroke.seed},{"transform",{o.transform.a,o.transform.b,o.transform.c,o.transform.d,o.transform.tx,o.transform.ty}}});std::ofstream("/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/G5-post-navigation-settle-r1/correctness/mixed-source-facts.json")<<sourceFacts.dump(2);runtime::RuntimeCompositionCore core;require(core.openDocument(snapshot.document_id),"wholeopen");
 require(core.restoreSnapshotBeforeSurface(snapshot,{snapshot.document_id,snapshot.document_revision,snapshot.recovery_frontier,snapshot.checkpoint_digest},1)==runtime::DocumentRestoreStatus::kRestored,"wholerestore");
 auto target=std::make_unique<WholeRuntimeTarget>(c);auto*provider=target.get();require(provider->resize(1431,1010).code==BackendSubmissionCode::kAccepted,"wholepreparedtarget");require(core.registerSurfaceProvider("whole-metal",std::move(target))&&core.bindSurface({1431,1010,1431,1010,1,1})&&core.setPlatformPresentationDeferred(true),"wholebind");
 for(const auto&r:snapshot.manifest.entries)if(r.resource_kind==semantic::ResourceKind::kImage)require(core.publishResource(r.resource_id.value,wholeRead(imagePath)),"wholeimage");
 auto&runtime=core.qualificationRuntime();const auto control=[&](runtime::CanvasControlPayload payload){static std::uint64_t serial=0;auto&f=core.controlFacade();runtime::CanvasControlRequest q{};q.clientId=991;q.requestId=++serial;q.target=f.mountedCanvasTargets().front();q.payload=payload;require(f.submitCanvasControl(q).state==runtime::ProductControlState::kQueued&&core.ownerPump(),"wholecontrol");};
 runtime::CanvasControlPayload zoom{};zoom.kind=runtime::CanvasControlPayloadKind::kZoomAt;zoom.x=0;zoom.y=0;zoom.scale=1;control(zoom);runtime::CanvasControlPayload pan{};pan.kind=runtime::CanvasControlPayloadKind::kPanBy;pan.x=200;pan.y=120;control(pan);
 const auto pump=[&](bool force){runtime::RuntimeFrameOpportunity frame;require(core.serviceFrameOpportunity(force,frame),"wholeframe");if(frame.canonicalWork)require(core.receivePlatformPresented({*frame.canonicalWork,PresentOutcome::kPresented,PresentEvidenceKind::kPlatformQualified,{}}),"wholefeedback");c->context->flushAndSubmit(GrSyncCpu::kYes);c->context->checkAsyncWorkCompletion();return frame;};
 for(unsigned n=0;runtime.canonicalPresentationDirty();++n){require(n<4096,"wholeinitialconverge");pump(true);}
 if(std::getenv("PS_NATIVE_CURVE")){
  ns::reset();json route=json::array();
  for(unsigned i=1;i<=180;++i){const auto part=(i-1)/60;const double a[]={1,2,.5},b[]={2,.5,1.5};zoom.x=715.5;zoom.y=505;zoom.scale=float(a[part]*std::pow(b[part]/a[part],double((i-1)%60+1)/60));control(zoom);auto before=provider->presentCount();pump(false);route.push_back({{"index",i},{"newPresent",provider->presentCount()>before}});}
  std::ofstream("/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/G5-post-navigation-settle-r1/correctness/native-curve-trace.json")<<json{{"rows",route},{"trace",ns::exportRows()}}.dump(2)<<'\n';return{{"nativeDiagnostic",true}};
 }
 ns::reset();const auto generation=runtime.semanticGeneration();const auto scene=runtime.sceneRevision();const auto ops=runtime.submittedOperationCount();json rows=json::array();
 for(unsigned i=0;i<48;++i){zoom.x=715.5;zoom.y=505;zoom.scale=float(i<24?1.-(i+1)*.5/24:.5+(i-23)*.5/24);control(zoom);const auto before=provider->presentCount();auto frame=pump(false);rows.push_back({{"input",i},{"scale",zoom.scale},{"presented",provider->presentCount()>before},{"nav_pending",frame.navigationPending},{"canonical",bool(frame.canonicalWork)}});require(frame.navigationPending&&!frame.canonicalWork,"navigationfalsereceipt");}
 const auto nav=ink_playground::InkPlaygroundHistoryTestAccess::geometry(runtime);std::ofstream("/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/out/G5-post-navigation-settle-r1/correctness/final-whole-runtime-diagnostic.json")<<json{{"bodyHits",nav.bodyCoverageHits},{"bodyMisses",nav.bodyCoverageMisses},{"bodyBuilds",nav.bodyCoverageBuilds},{"bodyBytes",nav.bodyCoverageBytes},{"regionQueries",nav.regionQueries},{"regionPublished",nav.regionPublications},{"rows",rows},{"trace",ns::exportRows()}}.dump(2)<<'\n';
 require(std::count_if(rows.begin(),rows.end(),[](const auto&r){return r["presented"].template get<bool>();})>=42,"FULL_RUNTIME_FRONTIER_STARVED_BEHIND_UNPUBLISHED_JOB");
 require(runtime.semanticGeneration()==generation&&runtime.sceneRevision()==scene&&runtime.submittedOperationCount()==ops,"wholeNavigationMutatedDocument");ns::state.enabled=false;require(core.loseSurface(),"wholelost");runtime::RuntimeFrameOpportunity lost;require(!core.serviceFrameOpportunity(false,lost)||!lost.navigationPresented,"lostnavpresent");
 return{{"status","WHOLE_RUNTIME_MIXED49_EXECUTED"},{"objects",snapshot.objects.size()},{"rows",rows},{"noOperation_or_navigationCanonicalReceipt",true},{"physical_Metal",true},{"nativeTrace",ns::exportRows()}};
}
#endif
int main(int argc,char**argv){
#if defined(__APPLE__)
 require(argc==4,"output snapshot image");auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");auto d=MTLCreateSystemDefaultDevice(),q=message(d,"newCommandQueue");auto c=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext b{sk_cfp<GrMTLHandle>(d),sk_cfp<GrMTLHandle>(q)};c->context=GrDirectContexts::MakeMetal(b);require(bool(c->context),"wholeMetal");auto result=wholeHandoff(c,argv[2],argv[3]);std::ofstream(argv[1])<<result.dump(2)<<'\n';std::cout<<result.dump()<<std::endl;c.reset();message(pool,"drain");return 0;
#else
 return 2;
#endif
}
