#pragma once
// Reference-only analytic material boundaries, derived without candidate pixels.
#include "canvas/render/direct_reference_source.hpp"
#include "canvas/scene/sketch_geometry.hpp"
#include "../../../runtime/render/src/skia_geometry_internal.hpp"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPathUtils.h"
namespace canvas::render::testing {
template<class Draw> void shapeMaterialBoundaries(const ReferenceTraversalEntry& entry,Draw&& draw){
 const auto* shape=std::get_if<ShapeReferenceCommand>(&entry.command);if(!shape)return;
 const auto& record=entry.record;const auto& content=shape->content.get();SkPathBuilder path;
 if(content.shape_kind==2)path.addOval(SkRect::MakeWH(content.width,content.height));
 else if(content.shape_kind==3){path.moveTo(content.width*.5,0);path.lineTo(content.width,content.height);path.lineTo(0,content.height);path.close();}
 else if(content.shape_kind==1)path.addRect(SkRect::MakeWH(content.width,content.height));else return;
 auto body=path.detach(),outline=body;double strokeScale=1;
 if(const auto derived=scene::sketchGeometry(record.content,record.properties)){body=internal::semanticPath(derived->body);outline=internal::semanticPath(derived->outline);strokeScale=derived->outlineWidthScale;}
 for(const auto& property:record.properties.entries){
  if(property.field_id==0x100U)if(const auto* fill=std::get_if<semantic::FillStyleValue>(&property.value))if(!std::holds_alternative<semantic::NoFill>(*fill))draw(body);
  if(property.field_id==0x101U)if(const auto* style=std::get_if<semantic::StrokeStyleValue>(&property.value))if(const auto* stroke=std::get_if<semantic::SolidStroke>(style);stroke&&stroke->color.a>0&&stroke->width>0){auto paint=internal::geometryStrokePaint(*stroke);paint.setStrokeWidth(stroke->width*strokeScale);draw(skpathutils::FillPathWithPaint(outline,paint));}
 }
}
}
