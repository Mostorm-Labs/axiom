#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <cmath>
#define main u3MetalFixtureMain
#include "../../render/tests/canonical_prepared_body_test.cpp"
#undef main
namespace canvas::ink_playground {class InkPlaygroundHistoryTestAccess {public:
 static void desktop(interaction::CanvasEditingRuntime&r){render::RenderCacheOptions o;o.maxBytes=256U*1024U*1024U;o.sharedMaxBytes=512U*1024U*1024U;o.maxEntryBytes=64U*1024U*1024U;r.skiaRenderer_.setRenderCacheOptions(o);}
 static auto state(const interaction::CanvasEditingRuntime&r){return nlohmann::json{{"generation",r.semanticGeneration_.current().value()},{"previewActive",r.previewActive()},{"previewDirty",r.previewRenderState().dirty},{"canonicalDirty",r.canonicalPresentationDirty_},{"retryable",r.canonicalPresentationRetryable_},{"handoffs",r.pendingCanonicalIdentities_.size()}};}
};}
using namespace canvas;using nlohmann::json;
class ReproMetalTarget final:public render::SkiaSurfaceProvider {
 NativeMetalTarget target_;
 public:explicit ReproMetalTarget(std::shared_ptr<NativeMetalCompletion> c):target_(std::move(c)){}
 render::RenderTargetInfo describe()const noexcept override{auto i=target_.describe();return {i.profileId,i.kind,i.backend,i.format,{1100,760,1100,760,1,1},i.capabilities};}
 std::shared_ptr<render::GpuCompletionService> gpuCompletionService()const noexcept override{return target_.gpuCompletionService();}
 render::SkiaSurfaceAcquireResult acquire()noexcept override{return target_.acquire();}void release()noexcept override{target_.release();}
 render::BackendSubmissionResult present()noexcept override{return target_.present();}
 render::BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h)noexcept override{return target_.resize(w,h);}
 render::BackendSubmissionResult readbackRgba(std::span<std::uint8_t> o)noexcept override{return target_.readbackRgba(o);}
 std::uint64_t readbackCount()const noexcept override{return target_.readbackCount();}std::uint64_t cpuCopyCount()const noexcept override{return target_.cpuCopyCount();}std::uint64_t presentCount()const noexcept override{return target_.presentCount();}
 std::uint64_t generation()const noexcept override{return target_.generation();}render::BackendSubmissionResult advanceGeneration()noexcept override{return target_.advanceGeneration();}
};
void need(bool value,const char*msg){if(!value){std::cerr<<msg<<'\n';std::exit(1);}}
int main(int argc,char**argv){need(argc==2,"fixture");std::ifstream in(argv[1],std::ios::binary);std::vector<std::uint8_t>bytes{std::istreambuf_iterator<char>(in),{}};auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);need(decoded.ok(),"decode");auto& snap=*decoded.snapshot;
 runtime::RuntimeCompositionCore core;need(core.openDocument(snap.document_id),"open");need(core.restoreSnapshotBeforeSurface(snap,{snap.document_id,snap.document_revision,snap.recovery_frontier,snap.checkpoint_digest},1)==runtime::DocumentRestoreStatus::kRestored,"restore");
 auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);need(bool(completion->context),"Metalcontext");
 auto canonical=std::make_unique<ReproMetalTarget>(completion),preview=std::make_unique<ReproMetalTarget>(completion);need(canonical->resize(1100,760).code==render::BackendSubmissionCode::kAccepted&&preview->resize(1100,760).code==render::BackendSubmissionCode::kAccepted,"resize");need(core.registerSurfaceProvider("u3repro-c",std::move(canonical)),"registercanonical");need(core.registerPreviewSurfaceProvider("u3repro-p",std::move(preview)),"registerpreview");need(core.bindSurface(1100,760),"bindsurface");need(core.registerSource(71),"registerinput");need(core.setPlatformPresentationDeferred(true),"deferred");ink_playground::InkPlaygroundHistoryTestAccess::desktop(core.qualificationRuntime());
 std::uint64_t seq=0,req=0;auto control=[&](runtime::CanvasControlPayload p){auto& f=core.controlFacade();runtime::CanvasControlRequest q{};q.clientId=72;q.requestId=++req;q.target=f.mountedCanvasTargets().front();q.payload=std::move(p);need(f.submitCanvasControl(q).state==runtime::ProductControlState::kQueued&&core.ownerPump(),"control");};
 auto pointer=[&](input::PointerPhase phase,double x,double y){input::PlatformPointerSample p{};p.source=71;p.pointer=1;p.sequence=++seq;p.timestampNs=90000000000+seq*8000000;p.x=x;p.y=y;p.pressure=.5;p.phase=phase;need(core.submit({{p}}),"pointer");};
 std::optional<runtime::RuntimeFrameOpportunity::PreviewWork> delayedPreview;
 auto frame=[&]{runtime::RuntimeFrameOpportunity out;const auto ok=core.serviceFrameOpportunity(false,out);if(out.previewWork){if(std::getenv("U3_DELAY_PREVIEW"))delayedPreview=out.previewWork;else need(core.finishFrameOpportunity({out.previewWork,true}),"previewack");}if(out.canonicalWork)need(core.receivePlatformPresented({*out.canonicalWork,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"ack");return ok;};
 for(unsigned n=0;n<1000&&core.canonicalPresentationDirty();++n)need(frame(),"initialframe");
 runtime::CanvasControlPayload p{};p.kind=runtime::CanvasControlPayloadKind::kSelectPreset;p.preset={"round-handdrawn",4};control(p);
 pointer(input::PointerPhase::kDown,120,650);for(unsigned i=1;i<720;++i){pointer(input::PointerPhase::kMove,120+i*.95,650+12*std::sin(i*.055));if(i%8==0)need(frame(),"liveframe");}pointer(input::PointerPhase::kUp,803.05,650+12*std::sin(719*.055));
 p={};p.kind=runtime::CanvasControlPayloadKind::kPanBy;p.x=8;p.y=4;control(p);p={};p.kind=runtime::CanvasControlPayloadKind::kZoomAt;p.x=550;p.y=380;p.hasAnchor=true;p.scale=std::exp(.04);control(p);
 p={};p.kind=runtime::CanvasControlPayloadKind::kSelectCreationPreset;p.creationPreset=runtime::CanvasCreationPreset::kRectangle;control(p);pointer(input::PointerPhase::kDown,900,250);pointer(input::PointerPhase::kMove,930,280);pointer(input::PointerPhase::kCancel,930,280);
 p={};p.kind=runtime::CanvasControlPayloadKind::kUndo;control(p);p.kind=runtime::CanvasControlPayloadKind::kRedo;control(p);
 if(delayedPreview){const auto revision=core.previewRenderState().contentRevision;const bool adapterWouldComplete=core.previewActive()&&revision==delayedPreview->contentRevision;const auto accepted=core.finishFrameOpportunity({delayedPreview,true});std::cout<<json{{"delayed_preview",true},{"adapterWouldComplete",adapterWouldComplete},{"finishAccepted",accepted},{"revision",revision},{"capturedRevision",delayedPreview->contentRevision}}<<'\n';if(adapterWouldComplete&&!accepted)return 3;}
 for(unsigned n=0;n<1000;++n){const bool ok=frame();const auto state=ink_playground::InkPlaygroundHistoryTestAccess::state(core.qualificationRuntime());if(!ok){std::cout<<json{{"frame",n},{"ok",ok},{"state",state}}<<'\n';return 1;}if(!core.canonicalPresentationDirty()&&!core.previewActive()){std::cout<<json{{"status","PROGRESSED"},{"frames",n},{"state",state}}<<'\n';return 0;}}
 std::cerr<<"native no progress\n";return 1;
}
