#include "canvas/render/selection_region_geometry.hpp"
#include "selection_region_geometry_internal.hpp"
#include "canvas/foundation/internal/g5_observation.hpp"
#include "canvas/scene/sketch_geometry.hpp"
#include "canvas/ink/compact_geometry.hpp"
#if CANVAS_RENDER_HAS_SKIA
#include "skia_prepared_geometry.hpp"
#include "skia_mask_history.hpp"
#include "skia_geometry_internal.hpp"
#include "compact_contour_skia_path.hpp"
#include "canvas/render/brush_outline_skia_path.hpp"
#include "canvas/render/image_crop_geometry.hpp"
#include "canvas/text/text_layout.hpp"
#include "canvas/semantic/table_content.hpp"
#include "include/core/SkMatrix.h"
#include "include/core/SkPathUtils.h"
#include "include/pathops/SkPathOps.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <new>
#include <list>
#if defined(AXIOM_ENABLE_FRAME_TIMING)
#include <cstdio>
#endif
namespace canvas::render {
namespace internal {
using ErasePathUsage=MaskHistoryUsage;
struct EraseMaskPathCache::Entry {
    semantic::EraseMaskGeometry geometry;SkPath path;resources::ResourceLease lease;
    std::shared_ptr<ErasePathUsage> usage;std::size_t bytes=0;std::uint64_t used=0;
    ~Entry(){if(usage)usage->bytes-=bytes;}
};
struct ExactHistoryProof final {
    scene::SharedVector<semantic::EraseMaskRecord> records;
    std::vector<std::weak_ptr<const EraseMaskPathCache::Entry>> paths;
    std::shared_ptr<MaskHistorySourceCharge> source;
    resources::ResourceLease metadata;
    std::shared_ptr<MaskHistoryUsage> usage;
    std::size_t bytes=0;
    std::uint64_t used=0,identity=0;
    ~ExactHistoryProof(){if(usage)usage->bytes-=bytes;}
};
}
namespace {
using Hit=SelectionGeometryHit;
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
        SkPath eraser;
        if(const auto* filled=std::get_if<semantic::FilledPathMask>(&mask))eraser=internal::semanticPath(filled->path);
        else if(const auto* swept=std::get_if<semantic::SweptCircleMask>(&mask))for(const auto& s:swept->segments) {
            SkPathBuilder b;b.moveTo(f(s.p0.position.x),f(s.p0.position.y));b.cubicTo(f(s.control1.x),f(s.control1.y),f(s.control2.x),f(s.control2.y),f(s.p1.position.x),f(s.p1.position.y));
            if(unions)++*unions;
            if(!unite(eraser,stroke(b.detach(),2*std::max(s.p0.radius,s.p1.radius),SkPaint::kRound_Cap)))return std::nullopt;
        }
    return eraser;
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
}
namespace {
SelectionGeometryHit eraseGeometryQuery(const RuntimeSceneRecord& record,const semantic::EraseMaskGeometry& mask,EraseGeometryWork* work,bool shareQuery,internal::EraseMaskPathCache* pathCache=nullptr,internal::ExactQueryWork* exact=nullptr,const internal::PreparedNavigationGeometry* prepared=nullptr,const std::shared_ptr<internal::ExactHistoryProof>& history={},MaskHistoryWork* historyWork=nullptr) noexcept {
    G5_SCOPE(g5Evaluate,foundation::internal::g5::Tag::wa_exact_eval,foundation::internal::g5::Purpose::none);
    try {
        std::vector<std::shared_ptr<const internal::EraseMaskPathCache::Entry>> pins;
        const auto pathFor=[&](const semantic::EraseMaskGeometry& geometry,bool historical=false){
            if(pathCache)if(auto entry=pathCache->get(geometry,historical)){pins.push_back(entry);return std::optional<SkPath>{entry->path};}
            return erasePath(geometry);
        };
        const auto historicalPath=[&](std::size_t i){
            if(pathCache&&history)if(auto entry=pathCache->exactHistoricalPath(history,i)){
                pins.push_back(entry);return std::optional<SkPath>{entry->path};
            }
            return pathFor(record.eraseMasks[i].geometry,true);
        };
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
}
SelectionGeometryHit intersectEraseGeometry(const RuntimeSceneRecord& record,const semantic::EraseMaskGeometry& mask,EraseGeometryWork* work) noexcept {
    return eraseGeometryQuery(record,mask,work,true);
}
namespace internal {
struct EraseMaskPathCache::Impl {
    std::shared_ptr<Entry> queryPrefix;
    std::size_t queryPrefixSegments=0;
    struct QueryResult {
        std::shared_ptr<const PreparedNavigationGeometry> body;
        semantic::EraseMaskGeometry query;
        std::shared_ptr<ExactHistoryProof> history;
        semantic::Transform2D transform{};
        std::uint32_t version=0;
        Hit result=Hit::kInvalid;
        resources::ResourceLease metadata;
        std::shared_ptr<ErasePathUsage> usage;
        std::size_t bytes=0;
        ~QueryResult(){if(usage)usage->bytes-=bytes;}
        QueryResult(std::shared_ptr<const PreparedNavigationGeometry> b,const semantic::EraseMaskGeometry& q,
            std::shared_ptr<ExactHistoryProof> h,semantic::Transform2D t,std::uint32_t v,Hit r,
            resources::ResourceLease lease,std::shared_ptr<ErasePathUsage> u,std::size_t n)
            :body(std::move(b)),query(q),history(std::move(h)),transform(t),version(v),result(r),metadata(std::move(lease)),usage(std::move(u)),bytes(n){usage->bytes+=bytes;}
    };
    std::shared_ptr<QueryResult> lastQuery;
    std::list<std::shared_ptr<QueryResult>> cohortQueries;
    struct Refusal {
        std::weak_ptr<const std::vector<semantic::EraseMaskRecord>> owner;
        const void* pointee=nullptr;
        std::shared_ptr<const SourceRetention> source;
        std::shared_ptr<MaskHistorySourceCharge> localSource;
        resources::ResourceLease metadata;
        std::shared_ptr<ErasePathUsage> usage;
        std::size_t bytes=0,pathBytes=0;
        std::uint64_t used=0;
        ~Refusal(){if(usage)usage->bytes-=bytes;}
    };
    Work stats;std::shared_ptr<ErasePathUsage> usage=std::make_shared<ErasePathUsage>();
    std::shared_ptr<resources::ResourceBudgetManager> budget;
    std::vector<std::shared_ptr<Entry>> entries;
    std::vector<std::shared_ptr<MaskHistoryPlan>> histories;
    std::vector<std::shared_ptr<ExactHistoryProof>> exactHistories;
    std::vector<std::shared_ptr<Refusal>> refusals;
    std::shared_ptr<SourceRetentionPool> sources=std::make_shared<SourceRetentionPool>();
    std::size_t capacity=0;std::uint64_t tick=0,nextHistory=0;
    unsigned cohortDepth=0;
    std::size_t size()const noexcept{return entries.size()+histories.size()+exactHistories.size()+refusals.size()+cohortQueries.size()+
        std::size_t(lastQuery&&std::find(cohortQueries.begin(),cohortQueries.end(),lastQuery)==cohortQueries.end())+std::size_t(bool(queryPrefix));}
    std::size_t pathBytes()const noexcept{auto bytes=usage->bytes;for(const auto& r:refusals)bytes-=r->bytes;return bytes;}
    bool evict(){
        // Draw flattening is optional to exact work. Active draw pins keep
        // their leases; only idle plans are reclaimed here.
        if(cohortDepth){
            auto draw=histories.end();for(auto it=histories.begin();it!=histories.end();++it)
                if(it->use_count()==1&&(draw==histories.end()||(*it)->used<(*draw)->used))draw=it;
            if(draw!=histories.end()){histories.erase(draw);return true;}
        }
        // A one-query memo is optional and can pin paths/body/history. Drop it
        // first on actual quota pressure so existing work remains reclaimable.
        if(lastQuery&&!cohortDepth){lastQuery.reset();return true;}
        if(queryPrefix){queryPrefix.reset();queryPrefixSegments=0;return true;}
        auto victim=entries.end();for(auto it=entries.begin();it!=entries.end();++it)
            if(it->use_count()==1&&(victim==entries.end()||(*it)->used<(*victim)->used))victim=it;
        auto plan=histories.end();for(auto it=histories.begin();it!=histories.end();++it)
            if(it->use_count()==1&&(plan==histories.end()||(*it)->used<(*plan)->used))plan=it;
        auto exact=exactHistories.end();for(auto it=exactHistories.begin();it!=exactHistories.end();++it)
            if(it->use_count()==1&&(exact==exactHistories.end()||(*it)->used<(*exact)->used))exact=it;
        if(exact!=exactHistories.end()&&(victim==entries.end()||(*exact)->used<(*victim)->used)&&
           (plan==histories.end()||(*exact)->used<(*plan)->used)){exactHistories.erase(exact);return true;}
        auto refusal=std::min_element(refusals.begin(),refusals.end(),[](const auto& a,const auto& b){return a->used<b->used;});
        if(refusal!=refusals.end()&&(victim==entries.end()||(*refusal)->used<(*victim)->used)&&
           (plan==histories.end()||(*refusal)->used<(*plan)->used)){refusals.erase(refusal);return true;}
        if(plan!=histories.end()&&(victim==entries.end()||(*plan)->used<(*victim)->used)){histories.erase(plan);return true;}
        if(victim!=entries.end()){entries.erase(victim);return true;}
        // Retire proven cohort results only after idle paths/proofs/draw plans;
        // their pins are the currently admitted work, not stale cache entries.
        if(!cohortQueries.empty()){
            auto it=std::find_if(cohortQueries.begin(),cohortQueries.end(),[&](const auto& q){return q!=lastQuery;});
            if(it!=cohortQueries.end()){cohortQueries.erase(it);return true;}
        }
        return false;
    }
    std::shared_ptr<MaskHistorySourceCharge> sourceCharge(const std::shared_ptr<const SourceRetention>& source){
        // Weak discoverability belongs to the same local accounting lifetime,
        // including externally held plans surviving clear(). It owns no source.
        if(const auto it=usage->sources.find(source.get());it!=usage->sources.end())
            if(auto known=it->second.lock();known&&known->source==source)return known;
        constexpr auto metadata=sizeof(MaskHistorySourceCharge)+256;
        if(!source||source->bytes>capacity||source->indexBytes>capacity-source->bytes||
           metadata>capacity-source->bytes-source->indexBytes)return {};
        const auto weight=source->bytes+source->indexBytes+metadata;
        std::optional<std::uint64_t> token;
        for(;;){
            if(usage->bytes<=capacity-weight)token=budget->reserve(resources::ResourceClass::kRecorded,metadata,true);
            if(token)break;if(!evict())return {};
        }
        resources::ResourceLease reservation(budget,*token);
        auto charge=std::make_shared<MaskHistorySourceCharge>();charge->source=source;charge->metadata=std::move(reservation);
        charge->usage=usage;charge->bytes=weight;usage->bytes+=weight;usage->sources.insert_or_assign(source.get(),charge);return charge;
    }
};
EraseMaskPathCache::EraseMaskPathCache():impl_(std::make_unique<Impl>()){}
EraseMaskPathCache::~EraseMaskPathCache()=default;
void EraseMaskPathCache::clear()noexcept{impl_->queryPrefix.reset();impl_->queryPrefixSegments=0;impl_->lastQuery.reset();impl_->cohortQueries.clear();std::vector<std::shared_ptr<Impl::Refusal>>{}.swap(impl_->refusals);std::vector<std::shared_ptr<MaskHistoryPlan>>{}.swap(impl_->histories);std::vector<std::shared_ptr<ExactHistoryProof>>{}.swap(impl_->exactHistories);std::vector<std::shared_ptr<Entry>>{}.swap(impl_->entries);}
void EraseMaskPathCache::beginCohort()noexcept{++impl_->cohortDepth;}
void EraseMaskPathCache::endCohort()noexcept{if(impl_->cohortDepth&&!--impl_->cohortDepth)impl_->cohortQueries.clear();}
void EraseMaskPathCache::setRetentionPool(std::shared_ptr<SourceRetentionPool> pool){if(pool&&pool!=impl_->sources){clear();impl_->sources=std::move(pool);}}
EraseMaskPathCache::Work EraseMaskPathCache::work()const noexcept{auto v=impl_->stats;v.bytes=impl_->usage->bytes;v.history.retainedBytes=v.bytes;return v;}
void EraseMaskPathCache::prepareQueryPrefix(const semantic::EraseMaskGeometry& geometry,
    std::shared_ptr<resources::ResourceBudgetManager> budget,std::size_t capacity,std::size_t maxSegments)noexcept {
    G5_SCOPE(g5Prefix,foundation::internal::g5::Tag::wa_prefix,foundation::internal::g5::Purpose::none);
    try{
        auto& s=*impl_;capacity=std::min<std::size_t>(capacity,8U*1024U*1024U);
        if(s.budget!=budget||s.capacity!=capacity){clear();s.usage=std::make_shared<ErasePathUsage>();s.budget=std::move(budget);s.capacity=capacity;}
        const auto* sweep=std::get_if<semantic::SweptCircleMask>(&geometry);
        if(!sweep||!s.budget||!maxSegments)return;
        // Unknown/forked inputs must prove their full prefix, including every
        // coordinate/radius. No owner pointer or mask ID qualifies this cache.
        for(const auto& p:sweep->segments)if(!std::isfinite(p.p0.position.x)||!std::isfinite(p.p0.position.y)||
            !std::isfinite(p.p1.position.x)||!std::isfinite(p.p1.position.y)||!std::isfinite(p.control1.x)||
            !std::isfinite(p.control1.y)||!std::isfinite(p.control2.x)||!std::isfinite(p.control2.y)||
            !std::isfinite(p.p0.radius)||!std::isfinite(p.p1.radius))return;
        auto old=s.queryPrefix;std::size_t completed=0;SkPath path;
        if(old){const auto* prefix=std::get_if<semantic::SweptCircleMask>(&old->geometry);
            if(prefix&&prefix->segments.size()<=sweep->segments.size()&&
               std::equal(prefix->segments.begin(),prefix->segments.end(),sweep->segments.begin())){
                completed=s.queryPrefixSegments;path=old->path;
            }
        }
        const auto base=sizeof(Entry)+sweep->segments.size()*sizeof(semantic::EraseCubicSegment)+65536U;
        const auto temporary=old?2*old->path.approximateBytesUsed():0U;
        if(base>capacity||temporary>capacity-base||sweep->segments.size()>(capacity-base-temporary)/4096U)return;
        const auto estimate=base+sweep->segments.size()*4096U+temporary;
        // Account old and new allocations during replacement. Optional work
        // simply declines when its current quota cannot hold both generations.
        if(s.usage->bytes>capacity-estimate)return;
        auto token=s.budget->reserve(resources::ResourceClass::kRecorded,estimate,true);if(!token)return;
        resources::ResourceLease lease(s.budget,*token);
        const auto end=std::min(sweep->segments.size(),completed+maxSegments);
        const auto started=std::chrono::steady_clock::now();
        for(;completed<end;++completed){const auto& p=sweep->segments[completed];SkPathBuilder b;
            b.moveTo(f(p.p0.position.x),f(p.p0.position.y));b.cubicTo(f(p.control1.x),f(p.control1.y),f(p.control2.x),f(p.control2.y),f(p.p1.position.x),f(p.p1.position.y));
            ++s.stats.history.queryUnionOps;
            if(!unite(path,stroke(b.detach(),2*std::max(p.p0.radius,p.p1.radius),SkPaint::kRound_Cap)))return;
            if(completed+1<end&&std::chrono::steady_clock::now()-started>=std::chrono::microseconds(2000)){++completed;break;}
        }
        auto value=std::make_shared<Entry>();value->geometry=geometry;value->path=std::move(path);
        const auto bytes=sizeof(Entry)+sweep->segments.size()*sizeof(semantic::EraseCubicSegment)+256+value->path.approximateBytesUsed();
        if(bytes>estimate||!lease.shrink(bytes))return;
        value->lease=std::move(lease);value->usage=s.usage;value->bytes=bytes;value->used=++s.tick;
        s.usage->bytes+=bytes;s.queryPrefix=std::move(value);s.queryPrefixSegments=completed;
    }catch(const std::bad_alloc&){}
}
std::shared_ptr<const EraseMaskPathCache::Entry> EraseMaskPathCache::get(const semantic::EraseMaskGeometry& geometry,bool historical){
    G5_SCOPE(g5PathLookup,foundation::internal::g5::Tag::wa_path_lookup,foundation::internal::g5::Purpose::none);
    auto& state=*impl_;const auto denied=[&]()->std::shared_ptr<const Entry>{++state.stats.denied;return {};};
    if(!historical&&state.queryPrefix){
        const auto* sweep=std::get_if<semantic::SweptCircleMask>(&geometry);
        const auto* prefix=std::get_if<semantic::SweptCircleMask>(&state.queryPrefix->geometry);
        if(sweep&&prefix&&prefix->segments.size()<=sweep->segments.size()&&
           std::equal(prefix->segments.begin(),prefix->segments.end(),sweep->segments.begin())){
            while(state.queryPrefixSegments<sweep->segments.size()){
                const auto before=state.queryPrefixSegments;
                prepareQueryPrefix(geometry,state.budget,state.capacity,sweep->segments.size()-before);
                if(state.queryPrefixSegments<=before)break;
            }
        }
        if(const auto* sweep=std::get_if<semantic::SweptCircleMask>(&geometry);sweep&&state.queryPrefix&&
           state.queryPrefix->geometry==geometry&&state.queryPrefixSegments==sweep->segments.size()){
            auto ready=std::move(state.queryPrefix);state.queryPrefixSegments=0;state.entries.push_back(ready);++state.stats.builds;return ready;
        }
    }
    for(auto& entry:state.entries)if((++state.stats.pathValueComparisons,state.stats.pathPrimitiveComparisons+=std::visit([](const auto& g)->std::size_t{if constexpr(requires{g.segments;})return g.segments.size();else return g.path.commands.size();},geometry),entry->geometry==geometry)){entry->used=++state.tick;++state.stats.hits;return entry;}
    const auto units=std::visit([](const auto& g){if constexpr(requires{g.segments;})return g.segments.size();else return g.path.commands.size();},geometry);
    const auto raw=std::visit([](const auto& g){if constexpr(requires{g.segments;})return g.segments.size()*sizeof(semantic::EraseCubicSegment);else return g.path.commands.size()*sizeof(semantic::PathCommand);},geometry);
    const auto base=sizeof(Entry)+raw+65536;
    if(!state.budget||base>state.capacity||units>(state.capacity-base)/4096)return denied();
    const auto estimate=base+units*4096;
    std::optional<std::uint64_t> token;
    for(;;){
        if(state.size()<512&&state.usage->bytes<=state.capacity-estimate)
            token=state.budget->reserve(resources::ResourceClass::kRecorded,estimate,true);
        if(token)break;if(!state.evict())return denied();
    }
    resources::ResourceLease lease(state.budget,*token);
#if defined(AXIOM_G5_UR0_DIAGNOSTICS) || defined(AXIOM_UR2D_TESTS)
    const auto started=std::chrono::steady_clock::now();
#endif
    auto& unions=historical?state.stats.history.historyUnionOps:state.stats.history.queryUnionOps;
    const auto path=erasePath(geometry,&unions);
#if defined(AXIOM_G5_UR0_DIAGNOSTICS) || defined(AXIOM_UR2D_TESTS)
    (historical?state.stats.history.historyPathNanoseconds:state.stats.history.queryPathNanoseconds)+=
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count();
#endif
    ++(historical?state.stats.history.historyPathBuilds:state.stats.history.queryPathBuilds);
    if(!path||!path->isFinite())return denied();
    auto candidate=std::make_shared<Entry>();candidate->geometry=geometry;candidate->path=*path;
    const auto bytes=sizeof(Entry)+raw+256+path->approximateBytesUsed();
    if(bytes>estimate||!lease.shrink(bytes))return denied();
    candidate->lease=std::move(lease);candidate->bytes=bytes;candidate->usage=state.usage;candidate->used=++state.tick;
    state.usage->bytes+=bytes;state.entries.push_back(candidate);++state.stats.builds;return candidate;
}
namespace {
bool equalMask(const semantic::EraseMaskRecord& a,const semantic::EraseMaskRecord& b,MaskHistoryWork& work){
    G5_SCOPE(g5Value,foundation::internal::g5::Tag::wa_history_value,foundation::internal::g5::Purpose::none);
    // This proves derived geometry only. Each plan retains its own complete
    // canonical records/source lease; mask IDs never alias canonical payloads.
    ++work.valueRecords;if(a.geometry.index()!=b.geometry.index())return false;
    return std::visit([&](const auto& x){
        using T=std::decay_t<decltype(x)>;const auto& y=std::get<T>(b.geometry);
        if constexpr(requires{x.segments;}) {
            return x.segments.size()==y.segments.size()&&std::equal(x.segments.begin(),x.segments.end(),y.segments.begin(),
                [&](const auto& p,const auto& q){++work.valuePrimitives;return p==q;});
        }else return x.path.fill_rule==y.path.fill_rule&&x.path.commands.size()==y.path.commands.size()&&
            std::equal(x.path.commands.begin(),x.path.commands.end(),y.path.commands.begin(),
                [&](const auto& p,const auto& q){++work.valuePrimitives;return p==q;});
    },a.geometry);
}
}
std::shared_ptr<MaskHistoryPlan> EraseMaskPathCache::history(
    const scene::SharedVector<semantic::EraseMaskRecord>& records,
    std::shared_ptr<resources::ResourceBudgetManager> budget,std::size_t capacity,
    const std::shared_ptr<MaskHistoryPlan>& previous,MaskHistoryAdmission* admission){
    G5_SCOPE(g5HistoryLookup,foundation::internal::g5::Tag::wa_history_lookup,foundation::internal::g5::Purpose::none);
    auto& state=*impl_;capacity=std::min<std::size_t>(capacity,8U*1024U*1024U);
    if(state.budget!=budget||state.capacity!=capacity){clear();state.usage=std::make_shared<ErasePathUsage>();state.budget=std::move(budget);state.capacity=capacity;}
    auto status=MaskHistoryAdmission::kNotAdmitted;
    bool refusalHit=false,structural=false;
    const auto storage=records.storage();
    const auto result=[&]()->std::shared_ptr<MaskHistoryPlan>{
    if(records.empty())return {};
    for(auto& plan:state.histories){const auto known=std::as_const(plan->records).storage();
        if(known.get()==storage.get()&&!known.owner_before(storage)&&!storage.owner_before(known)){
            ++state.stats.history.identityHits;plan->used=++state.tick;return plan;
        }
    }
    const auto denied=[&]()->std::shared_ptr<MaskHistoryPlan>{++state.stats.history.denied;return {};};
    if(!state.budget||!state.capacity)return denied();
    const auto liveBytes=state.pathBytes();
    std::erase_if(state.refusals,[&](const auto& r){return r->owner.expired()||r->pathBytes!=liveBytes;});
    for(auto& r:state.refusals)if(r->pointee==storage.get()&&!r->owner.owner_before(storage)&&!storage.owner_before(r->owner)){
        r->used=++state.tick;refusalHit=true;++state.stats.refusalHits;return denied();
    }
    // Optional residency has a structural lower bound, independent of GPU
    // completion. Reject it before constructing a doomed primitive prefix.
    // This is not a mask/document limit: the original GPU draw remains valid.
    std::size_t count=0;for(const auto& mask:records){const auto n=std::visit([](const auto& g)->std::size_t{
        if constexpr(requires{g.segments;})return g.segments.size();else return 1;},mask.geometry);
        if(n>capacity/sizeof(std::shared_ptr<const PreparedMaskPrimitive>)-count){structural=true;return denied();}count+=n;
    }
    std::size_t minimum=0;
    const auto charge=[&](std::size_t n,std::size_t bytes){if(bytes&&n>(capacity-minimum)/bytes)return false;minimum+=n*bytes;return true;};
    if(!charge(1,sizeof(MaskHistoryPlan)+256)||!charge(count,sizeof(std::shared_ptr<const PreparedMaskPrimitive>))||
       !charge(records.size(),sizeof(std::size_t)+sizeof(std::shared_ptr<const void>))||
       !charge(1,sourceMaskBytes(*storage))||!charge(count,sizeof(PreparedMaskPrimitive)+128)){
        structural=true;return denied();
    }
    std::shared_ptr<MaskHistoryPlan> prefix;std::size_t common=0;
    const auto compare=[&](const std::shared_ptr<MaskHistoryPlan>& plan){
        if(!plan||plan->usage!=state.usage)return;std::size_t n=0;const auto& old=std::as_const(plan->records);
        while(n<std::min(old.size(),records.size())&&equalMask(old[n],records[n],state.stats.history))++n;
        if(n>common){prefix=plan;common=n;}
    };
    if(previous)compare(previous);
    else for(const auto& plan:state.histories)compare(plan);
    const auto metadata=sizeof(MaskHistoryPlan)+256+count*sizeof(std::shared_ptr<const PreparedMaskPrimitive>)+
        records.size()*(sizeof(std::size_t)+sizeof(std::shared_ptr<const void>));
    const auto quotaDenials=state.sources->statistics().quotaDenials;
    auto retention=state.sources->getMasks(records,state.budget,true);if(!retention){
        if(state.sources->statistics().quotaDenials!=quotaDenials)status=MaskHistoryAdmission::kResourcePending;
        return denied();
    }
    auto sourceCharge=state.sourceCharge(retention);if(!sourceCharge)return denied();
    const auto accounted=metadata;
    const auto reserve=[&](std::size_t bytes,std::size_t weight)->std::optional<std::uint64_t>{
        if(weight>state.capacity)return {};
        for(;;){
            const bool room=state.size()<512&&state.usage->bytes<=state.capacity-weight;
            if(room)
                if(auto token=state.budget->reserve(resources::ResourceClass::kRecorded,bytes,true))return token;
            if(!state.evict()){status=room?MaskHistoryAdmission::kResourcePending:MaskHistoryAdmission::kNotAdmitted;return {};}
        }
    };
    auto token=reserve(metadata,accounted);if(!token)return denied();resources::ResourceLease lease(state.budget,*token);
    auto result=std::make_shared<MaskHistoryPlan>();result->records=records;result->metadata=std::move(lease);
    result->source=std::move(retention);result->localSource=std::move(sourceCharge);result->usage=state.usage;result->bytes=accounted;state.usage->bytes+=accounted;
    result->identity=++state.nextHistory;result->used=++state.tick;
    result->primitives.reserve(count);result->primitiveEnds.reserve(records.size());result->exactPaths.resize(records.size());
    if(prefix){
        const auto end=prefix->primitiveEnds[common-1];result->prefixIdentity=prefix->identity;result->prefixPrimitives=end;
        result->primitives.insert(result->primitives.end(),prefix->primitives.begin(),prefix->primitives.begin()+end);
        result->primitiveEnds.insert(result->primitiveEnds.end(),prefix->primitiveEnds.begin(),prefix->primitiveEnds.begin()+common);
        std::copy_n(prefix->exactPaths.begin(),common,result->exactPaths.begin());
        state.stats.history.prefixReuses+=common;state.stats.history.copiedBytes+=end*sizeof(result->primitives.front())+
            common*(sizeof(std::size_t)+sizeof(std::shared_ptr<const void>));
    }else ++state.stats.history.resets;
    const auto prepare=[&](const auto& value)->bool{
        const auto units=[&]()->std::size_t{if constexpr(requires{value.path;})return value.path.commands.size();else return 4;}();
        if(units>(state.capacity-std::min(state.capacity,sizeof(PreparedMaskPrimitive)+65536U))/256U)return false;
        const auto estimate=sizeof(PreparedMaskPrimitive)+65536U+units*256U;
        auto t=reserve(estimate,estimate);if(!t)return false;resources::ResourceLease reservation(state.budget,*t);
        auto primitive=makePreparedMaskPrimitive();
        if constexpr(requires{value.p0;}){
            primitive->swept=true;primitive->width=float(2*std::max(value.p0.radius,value.p1.radius));
            SkPathBuilder path;path.moveTo(float(value.p0.position.x),float(value.p0.position.y));
            path.cubicTo(float(value.control1.x),float(value.control1.y),float(value.control2.x),float(value.control2.y),float(value.p1.position.x),float(value.p1.position.y));
            primitive->path=path.detach();
            primitive->bounds=SkRect::MakeLTRB(float(std::min({value.p0.position.x,value.p1.position.x,value.control1.x,value.control2.x})),float(std::min({value.p0.position.y,value.p1.position.y,value.control1.y,value.control2.y})),float(std::max({value.p0.position.x,value.p1.position.x,value.control1.x,value.control2.x})),float(std::max({value.p0.position.y,value.p1.position.y,value.control1.y,value.control2.y})));
            primitive->bounds.outset(float(std::max(value.p0.radius,value.p1.radius)),float(std::max(value.p0.radius,value.p1.radius)));
        }else {primitive->path=semanticPath(value.path);primitive->bounds=primitive->path.getBounds();}
        const auto bytes=sizeof(PreparedMaskPrimitive)+128+primitive->path.approximateBytesUsed();
        if(bytes>estimate||!reservation.shrink(bytes))return false;
        primitive->lease=std::move(reservation);primitive->usage=state.usage;primitive->bytes=bytes;state.usage->bytes+=bytes;
        result->primitives.push_back(std::move(primitive));++state.stats.history.primitiveBuilds;++state.stats.history.flattenedPrimitives;return true;
    };
    for(std::size_t i=common;i<records.size();++i){
        bool ok=true;std::visit([&](const auto& g){if constexpr(requires{g.segments;}){
            for(const auto& segment:g.segments)if(!prepare(segment)){ok=false;break;}
        }else ok=prepare(g);},records[i].geometry);
        if(!ok)return denied();result->primitiveEnds.push_back(result->primitives.size());
    }
    state.histories.push_back(result);++state.stats.history.admissions;return result;
    }();
    if(result||records.empty())status=MaskHistoryAdmission::kReady;
    else if(status==MaskHistoryAdmission::kResourcePending)++state.stats.resourceWaits;
    else {
        ++state.stats.localRefusals;
        // Pin the immutable source: a weak identity alone would not force COW
        // when a caller mutates its last SharedVector owner. The source and
        // witness share the existing local/global quota and512 cardinality.
        // Occupancy changes invalidate it so held-plan pressure can recover.
        constexpr auto bytes=sizeof(Impl::Refusal)+sizeof(std::shared_ptr<Impl::Refusal>)+128;
        if(!refusalHit&&!structural&&state.budget&&bytes<=capacity){
            auto retained=state.sources->getMasks(records,state.budget,true);
            if(!retained||retained->bytes>capacity-bytes){if(admission)*admission=status;return result;}
            auto sourceCharge=state.sourceCharge(retained);if(!sourceCharge){if(admission)*admission=status;return result;}
            const auto weight=bytes;
            std::optional<std::uint64_t> token;
            for(;;){
                if(state.size()<512&&state.usage->bytes<=capacity-weight)
                    token=state.budget->reserve(resources::ResourceClass::kRecorded,bytes,true);
                if(token||!state.evict())break;
            }
            if(token){
                auto r=std::make_shared<Impl::Refusal>();r->owner=storage;r->pointee=storage.get();r->pathBytes=state.pathBytes();
                r->source=std::move(retained);r->localSource=std::move(sourceCharge);r->metadata=resources::ResourceLease(state.budget,*token);r->usage=state.usage;r->bytes=weight;r->used=++state.tick;
                state.usage->bytes+=weight;state.refusals.push_back(std::move(r));
            }
        }
    }
    if(admission)*admission=status;
    return result;
}
std::shared_ptr<const EraseMaskPathCache::Entry> EraseMaskPathCache::historicalPath(const std::shared_ptr<MaskHistoryPlan>& plan,std::size_t index){
    if(!plan||plan->usage!=impl_->usage||index>=plan->records.size())return {};
    ++impl_->stats.history.exactPathLookups;
    if(plan->exactPaths[index]){++impl_->stats.history.exactPathHits;return std::static_pointer_cast<const Entry>(plan->exactPaths[index]);}
    auto built=get(std::as_const(plan->records)[index].geometry,true);if(built)plan->exactPaths[index]=built;return built;
}
std::shared_ptr<ExactHistoryProof> EraseMaskPathCache::exactHistory(const scene::SharedVector<semantic::EraseMaskRecord>& records){
    auto& state=*impl_;if(records.empty())return {};
    const auto owner=records.storage();
    for(auto& proof:state.exactHistories){const auto known=proof->records.storage();
        if(known.get()==owner.get()&&!known.owner_before(owner)&&!owner.owner_before(known)){
            ++state.stats.history.identityHits;proof->used=++state.tick;return proof;
        }
        const auto& old=std::as_const(proof->records);
        if(old.size()==records.size()&&std::equal(old.begin(),old.end(),records.begin(),
            [&](const auto& a,const auto& b){return equalMask(a,b,state.stats.history);})){
            proof->used=++state.tick;return proof;
        }
    }
    const auto denied=[&]()->std::shared_ptr<ExactHistoryProof>{++state.stats.history.denied;return {};};
    const auto count=records.size();
    if(!state.budget||!state.capacity||count>(state.capacity-std::min(state.capacity,sizeof(ExactHistoryProof)+256))/sizeof(std::shared_ptr<const Entry>))return denied();
    auto retained=state.sources->getMasks(records,state.budget,true);if(!retained)return denied();
    auto sourceCharge=state.sourceCharge(retained);if(!sourceCharge)return denied();
    const auto bytes=sizeof(ExactHistoryProof)+256+count*sizeof(std::shared_ptr<const Entry>);
    std::optional<std::uint64_t> token;
    for(;;){
        if(bytes<=state.capacity&&state.size()<512&&state.usage->bytes<=state.capacity-bytes)
            token=state.budget->reserve(resources::ResourceClass::kRecorded,bytes,true);
        if(token)break;if(!state.evict())return denied();
    }
    resources::ResourceLease lease(state.budget,*token);
    auto proof=std::make_shared<ExactHistoryProof>();proof->records=records;proof->paths.resize(count);
    proof->source=std::move(sourceCharge);proof->metadata=std::move(lease);proof->usage=state.usage;proof->bytes=bytes;
    proof->used=++state.tick;proof->identity=++state.nextHistory;state.usage->bytes+=bytes;state.exactHistories.push_back(proof);return proof;
}
std::shared_ptr<const EraseMaskPathCache::Entry> EraseMaskPathCache::exactHistoricalPath(const std::shared_ptr<ExactHistoryProof>& proof,std::size_t index){
    if(!proof||proof->usage!=impl_->usage||index>=proof->records.size())return {};
    ++impl_->stats.history.exactPathLookups;
    if(auto known=proof->paths[index].lock()){++impl_->stats.history.exactPathHits;return known;}
    auto path=get(std::as_const(proof->records)[index].geometry,true);
    if(path)proof->paths[index]=path;
    return path;
}

SelectionGeometryHit EraseMaskPathCache::query(const RuntimeSceneRecord& record,const semantic::EraseMaskGeometry& mask,
    std::shared_ptr<resources::ResourceBudgetManager> budget,std::size_t capacity,PreparedGeometryCache* prepared,const RenderCacheOptions* options)noexcept{
    capacity=std::min<std::size_t>(capacity,8U*1024U*1024U);
    [[maybe_unused]]const auto historyBefore=impl_->stats.history;
    std::shared_ptr<ExactHistoryProof> historyPlan;
    try{
        if(impl_->budget!=budget||impl_->capacity!=capacity){clear();impl_->usage=std::make_shared<ErasePathUsage>();impl_->budget=std::move(budget);impl_->capacity=capacity;}
    }
    catch(const std::bad_alloc&){++impl_->stats.history.denied;}
    EraseGeometryWork pathOps;
    auto& cost=impl_->stats.exact;[[maybe_unused]]const auto before=cost;++cost.queries;cost.bodyIdentity=0;cost.fallbackReason=0;
    const auto run=[&]()->SelectionGeometryHit{
        try{
            std::shared_ptr<const PreparedNavigationGeometry> body;
            resources::ResourceLease scratch;
            const auto* compact=std::get_if<semantic::CompactStrokeContent>(&record.content.get());
            if(prepared&&options&&options->enabled&&compact&&record.kind==semantic::ObjectKind::kVectorStroke&&
                compact->stroke.config.algorithmVersion==4&&
                (compact->stroke.config.family==foundation::InkFamily::kRound||compact->stroke.config.family==foundation::InkFamily::kMarker)&&
                compact->stroke.config.opacity*compact->stroke.config.color[3]>0){
                G5_SCOPE(g5BodyLookup,foundation::internal::g5::Tag::wa_body_lookup,foundation::internal::g5::Purpose::none);
                // The source lease is O(1), not a record/history/geometry clone.
                ReferenceTraversalEntry entry;entry.record.content=record.content;
                entry.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(record.content)};
                const auto initial=prepared->statistics();
                body=prepared->get(entry,*options,impl_->budget,true,PreparedConsumer::kExact,true,true);
                if(!body){
                    const auto valid=semantic::validCompactStroke(compact->stroke);++cost.fullValidations;
                    if(valid)cost.validationKnots+=compact->stroke.knots.size();else ++cost.invalidValidationScans;
                    if(valid)body=prepared->get(entry,*options,impl_->budget,true,PreparedConsumer::kExact);
                    else cost.fallbackReason=6;
                }
                const auto after=prepared->statistics();
                cost.preparedValidationKnots+=after.validationKnots-initial.validationKnots;
                cost.preparedCompileCalls+=after.syncCompileCalls-initial.syncCompileCalls;
                cost.preparedCompileInputs+=after.syncCompileInputs-initial.syncCompileInputs;
                cost.preparedComponents+=after.syncPathPrimitives-initial.syncPathPrimitives;
                cost.contentComparisonKnots+=after.contentComparisonKnots-initial.contentComparisonKnots;
                if(body&&!body->ready()){cost.fallbackReason=3;body.reset();}
                else if(body&&!body->exactValidated()){cost.fallbackReason=5;body.reset();}
                else if(!body&&!cost.fallbackReason)cost.fallbackReason=4;
                if(body){
                    // Bounded per-query path/callback scratch; source/replay and
                    // index capacity remain charged to the pinned prepared body.
                    auto token=impl_->budget->reserve(resources::ResourceClass::kScratch,4096,true);
                    if(token)scratch=resources::ResourceLease(impl_->budget,*token);
                    else {cost.fallbackReason=8;body.reset();}
                }
            }else cost.fallbackReason=prepared&&options?(options->enabled?2:7):1;
            if(body){++cost.bodyUses;cost.bodyIdentity=body->identity();}
            else ++cost.fallbacks;
            // The one retained decision pins its validated complete body and
            // canonical query path/history, plus charged metadata. IDs alone
            // never qualify equality. Unknown allocations still go through
            // PreparedGeometryCache's complete body comparison above.
            const auto sameHistory=[&](const auto& old){
                if(!old)return record.eraseMasks.empty();
                const auto& a=std::as_const(old->records);const auto& b=record.eraseMasks;
                return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),[&](const auto& x,const auto& y){
                    return equalMask(x,y,impl_->stats.history);});
            };
            // Prove a repeat before requesting any new draw/history/path
            // allocation. Otherwise the reservation itself can evict the result
            // that would have avoided all of this work under local pressure.
            const auto equivalent=[&](const auto& old){return old&&old->body==body&&old->query==mask&&
                old->transform==record.transform&&old->version==record.kindVersion&&sameHistory(old->history);};
            if(body){
                if(equivalent(impl_->lastQuery)){
                    ++impl_->stats.equivalentQueryHits;UR2D_COUNT(equivalentHits);return impl_->lastQuery->result;
                }
                for(const auto& old:impl_->cohortQueries)if(equivalent(old)){
                    impl_->lastQuery=old;++impl_->stats.equivalentQueryHits;UR2D_COUNT(equivalentHits);return old->result;
                }
            }
            historyPlan=exactHistory(record.eraseMasks);
            const auto result=eraseGeometryQuery(record,mask,&pathOps,true,this,&cost,body.get(),historyPlan,&impl_->stats.history);
            if(body&&(record.eraseMasks.empty()||historyPlan)&&result!=Hit::kInvalid&&result!=Hit::kUnsupported){
                impl_->lastQuery.reset();
                const auto raw=std::visit([](const auto& g)->std::size_t{
                    if constexpr(requires{g.segments;})return g.segments.size()*sizeof(semantic::EraseCubicSegment);
                    else return g.path.commands.size()*sizeof(semantic::PathCommand);
                },mask);
                const auto bytes=sizeof(Impl::QueryResult)+256+raw;
                std::optional<std::uint64_t> token;
                for(;;){
                    if(impl_->size()<512&&bytes<=impl_->capacity&&impl_->usage->bytes<=impl_->capacity-bytes)
                        token=impl_->budget->reserve(resources::ResourceClass::kRecorded,bytes,true);
                    if(token||!impl_->evict())break;
                }
                if(token){
                    resources::ResourceLease lease(impl_->budget,*token);
                    impl_->lastQuery=std::make_shared<Impl::QueryResult>(body,mask,historyPlan,record.transform,record.kindVersion,result,
                        std::move(lease),impl_->usage,bytes);
                    // The per-result256-byte allowance includes the list node
                    // and shared control block, released with that result.
                    if(impl_->cohortDepth)impl_->cohortQueries.push_back(impl_->lastQuery);
                }
            }
            return result;
        }catch(const std::bad_alloc&){++cost.fallbacks;cost.fallbackReason=4;return eraseGeometryQuery(record,mask,nullptr,true,this,&cost);}
    };
    const auto result=run();
    impl_->stats.history.historicalDifferences+=pathOps.historicalDifferences;
    impl_->stats.history.componentIntersections+=pathOps.componentIntersections;
    G5_OBSERVE(foundation::internal::g5::Kind::Counter,foundation::internal::g5::Tag::wa_history_lookup,false,
        1,impl_->capacity,impl_->usage->bytes,historyPlan?1ULL:0ULL,record.eraseMasks.size(),
        impl_->stats.history.denied-historyBefore.denied,impl_->stats.history.primitiveBuilds-historyBefore.primitiveBuilds,
        impl_->stats.history.queryUnionOps-historyBefore.queryUnionOps,impl_->stats.history.historyUnionOps-historyBefore.historyUnionOps,
        impl_->stats.equivalentQueryHits,cost.fallbackReason);
    namespace g5=foundation::internal::g5;
    [[maybe_unused]]const auto historyAfter=impl_->stats.history;
    G5_OBSERVE(g5::Kind::Counter,g5::Tag::mask_history,false,2,g5::word(record.objectId,0),g5::word(record.objectId,1),
        reinterpret_cast<std::uintptr_t>(record.eraseMasks.storage().get()),historyPlan?historyPlan->identity:0,
        historyAfter.identityHits-historyBefore.identityHits,historyAfter.valueRecords-historyBefore.valueRecords,
        historyAfter.valuePrimitives-historyBefore.valuePrimitives,historyAfter.flattenedPrimitives-historyBefore.flattenedPrimitives,
        historyAfter.exactPathHits-historyBefore.exactPathHits,historyAfter.exactPathLookups-historyBefore.exactPathLookups,
        pathOps.historicalDifferences,pathOps.componentIntersections,static_cast<unsigned>(result));
    G5_OBSERVE(g5::Kind::Counter,g5::Tag::mask_history,false,4,g5::word(record.objectId,0),g5::word(record.objectId,1),
        reinterpret_cast<std::uintptr_t>(this),historyPlan?historyPlan->identity:0,
        historyAfter.queryUnionOps-historyBefore.queryUnionOps,historyAfter.historyUnionOps-historyBefore.historyUnionOps,
        historyAfter.queryPathBuilds-historyBefore.queryPathBuilds,historyAfter.historyPathBuilds-historyBefore.historyPathBuilds,
        historyAfter.differenceNanoseconds-historyBefore.differenceNanoseconds,historyAfter.intersectionNanoseconds-historyBefore.intersectionNanoseconds,
        historyAfter.queryPathNanoseconds-historyBefore.queryPathNanoseconds,historyAfter.historyPathNanoseconds-historyBefore.historyPathNanoseconds);
    G5_OBSERVE(g5::Kind::Counter,g5::Tag::exact_body,false,1,g5::word(record.objectId,0),g5::word(record.objectId,1),reinterpret_cast<std::uintptr_t>(prepared),reinterpret_cast<std::uintptr_t>(record.content.shared().get()),cost.bodyIdentity,cost.bodyUses-before.bodyUses,cost.fallbackReason,static_cast<unsigned>(result),record.eraseMasks.size(),cost.fullValidations-before.fullValidations,cost.validationKnots-before.validationKnots,cost.compileCalls-before.compileCalls,cost.compileInputs-before.compileInputs,cost.constructedComponents-before.constructedComponents);
    G5_OBSERVE(g5::Kind::Counter,g5::Tag::exact_body,false,2,cost.bodyIdentity,cost.indexNodes-before.indexNodes,cost.boundsTests-before.boundsTests,cost.componentChecks-before.componentChecks,cost.replayKnots-before.replayKnots,cost.materializedPaths-before.materializedPaths,cost.preparedValidationKnots-before.preparedValidationKnots,cost.preparedCompileCalls-before.preparedCompileCalls,cost.preparedCompileInputs-before.preparedCompileInputs,cost.preparedComponents-before.preparedComponents,cost.contentComparisonKnots-before.contentComparisonKnots);
    return result;
}
SelectionGeometryHit intersectEraseGeometryComponentOracle(const RuntimeSceneRecord& record,const semantic::EraseMaskGeometry& mask) noexcept {
    return eraseGeometryQuery(record,mask,nullptr,false);
}
SelectionGeometryHit EraseMaskPathCache::selectionQuery(const RuntimeSceneRecord& record,
    std::span<const foundation::WorldPoint> region,std::shared_ptr<resources::ResourceBudgetManager> budget,
    std::size_t capacity,PreparedGeometryCache* prepared,const RenderCacheOptions* options)noexcept {
    const auto fallback=[&]{return intersectVisibleSelectionGeometry(record,region);};
    const auto* compact=std::get_if<semantic::CompactStrokeContent>(&record.content.get());
    if(!budget||!prepared||!options||!options->enabled||record.kind!=semantic::ObjectKind::kVectorStroke||!compact||
       compact->stroke.config.algorithmVersion!=4||
       (compact->stroke.config.family!=foundation::InkFamily::kRound&&compact->stroke.config.family!=foundation::InkFamily::kMarker)||
       compact->stroke.config.opacity*compact->stroke.config.color[3]<=0)return fallback();
    try {
        const auto& t=record.transform;const double determinant=t.a*t.d-t.b*t.c;
        if(region.size()<3||!std::isfinite(determinant)||std::abs(determinant)<1e-12)return Hit::kInvalid;
        SkPathBuilder builder(SkPathFillType::kEvenOdd);
        for(std::size_t i=0;i<region.size();++i){
            const double x=region[i].x-t.tx,y=region[i].y-t.ty;
            const float lx=f((t.d*x-t.c*y)/determinant),ly=f((-t.b*x+t.a*y)/determinant);
            if(!std::isfinite(lx)||!std::isfinite(ly))return Hit::kInvalid;
            if(i)builder.lineTo(lx,ly);else builder.moveTo(lx,ly);
        }
        builder.close();auto selection=builder.detach();auto contact=stroke(selection,2e-4);
        auto plan=history(record.eraseMasks,budget,std::min<std::size_t>(capacity,8*1024*1024));
        ReferenceTraversalEntry entry;entry.record.content=record.content;
        entry.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(record.content)};
        auto& work=impl_->stats.exact;++work.queries;
        const auto before=prepared->statistics();
        auto body=prepared->get(entry,*options,budget,true,PreparedConsumer::kExact,true,true);
        if(!body){
            ++work.fullValidations;const bool valid=semantic::validCompactStroke(compact->stroke);
            if(!valid)return fallback();work.validationKnots+=compact->stroke.knots.size();
            body=prepared->get(entry,*options,budget,true,PreparedConsumer::kExact);
        }
        const auto after=prepared->statistics();
        work.preparedCompileCalls+=after.syncCompileCalls-before.syncCompileCalls;
        work.preparedCompileInputs+=after.syncCompileInputs-before.syncCompileInputs;
        work.preparedValidationKnots+=after.validationKnots-before.validationKnots;
        work.contentComparisonKnots+=after.contentComparisonKnots-before.contentComparisonKnots;
        if(!body||!body->ready()||!body->exactValidated())return fallback();
        auto token=budget->reserve(resources::ResourceClass::kScratch,4096,true);
        if(!token)return fallback();resources::ResourceLease scratch(budget,*token);
        ++work.bodyUses;work.bodyIdentity=body->identity();
        // Validate every historical path before an empty/disjoint shortcut.
        // The old selection evaluates the entire source/history, unlike erase.
        std::vector<std::shared_ptr<const Entry>> pins;pins.reserve(record.eraseMasks.size());
        std::vector<SkPath> paths;paths.reserve(record.eraseMasks.size());
        for(std::size_t i=0;i<record.eraseMasks.size();++i){
            auto path=plan?historicalPath(plan,i):get(record.eraseMasks[i].geometry,true);
            if(!path||!path->path.isFinite())return fallback();
            paths.push_back(path->path);pins.push_back(std::move(path));
        }
        // Preserve both original tests separately. The contact outline is a
        // stroke of the original selection, never a stroke of query-minus-mask.
        for(const auto& path:paths){
            auto remaining=Op(selection,path,kDifference_SkPathOp);
            auto remainingContact=Op(contact,path,kDifference_SkPathOp);
            impl_->stats.history.historicalDifferences+=2;
            if(!remaining||!remainingContact)return fallback();
            selection=*remaining;contact=*remainingContact;
        }
        bool invalid=false,hit=false;SkRect demand=selection.getBounds();demand.join(contact.getBounds());
        if(selection.isEmpty()&&contact.isEmpty())return Hit::kMiss;
        ExactComponentWork local;
        body->visitExact(demand,[&](const SkPath& part){
            for(const auto* query:{&selection,&contact}){
                if(part.isEmpty()||query->isEmpty()||!SkRect::Intersects(part.getBounds(),query->getBounds()))continue;
                ++impl_->stats.history.componentIntersections;
                const auto overlap=Op(part,*query,kIntersect_SkPathOp);
                if(!overlap){invalid=true;return false;}
                if(!overlap->isEmpty()){hit=true;return false;}
            }return true;
        },local);
        work.indexNodes+=local.groups;work.boundsTests+=local.boundsTests;work.componentChecks+=local.components;
        work.replayKnots+=local.replayKnots;work.materializedPaths+=local.materializedPaths;
        return invalid?fallback():hit?Hit::kHit:Hit::kMiss;
    }catch(const std::bad_alloc&){return fallback();}
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
#else
namespace canvas::render {
SelectionGeometryHit intersectEraseGeometry(const RuntimeSceneRecord&,const semantic::EraseMaskGeometry&,EraseGeometryWork*) noexcept {return SelectionGeometryHit::kUnsupported;}
SelectionGeometryHit intersectVisibleSelectionGeometry(const RuntimeSceneRecord&,std::span<const foundation::WorldPoint>) noexcept {return SelectionGeometryHit::kUnsupported;}
}
#endif
