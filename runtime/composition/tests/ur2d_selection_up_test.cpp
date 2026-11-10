#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/foundation/internal/g5_observation.hpp"
#include "canvas/render/selection_region_geometry.hpp"
#include "../../interaction/src/partial_erase_geometry.hpp"
#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cmath>
#include <iostream>
namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {public:
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
int main(int argc,char**argv){try{
 auto out=std::filesystem::path(argc>1?argv[1]:"out/ur2d-up");std::filesystem::create_directories(out);json rows=json::array();
 for(unsigned copies:{1U,8U,16U,32U})for(unsigned repeat=0;repeat<3;++repeat){
  Host host(copies);auto& runtime=host.core.qualificationRuntime();const auto before=runtime.snapshotObjects();const auto generation=runtime.semanticGeneration().value();const auto history=ink_playground::InkPlaygroundHistoryTestAccess::history(runtime);const auto scene=ink_playground::InkPlaygroundHistoryTestAccess::scene(runtime);
  const float x=400+repeat*6;require(host.pointer(input::PointerPhase::kDown,x,160)&&host.pointer(input::PointerPhase::kMove,x+2,190),"UR2dactualDownMove");
  auto trace=ink_playground::InkPlaygroundHistoryTestAccess::traces(runtime).begin()->second;
  foundation::internal::g5::ur2dUpWork={};const auto start=std::chrono::steady_clock::now();require(host.pointer(input::PointerPhase::kUp,x+4,220),"UR2dactualUp");const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();const auto work=foundation::internal::g5::ur2dUpWork;
  const auto after=runtime.snapshotObjects();require(runtime.semanticGeneration().value()==generation+1&&ink_playground::InkPlaygroundHistoryTestAccess::history(runtime)==history+1,"UR2dexactlyonce");
  auto mask=interaction::partialEraseGeometry(trace,{},6);require(bool(mask),"UR2dmask");
  for(const auto& old:before){const auto found=std::find_if(after.begin(),after.end(),[&](const auto& object){return object.id==old.id;});
   require(found!=after.end()&&found->content==old.content&&found->erase_masks.size()==old.erase_masks.size()+1,"UR2dcanonicalobjectidentity");
   for(const auto& prior:old.erase_masks)require(std::find(found->erase_masks.begin(),found->erase_masks.end(),prior)!=found->erase_masks.end(),"UR2doldhistorypreserved");
   unsigned added=0;for(const auto& addedMask:found->erase_masks)if(std::none_of(old.erase_masks.begin(),old.erase_masks.end(),[&](const auto& previous){return previous.id==addedMask.id;})){++added;require(addedMask.geometry==*mask,"UR2dcanonicalmaskbytes");}
   require(added==1,"UR2doneuniquemaskperobject");
  }
  require(host.pointer(input::PointerPhase::kDown,600,400)&&host.pointer(input::PointerPhase::kUp,600,400),"UR2dimmediatenextDownNoop");require(runtime.snapshotObjects()==after,"UR2dnoopchangedcanonical");
  require(host.pointer(input::PointerPhase::kDown,x,160)&&host.pointer(input::PointerPhase::kCancel,x+2,190),"UR2dcancel");require(runtime.snapshotObjects()==after,"UR2dcancelmutated");
  runtime::CanvasControlPayload p{};p.kind=runtime::CanvasControlPayloadKind::kUndo;host.control(p);require(runtime.snapshotObjects()==before,"UR2dUndo");p.kind=runtime::CanvasControlPayloadKind::kRedo;host.control(p);require(runtime.snapshotObjects()==after,"UR2dRedo");
  const auto sceneAfter=ink_playground::InkPlaygroundHistoryTestAccess::scene(runtime);
  json row{{"copies",copies},{"repeat",repeat},{"up_ms",elapsed},{"phase_nanoseconds",work.nanoseconds},{"masks_built",work.masksBuilt},{"exact_calls",work.exactCalls},{"equivalent_hits",work.equivalentHits},{"canonical_mask_bytes_equal",true},{"history_exactly_once",true},{"noop_cancel_undo_redo",true},{"scene_materializations",sceneAfter.materializationCalls-scene.materializationCalls}};
  rows.push_back(row);std::cout<<row.dump()<<std::endl;
 }
 std::ofstream(out/"up-phases.json")<<rows.dump(2)<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
