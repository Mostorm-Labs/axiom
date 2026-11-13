// Focused verification microfixtures, never full-scene or GPU rendering.
#include "mixed_reference_material_boundary.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/render/frame_plan.hpp"
#include "canvas/scene/scene_types.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace canvas;using nlohmann::json;
namespace {
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
std::string prefix;json artifactRows=json::array();
void persist(const char* name,const std::vector<std::uint8_t>& a,const std::vector<std::uint8_t>& b,const std::vector<std::uint8_t>& e){
 const auto save=[](const std::string& p,const auto& data){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),data.size());};
 const auto base=prefix+"-"+name;save(base+"-actual.rgba",a);save(base+"-expected.rgba",b);save(base+".edge",e);
 artifactRows.push_back({{"case",name},{"actual",base+"-actual.rgba"},{"expected",base+"-expected.rgba"},{"edge",base+".edge"}});
}
render::ReferenceTraversalEntry entry(semantic::ObjectRecord object){auto projection=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});render::ReferenceTraversalEntry e;e.record=projection.records.front();if(object.kind==semantic::ObjectKind::kTable)e.command=render::TableReferenceCommand{std::get<semantic::TableContent>(object.content)};else e.command=render::StickyReferenceCommand{std::get<semantic::StickyContent>(object.content)};return e;}
std::vector<std::uint8_t> rgba(const semantic::ObjectRecord& object){
 render::FrameState frame{render::ViewId{1},render::CameraState{{160,90},1,0,render::CameraGeneration{1}},{0,0,320,180},{320,180,320,180,1,1},semantic::SemanticGeneration{1},SceneRevision{1},render::SurfaceGeneration{1},render::MetricsGeneration{1},render::FrameId{1},{0,0,320,180}};
 render::ReferenceDrawList draw{frame};draw.viewportClip=frame.viewportClip;draw.entries={entry(object)};render::FramePlan plan{frame,std::move(draw),{},false,true};
 auto s=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320,180));require(render::internal::drawReferencePlanToSkCanvas(*s->getCanvas(),plan).code==render::BackendSubmissionCode::kAccepted,"actual original micro mapper");
 std::vector<std::uint8_t> out(320*180*4);require(s->readPixels(SkImageInfo::Make(320,180,kRGBA_8888_SkColorType,kPremul_SkAlphaType),out.data(),1280,0,0),"micro read");return out;
}
std::vector<std::uint8_t> edge(const semantic::ObjectRecord& object){auto surface=SkSurfaces::Raster(SkImageInfo::MakeA8(320,180));surface->getCanvas()->clear(SK_ColorTRANSPARENT);
 SkPaint p;p.setStyle(SkPaint::kStroke_Style);p.setStrokeWidth(2);p.setAntiAlias(true);p.setColor(SK_ColorWHITE);
 const auto& t=object.transform;const auto matrix=SkMatrix::MakeAll(float(t.a),float(t.c),float(t.tx),float(t.b),float(t.d),float(t.ty),0,0,1);
 auto e=entry(object);const auto work=render::testing::mixed_reference::materialBoundaries(e,[&](const SkPath& path){surface->getCanvas()->drawPath(path.makeTransform(matrix),p);});require(!work.unsupported,"micro material unsupported");
 std::vector<std::uint8_t> out(320*180);require(surface->readPixels(SkImageInfo::MakeA8(320,180),out.data(),320,0,0),"edge read");return out;
}
std::size_t compare(const std::vector<std::uint8_t>& a,const std::vector<std::uint8_t>& b,const std::vector<std::uint8_t>& edge){std::size_t bad=0;for(std::size_t i=0;i<edge.size();++i)if(!edge[i]){unsigned delta=0;for(unsigned c=0;c<4;++c)delta=std::max(delta,unsigned(std::abs(int(a[4*i+c])-int(b[4*i+c]))));bad+=delta>2;}return bad;}
}
int main(int argc,char** argv)try{
 require(argc==2,"output.json");prefix=std::string(argv[1])+".micro";json rows=json::array();
 semantic::TableContent t;t.rows={{{},48},{{},48}};t.columns={{{},120},{{},120}};t.cells.resize(4);t.grid.width=12;
 semantic::ObjectRecord table;table.id=foundation::ObjectId::fromUint64(1);table.kind=semantic::ObjectKind::kTable;table.kind_version=1;table.content=t;table.transform.tx=20;table.transform.ty=20;
 auto expected=rgba(table);const auto band=edge(table);require(compare(expected,expected,band)==0,"valid table rejected");
 auto wrongFill=table;std::get<semantic::TableContent>(wrongFill.content).cells.front().fill={1,0,0,1};persist("wrong_table_cell_color",rgba(wrongFill),expected,band);const auto fill=compare(rgba(wrongFill),expected,band);require(fill>100,"bad table interior hidden");rows.push_back({{"case","wrong_table_cell_interior_color"},{"rejected_pixels",fill}});
 auto omitted=table;std::get<semantic::TableContent>(omitted.content).grid.width=0;persist("omitted_grid",rgba(omitted),expected,band);const auto grid=compare(rgba(omitted),expected,band);require(grid>100,"omitted wide grid hidden by boundary");rows.push_back({{"case","omitted_internal_and_outer_grid"},{"rejected_pixels",grid}});
 auto pose=table;pose.transform.tx+=12;persist("wrong_table_pose",rgba(pose),expected,band);const auto shifted=compare(rgba(pose),expected,band);require(shifted>100,"wrong table pose hidden");rows.push_back({{"case","wrong_table_pose"},{"rejected_pixels",shifted}});
 auto wrongGrid=table;std::get<semantic::TableContent>(wrongGrid.content).grid.color={1,0,0,1};persist("wrong_grid_color",rgba(wrongGrid),expected,band);const auto color=compare(rgba(wrongGrid),expected,band);require(color>100,"wrong grid interior color hidden");rows.push_back({{"case","wrong_grid_color"},{"rejected_pixels",color}});
 semantic::ObjectRecord sticky;sticky.id=foundation::ObjectId::fromUint64(2);sticky.kind=semantic::ObjectKind::kSticky;sticky.kind_version=1;sticky.content=semantic::StickyContent{200,100};sticky.transform.tx=20;sticky.transform.ty=20;
 expected=rgba(sticky);const auto stickyBand=edge(sticky);auto red=sticky;red.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{1,0,0,1}}}}};persist("wrong_sticky_fill",rgba(red),expected,stickyBand);const auto redPixels=compare(rgba(red),expected,stickyBand);require(redPixels>1000,"wrong sticky fill hidden");rows.push_back({{"case","wrong_sticky_default_fill"},{"rejected_pixels",redPixels}});
 auto move=sticky;move.transform.ty+=10;persist("wrong_sticky_pose",rgba(move),expected,stickyBand);const auto posePixels=compare(rgba(move),expected,stickyBand);require(posePixels>1000,"wrong sticky pose hidden");rows.push_back({{"case","wrong_sticky_pose"},{"rejected_pixels",posePixels}});
 auto broad=stickyBand;std::fill(broad.begin(),broad.end(),1);require(compare(rgba(red),expected,broad)==0&&redPixels>1000,"negative doesn't detect wholeAABBexclusion failure");
 json result={{"artifacts",artifactRows},{"status","PASS"},{"source_only_band_width_device_pixels",2},{"exterior_channel_threshold",2},{"rows",rows}};std::ofstream(argv[1])<<result.dump(2)<<'\n';std::cout<<result.dump()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
