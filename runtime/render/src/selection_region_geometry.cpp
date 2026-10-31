#include "canvas/render/selection_region_geometry.hpp"
#if CANVAS_RENDER_HAS_SKIA
#include "skia_geometry_internal.hpp"
#include "canvas/render/brush_outline_skia_path.hpp"
#include "canvas/render/image_crop_geometry.hpp"
#include "canvas/text/text_layout.hpp"
#include "canvas/semantic/table_content.hpp"
#include "include/core/SkMatrix.h"
#include "include/core/SkPathUtils.h"
#include "include/pathops/SkPathOps.h"
#include <algorithm>
#include <cmath>
#include <new>
namespace canvas::render {
namespace {
using Hit=SelectionGeometryHit;
float f(double v){return static_cast<float>(v);}
SkRect rect(double x,double y,double w,double h){return SkRect::MakeXYWH(f(x),f(y),f(w),f(h));}
SkPath rectangle(double x,double y,double w,double h){SkPathBuilder b;b.addRect(rect(x,y,w,h));return b.detach();}
SkPath stroke(const SkPath& path,double width,SkPaint::Cap cap=SkPaint::kButt_Cap) {
    SkPaint p;p.setStyle(SkPaint::kStroke_Style);p.setStrokeWidth(f(width));p.setStrokeCap(cap);p.setStrokeJoin(SkPaint::kRound_Join);
    return skpathutils::FillPathWithPaint(path,p);
}
bool unite(SkPath& a,const SkPath& b){auto r=Op(a,b,kUnion_SkPathOp);if(!r)return false;a=*r;return true;}
bool fillVisible(const RuntimeSceneRecord& r) {
    for(const auto& p:r.properties.entries)if(p.field_id==0x100U)if(const auto* v=std::get_if<semantic::FillStyleValue>(&p.value)) {
        const auto* solid=std::get_if<semantic::SolidFill>(v);return solid&&solid->color.a>0;
    }
    return true;
}
Hit geometry(const RuntimeSceneRecord& r,SkPath& path) {
    switch(r.kind) {
    case semantic::ObjectKind::kVectorPath: {
        const auto* c=std::get_if<semantic::VectorPathContent>(&r.content);if(!c)return Hit::kInvalid;
        if(fillVisible(r))path=internal::semanticPath(c->geometry);break;
    }
    case semantic::ObjectKind::kVectorStroke: {
        if(const auto* c=std::get_if<semantic::BrushStrokeContent>(&r.content)) {
            if(c->stroke.snapshot.paint.alpha*c->stroke.snapshot.paint.opacity>0)
                path=buildVectorBrushOutlineSkPath(c->stroke.vector_output.outline,c->stroke.vector_output.fill_rule,c->stroke.vector_output.closed);
        } else if(const auto* legacy=std::get_if<semantic::VectorStrokeContent>(&r.content)) {
            const auto* data=std::get_if<semantic::VectorStrokeData>(&legacy->stroke.data);if(!data)return Hit::kInvalid;
            if(legacy->stroke.brush.opacity*legacy->stroke.brush.color.a<=0)break;
            for(std::size_t i=1;i<data->samples.size();++i) {
                const auto a=data->samples[i-1].position,b=data->samples[i].position;
                SkPathBuilder line;line.moveTo(f(a.x),f(a.y));line.lineTo(f(b.x),f(b.y));
                if(!unite(path,stroke(line.detach(),legacy->stroke.brush.nominal_size)))return Hit::kInvalid;
            }
        } else return Hit::kInvalid;
        break;
    }
    case semantic::ObjectKind::kDabStroke: {
        const semantic::DabStrokeData* data=nullptr;bool disk=false;double alpha=0;
        if(const auto* c=std::get_if<semantic::DabBrushStrokeContent>(&r.content)) {
            // Textured chalk/membrane alpha coverage is not a disk oracle.
            if(c->stroke.snapshot.material_mode!=1||c->stroke.snapshot.package_revision>=3)return Hit::kUnsupported;
            data=&c->stroke.dab_output;disk=true;alpha=c->stroke.snapshot.paint.alpha*c->stroke.snapshot.paint.opacity;
        } else if(const auto* legacy=std::get_if<semantic::DabStrokeContent>(&r.content)) {
            data=std::get_if<semantic::DabStrokeData>(&legacy->stroke.data);alpha=legacy->stroke.brush.opacity*legacy->stroke.brush.color.a;
        }
        if(!data)return Hit::kInvalid;if(alpha<=0)break;
        for(const auto& d:data->dabs)if(d.opacity>0&&d.size>0) {
            SkPathBuilder b;const auto area=rect(d.center.x-d.size/2,d.center.y-d.size/2,d.size,d.size);
            if(disk)b.addOval(area);else b.addRect(area);
            if(!unite(path,b.detach()))return Hit::kInvalid;
        }
        break;
    }
    case semantic::ObjectKind::kConnector: {
        const auto* g=std::get_if<scene::ConnectorGeometry>(&r.derivedGeometry);
        if(!g||g->status!=runtime::ConnectorRouteStatus::kReady||g->route.size()<2)return Hit::kUnsupported;
        semantic::SolidStroke style{{1,1,1,1},4};
        for(const auto& p:r.properties.entries)if(p.field_id==0x101U)if(const auto* v=std::get_if<semantic::StrokeStyleValue>(&p.value)) {
            const auto* solid=std::get_if<semantic::SolidStroke>(v);if(!solid)return Hit::kMiss;style=*solid;
        }
        if(style.width<=0||style.color.a<=0)return Hit::kMiss;
        for(std::size_t i=1;i<g->route.size();++i) {
            SkPathBuilder b;b.moveTo(g->route[i-1].x,g->route[i-1].y);b.lineTo(g->route[i].x,g->route[i].y);
            if(!unite(path,skpathutils::FillPathWithPaint(b.detach(),internal::geometryStrokePaint(style))))return Hit::kInvalid;
        }
        if(g->arrow_end) {
            const auto h=scene::arrowHead(*g);SkPathBuilder b;b.moveTo(h.tip.x,h.tip.y);b.lineTo(h.left.x,h.left.y);b.lineTo(h.right.x,h.right.y);b.close();
            if(!unite(path,b.detach()))return Hit::kInvalid;
        }
        break;
    }
    case semantic::ObjectKind::kTable: {
        const auto* t=std::get_if<semantic::TableContent>(&r.content);if(!t)return Hit::kInvalid;
        for(std::size_t i=0;i<t->cells.size();++i){const auto area=semantic::tableCellRect(*t,i);const auto cell=rectangle(area[0],area[1],area[2]-area[0],area[3]-area[1]);
            if(t->cells[i].fill.a>0&&!unite(path,cell))return Hit::kInvalid;
            if(r.textLayout&&i<r.textLayout->tableCells.size())if(const auto& layout=r.textLayout->tableCells[i])for(const auto& line:layout->lines)if(line.utf8End>line.utf8Start){const auto& b=line.bounds;const auto text=rectangle(area[0]+6+b.left,area[1]+6+b.top,b.right-b.left,b.bottom-b.top);const auto clipped=Op(text,cell,kIntersect_SkPathOp);if(!clipped||!unite(path,*clipped))return Hit::kInvalid;}
        }
        if(t->grid.width>0&&t->grid.color.a>0){const auto width=semantic::tableWidth(*t),height=semantic::tableHeight(*t);double x=0,y=0;SkPathBuilder lines;lines.moveTo(0,0);lines.lineTo(0,f(height));for(const auto& a:t->columns){x+=a.size;lines.moveTo(f(x),0);lines.lineTo(f(x),f(height));}lines.moveTo(0,0);lines.lineTo(f(width),0);for(const auto& a:t->rows){y+=a.size;lines.moveTo(0,f(y));lines.lineTo(f(width),f(y));}if(!unite(path,stroke(lines.detach(),t->grid.width)))return Hit::kInvalid;}
        break;
    }
    case semantic::ObjectKind::kImage: {
        const auto* c=std::get_if<semantic::ImageContent>(&r.content);if(!c)return Hit::kInvalid;
        if(c->content_mode==semantic::ImageContentMode::kFit) {
            if(c->intrinsic_width<=0||c->intrinsic_height<=0)return Hit::kUnsupported;
            const auto source=c->source_rect.value_or(semantic::NormalizedRect{0,0,1,1});
            const auto full=imageCropSourceBounds(*c);
            path=rectangle(full.left+source.x*(full.right-full.left),full.top+source.y*(full.bottom-full.top),
                source.width*(full.right-full.left),source.height*(full.bottom-full.top));
        } else path=rectangle(0,0,c->width,c->height);
        break;
    }
    case semantic::ObjectKind::kRichText: {
        if(!r.textLayout||!r.textLayout->diagnostics.empty())return Hit::kUnsupported;
        if(r.textLayout->lines.empty())if(const auto* text=std::get_if<semantic::RichTextContent>(&r.content))
            for(const auto& paragraph:text->document.paragraphs)for(const auto& run:paragraph.runs)
                if(!run.text.empty())return Hit::kUnsupported;
        for(const auto& line:r.textLayout->lines)if(line.utf8End>line.utf8Start) {
            const auto& b=line.bounds;if(!unite(path,rectangle(b.left,b.top,b.right-b.left,b.bottom-b.top)))return Hit::kInvalid;
        }
        break;
    }
    case semantic::ObjectKind::kShape: {
        const auto* c=std::get_if<semantic::ShapeContent>(&r.content);if(!c)return Hit::kInvalid;
        SkPathBuilder b;
        if(c->shape_kind==1)b.addRect(rect(0,0,c->width,c->height));
        else if(c->shape_kind==2)b.addOval(rect(0,0,c->width,c->height));
        else if(c->shape_kind==3){b.moveTo(f(c->width/2),0);b.lineTo(f(c->width),f(c->height));b.lineTo(0,f(c->height));b.close();}
        else return Hit::kUnsupported;
        const auto shape=b.detach();if(fillVisible(r))path=shape;
        for(const auto& p:r.properties.entries)if(p.field_id==0x101U)if(const auto* v=std::get_if<semantic::StrokeStyleValue>(&p.value))
            if(const auto* solid=std::get_if<semantic::SolidStroke>(v);solid&&solid->width>0&&solid->color.a>0)
                if(!unite(path,skpathutils::FillPathWithPaint(shape,internal::geometryStrokePaint(*solid))))return Hit::kInvalid;
        if(c->label) {
            const bool hasText=std::any_of(c->label->document.paragraphs.begin(),c->label->document.paragraphs.end(),[](const auto& paragraph){
                return std::any_of(paragraph.runs.begin(),paragraph.runs.end(),[](const auto& run){return !run.text.empty();});
            });
            if(hasText&&(!r.textLayout||!r.textLayout->diagnostics.empty()||r.textLayout->lines.empty()))return Hit::kUnsupported;
        }
        if(r.textLayout&&!r.textLayout->placeholder) {
            if(!r.textLayout->diagnostics.empty())return Hit::kUnsupported;
            SkPath label;
            const auto sx=r.textLayout->shapeLabel?r.textLayout->labelScaleX:1.0F;
            const auto sy=r.textLayout->shapeLabel?r.textLayout->labelScaleY:1.0F;
            for(const auto& line:r.textLayout->lines)if(line.utf8End>line.utf8Start) {
                const auto& area=line.bounds;
                if(!unite(label,rectangle(area.left*sx,area.top*sy,(area.right-area.left)*sx,(area.bottom-area.top)*sy)))return Hit::kInvalid;
            }
            // Published fragments already contain the label offset. Match the
            // painter's scale and shape clip without measuring text again.
            const auto clipped=Op(label,shape,kIntersect_SkPathOp);
            if(!clipped||!unite(path,*clipped))return Hit::kInvalid;
        }
        break;
    }
    default:return Hit::kUnsupported;
    }
    for(const auto& mask:r.eraseMasks) {
        SkPath eraser;
        if(const auto* filled=std::get_if<semantic::FilledPathMask>(&mask.geometry))eraser=internal::semanticPath(filled->path);
        else if(const auto* swept=std::get_if<semantic::SweptCircleMask>(&mask.geometry))for(const auto& s:swept->segments) {
            SkPathBuilder b;b.moveTo(f(s.p0.position.x),f(s.p0.position.y));b.cubicTo(f(s.control1.x),f(s.control1.y),f(s.control2.x),f(s.control2.y),f(s.p1.position.x),f(s.p1.position.y));
            if(!unite(eraser,stroke(b.detach(),2*std::max(s.p0.radius,s.p1.radius),SkPaint::kRound_Cap)))return Hit::kInvalid;
        }
        auto remaining=Op(path,eraser,kDifference_SkPathOp);if(!remaining)return Hit::kInvalid;path=*remaining;
    }
    return path.isFinite()?Hit::kHit:Hit::kInvalid;
}
}
SelectionGeometryHit intersectVisibleSelectionGeometry(const RuntimeSceneRecord& record,std::span<const foundation::WorldPoint> region) noexcept {
    try {
        const auto& t=record.transform;
        const double determinant=t.a*t.d-t.b*t.c;
        if(region.size()<3||!std::isfinite(determinant)||std::abs(determinant)<1e-12)return Hit::kInvalid;
        SkPathBuilder regionPath(SkPathFillType::kEvenOdd);
        for(std::size_t i=0;i<region.size();++i) {
            const double x=region[i].x-t.tx,y=region[i].y-t.ty;
            const float lx=f((t.d*x-t.c*y)/determinant),ly=f((-t.b*x+t.a*y)/determinant);
            if(!std::isfinite(lx)||!std::isfinite(ly))return Hit::kInvalid;
            if(i==0)regionPath.moveTo(lx,ly);else regionPath.lineTo(lx,ly);
        }
        regionPath.close();auto selection=regionPath.detach();SkPath visible;
        const auto status=geometry(record,visible);if(status!=Hit::kHit)return status;
        if(visible.isEmpty())return Hit::kMiss;
        // Include boundary contact within the Skia path kernel's local precision.
        // This is a narrow 1e-4 local-unit tolerance, never an AABB fallback.
        const auto overlap=Op(visible,selection,kIntersect_SkPathOp);
        if(!overlap)return Hit::kInvalid;
        if(!overlap->isEmpty())return Hit::kHit;
        // Do not union a subpixel stroke into an even-odd selection: Skia may
        // collapse its interior. Test the narrow contact ring separately.
        const auto contact=Op(visible,stroke(selection,2e-4),kIntersect_SkPathOp);
        if(!contact)return Hit::kInvalid;
        return contact->isEmpty()?Hit::kMiss:Hit::kHit;
    } catch(const std::bad_alloc&){return Hit::kInvalid;}
}
}
#else
namespace canvas::render {
SelectionGeometryHit intersectVisibleSelectionGeometry(const RuntimeSceneRecord&,std::span<const foundation::WorldPoint>) noexcept {return SelectionGeometryHit::kUnsupported;}
}
#endif
