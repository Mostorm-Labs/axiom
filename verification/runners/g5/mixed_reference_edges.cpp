// Standalone verification only. No candidate image/color input is accepted.
#include "mixed_reference_material_boundary.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/table_content.hpp"
#include "canvas/scene/scene_types.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkMatrix.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace canvas;using nlohmann::json;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::string& p){std::ifstream f(p,std::ios::binary);require(bool(f),"input missing");return {std::istreambuf_iterator<char>(f),{}};}
void write(const std::string& p,std::span<const std::uint8_t> bytes){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(bool(f),"output write");}
std::size_t textBytes(const semantic::RichTextContent& value){std::size_t n=0;for(const auto& p:value.document.paragraphs)for(const auto& r:p.runs)n+=r.text.size();return n;}
render::ReferenceTraversalEntry entry(semantic::ObjectContent c,semantic::ObjectKind kind){semantic::ObjectRecord o;o.id=foundation::ObjectId::fromUint64(1);o.kind=kind;o.kind_version=1;o.content=std::move(c);auto records=projectRuntimeScene({&o,1},semantic::SemanticGeneration{1});render::ReferenceTraversalEntry e;e.record=records.records.front();if(kind==semantic::ObjectKind::kTable)e.command=render::TableReferenceCommand{std::get<semantic::TableContent>(o.content)};else e.command=render::StickyReferenceCommand{std::get<semantic::StickyContent>(o.content)};return e;}
void boundaries(const render::ReferenceTraversalEntry& e,const std::function<void(const SkPath&)>& draw){render::testing::mixed_reference::materialBoundaries(e,draw);}
void micro(){
 semantic::TableContent table;table.rows={{{},48},{{},48}};table.columns={{{},120},{{},120}};table.cells.resize(4);
 auto surface=SkSurfaces::Raster(SkImageInfo::MakeA8(256,128));surface->getCanvas()->clear(SK_ColorTRANSPARENT);
 SkPaint paint;paint.setColor(SK_ColorWHITE);paint.setStyle(SkPaint::kStroke_Style);paint.setStrokeWidth(2);paint.setAntiAlias(true);
 boundaries(entry(table,semantic::ObjectKind::kTable),[&](const SkPath& path){surface->getCanvas()->drawPath(path,paint);});
 std::vector<std::uint8_t> b(256*128);require(surface->readPixels(SkImageInfo::MakeA8(256,128),b.data(),256,0,0),"micro read");
 std::cout<<json{{"table_internal_boundary",unsigned(b[48*256+60])},{"table_interior",unsigned(b[24*256+60])}}.dump()<<'\n';
 require(b[48*256+60]>0,"OLD_HELPER_OMITS_TABLE_INTERNAL_GRID_MATERIAL_BOUNDARY");
 require(b[24*256+60]==0,"whole table interior excluded");
 surface->getCanvas()->clear(SK_ColorTRANSPARENT);boundaries(entry(semantic::StickyContent{200,100},semantic::ObjectKind::kSticky),[&](const auto& p){surface->getCanvas()->drawPath(p,paint);});
 require(surface->readPixels(SkImageInfo::MakeA8(256,128),b.data(),256,0,0),"sticky read");require(b[100*256+60]>0&&b[50*256+60]==0,"OLD_HELPER_OMITS_STICKY_DEFAULT_FILL_BOUNDARY");
}
}
int main(int argc,char** argv)try{
 if(argc==2&&std::string_view(argv[1])=="--micro"){micro();return 0;}
 require(argc==3,"request outputprefix");const auto request=json::parse(read(argv[1]));const std::string prefix=argv[2];
 auto decoded=semantic::SnapshotCodec::decode(read(request.at("snapshot")),semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(decoded.ok(),"typed Snapshot decode");
 const auto& snapshot=*decoded.snapshot;
 const unsigned width=request.at("width"),height=request.at("height");require(width>0&&height>0&&std::uint64_t(width)*height<=24000000,"edge target capacity");
 const double lw=request.at("logical_width"),lh=request.at("logical_height");require(lw>0&&lh>0,"logical target");
 const auto camera=request.at("camera");const float zoom=camera.at("scale"),tx=camera.at("x"),ty=camera.at("y");
 const auto projection=projectRuntimeScene(snapshot.objects,semantic::SemanticGeneration{1});
 auto surface=SkSurfaces::Raster(SkImageInfo::MakeA8(width,height));require(bool(surface),"edge target allocation");surface->getCanvas()->clear(SK_ColorTRANSPARENT);
 SkPaint paint;paint.setColor(SK_ColorWHITE);paint.setStyle(SkPaint::kStroke_Style);paint.setStrokeWidth(2);paint.setAntiAlias(true);
 json evidence={{"typed_objects",snapshot.objects.size()},{"edge_width_device_pixels",2},{"source_only",true},{"objects",json::array()}};
 std::size_t unsupported=0;
 for(std::size_t i=0;i<snapshot.objects.size();++i){const auto& object=snapshot.objects[i];
  const auto found=std::find_if(projection.records.begin(),projection.records.end(),[&](const auto& r){return r.objectId==object.id;});require(found!=projection.records.end(),"projected object identity");
  const auto& record=*found;render::ReferenceTraversalEntry e;e.record=record;
  require(!object.placement.parent_id,"parented material outside literal root qualification");
  const auto& affine=object.transform;require(std::isfinite(affine.a)&&std::isfinite(affine.b)&&std::isfinite(affine.c)&&std::isfinite(affine.d)&&std::isfinite(affine.tx)&&std::isfinite(affine.ty),"invalid material affine");
  if(const auto* table=std::get_if<semantic::TableContent>(&object.content)){
   std::size_t bytes=0;for(const auto& cell:table->cells)bytes+=textBytes(cell.text);
   json rows=json::array(),cols=json::array();for(const auto& r:table->rows)rows.push_back(r.size);for(const auto& c:table->columns)cols.push_back(c.size);
   evidence["objects"].push_back({{"kind","Table"},{"rows",rows},{"columns",cols},{"cells",table->cells.size()},{"text_bytes",bytes},{"grid_width",table->grid.width}});
   if(bytes){++unsupported;continue;}e.command=render::TableReferenceCommand{*table};
  }else if(const auto* sticky=std::get_if<semantic::StickyContent>(&object.content)){
   // Sticky v1 contains dimensions only; no embedded label field. Child text
   // would remain a separate typed object and is checked below.
   evidence["objects"].push_back({{"kind","Sticky"},{"width",sticky->width},{"height",sticky->height},{"embedded_text_bytes",0},{"kind_version",object.kind_version}});e.command=render::StickyReferenceCommand{*sticky};
  }else if(const auto* image=std::get_if<semantic::ImageContent>(&object.content)){
   evidence["image"]={{"intrinsic_width",image->intrinsic_width},{"intrinsic_height",image->intrinsic_height},{"width",image->width},{"height",image->height},{"mode",unsigned(image->content_mode)},{"pose",{object.transform.a,object.transform.b,object.transform.c,object.transform.d,object.transform.tx,object.transform.ty}}};continue;
  }else {
   if(const auto* text=std::get_if<semantic::RichTextContent>(&object.content);text&&textBytes(*text))++unsupported;
   if(const auto* shape=std::get_if<semantic::ShapeContent>(&object.content);shape&&shape->label&&textBytes(*shape->label))++unsupported;
   continue;
  }
  const auto& t=object.transform;SkMatrix world=SkMatrix::MakeAll(zoom,0,tx,0,zoom,ty,0,0,1);
  SkMatrix pose=SkMatrix::MakeAll(float(t.a),float(t.c),float(t.tx),float(t.b),float(t.d),float(t.ty),0,0,1);
  auto matrix=SkMatrix::Scale(float(width/lw),float(height/lh));matrix.preConcat(world);matrix.preConcat(pose);
  const auto work=render::testing::mixed_reference::materialBoundaries(e,[&](const SkPath& path){surface->getCanvas()->drawPath(path.makeTransform(matrix),paint);});
  evidence["objects"].back()["fills"]=work.fills;evidence["objects"].back()["strokes"]=work.strokes;unsupported+=work.unsupported;
 }
 std::vector<std::uint8_t> edge(std::size_t(width)*height);require(surface->readPixels(SkImageInfo::MakeA8(width,height),edge.data(),width,0,0),"edge readback");
 for(auto& v:edge)v=v?1:0;write(prefix+".edge",edge);evidence["unsupported_text_or_material"]=unsupported;evidence["edge_pixels"]=std::count(edge.begin(),edge.end(),1);
 std::ofstream(prefix+".json")<<evidence.dump(2)<<'\n';std::cout<<evidence.dump()<<'\n';require(unsupported==0,"visible text/material outside boundary qualification");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
