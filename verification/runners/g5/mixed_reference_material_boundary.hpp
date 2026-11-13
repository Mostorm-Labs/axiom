#pragma once
// Verification-only material interfaces. No actual pixels or mismatch positions.
#include "cpu_reference_boundary.hpp"
#include "canvas/semantic/table_content.hpp"
#include "canvas/scene/sketch_geometry.hpp"
#include "../../../runtime/render/src/skia_geometry_internal.hpp"
namespace canvas::render::testing::mixed_reference {
struct MaterialBoundaryWork {std::size_t fills=0,strokes=0;bool unsupported=false;};
template<class Draw>MaterialBoundaryWork materialBoundaries(const ReferenceTraversalEntry& entry,Draw&& draw){
 MaterialBoundaryWork work;const auto& record=entry.record;
 const auto fill=[&](const SkPath& p){draw(p);++work.fills;};
 const auto stroke=[&](const SkPath& p,const SkPaint& paint){draw(skpathutils::FillPathWithPaint(p,paint));++work.strokes;};
 if(const auto* command=std::get_if<TableReferenceCommand>(&entry.command)){
  const auto& t=command->content.get();
  if(t.rows.empty()||t.columns.empty()||t.cells.size()!=t.rows.size()*t.columns.size()){work.unsupported=true;return work;}
  for(std::size_t i=0;i<t.cells.size();++i)if(t.cells[i].fill.a>0){
   const auto rect=semantic::tableCellRect(t,i);SkPathBuilder p;p.addRect(SkRect::MakeLTRB(float(rect[0]),float(rect[1]),float(rect[2]),float(rect[3])));fill(p.detach());}
  if(t.grid.width>0&&t.grid.color.a>0){
   SkPaint p;p.setStyle(SkPaint::kStroke_Style);p.setStrokeWidth(float(t.grid.width));p.setAntiAlias(true);
   const auto derived=scene::sketchGeometry(record.content,record.properties);
   if(derived){p.setStrokeWidth(float(t.grid.width*derived->outlineWidthScale));stroke(internal::semanticPath(derived->outline),p);}
   else {double x=0,y=0;const float w=float(semantic::tableWidth(t)),h=float(semantic::tableHeight(t));
    const auto line=[&](float x0,float y0,float x1,float y1){SkPathBuilder b;b.moveTo(x0,y0);b.lineTo(x1,y1);stroke(b.detach(),p);};
    line(0,0,0,h);for(const auto& c:t.columns){x+=c.size;line(float(x),0,float(x),h);}
    line(0,0,w,0);for(const auto& r:t.rows){y+=r.size;line(0,float(y),w,float(y));}}
  }
  // Active-cell editing chrome is not a canonical material boundary.
  return work;
 }
 if(const auto* command=std::get_if<StickyReferenceCommand>(&entry.command)){
  const auto& c=command->content.get();const auto derived=scene::sketchGeometry(record.content,record.properties);
  SkPathBuilder rect;rect.addRect(SkRect::MakeWH(float(c.width),float(c.height)));
  const auto regular=rect.detach();const auto body=derived?internal::semanticPath(derived->body):regular;
  semantic::FillStyleValue style=semantic::SolidFill{{1,1,.5F,1}};
  for(const auto& prop:record.properties.entries)if(prop.field_id==0x100U)if(const auto* s=std::get_if<semantic::FillStyleValue>(&prop.value))style=*s;
  if(const auto* solid=std::get_if<semantic::SolidFill>(&style);solid&&solid->color.a>0)fill(body);
  if(std::holds_alternative<semantic::PatternFillV1>(style)){work.unsupported=true;return work;}
  for(const auto& prop:record.properties.entries)if(prop.field_id==0x101U)
   if(const auto* style=std::get_if<semantic::StrokeStyleValue>(&prop.value))
    if(const auto* s=std::get_if<semantic::SolidStroke>(style);s&&s->color.a>0&&s->width>0){
     auto p=internal::geometryStrokePaint(*s);if(derived)p.setStrokeWidth(float(s->width*derived->outlineWidthScale));
     stroke(derived?internal::semanticPath(derived->outline):regular,p);}
  return work;
 }
 return work;
}
}
