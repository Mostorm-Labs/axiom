// Verification-only uncached CPU table reference from canonical SnapshotCodec.
#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/table_content.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/frame_plan.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/scene/scene_types.hpp"
#include "canvas/scene/sketch_geometry.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
using namespace canvas;using nlohmann::json;
void need(bool v,const char*m){if(!v)throw std::runtime_error(m);}
std::string hex(foundation::ObjectId id){std::ostringstream s;for(auto b:id.bytes)s<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(b);return s.str();}
semantic::SemanticSnapshot decode(const char*p){std::ifstream f(p,std::ios::binary);std::vector<std::uint8_t>b{std::istreambuf_iterator<char>(f),{}};auto d=semantic::SnapshotCodec::decode(b,semantic::SnapshotCapability::kAllWithCompactStrokeV1);need(d.ok(),"canonical decode");return std::move(*d.snapshot);}
int main(int argc,char**argv){try{
 need(argc==4,"usage snapshot request outputprefix");auto s=decode(argv[1]);std::ifstream in(argv[2]);const json q=json::parse(in);const semantic::ObjectRecord* original=nullptr;for(const auto&o:s.objects)if(o.kind==semantic::ObjectKind::kTable&&(q.value("objectId",std::string{})==""||hex(o.id)==q["objectId"].get<std::string>()))original=&o;need(original,"table absent");auto o=*original;
 const auto& table=std::get<semantic::TableContent>(o.content);for(const auto& c:table.cells)for(const auto&p:c.text.document.paragraphs)for(const auto&r:p.runs)need(r.text.empty(),"nonempty table text needs external font oracle");
 const unsigned width=q.value("width",2862U),height=q.value("height",2020U);const double lw=q.value("logical_width",1431.),lh=q.value("logical_height",1010.);need(width&&height&&std::uint64_t(width)*height<=16777216,"reference dimensions");const auto camera=q.value("camera",json{{"scale",1},{"x",0},{"y",0}});const double scale=camera["scale"],tx=camera["x"],ty=camera["y"];need(scale>0,"camera scale");
 const auto delta=q.value("translation",std::array<double,2>{0,0});o.transform.tx+=delta[0];o.transform.ty+=delta[1];
 RuntimeScene scene;const auto projected=projectRuntimeScene(std::span(&o,1),semantic::SemanticGeneration{1});need(projected.records.size()==1,"table projection");
 const render::FrameState frame{render::ViewId{1},render::CameraState{{float((lw*.5-tx)/scale),float((lh*.5-ty)/scale)},float(scale),0,render::CameraGeneration{1}},{float(-tx/scale),float(-ty/scale),float((lw-tx)/scale),float((lh-ty)/scale)},{float(lw),float(lh),width,height,float(width/lw),1},semantic::SemanticGeneration{1},SceneRevision{1},render::SurfaceGeneration{1},render::MetricsGeneration{1},render::FrameId{1},{0,0,float(lw),float(lh)}};
 render::ReferenceDrawList list{frame};list.viewportClip=frame.viewportClip;list.worldToView={scale,0,0,scale,tx,ty};const auto& record=projected.records.front();list.entries.push_back({record,true,render::TableReferenceCommand{scene::ImmutableValue<semantic::TableContent>::alias(record.content),nullptr}});const render::FramePlan plan{frame,std::move(list),s.background,false,true};
 auto surface=SkSurfaces::Raster(SkImageInfo::Make(width,height,kRGBA_8888_SkColorType,kPremul_SkAlphaType,SkColorSpace::MakeSRGB()));need(bool(surface),"reference surface");const auto result=render::internal::drawReferencePlanToSkCanvas(*surface->getCanvas(),plan,nullptr,nullptr,true,nullptr);need(result.code==render::BackendSubmissionCode::kAccepted,"uncached table reference");std::vector<std::uint8_t>pixels(std::size_t(width)*height*4);need(surface->readPixels(surface->imageInfo(),pixels.data(),width*4,0,0),"reference readback");std::ofstream out(std::string(argv[3])+".rgba",std::ios::binary);out.write((char*)pixels.data(),pixels.size());out.close();
 json props=json::array();for(const auto&p:o.properties.entries){json row{{"field_id",p.field_id},{"variant",p.value.index()}};if(const auto*v=std::get_if<semantic::SketchStyleV1>(&p.value))row["sketch"]={{"enabled",v->enabled},{"roughness",v->roughness},{"seed",std::to_string(v->seed)},{"version",v->version}};props.push_back(row);}
 const auto sketch=scene::sketchGeometry(o.content,o.properties);json meta{{"status","REFERENCE_RENDERED"},{"objectId",hex(o.id)},{"transform",{o.transform.a,o.transform.b,o.transform.c,o.transform.d,o.transform.tx,o.transform.ty}},{"width",semantic::tableWidth(table)},{"height",semantic::tableHeight(table)},{"grid",{{"width",table.grid.width},{"color",{table.grid.color.r,table.grid.color.g,table.grid.color.b,table.grid.color.a}}}},{"rows",json::array()},{"columns",json::array()},{"properties",props},{"sketch_present",bool(sketch)},{"outlineWidthScale",sketch?sketch->outlineWidthScale:1.},{"pixel_width",width},{"pixel_height",height},{"request",q},{"scope","Existing shared Table painter CPU Raster Direct, cache/prepared/text absent; independent of GPU/cache/scheduler, shares table geometry algorithm; no candidate pixels."}};
 for(const auto&a:table.rows)meta["rows"].push_back({{"id",hex(a.id)},{"size",a.size}});for(const auto&a:table.columns)meta["columns"].push_back({{"id",hex(a.id)},{"size",a.size}});std::ofstream(std::string(argv[3])+".json")<<meta.dump(2)<<'\n';std::cout<<meta.dump()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
