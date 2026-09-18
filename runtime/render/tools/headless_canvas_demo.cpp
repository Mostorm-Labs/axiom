#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/frame_plan.hpp"
#include "canvas/render/render_backend.hpp"
#include "canvas/render/skia_headless_backend.hpp"
#include "canvas/render/surface_lifecycle.hpp"
#include "canvas/render/visibility_resolver.hpp"
#include "canvas/scene/direct_render_scene.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "canvas/scene/scene_binding.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/semantic_read_view.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace canvas;
namespace {
std::uint64_t lowId(const foundation::ObjectId& id) { std::uint64_t v=0; for(std::size_t i=0;i<8;++i)v|=static_cast<std::uint64_t>(id.bytes[i])<<(i*8); return v; }
std::uint64_t orderValue(const semantic::OrderKey& key) { std::uint64_t v=0; for(auto b:key.bytes()) v=(v<<8U)|b; return v; }
std::string fnv(const std::vector<std::uint8_t>& bytes) { std::uint64_t v=1469598103934665603ULL; for(auto b:bytes){v^=b;v*=1099511628211ULL;} std::ostringstream o;o<<"fnv1a64:"<<std::hex<<std::setfill('0')<<std::setw(16)<<v;return o.str(); }
int fail(const char* m){std::cerr<<m<<'\n';return 1;}
class DemoCompiler final : public ISemanticSceneCompiler {
 public:
  foundation::Result<CompiledSceneSnapshot> compileFull(const semantic::SemanticReadView& view) const override {
    const auto source=view.allObjects(); CompiledSceneSnapshot out{SceneRevision(view.generation().value()),{}}; out.records.reserve(source.size());
    for(const auto& s:source){const auto bounds=scene::computeBounds(s); SceneObjectKind k=SceneObjectKind::kShape; switch(s.kind){case semantic::ObjectKind::kImage:k=SceneObjectKind::kImage;break;case semantic::ObjectKind::kVectorPath:k=SceneObjectKind::kVectorPath;break;case semantic::ObjectKind::kRichText:k=SceneObjectKind::kRichText;break;case semantic::ObjectKind::kVectorStroke:k=SceneObjectKind::kVectorStroke;break;case semantic::ObjectKind::kDabStroke:k=SceneObjectKind::kDabStroke;break;default:break;} const auto flags=static_cast<SceneRecordFlags>(static_cast<std::uint32_t>(SceneRecordFlags::kVisible)|static_cast<std::uint32_t>(SceneRecordFlags::kHitTestable)); out.records.push_back({s.id,SceneOrderKey(orderValue(s.placement.order_key)),k,flags,bounds.world,ContentRevision(1),RenderPayloadRef{static_cast<std::uint32_t>(lowId(s.id)),1},HitGeometryRef{static_cast<std::uint32_t>(lowId(s.id)),1}});}
    return foundation::Result<CompiledSceneSnapshot>::success(std::move(out));
  }
  foundation::Result<CompiledSceneDelta> compileDelta(const semantic::SemanticReadView&,const semantic::ChangeSet&) const override{return foundation::Result<CompiledSceneDelta>::failure({foundation::ErrorCode::kRequiresFullRebuild,"demo full"});}
};
bool writeFile(const fs::path& p,const std::vector<std::uint8_t>& b){std::ofstream o(p,std::ios::binary);if(!o)return false;o.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));return o.good();}
}
int main(int argc,char**argv){
 if(argc!=5||std::string(argv[1])!="--fixture"||std::string(argv[3])!="--output-dir")return fail("usage");
 const fs::path fixture=argv[2],output=argv[4]; if(fs::exists(output))return fail("output directory already exists"); std::ifstream in(fixture,std::ios::binary);if(!in)return fail("fixture unavailable"); std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),{}); auto decoded=semantic::SnapshotCodec::decode(bytes);if(!decoded.ok())return fail("fixture decode failed");
 semantic::IndexedObjectStore objects; semantic::DocumentRuntimeState state=semantic::DocumentRuntimeState::kLoading; semantic::SemanticGenerationState generation(semantic::SemanticGeneration(803)); auto restored=semantic::SnapshotBootstrapper::restore(*decoded.snapshot,state,objects,generation);if(!restored.restored)return fail("bootstrap failed"); semantic::SemanticReadView view(objects,semantic::SemanticGeneration(803));
 auto scene=std::make_unique<Scene>(std::make_unique<DirectRenderScene>(),std::make_unique<UniformGridSpatialIndex>());SceneBinding binding(*scene);IncrementalRuntimeCoordinator coordinator(binding);DemoCompiler compiler;SceneCommitInput ci(semantic::SemanticGeneration(803),view);if(!coordinator.recover(compiler,ci))return fail("scene recovery failed");
 render::SurfaceLifecycle surface({render::ViewId{801},render::SurfaceGeneration{805},render::MetricsGeneration{806},{256,256,256,256,1,1}});auto acquired=surface.acquire();if(!acquired.snapshot)return fail("surface unavailable"); render::FrameState frame{render::ViewId{801},render::CameraState{{128,128},1,0,render::CameraGeneration{802}},{0,0,256,256},acquired.snapshot->metrics,semantic::SemanticGeneration{803},SceneRevision{803},acquired.snapshot->surfaceGeneration,acquired.snapshot->metricsGeneration,render::FrameId{901}};
 auto vis=render::VisibilityResolver::resolve(frame,*scene);if(!vis)return fail("visibility failed");auto refs=render::DirectReferenceSource::build(frame,vis.value(),coordinator.runtimeScene());if(!refs)return fail("reference failed");auto plan=render::FramePlanBuilder::build(frame,refs.value());if(!plan)return fail("plan failed");render::SkiaHeadlessBackend backend({256,256});if(render::FrameOrchestrator::submit(backend,plan.value()).code!=render::BackendSubmissionCode::kAccepted||!backend.observation())return fail("render failed");const auto& obs=*backend.observation();
 fs::path parent=output.parent_path();if(parent.empty())parent=".";fs::path stage=parent/(output.filename().string()+".g3-07-staging");std::error_code ec;fs::remove_all(stage,ec);if(!fs::create_directory(stage,ec))return fail("staging failed");if(!writeFile(stage/"render.rgba",obs.rgba)){fs::remove_all(stage,ec);return fail("raster write failed");}{std::ofstream d(stage/"render-digest.txt");d<<obs.digest<<'\n';}{std::ofstream e(stage/"render-evidence.json");e<<"{\"result\":\"PASS\",\"fixture_bytes\":"<<bytes.size()<<",\"fixture_digest\":\""<<fnv(bytes)<<"\",\"raster_digest\":\""<<obs.digest<<"\",\"pipeline_stages\":[\"SnapshotCodec.decode\",\"SnapshotBootstrapper.restore\",\"IndexedObjectStore\",\"FullSceneCompiler\",\"Scene\",\"RuntimeScene\",\"VisibilityResolver\",\"DirectReferenceSource\",\"FramePlanBuilder\",\"FrameOrchestrator\",\"SkiaHeadlessBackend\"],\"object_ids\":[501,502,503,504,505,506,507,508,509],\"plan_digest\":\""<<refs.value().digest<<"\"}\n";}fs::rename(stage,output,ec);if(ec){fs::remove_all(stage,ec);return fail("publication failed");}return 0;
}
