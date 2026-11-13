// Test-only anchored reference: c5ba5599 selection_region_geometry.cpp helpers/component evaluator.
// No optimized production exact cache is used by this evaluator.
#pragma once
#include "../src/skia_geometry_internal.hpp"
#include "../src/selection_region_geometry_internal.hpp"
#include "../src/skia_prepared_geometry.hpp"
#include "../src/compact_contour_skia_path.hpp"
#include "canvas/render/brush_outline_skia_path.hpp"
#include "canvas/render/image_crop_geometry.hpp"
#include "canvas/text/text_layout.hpp"
#include "canvas/scene/sketch_geometry.hpp"
#include "canvas/semantic/table_content.hpp"
#include "include/core/SkPathUtils.h"
#include "include/pathops/SkPathOps.h"
namespace canvas::render::cpuw6b_reference {
using Hit=SelectionGeometryHit;
inline std::optional<SkPath> cpuw6bSequentialPath(const semantic::EraseMaskGeometry& geometry,std::uint64_t* unions) {
 SkPath out;
 if(const auto* fill=std::get_if<semantic::FilledPathMask>(&geometry))return internal::semanticPath(fill->path);
 for(const auto& s:std::get<semantic::SweptCircleMask>(geometry).segments){
  SkPathBuilder b;b.moveTo(float(s.p0.position.x),float(s.p0.position.y));b.cubicTo(float(s.control1.x),float(s.control1.y),float(s.control2.x),float(s.control2.y),float(s.p1.position.x),float(s.p1.position.y));
  SkPaint p;p.setStyle(SkPaint::kStroke_Style);p.setStrokeWidth(float(2*std::max(s.p0.radius,s.p1.radius)));p.setStrokeCap(SkPaint::kRound_Cap);p.setStrokeJoin(SkPaint::kRound_Join);
  const auto part=skpathutils::FillPathWithPaint(b.detach(),p);if(unions)++*unions;auto next=Op(out,part,kUnion_SkPathOp);if(!next)return {};out=*next;
 }return out;
}
float f(double v){return static_cast<float>(v);}
std::optional<SkPath> timedQueryOp(const SkPath& a,const SkPath& b,SkPathOp op,std::uint64_t* nanoseconds){
    G5_SCOPE(g5ExactOp,op==kDifference_SkPathOp?foundation::internal::g5::Tag::wa_difference:foundation::internal::g5::Tag::wa_intersection,foundation::internal::g5::Purpose::none);
#if defined(AXIOM_G5_UR0_DIAGNOSTICS) || defined(AXIOM_UR2D_TESTS)
    const auto started=std::chrono::steady_clock::now();const auto result=Op(a,b,op);
    if(nanoseconds)*nanoseconds+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count();return result;
#else
    (void)nanoseconds;return Op(a,b,op);
#endif
}

SkRect rect(double x,double y,double w,double h){return SkRect::MakeXYWH(f(x),f(y),f(w),f(h));}
SkPath rectangle(double x,double y,double w,double h){SkPathBuilder b;b.addRect(rect(x,y,w,h));return b.detach();}
SkPath stroke(const SkPath& path,double width,SkPaint::Cap cap=SkPaint::kButt_Cap) {
    SkPaint p;p.setStyle(SkPaint::kStroke_Style);p.setStrokeWidth(f(width));p.setStrokeCap(cap);p.setStrokeJoin(SkPaint::kRound_Join);
    return skpathutils::FillPathWithPaint(path,p);
}
bool unite(SkPath& a,const SkPath& b){auto r=Op(a,b,kUnion_SkPathOp);if(!r)return false;a=*r;return true;}
std::optional<SkPath> erasePath(const semantic::EraseMaskGeometry& mask,std::uint64_t* unions=nullptr) {
        G5_SCOPE(g5Build,foundation::internal::g5::Tag::wa_path_build,foundation::internal::g5::Purpose::none);
        return cpuw6bSequentialPath(mask,unions);
}
bool fillVisible(const RuntimeSceneRecord& r) {
    for(const auto& p:r.properties.entries)if(p.field_id==0x100U)if(const auto* v=std::get_if<semantic::FillStyleValue>(&p.value)) {
        const auto* solid=std::get_if<semantic::SolidFill>(v);if(const auto* pattern=std::get_if<semantic::PatternFillV1>(v))return pattern->color.a>0;return solid&&solid->color.a>0;
    }
    return r.kind!=semantic::ObjectKind::kVectorPath || std::none_of(r.properties.entries.begin(),r.properties.entries.end(),[](const auto& p){return p.field_id==0x102U;});
}
Hit geometry(const RuntimeSceneRecord& r,SkPath& path,std::vector<SkPath>& decorations,bool applyMasks=true,internal::ExactQueryWork* exact=nullptr) {
    switch(r.kind) {
    case semantic::ObjectKind::kVectorPath: {
        const auto* c=std::get_if<semantic::VectorPathContent>(&r.content.get());if(!c)return Hit::kInvalid;
        const auto body=internal::semanticPath(c->geometry);
        if(fillVisible(r))path=body;
        for(const auto& e:r.properties.entries)if(e.field_id==0x101U)if(const auto* v=std::get_if<semantic::StrokeStyleValue>(&e.value))if(const auto* s=std::get_if<semantic::SolidStroke>(v);s&&s->color.a>0&&s->width>0)if(!unite(path,skpathutils::FillPathWithPaint(body,internal::geometryStrokePaint(*s))))return Hit::kInvalid;
        break;
    }
    case semantic::ObjectKind::kVectorStroke: {
        if(const auto* c=std::get_if<semantic::BrushStrokeContent>(&r.content.get())) {
            if(c->stroke.snapshot.paint.alpha*c->stroke.snapshot.paint.opacity>0)
                path=buildVectorBrushOutlineSkPath(c->stroke.vector_output.outline,c->stroke.vector_output.fill_rule,c->stroke.vector_output.closed);
        } else if(const auto* compact=std::get_if<semantic::CompactStrokeContent>(&r.content.get())) {
            const auto& s=compact->stroke;const auto& config=s.config;
            const auto valid=semantic::validCompactStroke(s);
            if(exact){++exact->fullValidations;if(valid)exact->validationKnots+=s.knots.size();else ++exact->invalidValidationScans;}
            if(!valid)return Hit::kInvalid;
            if(config.family!=foundation::InkFamily::kRound&&config.family!=foundation::InkFamily::kMarker)return Hit::kUnsupported;
            if(config.opacity*config.color[3]<=0)break;
            if(config.algorithmVersion!=4)return Hit::kUnsupported;
            // Reuse the renderer's compiler and ribbon path, including pressure,
            // smoothing, caps and tapers. No second brush geometry algorithm.
            if(exact){++exact->compileCalls;exact->compileInputs+=s.knots.size();}
            const auto contour=ink::compileCompactContour(config,s.knots);
            for(std::size_t i=0;i+1<contour.patches.size();++i)
                {decorations.push_back(internal::compactContourRibbon(std::span(contour.patches).subspan(i,2)));if(exact)++exact->constructedComponents;}
            for(const auto& cap:contour.caps){
                SkPathBuilder b;b.addCircle(0,0,f(cap.radius));auto p=b.detach();
                SkMatrix matrix;matrix.setTranslate(f(cap.x),f(cap.y));
                if(config.family==foundation::InkFamily::kMarker){matrix.preRotate(f(config.markerAngle*57.29577951308232));matrix.preScale(1,f(config.markerFlatness));}
                decorations.push_back(p.makeTransform(matrix));if(exact)++exact->constructedComponents;
            }
        } else if(const auto* legacy=std::get_if<semantic::VectorStrokeContent>(&r.content.get())) {
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
        if(const auto* c=std::get_if<semantic::DabBrushStrokeContent>(&r.content.get())) {
            // Textured chalk/membrane alpha coverage is not a disk oracle.
            if(c->stroke.snapshot.material_mode!=1||c->stroke.snapshot.package_revision>=3)return Hit::kUnsupported;
            data=&c->stroke.dab_output;disk=true;alpha=c->stroke.snapshot.paint.alpha*c->stroke.snapshot.paint.opacity;
        } else if(const auto* legacy=std::get_if<semantic::DabStrokeContent>(&r.content.get())) {
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
        if(std::any_of(r.properties.entries.begin(),r.properties.entries.end(),[](const auto& e){return e.field_id==0x103U;})) {
            const auto rounded=scene::sketchGeometry(r.content,r.properties,g);if(!rounded)return Hit::kInvalid;
            path=stroke(internal::semanticPath(rounded->outline),style.width*rounded->outlineWidthScale);
            break;
        }
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
        const auto* t=std::get_if<semantic::TableContent>(&r.content.get());if(!t)return Hit::kInvalid;
        for(std::size_t i=0;i<t->cells.size();++i){const auto area=semantic::tableCellRect(*t,i);const auto cell=rectangle(area[0],area[1],area[2]-area[0],area[3]-area[1]);
            if(t->cells[i].fill.a>0&&!unite(path,cell))return Hit::kInvalid;
            if(r.textLayout&&i<r.textLayout->tableCells.size())if(const auto& layout=r.textLayout->tableCells[i])for(const auto& line:layout->lines)if(line.utf8End>line.utf8Start){const auto& b=line.bounds;const auto text=rectangle(area[0]+6+b.left,area[1]+6+b.top,b.right-b.left,b.bottom-b.top);const auto clipped=Op(text,cell,kIntersect_SkPathOp);if(!clipped||!unite(path,*clipped))return Hit::kInvalid;}
        }
        if(t->grid.width>0&&t->grid.color.a>0){const auto width=semantic::tableWidth(*t),height=semantic::tableHeight(*t);double x=0,y=0;SkPathBuilder lines;lines.moveTo(0,0);lines.lineTo(0,f(height));for(const auto& a:t->columns){x+=a.size;lines.moveTo(f(x),0);lines.lineTo(f(x),f(height));}lines.moveTo(0,0);lines.lineTo(f(width),0);for(const auto& a:t->rows){y+=a.size;lines.moveTo(0,f(y));lines.lineTo(f(width),f(y));}if(!unite(path,stroke(lines.detach(),t->grid.width)))return Hit::kInvalid;}
        break;
    }
    case semantic::ObjectKind::kSection: {
        const auto* c=std::get_if<semantic::SectionContent>(&r.content.get());if(!c)return Hit::kInvalid;
        // Keep the established whole-boundary acquisition area. Membership and
        // header semantics still use the stable authored rectangle.
        path=rectangle(c->boundary.x,c->boundary.y,c->boundary.width,c->boundary.height);
        break;
    }
    case semantic::ObjectKind::kImage: {
        const auto* c=std::get_if<semantic::ImageContent>(&r.content.get());if(!c)return Hit::kInvalid;
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
        if(r.textLayout->lines.empty())if(const auto* text=std::get_if<semantic::RichTextContent>(&r.content.get()))
            for(const auto& paragraph:text->document.paragraphs)for(const auto& run:paragraph.runs)
                if(!run.text.empty())return Hit::kUnsupported;
        for(const auto& line:r.textLayout->lines)if(line.utf8End>line.utf8Start) {
            const auto& b=line.bounds;if(!unite(path,rectangle(b.left,b.top,b.right-b.left,b.bottom-b.top)))return Hit::kInvalid;
        }
        break;
    }
    case semantic::ObjectKind::kShape: {
        const auto* c=std::get_if<semantic::ShapeContent>(&r.content.get());if(!c)return Hit::kInvalid;
        SkPathBuilder b;
        if(c->shape_kind==1)b.addRect(rect(0,0,c->width,c->height));
        else if(c->shape_kind==2)b.addOval(rect(0,0,c->width,c->height));
        else if(c->shape_kind==3){b.moveTo(f(c->width/2),0);b.lineTo(f(c->width),f(c->height));b.lineTo(0,f(c->height));b.close();}
        else return Hit::kUnsupported;
        // Preserve the established Skia oval/conic representation for unrounded shapes.
        // Replacing it with an equivalent cubic ellipse can make a union with
        // particular rough strokes fail in Skia PathOps.
        const bool rounded=std::any_of(r.properties.entries.begin(),r.properties.entries.end(),[](const auto& p){return p.field_id==0x103U;});
        const auto derived=rounded?scene::sketchGeometry(r.content,r.properties):nullptr;
        const auto shape=derived?internal::semanticPath(derived->body):b.detach();if(fillVisible(r))path=shape;
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
    if(const auto sketch=scene::sketchGeometry(r.content,r.properties,std::get_if<scene::ConnectorGeometry>(&r.derivedGeometry))) {
        double width=r.kind==semantic::ObjectKind::kConnector?4:r.kind==semantic::ObjectKind::kSection?1:0;
        if(const auto* table=std::get_if<semantic::TableContent>(&r.content.get()))width=table->grid.color.a>0?table->grid.width:0;
        for(const auto& e:r.properties.entries)if(const auto* v=std::get_if<semantic::StrokeStyleValue>(&e.value)){if(const auto* s=std::get_if<semantic::SolidStroke>(v))width=s->color.a>0?s->width:0;else width=0;}
        // The visible set is a union, but acquisition need not first Boolean-union
        // a complex random outline with its body. Skia PathOps can reject that
        // intermediate union even when both parts are individually valid.
        const bool roundedConnector=r.kind==semantic::ObjectKind::kConnector&&std::any_of(r.properties.entries.begin(),r.properties.entries.end(),[](const auto& p){return p.field_id==0x103U;});
        if(width>0&&!roundedConnector)decorations.push_back(stroke(internal::semanticPath(sketch->outline),width));
        if(width>0&&!sketch->arrowFill.commands.empty())decorations.push_back(internal::semanticPath(sketch->arrowFill));
    }
    if(applyMasks)for(const auto& mask:r.eraseMasks) {
        const auto built=erasePath(mask.geometry);if(!built)return Hit::kInvalid;
        const auto& eraser=*built;
        const auto subtract=[&](SkPath& candidate){auto remaining=Op(candidate,eraser,kDifference_SkPathOp);if(!remaining)return false;candidate=*remaining;return true;};
        if(!subtract(path))return Hit::kInvalid;
        for(auto& decoration:decorations)if(!subtract(decoration))return Hit::kInvalid;
    }
    return path.isFinite()&&std::all_of(decorations.begin(),decorations.end(),[](const auto& p){return p.isFinite();})?Hit::kHit:Hit::kInvalid;
}
SelectionGeometryHit eraseGeometryQuery(const RuntimeSceneRecord& record,const semantic::EraseMaskGeometry& mask,EraseGeometryWork* work,bool shareQuery,internal::EraseMaskPathCache* pathCache=nullptr,internal::ExactQueryWork* exact=nullptr,const internal::PreparedNavigationGeometry* prepared=nullptr,const std::shared_ptr<internal::ExactHistoryProof>& history={},MaskHistoryWork* historyWork=nullptr,const SkPath* provenQuery=nullptr,bool strictReady=false) noexcept {
    G5_SCOPE(g5Evaluate,foundation::internal::g5::Tag::wa_exact_eval,foundation::internal::g5::Purpose::none);
    try {
        (void)pathCache;(void)history;(void)provenQuery;(void)strictReady;
        const auto pathFor=[&](const semantic::EraseMaskGeometry& value,bool=false){return erasePath(value);};
        const auto historicalPath=[&](std::size_t i){return erasePath(record.eraseMasks[i].geometry);};
        const auto invalidGeometry=[]([[maybe_unused]]const char* stage){
#if defined(AXIOM_ENABLE_FRAME_TIMING)
            std::fprintf(stderr,"AXIOM_ERASE_PATHOPS_FAILURE stage=%s\n",stage);
#endif
            return Hit::kInvalid;
        };
        SkPath visible;std::vector<SkPath> decorations;
        const auto status=prepared?Hit::kHit:geometry(record,visible,decorations,false,exact);if(status!=Hit::kHit)return status;
        const auto eraser=pathFor(mask);if(!eraser||!eraser->isFinite())return invalidGeometry("new_mask_path");
        const auto visitComponents=[&](const SkPath& demand,const auto& visitor){
            if(prepared){internal::ExactComponentWork local;
                const auto completed=prepared->visitExact(demand.getBounds(),visitor,local);
                if(exact){exact->indexNodes+=local.groups;exact->boundsTests+=local.boundsTests;exact->componentChecks+=local.components;exact->replayKnots+=local.replayKnots;exact->materializedPaths+=local.materializedPaths;}
                return completed;
            }
            if(!visitor(visible))return false;
            for(const auto& part:decorations)if(!visitor(part))return false;return true;
        };
        // Query only the new intersection. Subtraction distributes over the
        // component union, so a visible hit need not prepare unrelated parts.
        // Paths are query-local and built lazily once, never retained in Scene.
        std::vector<SkPath> previousMasks;
        const auto disjoint=[](const SkPath& a,const SkPath& b){
            const auto x=a.getBounds(),y=b.getBounds();
            return x.isFinite()&&y.isFinite()&&!x.isEmpty()&&!y.isEmpty()&&!SkRect::Intersects(x,y);
        };
        // Factor history out of the component loop:
        // (body intersect query) minus history == body intersect (query minus history).
        // This remains an exact PathOps query, never a raster/GPU hit test.
        // Retain the original component query below as the failure oracle:
        // a failed optional PathOps proof cannot reject an otherwise valid hit.
        if(shareQuery&&!record.eraseMasks.empty()){
            const auto sharedQuery=[&]()->std::optional<Hit>{
                bool outside=true;visitComponents(*eraser,[&](const SkPath& part){if(!part.isEmpty()&&!disjoint(part,*eraser)){outside=false;return false;}return true;});
                if(outside)return Hit::kMiss;
                auto remaining=*eraser;
                for(std::size_t i=0;i<record.eraseMasks.size();++i){
                    const auto& maskRecord=record.eraseMasks[i];
                    if(maskRecord.geometry==mask)return Hit::kMiss;
                    const auto old=historicalPath(i);if(!old||!old->isFinite())return {};
                    if(disjoint(remaining,*old))continue;
                    if(work)++work->historicalDifferences;
                    auto next=timedQueryOp(remaining,*old,kDifference_SkPathOp,historyWork?&historyWork->differenceNanoseconds:nullptr);if(!next)return {};
                    remaining=*next;if(remaining.isEmpty())return Hit::kMiss;
                }
                const auto hit=[&](const SkPath& part)->std::optional<Hit>{
                    if(part.isEmpty()||disjoint(part,remaining))return Hit::kMiss;
                    if(work)++work->componentIntersections;
                    const auto result=timedQueryOp(part,remaining,kIntersect_SkPathOp,historyWork?&historyWork->intersectionNanoseconds:nullptr);if(!result)return {};
                    return result->isEmpty()?Hit::kMiss:Hit::kHit;
                };
                std::optional<Hit> result=Hit::kMiss;
                visitComponents(remaining,[&](const SkPath& part){result=hit(part);return result&&*result!=Hit::kHit;});
                return result;
            };
            if(const auto result=sharedQuery())return *result;
        }
        // A repeat/no-op must not exhaust every brush ribbon. This is only
        // a proof of an empty query, never an approximate positive hit. Keep
        // source/new-mask validation above, including invalid-source handling.
        if(!record.eraseMasks.empty()){
            auto remaining=*eraser;
            // Optional proofs are bounded; a long mask history keeps the
            // existing lazy component query rather than front-loading it.
            constexpr std::size_t proofMasks=8;
            for(std::size_t i=0;i<std::min(record.eraseMasks.size(),proofMasks);++i){
                if(record.eraseMasks[i].geometry==mask)return Hit::kMiss;
                const auto built=historicalPath(i);
                // Failure of this optional proof is not itself rejection:
                // the original component query below still owns the result.
                if(!built||!built->isFinite())break;
                previousMasks.push_back(*built);
                if(disjoint(remaining,*built))continue;
                if(work)++work->historicalDifferences;
                auto next=timedQueryOp(remaining,*built,kDifference_SkPathOp,historyWork?&historyWork->differenceNanoseconds:nullptr);
                if(!next)break;
                remaining=*next;
                if(remaining.isEmpty())return Hit::kMiss;
            }
        }
        const auto intersects=[&](const SkPath& body){
            if(body.isEmpty()||disjoint(body,*eraser))return Hit::kMiss;
            if(work)++work->componentIntersections;
            auto overlap=timedQueryOp(body,*eraser,kIntersect_SkPathOp,historyWork?&historyWork->intersectionNanoseconds:nullptr);if(!overlap)return invalidGeometry("component_intersection");
            if(overlap->isEmpty())return Hit::kMiss;
            for(std::size_t i=0;i<record.eraseMasks.size();++i){
                if(i==previousMasks.size()){
                    const auto built=historicalPath(i);
                    if(!built||!built->isFinite())return invalidGeometry("previous_mask_path");
                    previousMasks.push_back(*built);
                }
                if(disjoint(*overlap,previousMasks[i]))continue;
                if(work)++work->historicalDifferences;
                overlap=timedQueryOp(*overlap,previousMasks[i],kDifference_SkPathOp,historyWork?&historyWork->differenceNanoseconds:nullptr);
                if(!overlap)return invalidGeometry("previous_mask_difference");
                if(overlap->isEmpty())return Hit::kMiss;
            }
            return Hit::kHit;
        };
        bool invalid=false,found=false;
        visitComponents(*eraser,[&](const SkPath& part){const auto hit=intersects(part);found=hit==Hit::kHit;invalid=invalid||hit==Hit::kInvalid;return !found;});
        return found?Hit::kHit:invalid?Hit::kInvalid:Hit::kMiss;
    }catch(const std::bad_alloc&){return Hit::kInvalid;}
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
        std::vector<SkPath> decorations;decorations.reserve(2);
        const auto status=geometry(record,visible,decorations);if(status!=Hit::kHit)return status;
        const auto intersects=[&](const SkPath& part){
            if(part.isEmpty())return Hit::kMiss;
            const auto overlap=Op(part,selection,kIntersect_SkPathOp);
            if(!overlap)return Hit::kInvalid;
            if(!overlap->isEmpty())return Hit::kHit;
            // Preserve the existing narrow local contact tolerance, independently
            // of each part's winding/fill rule. No AABB hit fallback.
            const auto contact=Op(part,stroke(selection,2e-4),kIntersect_SkPathOp);
            if(!contact)return Hit::kInvalid;
            return contact->isEmpty()?Hit::kMiss:Hit::kHit;
        };
        // A proven intersection with any visible component proves intersection
        // with their union. Do not ask a complex irrelevant outline to invalidate
        // a filled-body hit. Unsupported/missing layout was rejected by geometry.
        const auto hit=intersects(visible);if(hit==Hit::kHit)return hit;
        bool invalid=hit==Hit::kInvalid;
        for(const auto& part:decorations){const auto next=intersects(part);if(next==Hit::kHit)return next;invalid=invalid||next==Hit::kInvalid;}
        return invalid?Hit::kInvalid:Hit::kMiss;
    } catch(const std::bad_alloc&){return Hit::kInvalid;}
}
}
