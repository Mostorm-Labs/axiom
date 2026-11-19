#define main previousCanonicalFixtureMain
#include "../../render/tests/canonical_prepared_body_test.cpp"
#undef main
#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "../../render/src/navigation_sharing_observation.hpp"
#include "../../render/src/continuous_zoom_observation.hpp"
#include "../../render/tests/navigation-sharing-serialize.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using nlohmann::json;
#if defined(__APPLE__)
class RuntimeMetalTarget final:public SkiaSurfaceProvider {
 NativeMetalTarget inner_;public:explicit RuntimeMetalTarget(std::shared_ptr<NativeMetalCompletion>c):inner_(c){}
 RenderTargetInfo describe()const noexcept override{auto d=inner_.describe();return RenderTargetInfo{.profileId=d.profileId,.kind=d.kind,.backend=d.backend,.format=d.format,.metrics={256,128,256,128,1,1},.capabilities=d.capabilities};}
 std::shared_ptr<GpuCompletionService>gpuCompletionService()const noexcept override{return inner_.gpuCompletionService();}
 SkiaSurfaceAcquireResult acquire()noexcept override{return inner_.acquire();}void release()noexcept override{inner_.release();}
 BackendSubmissionResult present()noexcept override{auto a=inner_.acquire();if(auto*x=GrAsDirectContext(a.frame.surface->recordingContext()))x->flushAndSubmit(GrSyncCpu::kNo);return BackendSubmissionResult::accepted();}BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h)noexcept override{return inner_.resize(w,h);}
 BackendSubmissionResult readbackRgba(std::span<std::uint8_t>b)noexcept override{return inner_.readbackRgba(b);}
 std::uint64_t readbackCount()const noexcept override{return inner_.readbackCount();}std::uint64_t cpuCopyCount()const noexcept override{return inner_.cpuCopyCount();}std::uint64_t presentCount()const noexcept override{return inner_.presentCount();}
 std::uint64_t generation()const noexcept override{return inner_.generation();}BackendSubmissionResult advanceGeneration()noexcept override{return inner_.advanceGeneration();}
};
class ProofRuntime final:public interaction::CanvasEditingRuntime {
 public:
 render::NavigationWork work()const{return skiaRenderer_.navigationWork();}
 auto changed()const{return navigationChangedAt_;}
};
json fullRuntimeProof(std::shared_ptr<NativeMetalCompletion> c){
 ProofRuntime r;auto target=std::make_unique<RuntimeMetalTarget>(c);auto*surface=target.get();require(surface->resize(256,128).code==BackendSubmissionCode::kAccepted,"preparedruntimeprovider");
 require(r.registerSurfaceProvider("progressive-proof-metal",std::move(target)),"runtime provider stage");
 require(r.bindSurface(256,128),"runtime provider bind");r.setPlatformPresentationDeferred(true);
 zoom_observation::observer=[](const auto&){};auto clock=std::chrono::steady_clock::time_point{};ns::clock=[&]{return clock;};ns::gateHook=[&](unsigned){clock+=std::chrono::microseconds(25);};
 const auto service=[&]{for(unsigned i=0;;++i){clock={};bool presented=false;require(r.serviceNavigationPresentation(r.changed(),presented),"runtime navigation route");c->context->flushAndSubmit(GrSyncCpu::kYes);c->context->checkAsyncWorkCompletion();if(!r.work().pending&&r.work().coverageComplete)break;require(i<4096,"runtime proof convergence");}};
 ns::reset();require(r.setViewportZoomAt(2,0,0),"runtime empty zoom");service();auto emptyTrace=ns::exportRows();unsigned emptyProofs=0;for(const auto&x:emptyTrace["rows"])emptyProofs+=x["proof_publishes"].get<unsigned>();
 require(emptyProofs>0,"ACTUAL_RUNTIME_RAW_SCENE_EMPTY_PROOF_NOT_CONSUMED");
 require(r.work().tilesBuilt==0&&!r.lastSubmittedCanonicalFrame()&&r.submittedOperationCount()==0,"empty proof allocated texture or fabricated canonical receipt");
 std::vector<std::uint8_t> empty(256*128*4);require(surface->readbackRgba(empty).code==BackendSubmissionCode::kAccepted,"emptyread");require(std::all_of(empty.begin(),empty.end(),[](auto v){return v==255;}),"emptyproofpixels");
 semantic::ObjectRecord shape;shape.id=ObjectId::fromUint64(991);shape.kind=semantic::ObjectKind::kShape;shape.kind_version=1;shape.content=semantic::ShapeContent{1,80,60};shape.placement.order_key=semantic::OrderKey({1});shape.transform.tx=20;shape.transform.ty=10;shape.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{.2F,.4F,.8F,1}}}}};
 semantic::Operation insert{semantic::OperationId{ObjectId::fromUint64(992)},r.documentId(),1,1,semantic::InsertObjectsOp{{shape}}};require(r.applyExternalOperation(insert,semantic::ApplySource::kRemoteSync).disposition==semantic::ApplyDisposition::kApplied,"runtime insert");
 const auto semantic=r.semanticGeneration();const auto scene=r.sceneRevision();const auto operations=r.submittedOperationCount();
 ns::reset();require(r.setViewportZoomAt(1,0,0),"runtime content zoom");service();auto contentTrace=ns::exportRows();require(r.work().tilesBuilt>0,"nonemptyRuntimeSourceWasBlank");
 std::vector<std::uint8_t> actual(empty.size());require(surface->readbackRgba(actual).code==BackendSubmissionCode::kAccepted&&actual!=empty,"newcontenthiddenbyproof");
 require(r.semanticGeneration()==semantic&&r.sceneRevision()==scene&&r.submittedOperationCount()==operations&&!r.lastSubmittedCanonicalFrame(),"navigationmutateddocumentorreceipt");
 require(r.loseSurface(),"runtime loss");bool presented=false;require(!r.serviceNavigationPresentation(r.changed(),presented),"losttargetproofreused");
 zoom_observation::observer={};ns::clock={};ns::gateHook={};ns::state.enabled=false;return{{"actual_Runtime_Scene_Render_Metal_route",true},{"virtual_clock_not_latency",true},{"empty_proof_publications",emptyProofs},{"empty_no_texture",true},{"inserted_content_visible",true},{"no_operation_or_canonical_receipt",true},{"lost_target_rejected",true},{"empty_trace",emptyTrace},{"content_trace",contentTrace}};
}
#endif
int main(int argc,char**argv){
#if defined(__APPLE__)
 auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");auto d=MTLCreateSystemDefaultDevice(),q=message(d,"newCommandQueue");auto c=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext b{sk_cfp<GrMTLHandle>(d),sk_cfp<GrMTLHandle>(q)};c->context=GrDirectContexts::MakeMetal(b);require(bool(c->context),"proofActualMetal");auto report=fullRuntimeProof(c);if(argc>1)std::ofstream(argv[1])<<report.dump(2)<<'\n';std::cout<<report.dump()<<'\n';c.reset();message(pool,"drain");return 0;
#else
 return 2;
#endif
}
