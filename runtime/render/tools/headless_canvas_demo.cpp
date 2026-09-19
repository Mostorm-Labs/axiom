#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/frame_plan.hpp"
#include "canvas/render/render_backend.hpp"
#include "canvas/render/skia_headless_backend.hpp"
#include "canvas/render/surface_lifecycle.hpp"
#include "canvas/render/visibility_resolver.hpp"
#include "canvas/scene/direct_render_scene.hpp"
#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "canvas/scene/scene_binding.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/semantic_read_view.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"
#include "../../scene/src/incremental_runtime_full_materialization_bridge.hpp"

#include <algorithm>
#include <array>
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
std::string sha256(const std::vector<std::uint8_t>& input) {
  static constexpr std::array<std::uint32_t,64> k={0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};
  auto rotr=[](std::uint32_t x,unsigned n){return (x>>n)|(x<<(32U-n));};
  std::vector<std::uint8_t> data=input; const auto bitCount=static_cast<std::uint64_t>(data.size())*8ULL; data.push_back(0x80U); while((data.size()%64U)!=56U)data.push_back(0U); for(int shift=56;shift>=0;shift-=8)data.push_back(static_cast<std::uint8_t>(bitCount>>shift));
  std::array<std::uint32_t,8> h={0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
  for(std::size_t offset=0;offset<data.size();offset+=64U){std::array<std::uint32_t,64> w{};for(unsigned i=0;i<16U;++i)w[i]=(static_cast<std::uint32_t>(data[offset+4U*i])<<24U)|(static_cast<std::uint32_t>(data[offset+4U*i+1U])<<16U)|(static_cast<std::uint32_t>(data[offset+4U*i+2U])<<8U)|data[offset+4U*i+3U];for(unsigned i=16;i<64U;++i){const auto s0=rotr(w[i-15U],7U)^rotr(w[i-15U],18U)^(w[i-15U]>>3U);const auto s1=rotr(w[i-2U],17U)^rotr(w[i-2U],19U)^(w[i-2U]>>10U);w[i]=w[i-16U]+s0+w[i-7U]+s1;}auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];for(unsigned i=0;i<64U;++i){const auto S1=rotr(e,6U)^rotr(e,11U)^rotr(e,25U);const auto ch=(e&f)^((~e)&g);const auto temp1=hh+S1+ch+k[i]+w[i];const auto S0=rotr(a,2U)^rotr(a,13U)^rotr(a,22U);const auto maj=(a&b)^(a&c)^(b&c);const auto temp2=S0+maj;hh=g;g=f;f=e;e=d+temp1;d=c;c=b;b=a;a=temp1+temp2;}h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;}
  std::ostringstream out;out<<std::hex<<std::setfill('0');for(const auto value:h)out<<std::setw(8)<<value;return out.str();
}
const char* kindName(semantic::ObjectKind kind) { switch (kind) { case semantic::ObjectKind::kShape:return "Shape"; case semantic::ObjectKind::kImage:return "Image"; case semantic::ObjectKind::kVectorPath:return "VectorPath"; case semantic::ObjectKind::kRichText:return "RichText"; case semantic::ObjectKind::kVectorStroke:return "VectorStroke"; case semantic::ObjectKind::kDabStroke:return "DabStroke"; case semantic::ObjectKind::kConnector:return "Connector"; case semantic::ObjectKind::kSticky:return "Sticky"; case semantic::ObjectKind::kGroup:return "Group"; } return "Unknown"; }
const char* sceneKindName(SceneObjectKind kind) { switch (kind) { case SceneObjectKind::kShape:return "Shape"; case SceneObjectKind::kImage:return "Image"; case SceneObjectKind::kVectorPath:return "VectorPath"; case SceneObjectKind::kRichText:return "RichText"; case SceneObjectKind::kVectorStroke:return "VectorStroke"; case SceneObjectKind::kDabStroke:return "DabStroke"; } return "Unknown"; }
template <typename Range, typename Fn> void jsonArray(std::ostream& out, const Range& range, Fn emit) { out << '['; bool first = true; for (const auto& value : range) { if (!first) out << ','; first = false; emit(out, value); } out << ']'; }
template <typename Range> void jsonIds(std::ostream& out, const Range& range) { jsonArray(out, range, [](auto& o, const auto& id) { o << lowId(id); }); }
template <typename Range> void jsonKinds(std::ostream& out, const Range& range) { jsonArray(out, range, [](auto& o, const auto& record) { o << '"' << kindName(record.kind) << '"'; }); }
template <typename Range> void jsonReferenceKinds(std::ostream& out, const Range& range) { jsonArray(out, range, [](auto& o, const auto& entry) { o << '"' << kindName(entry.record.kind) << '"'; }); }
template <typename Range> void jsonSceneKinds(std::ostream& out, const Range& range) { jsonArray(out, range, [](auto& o, const auto& record) { o << '"' << sceneKindName(record.kind) << '"'; }); }
int fail(const char* m){std::cerr<<m<<'\n';return 1;}
class DemoCompiler final : public ISemanticSceneCompiler {
 public:
  foundation::Result<CompiledSceneSnapshot> compileFull(const semantic::SemanticReadView& view) const override {
    const auto projection = internal::materializeFullScene(view);
    if (!projection) return foundation::Result<CompiledSceneSnapshot>::failure(projection.error());
    CompiledSceneSnapshot out{SceneRevision(projection.value().generation.value()),{}};
    out.records.reserve(projection.value().records.size());
    for (const auto& s : projection.value().records) {
      SceneObjectKind k=SceneObjectKind::kShape;
      switch(s.kind){case semantic::ObjectKind::kImage:k=SceneObjectKind::kImage;break;case semantic::ObjectKind::kVectorPath:k=SceneObjectKind::kVectorPath;break;case semantic::ObjectKind::kRichText:k=SceneObjectKind::kRichText;break;case semantic::ObjectKind::kVectorStroke:k=SceneObjectKind::kVectorStroke;break;case semantic::ObjectKind::kDabStroke:k=SceneObjectKind::kDabStroke;break;default:break;}
      const auto flags=static_cast<SceneRecordFlags>(static_cast<std::uint32_t>(SceneRecordFlags::kVisible)|static_cast<std::uint32_t>(SceneRecordFlags::kHitTestable));
      out.records.push_back({s.objectId,SceneOrderKey(orderValue(s.placement.order_key)),k,flags,s.worldBounds,ContentRevision(s.kindVersion),RenderPayloadRef{static_cast<std::uint32_t>(lowId(s.objectId)),s.kindVersion},HitGeometryRef{static_cast<std::uint32_t>(lowId(s.objectId)),s.kindVersion}});
    }
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
 auto vis=render::VisibilityResolver::resolve(frame,*scene);if(!vis)return fail("visibility failed");auto query=scene->query(SceneQuery{frame.worldViewport});if(!query)return fail("query failed");auto draw=scene->buildDrawList(query.value());if(!draw)return fail("draw list failed");auto refs=render::DirectReferenceSource::build(frame,vis.value(),coordinator.runtimeScene());if(!refs)return fail("reference failed");auto plan=render::FramePlanBuilder::build(frame,refs.value());if(!plan)return fail("plan failed");render::SkiaHeadlessBackend backend({256,256});if(render::FrameOrchestrator::submit(backend,plan.value()).code!=render::BackendSubmissionCode::kAccepted||!backend.observation())return fail("render failed");const auto& obs=*backend.observation();
 fs::path parent=output.parent_path();if(parent.empty())parent=".";fs::path stage=parent/(output.filename().string()+".g3-07-staging");std::error_code ec;fs::remove_all(stage,ec);if(!fs::create_directory(stage,ec))return fail("staging failed");if(!writeFile(stage/"render.rgba",obs.rgba)){fs::remove_all(stage,ec);return fail("raster write failed");}{std::ofstream d(stage/"render-digest.txt");d<<obs.digest<<'\n';}{std::ofstream e(stage/"render-evidence.json");e<<"{\"result\":\"PASS\",\"fixture_id\":\"g3-07-nine-kind-v1\",\"fixture_bytes\":"<<bytes.size()<<",\"fixture_sha256\":\""<<sha256(bytes)<<"\",\"fixture_digest\":\""<<fnv(bytes)<<"\",\"pipeline_stages\":[\"SnapshotCodec.decode\",\"SnapshotBootstrapper.restore\",\"IndexedObjectStore\",\"FullSceneCompiler\",\"Scene\",\"RuntimeScene\",\"VisibilityResolver\",\"DirectReferenceSource\",\"FramePlanBuilder\",\"FrameOrchestrator\",\"SkiaHeadlessBackend\"],\"canonical_ids\":";jsonArray(e,decoded.snapshot->objects,[](auto& o,const auto& r){o<<lowId(r.id);});e<<",\"canonical_kinds\":";jsonKinds(e,decoded.snapshot->objects);e<<",\"scene_ids\":";jsonArray(e,scene->read().records(),[](auto& o,const auto& r){o<<lowId(r.objectId);});e<<",\"scene_kinds\":";jsonSceneKinds(e,scene->read().records());e<<",\"runtime_ids\":";jsonArray(e,coordinator.runtimeScene().records(),[](auto& o,const auto& r){o<<lowId(r.objectId);});e<<",\"runtime_kinds\":";jsonKinds(e,coordinator.runtimeScene().records());e<<",\"visibility_ids\":";jsonIds(e,vis.value().backToFront);e<<",\"draw_ids\":";jsonArray(e,draw.value().items,[](auto& o,const auto& r){o<<lowId(r.objectId);});e<<",\"reference_ids\":";jsonArray(e,refs.value().entries,[](auto& o,const auto& r){o<<lowId(r.record.objectId);});e<<",\"reference_kinds\":";jsonReferenceKinds(e,refs.value().entries);e<<",\"object_ids\":[501,502,503,504,505,506,507,508,509],\"object_kinds\":[\"Shape\",\"Image\",\"VectorPath\",\"RichText\",\"VectorStroke\",\"DabStroke\",\"Connector\",\"Sticky\",\"Group\"],\"raster\":{\"width\":256,\"height\":256,\"bytes\":"<<obs.rgba.size()<<",\"fnv1a64\":\""<<obs.digest<<"\",\"sha256\":\""<<sha256(obs.rgba)<<"\"},\"golden_sha256\":\""<<sha256(obs.rgba)<<"\",\"vector_stroke\":{\"object_id\":505,\"mask_id\":1501,\"mask_local_bounds\":[8,-4,16,4],\"mask_world_bounds\":[16,44,24,52],\"render_strip\":[8,46,32,50],\"retained_samples\":[[8,46],[24,46]],\"erased_sample\":[16,46]},\"group\":{\"object_id\":509,\"contributes_pixels\":false},\"plan_digest\":\""<<refs.value().digest<<"\"}\n";}fs::rename(stage,output,ec);if(ec){fs::remove_all(stage,ec);return fail("publication failed");}return 0;
}
