#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/foundation/internal/g5_observation.hpp"
#include "canvas/render/selection_region_geometry.hpp"
#include "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/runtime/interaction/src/partial_erase_geometry.hpp"
#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cmath>
#include <iostream>
namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {public:
 static auto render(const interaction::CanvasEditingRuntime& r){return r.skiaRenderer_.renderCacheWork();}
 static void profile(interaction::CanvasEditingRuntime& r){render::RenderCacheOptions o;o.maxBytes=256U*1024U*1024U;o.sharedMaxBytes=512U*1024U*1024U;o.maxEntryBytes=64U*1024U*1024U;r.skiaRenderer_.setRenderCacheOptions(o);}
 static std::size_t history(const interaction::CanvasEditingRuntime& r){return r.history_.size();}
 static auto scene(const interaction::CanvasEditingRuntime& r){return r.sceneCoordinator_->runtimeScene().publicationWork();}
 static const auto& objects(const interaction::CanvasEditingRuntime& r){return r.semanticObjects_;}
 static const auto& traces(const interaction::CanvasEditingRuntime& r){return r.eraserTraces_;}
};}
using namespace canvas;using json=nlohmann::json;
namespace {
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
struct Host {
 runtime::RuntimeCompositionCore core;std::uint64_t sequence=0,request=0;
 Host(unsigned copies){
  require(core.open(),"UR2dopen");auto c=std::make_unique<render::RasterSkiaSurfaceProvider>();auto p=std::make_unique<render::TransparentOverlaySkiaSurfaceProvider>();
  require(c->resize(800,500).code==render::BackendSubmissionCode::kAccepted&&p->resize(800,500).code==render::BackendSubmissionCode::kAccepted,"UR2dtargets");
  require(core.registerSurfaceProvider("ur2dcanonical",std::move(c))&&core.registerPreviewSurfaceProvider("ur2dpreview",std::move(p))&&core.bindSurface(800,500)&&core.registerSource(71)&&core.setPlatformPresentationDeferred(true),"UR2dproviders");
  semantic::InsertObjectsOp insert;
  semantic::CompactStrokeContent content;content.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;content.stroke.config.size=16;content.stroke.seed=42;
  for(unsigned i=0;i<7199;++i)content.stroke.knots.push_back({250+180*std::sin(i*.037),220+150*std::cos(i*.053),std::uint16_t(20000+(i%100)*300)});
  std::vector<semantic::EraseMaskRecord> history;
  for(unsigned h=0;h<2;++h){semantic::SweptCircleMask mask;for(unsigned i=1;i<=80;++i){semantic::Vec2 a{240+70*std::sin((i-1)*.17+h*.1),210+65*std::cos((i-1)*.11)},b{240+70*std::sin(i*.17+h*.1),210+65*std::cos(i*.11)};mask.segments.push_back({{a,15},{b,15},{a.x+(b.x-a.x)/3,a.y+(b.y-a.y)/3},{a.x+2*(b.x-a.x)/3,a.y+2*(b.y-a.y)/3}});}history.push_back({ObjectId::fromUint64(9000+h),mask});}
  for(unsigned i=0;i<copies;++i){semantic::ObjectRecord o;o.id=ObjectId::fromUint64(100+i);o.kind=semantic::ObjectKind::kVectorStroke;o.kind_version=3;o.content=content;o.erase_masks=history;o.placement.order_key=semantic::OrderKey({std::uint8_t(i+1)});insert.objects.push_back(std::move(o));}
  semantic::Operation op{semantic::OperationId{ObjectId::fromUint64(8000)},semantic::DocumentId{ObjectId::fromUint64(1)},1,1,std::move(insert)};
  require(core.applyExternalOperation(op,semantic::ApplySource::kRemoteSync).disposition==semantic::ApplyDisposition::kApplied,"UR2dseedcanonical");
  runtime::CanvasControlPayload tool{};tool.kind=runtime::CanvasControlPayloadKind::kSelectTool;tool.tool=runtime::CanvasToolKind::kEraser;control(tool);
  runtime::CanvasControlPayload opts{};opts.kind=runtime::CanvasControlPayloadKind::kEraserOptions;opts.eraser.mode=runtime::OptionPatch<std::uint32_t>::set(2);opts.eraser.diameterLogicalPx=runtime::OptionPatch<float>::set(12);control(opts);
 }
 void control(runtime::CanvasControlPayload p){auto& f=core.controlFacade();runtime::CanvasControlRequest req{};req.clientId=72;req.requestId=++request;req.target=f.mountedCanvasTargets().front();req.payload=std::move(p);require(f.submitCanvasControl(req).state==runtime::ProductControlState::kQueued&&core.ownerPump(),"UR2dcontrol");}
 bool pointer(input::PointerPhase phase,float x,float y){input::PlatformPointerSample p{};p.source=71;p.pointer=1;p.sequence=++sequence;p.timestampNs=sequence*8000000;p.x=x;p.y=y;p.phase=phase;p.pressure=.5;return core.submit({{p}});}
};
}
int main(){try{
 for(unsigned copies:{16U,40U})for(unsigned traceSize:{80U,400U}){
  Host host(copies);auto& runtime=host.core.qualificationRuntime();ink_playground::InkPlaygroundHistoryTestAccess::profile(runtime);
  for(unsigned round=0;round<3;++round){
   const auto generation=runtime.semanticGeneration().value();const auto workBefore=ink_playground::InkPlaygroundHistoryTestAccess::render(runtime);double x=395-round*18,y=150;auto start=std::chrono::steady_clock::now();require(host.pointer(input::PointerPhase::kDown,x,y),"Down");
   for(unsigned i=1;i<=traceSize;++i){const double phase=i*.19;x=395-round*18+4*std::sin(phase);y=150.+80.*i/traceSize;require(host.pointer(input::PointerPhase::kMove,x,y),"Move");}
   const auto moveMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
   foundation::internal::g5::ur2dUpWork={};start=std::chrono::steady_clock::now();require(host.pointer(input::PointerPhase::kUp,x,y),"Up");const auto upMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();const auto u=foundation::internal::g5::ur2dUpWork;const auto work=ink_playground::InkPlaygroundHistoryTestAccess::render(runtime);
   std::cout<<json{{"copies",copies},{"knots",7199},{"trace_samples",traceSize+1},{"round",round},{"move_dispatch_sum_ms",moveMs},{"up_ms",upMs},{"phase_ns",u.nanoseconds},{"equivalent_hits",u.equivalentHits},{"exact_calls",u.exactCalls},{"masks_built",u.masksBuilt},{"generation_delta",runtime.semanticGeneration().value()-generation},{"history_new_primitives",work.maskHistory.primitiveBuilds-workBefore.maskHistory.primitiveBuilds},{"history_value_primitives",work.maskHistory.valuePrimitives-workBefore.maskHistory.valuePrimitives},{"history_denied",work.maskHistory.denied-workBefore.maskHistory.denied},{"history_path_builds",work.maskHistory.historyPathBuilds-workBefore.maskHistory.historyPathBuilds},{"new_query_builds",work.maskHistory.queryPathBuilds-workBefore.maskHistory.queryPathBuilds},{"body_builds",work.preparedBodyBuilds-workBefore.preparedBodyBuilds}}.dump()<<std::endl;
  }
 }
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}
