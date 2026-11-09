#include "canvas/foundation/internal/g5_observation.hpp"
namespace g5 = canvas::foundation::internal::g5;
#include "canvas/foundation/internal/frame_timing.hpp"
#include "canvas/render/skia_renderer.hpp"
#include "resource_reclaim.hpp"
#include "render_execution.hpp"

#include "canvas/render/skia_brush_renderer.hpp"
#include "canvas/render/brush_outline_skia_path.hpp"
#include "canvas/render/skia_ink_backend.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "skia_render_cache_internal.hpp"
#include "skia_navigation_tiles.hpp"
#include "navigation_source_snapshot.hpp"
#include "skia_gpu_lifetime.hpp"
#include "selection_region_geometry_internal.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkBlendMode.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkSurface.h"
#include "include/core/SkImage.h"
#include "include/core/SkVertices.h"
#include "include/effects/SkDashPathEffect.h"
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <chrono>

#include <cmath>
#include <algorithm>
#include <type_traits>

namespace canvas::render {
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
namespace {
struct ObservationCounters final {
 const SkiaRenderer& renderer;bool on=g5::enabled();RenderCacheWork before=on?renderer.renderCacheWork():RenderCacheWork{};
 ~ObservationCounters(){if(!on)return;const auto w=renderer.renderCacheWork();
  G5_OBSERVE(g5::Kind::Counter,g5::Tag::mask,false,3,w.bodyTileBuilds-before.bodyTileBuilds,w.bodyTileHits-before.bodyTileHits,w.maskTileUpdates-before.maskTileUpdates,w.maskPrimitiveDraws-before.maskPrimitiveDraws,w.maskTileResets-before.maskTileResets,w.inkLayerBytes);
  G5_OBSERVE(g5::Kind::Counter,g5::Tag::coverage,false,4,w.gpuCoverageTiles-before.gpuCoverageTiles,w.cpuCoverageTiles-before.cpuCoverageTiles,w.gpuInteriorPixels-before.gpuInteriorPixels,w.cpuEdgePixels-before.cpuEdgePixels,w.preparationCoverageSteps-before.preparationCoverageSteps,w.coverageBuilds-before.coverageBuilds,w.coverageHits-before.coverageHits,w.retainedBytes,w.coverageBytes,w.scratchPeakBytes);
 }
};
}
#endif

SkiaRenderer::SkiaRenderer(){
    renderCacheOptions_.maxBytes=std::size_t(AXIOM_RENDER_CACHE_MIB)*1024*1024;
    renderCacheOptions_.sharedMaxBytes=std::size_t(AXIOM_RENDER_SHARED_MIB)*1024*1024;
}
SkiaRenderer::~SkiaRenderer(){
    // Seal quota before old contexts can abandon their completion callbacks.
    if(resourceBudget_)resourceBudget_->close();
    clearRenderCache();gpuRetirements_.reset();
}
void SkiaRenderer::serviceResourceRetirements()noexcept {if(gpuRetirements_)gpuRetirements_->service();}

// Constant-size validity only. Pixels remain owned by the qualified target;
// canonical source records and mask-prefix validation stay in RenderGroupManager.
struct SkiaRenderer::PartialEraseFrame final {
    FrameState frame;
    WorldToViewAffine transform;
    foundation::DocumentBackground background;
    bool transparent=false,isolate=false;
    const SkiaSurfaceProvider* provider=nullptr;
    const SkSurface* surface=nullptr;
    const internal::SkiaRenderCache* cache=nullptr;
    std::uint64_t sequence=0,resource=0,presents=0;
    std::uint32_t contents=0;
    explicit PartialEraseFrame(const FramePlan& p):frame(p.frame),transform(p.referenceDrawList.worldToView),
        background(p.background),transparent(p.transparentBackground),isolate(p.isolateEraseMasks){}
    bool matches(const SkiaSurfaceProvider& target,SkSurface& acquired,const FramePlan& p,
                 const internal::SkiaRenderCache& current,std::uint64_t epoch)const {
        const auto& f=p.frame;
        return provider==&target&&surface==&acquired&&cache==&current&&sequence+1==current.frameSequence()&&
            presents==target.presentCount()&&contents==acquired.generationID()&&resource==epoch&&
            frame.viewId==f.viewId&&frame.camera==f.camera&&frame.worldViewport==f.worldViewport&&
            frame.sceneGeneration==f.sceneGeneration&&frame.sceneReadToken==f.sceneReadToken&&
            frame.metrics==f.metrics&&frame.metricsGeneration==f.metricsGeneration&&
            frame.surfaceGeneration==f.surfaceGeneration&&frame.viewportClip==f.viewportClip&&
            frame.frameId.value()<f.frameId.value()&&transform==p.referenceDrawList.worldToView&&
            background==p.background&&transparent==p.transparentBackground&&isolate==p.isolateEraseMasks;
    }
};
struct SkiaRenderer::NavigationCache final {
    const void* provider=nullptr;
    sk_sp<SkImage> image;
    std::unique_ptr<FrameState> frame;
    WorldToViewAffine transform;
    resources::ResourceLease lease;
    std::shared_ptr<const internal::PreparedFrameImage> prepared;
    SkColor background=SK_ColorWHITE;
    std::shared_ptr<const internal::NavigationSourceSnapshot> sources;
    internal::NavigationRegions valid;
    std::uint64_t proofSemantic=0,proofScene=0,resource=0;
    bool isolate=false;
    std::optional<CanonicalViewportTransform> feedbackCamera;
    bool backgroundProofRejected=false;
    bool sameTarget(const void* target,const FrameState& current)const noexcept {
        return image&&frame&&provider==target&&frame->viewId==current.viewId&&
            frame->metrics==current.metrics&&frame->metricsGeneration==current.metricsGeneration&&
            frame->surfaceGeneration==current.surfaceGeneration&&frame->viewportClip==current.viewportClip&&
            transform.a>0&&std::isfinite(transform.a)&&transform.d==transform.a&&transform.b==0&&transform.c==0&&
            std::isfinite(transform.tx)&&std::isfinite(transform.ty);
    }
    bool matches(const void* target,const FrameState& current)const noexcept {
        return sameTarget(target,current)&&valid.count&&proofSemantic==current.sceneGeneration.value()&&proofScene==current.sceneReadToken.value();
    }
    foundation::WorldRect worldBounds()const noexcept {
        return {float(-transform.tx/transform.a),float(-transform.ty/transform.a),
            float((frame->metrics.logicalWidth-transform.tx)/transform.a),
            float((frame->metrics.logicalHeight-transform.ty)/transform.a)};
    }
    double worldArea()const noexcept {
        return frame->metrics.logicalWidth*frame->metrics.logicalHeight/(transform.a*transform.a);
    }
};
struct SkiaRenderer::CompactCache final {
    struct Entry {std::shared_ptr<const ink::BrushPreviewDelta::Chunk> prefix;sk_sp<SkSurface> surface;foundation::InkBrushConfig config;};
    std::unordered_map<std::uint64_t,Entry> entries;
    sk_sp<SkSurface> scratch;
    std::uint64_t generation=0;float scale=0,x=0,y=0,px=0,py=0;int width=0,height=0;
};
namespace {
// One connected mesh, including its edge fringe and end caps. Adjacent samples
// share coverage: per-segment round caps would accumulate alpha into dark dots.
void drawFadingTrail(SkCanvas& canvas,const ink::FadingTrailPreview& input,SkColor4f paintColor){
    std::vector<ink::FadingTrailPoint> points;points.reserve(input.points.size());
    for(const auto& point:input.points){
        if(!points.empty()&&std::hypot(point.x-points.back().x,point.y-points.back().y)<1e-5)points.back()=point;
        else points.push_back(point);
    }
    if(points.empty())return;
    const auto colorAt=[&](float opacity){auto color=paintColor;color.fA*=std::clamp(opacity,0.0F,1.0F);return color.toSkColor();};
    SkPaint paint;paint.setAntiAlias(true);paint.setColor(SK_ColorWHITE);
    if(points.size()==1){paint.setColor(colorAt(points[0].opacity));canvas.drawCircle(float(points[0].x),float(points[0].y),points[0].width*.5F,paint);return;}
    struct Vertex {SkPoint position;SkColor color;};
    std::vector<SkPoint> positions;std::vector<SkColor> colors;
    positions.reserve(points.size()*18+216);colors.reserve(points.size()*18+216);
    const auto triangle=[&](Vertex a,Vertex b,Vertex c){for(const auto v:{a,b,c}){positions.push_back(v.position);colors.push_back(v.color);}};
    const auto quad=[&](Vertex a,Vertex b,Vertex c,Vertex d){triangle(a,b,c);triangle(b,d,c);};
    const auto direction=[&](std::size_t from,std::size_t to){const auto dx=float(points[to].x-points[from].x),dy=float(points[to].y-points[from].y);const auto length=std::hypot(dx,dy);return SkPoint::Make(dx/length,dy/length);};
    const float fringe=1.0F/std::max(1e-5F,canvas.getTotalMatrix().getMaxScale());
    std::vector<std::array<Vertex,4>> sections;sections.reserve(points.size());
    for(std::size_t i=0;i<points.size();++i){
        const auto previous=direction(i?i-1:i,i?i:i+1);
        const auto next=direction(i+1<points.size()?i:i-1,i+1<points.size()?i+1:i);
        float nx=-(previous.y()+next.y()),ny=previous.x()+next.x();
        const auto length=std::hypot(nx,ny);
        if(length>1e-5F){nx/=length;ny/=length;}else{nx=-next.y();ny=next.x();}
        const float miter=1.0F/std::max(.5F,std::abs(nx*-next.y()+ny*next.x()));
        const auto& point=points[i];const float radius=point.width*.5F;
        const float inner=std::max(0.0F,radius-fringe*.5F),outer=radius+fringe*.5F;
        const auto vertex=[&](float offset,SkColor color){return Vertex{SkPoint::Make(float(point.x)+nx*miter*offset,float(point.y)+ny*miter*offset),color};};
        const auto solid=colorAt(point.opacity*std::min(1.0F,point.width/fringe)),clear=colorAt(0);
        sections.push_back({vertex(outer,clear),vertex(inner,solid),vertex(-inner,solid),vertex(-outer,clear)});
        if(i)for(std::size_t j=0;j<3;++j)quad(sections[i-1][j],sections[i-1][j+1],sections[i][j],sections[i][j+1]);
    }
    constexpr unsigned capSteps=12;
    for(const bool head:{false,true}){
        const auto index=head?points.size()-1:0;const auto& point=points[index];
        const auto tangent=head?direction(index-1,index):direction(0,1);
        const float base=std::atan2(tangent.x(),-tangent.y());
        const auto solid=colorAt(point.opacity*std::min(1.0F,point.width/fringe)),clear=colorAt(0);
        const Vertex center{SkPoint::Make(float(point.x),float(point.y)),solid};
        const float inner=std::max(0.0F,point.width*.5F-fringe*.5F),outer=point.width*.5F+fringe*.5F;
        auto lastInner=sections[index][1],lastOuter=sections[index][0];
        for(unsigned k=1;k<=capSteps;++k){
            const float angle=base+(head?-1:1)*3.14159265358979323846F*float(k)/capSteps;
            Vertex nextInner{SkPoint::Make(float(point.x)+std::cos(angle)*inner,float(point.y)+std::sin(angle)*inner),solid};
            Vertex nextOuter{SkPoint::Make(float(point.x)+std::cos(angle)*outer,float(point.y)+std::sin(angle)*outer),clear};
            if(k==capSteps){nextInner=sections[index][2];nextOuter=sections[index][3];}
            triangle(center,lastInner,nextInner);quad(lastInner,lastOuter,nextInner,nextOuter);
            lastInner=nextInner;lastOuter=nextOuter;
        }
    }
    const auto mesh=SkVertices::MakeCopy(SkVertices::kTriangles_VertexMode,static_cast<int>(positions.size()),positions.data(),nullptr,colors.data());
    if(mesh)canvas.drawVertices(mesh,SkBlendMode::kModulate,paint);
}
void drawAcquisition(SkCanvas& canvas,const EditingOverlay& overlay) {
    SkPaint paint;paint.setAntiAlias(true);paint.setStrokeWidth(1.5F);
    const auto drawOutline=[&](const SelectionOutline& outline,bool candidate) {
        if (!outline.visible) return;
        SkPathBuilder path;path.moveTo(outline.corners[0].x,outline.corners[0].y);
        for (std::size_t i=1;i<4;++i) path.lineTo(outline.corners[i].x,outline.corners[i].y);
        path.close();const auto polygon=path.detach();
        if (candidate) {paint.setStyle(SkPaint::kFill_Style);paint.setColor4f({.12F,.5F,.9F,.10F},nullptr);canvas.drawPath(polygon,paint);}
        paint.setStyle(SkPaint::kStroke_Style);paint.setColor4f({.12F,.65F,.85F,candidate?.55F:.95F},nullptr);canvas.drawPath(polygon,paint);
    };
    for (const auto& outline:overlay.readOnlyOutlines) drawOutline(outline,false);
    for (const auto& outline:overlay.candidateOutlines) drawOutline(outline,true);
    if (overlay.acquisitionOutline.size()<2) return;
    SkPathBuilder path;path.setFillType(SkPathFillType::kEvenOdd);
    path.moveTo(overlay.acquisitionOutline[0].x,overlay.acquisitionOutline[0].y);
    for (std::size_t i=1;i<overlay.acquisitionOutline.size();++i) path.lineTo(overlay.acquisitionOutline[i].x,overlay.acquisitionOutline[i].y);
    path.close();const auto polygon=path.detach();
    paint.setStyle(SkPaint::kFill_Style);paint.setColor4f({.12F,.5F,.9F,.08F},nullptr);canvas.drawPath(polygon,paint);
    paint.setStyle(SkPaint::kStroke_Style);paint.setColor4f({.12F,.5F,.9F,.85F},nullptr);canvas.drawPath(polygon,paint);
}
}

namespace internal {
void drawEditingOverlayToSkCanvas(SkCanvas& canvas, const SurfaceMetrics& metrics, const EditingOverlay& overlay) {
    canvas.save();
    canvas.scale(metrics.physicalWidth / metrics.logicalWidth,
                  metrics.physicalHeight / metrics.logicalHeight);
    drawAcquisition(canvas,overlay);
    if (overlay.textBox) {
        const auto& q=*overlay.textBox;
        SkPathBuilder path;path.moveTo(q[0],q[1]);
        for(std::size_t i=1;i<4;++i)path.lineTo(q[i*2],q[i*2+1]);
        path.close();
        SkPaint paint;paint.setAntiAlias(true);paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(1);paint.setColor4f({.50F,.45F,1,.75F},nullptr);
        constexpr SkScalar dashes[]{7,5};
        paint.setPathEffect(SkDashPathEffect::Make(SkSpan<const SkScalar>(dashes,2),0));
        canvas.drawPath(path.detach(),paint);
    }
    if(overlay.textCaret){
        SkPaint paint;paint.setAntiAlias(true);paint.setColor4f({0.12F,0.45F,0.9F,0.25F},nullptr);
        const auto draw=[&](const auto& quad){SkPathBuilder path;path.moveTo(quad[0],quad[1]);for(std::size_t i=1;i<4;++i)path.lineTo(quad[i*2],quad[i*2+1]);path.close();canvas.drawPath(path.detach(),paint);};
        for(const auto& quad:overlay.textSelection)draw(quad);
        if (overlay.textCaretVisible) {
            paint.setColor4f({0.05F,0.12F,0.2F,1},nullptr);draw(*overlay.textCaret);
        }
    }
    SkPaint guidePaint;
    guidePaint.setAntiAlias(true);
    guidePaint.setStyle(SkPaint::kStroke_Style);
    guidePaint.setStrokeWidth(1.0F);
    guidePaint.setColor4f({0.18F, 0.72F, 0.86F, 0.78F}, nullptr);
    for (const auto& guide : overlay.guides()) {
        canvas.drawLine(guide.start.x, guide.start.y, guide.end.x, guide.end.y,
                         guidePaint);
    }
    SkPaint attachmentPaint;
    attachmentPaint.setAntiAlias(true);
    attachmentPaint.setColor4f({0.95F, 0.58F, 0.12F, 0.95F}, nullptr);
    for (const auto& point : overlay.attachmentPoints()) {
        if (point.active) {
            attachmentPaint.setColor4f(point.hovered
                ? SkColor4f{0.35F, 0.72F, 1.0F, 0.38F}
                : SkColor4f{0.95F, 0.58F, 0.12F, 0.95F}, nullptr);
            canvas.drawCircle(point.center.x, point.center.y, point.hovered ? 8.0F : 4.0F,
                               attachmentPaint);
        }
    }
    if (overlay.selectionOutline().visible) {
        SkPaint chrome;
        chrome.setAntiAlias(true);
        chrome.setStyle(SkPaint::kStroke_Style);
        chrome.setStrokeWidth(1.25F);
        chrome.setColor(SkColorSetRGB(0x69, 0x65, 0xFF));
        const auto& outline = overlay.selectionOutline();
        SkPathBuilder path;
        path.moveTo(outline.corners[0].x, outline.corners[0].y);
        path.lineTo(outline.corners[1].x, outline.corners[1].y);
        path.lineTo(outline.corners[2].x, outline.corners[2].y);
        path.lineTo(outline.corners[3].x, outline.corners[3].y);
        path.close();
        canvas.drawPath(path.detach(), chrome);
        // Four quiet corner grips; edge midpoint hit targets retain single-axis
        // resizing without adding more dots to the selection outline.
        for (const auto kind : {HandleKind::kTopLeft, HandleKind::kTopRight,
                                HandleKind::kBottomRight, HandleKind::kBottomLeft,
                                HandleKind::kRotation}) {
            const auto& handle = overlay.handle(kind);
            if (handle.kind == HandleKind::kNone) continue;
            const auto drawGrip = [&](const SkPaint& paint) {
                if (kind == HandleKind::kRotation) {
                    canvas.drawCircle(handle.center.x, handle.center.y,
                                      handle.visualRadius, paint);
                } else {
                    const auto bounds = SkRect::MakeLTRB(
                        handle.center.x - handle.visualRadius, handle.center.y - handle.visualRadius,
                        handle.center.x + handle.visualRadius, handle.center.y + handle.visualRadius);
                    canvas.drawRoundRect(bounds, 2.0F, 2.0F, paint);
                }
            };
            SkPaint fill;
            fill.setAntiAlias(true);
            fill.setColor(SK_ColorWHITE);
            drawGrip(fill);
            drawGrip(chrome);
        }
    }
    canvas.restore();
}
}

BackendSubmissionResult SkiaRenderer::renderFrame(
    SkiaSurfaceProvider& provider, const FramePlan& plan,
    const EditingOverlay* selectionOverlay, const RenderPreparationBudget* preparation) {
    G5_SCOPE(g5Render,g5::Tag::material,g5::Purpose::none);
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
    ObservationCounters observationCounters{*this};
#endif
    G5_ROUTE(g5Route,g5::Route::canonical_prepared);
    G5_OBSERVE(g5::Kind::Basis,g5::Tag::canonical_basis,true,plan.frame.viewId.value(),plan.frame.frameId.value(),plan.frame.sceneGeneration.value(),plan.frame.sceneReadToken.value(),plan.frame.camera.generation.value(),plan.frame.surfaceGeneration.value(),plan.frame.metricsGeneration.value(),resourceEpoch_,renderCacheOptions_.maxBytes,renderCacheOptions_.sharedMaxBytes);

    partialEraseFrame_.reset();
    serviceResourceRetirements();
    AXIOM_FRAME_TIMING_SCOPE(renderTiming,render);
    const auto& metrics=plan.frame.metrics;
    if(!std::isfinite(metrics.logicalWidth)||!std::isfinite(metrics.logicalHeight)||
       metrics.logicalWidth<=0||metrics.logicalHeight<=0||!metrics.physicalWidth||!metrics.physicalHeight)
        return BackendSubmissionResult::rejected("invalid frame surface metrics");
    AXIOM_FRAME_TIMING_SCOPE(acquireTiming,acquire);
    const auto acquired = provider.acquire();
    AXIOM_FRAME_TIMING_END(acquireTiming);
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired || acquired.frame.surface == nullptr) {
        return acquired.code == SkiaSurfaceAcquireCode::kUnavailable
            ? BackendSubmissionResult::retry(acquired.message)
            : BackendSubmissionResult::rejected(acquired.message);
    }
    if (acquired.frame.generation != plan.frame.surfaceGeneration.value()) {
        provider.release();
        return BackendSubmissionResult::rejected("stale Skia surface generation");
    }
    if(acquired.frame.surface->width()!=static_cast<int>(metrics.physicalWidth)||
       acquired.frame.surface->height()!=static_cast<int>(metrics.physicalHeight)){
        provider.release();return BackendSubmissionResult::rejected("frame target dimensions differ");
    }
    if (plan.referenceDrawList.frame.surfaceGeneration != plan.frame.surfaceGeneration ||
        plan.referenceDrawList.frame.frameId != plan.frame.frameId ||
        plan.referenceDrawList.frame.sceneGeneration != plan.frame.sceneGeneration) {
        provider.release();
        return BackendSubmissionResult::rejected("inconsistent FramePlan identity");
    }
    std::unordered_set<foundation::ObjectId,foundation::ObjectIdHash> uniqueSources;
    for(const auto& entry:plan.referenceDrawList.entries)
        if(entry.record.objectId.isZero()||!uniqueSources.insert(entry.record.objectId).second){
            provider.release();return BackendSubmissionResult::rejected("invalid or duplicated render source identity");
        }
    const auto validation=internal::validateReferencePlan(plan);
    if(validation.code!=BackendSubmissionCode::kAccepted){provider.release();return validation;}
    try{
        if(!resourceBudget_)resourceBudget_=std::make_shared<resources::ResourceBudgetManager>(
            resources::BudgetLimits{renderCacheOptions_.maxBytes,renderCacheOptions_.sharedMaxBytes});
        if(!gpuRetirements_)gpuRetirements_=std::make_shared<internal::GpuResourceRetirement>();
    }catch(const std::bad_alloc&){provider.release();return BackendSubmissionResult::retry("derived completion metadata unavailable");}
    auto* gpuContext=GrAsDirectContext(acquired.frame.surface->recordingContext());
    internal::SkiaGpuLifetime gpu(*gpuRetirements_,resourceBudget_,provider,gpuContext,
        internal::GpuResourceRetirement::canonicalAllocations,internal::GpuResourceRetirement::canonicalReads);
    internal::RenderExecutionScope execution(internal::ExecutionPurpose::kCanonical,plan,
        *acquired.frame.surface,&provider,resourceEpoch_,renderCacheOptions_.gpuCoverageCandidate,
        renderCacheOptions_.sharedMaxBytes,executionWork_,provider.describe().kind);
    if(execution.rejected()){provider.release();return BackendSubmissionResult::rejected("provider target kind does not match actual surface");}
    if(gpuContext&&!gpu.ready()){
        const bool permanent=!provider.gpuCompletionService();
        internal::ExecutionDecision unavailable;
        execution.unavailable(unavailable,permanent?internal::ExecutionReason::kMissingCompletion:internal::ExecutionReason::kAllocationDenied,permanent);
        provider.release();return permanent?BackendSubmissionResult::rejected("GPU completion service unavailable"):BackendSubmissionResult::retry("GPU completion metadata pending");
    }
    const bool cachedGpuSafe=!gpuContext||gpu.ready();
    if(gpu.ready())gpu.touch();
    internal::SkiaRenderCache* cache=nullptr;
    if(!cachedGpuSafe)clearRenderCache();
    if(cachedGpuSafe)try {
        if(!resourceBudget_)resourceBudget_=std::make_shared<resources::ResourceBudgetManager>(
            resources::BudgetLimits{renderCacheOptions_.maxBytes,renderCacheOptions_.sharedMaxBytes});
        if (!renderCache_) renderCache_=std::make_shared<internal::SkiaRenderCache>();
        if(!sourceRetention_)sourceRetention_=std::make_shared<internal::SourceRetentionPool>();
        if(!preparedGeometry_)preparedGeometry_=std::make_shared<internal::PreparedGeometryCache>();
        preparedGeometry_->setRetentionPool(sourceRetention_);
        renderCache_->sourcePool=sourceRetention_;renderCache_->preparedGeometry=preparedGeometry_;
        renderCache_->budget=resourceBudget_;
        renderCache_->options=renderCacheOptions_;
        AXIOM_FRAME_TIMING_SCOPE(beginTiming,cache_begin);
        renderCache_->begin(plan,&provider,*acquired.frame.surface,resourceEpoch_,
            selectionOverlay?selectionOverlay->selectedObject():foundation::ObjectId{});
        cache=renderCache_.get();execution.bindTargets(cache->executionTargets());
    } catch(const std::bad_alloc&) {clearRenderCache();}
    if(!cache&&renderCacheOptions_.enabled&&renderCacheOptions_.gpuCoverageCandidate&&gpuContext&&
       std::any_of(plan.referenceDrawList.entries.begin(),plan.referenceDrawList.entries.end(),[](const auto& e){const auto* c=std::get_if<CompactStrokeReferenceCommand>(&e.command);return c&&c->content.get().stroke.config.algorithmVersion==4;})){
        provider.release();return BackendSubmissionResult::retry("canonical prepared owner unavailable");
    }
    AXIOM_PREPARED_TRACE(preparedTrace,preparedGeometry_,"canonical");
    internal::PreparationAllowance allowance(preparation?*preparation:RenderPreparationBudget{});
    if(cache){
        const auto bodies=cache->prepareBodies(*acquired.frame.surface->getCanvas(),plan,allowance);
        if(bodies!=internal::SkiaRenderCache::Preparation::kReady){
            provider.release();return bodies==internal::SkiaRenderCache::Preparation::kRejected?BackendSubmissionResult::rejected("canonical body cannot fit preparation capacity"):BackendSubmissionResult::retry("canonical body preparation pending");
        }
    }
    if(cache&&cache->stagedBodies()){
        const auto staged=cache->prepareWorkingSet(*acquired.frame.surface->getCanvas(),plan,allowance,images_,&textResources_,false);
        if(staged!=internal::SkiaRenderCache::Preparation::kReady){
            provider.release();return staged==internal::SkiaRenderCache::Preparation::kRejected?BackendSubmissionResult::rejected("canonical working set cannot fit staging capacity"):BackendSubmissionResult::retry("canonical working set pending");
        }
    }
    if(preparation && cache&&!cache->stagedBodies()){
        auto* target=acquired.frame.surface->getCanvas();target->save();
        target->scale(metrics.physicalWidth/metrics.logicalWidth,metrics.physicalHeight/metrics.logicalHeight);
        const auto& clip=plan.referenceDrawList.viewportClip;
        target->clipRect(SkRect::MakeLTRB(clip.left,clip.top,clip.right,clip.bottom));
        auto prepared=internal::SkiaRenderCache::Preparation::kFallback;
        const auto beforePreparation=cache->work.rasterizedGroups;
        try{
            // Reclaim only after an actual optional-record admission failure.
            // The pending job owns a weak navigation reference, not this renderer.
            const auto navigation=std::weak_ptr<internal::SkiaNavigationTiles>(navigationTiles_);
            cache->reclaimRecorded=[navigation,budget=resourceBudget_,limit=renderCacheOptions_.maxBytes,frame=plan.frame,providerIdentity=&provider,resource=resourceEpoch_](std::size_t bytes){
                if(const auto tiles=navigation.lock())tiles->reclaimForExact(budget,limit,bytes,frame,providerIdentity,resource);
            };
            cache->reclaimPreparation=[this,navigation,frame=plan.frame,providerIdentity=&provider](std::size_t bytes){
                // Already-released scratch/frame bytes can cover this minimum.
                // Wait for their real completion before sacrificing feedback.
                if(internal::releasedQuotaSatisfies(*resourceBudget_,renderCacheOptions_.sharedMaxBytes,bytes))return;
                navigationFallback_.reset();
                if(internal::releasedQuotaSatisfies(*resourceBudget_,renderCacheOptions_.sharedMaxBytes,bytes))return;
                navigationCache_.reset();
                // Retiring the optional whole frame may itself be sufficient.
                // Keep useful navigation cells while that GPU read finishes.
                if(internal::releasedQuotaSatisfies(*resourceBudget_,renderCacheOptions_.sharedMaxBytes,bytes))return;
                if(const auto tiles=navigation.lock())tiles->reclaimForExact(resourceBudget_,
                    renderCacheOptions_.sharedMaxBytes,bytes,frame,providerIdentity,resourceEpoch_,true);
            };
            AXIOM_FRAME_TIMING_SCOPE(prepareTiming,cache_prepare);prepared=cache->prepare(*target,plan,*preparation,images_,&textResources_,&allowance);
        }
        catch(const std::bad_alloc&){cache->clear();prepared=internal::SkiaRenderCache::Preparation::kResourcePending;}
        target->restore();
        rasterizations_+=cache->work.rasterizedGroups-beforePreparation;
        if(prepared==internal::SkiaRenderCache::Preparation::kRejected){
            provider.release();return BackendSubmissionResult::rejected("canonical work buffers exceed shared resource capacity");
        }
        if(prepared==internal::SkiaRenderCache::Preparation::kPending ||
           prepared==internal::SkiaRenderCache::Preparation::kResourcePending){
            provider.release();return execution.rejected()?BackendSubmissionResult::rejected("execution target rejected"):BackendSubmissionResult::retry("exact cache preparation pending");
        }
    }
    if(!renderCache_){cache=nullptr;execution.unbindTargets();}
    const auto beforeRaster=cache?cache->work.rasterizedGroups:0;
    AXIOM_FRAME_TIMING_SCOPE(sceneTiming,scene_draw);
    const auto result = (cache && (preparation||cache->stagedBodies()) && cache->compositePreparedFrame(*acquired.frame.surface->getCanvas()))
        ? BackendSubmissionResult::accepted() : internal::drawReferencePlanToSkCanvas(
        *acquired.frame.surface->getCanvas(), plan, images_, &textResources_,true,cache);
    AXIOM_FRAME_TIMING_END(sceneTiming);
    if(execution.pending()||execution.rejected()){
        provider.release();return execution.rejected()?BackendSubmissionResult::rejected("execution target rejected"):BackendSubmissionResult::retry("coverage target pending");
    }
    if(result.code==BackendSubmissionCode::kAccepted){
        AXIOM_FRAME_TIMING_SCOPE(snapshotTiming,navigation_snapshot);
        try {
            const auto bytes=std::uint64_t(metrics.physicalWidth)*metrics.physicalHeight*4;
            if(cachedGpuSafe && resourceBudget_ && renderCacheOptions_.enabled && (!selectionOverlay || (!selectionOverlay->creationPreview() && !selectionOverlay->textCaret)) &&
               plan.background.mode==foundation::BackgroundMode::kPlain && !plan.transparentBackground){
                const auto previous=navigationCache_;
                navigationCache_.reset();
                const auto shared=(preparation&&cache)?cache->completedFrame():nullptr;
                const auto reservation=!shared&&bytes<=renderCacheOptions_.maxBytes
                    ?resourceBudget_->reserve(resources::ResourceClass::kNavigation,bytes,true):std::optional<std::uint64_t>{};
                // This retained navigation snapshot is optional. Denial must
                // not throw out the already drawn canonical frame/receipt.
                // In particular,WASM consumers need an explicit branch here.
                if(shared||reservation){
                    resources::ResourceLease lease(resourceBudget_,reservation.value_or(0));
                    auto candidate=std::make_shared<NavigationCache>();candidate->lease=std::move(lease);candidate->prepared=shared;
                    candidate->provider=&provider;candidate->image=shared?shared->image:acquired.frame.surface->makeImageSnapshot();
                    candidate->frame=std::make_unique<FrameState>(plan.frame);
                    candidate->transform=plan.referenceDrawList.worldToView;
                    const auto& color=plan.background.color;
                    candidate->background=SkColor4f{color[0],color[1],color[2],color[3]}.toSkColor();
                    candidate->valid.reset(candidate->worldBounds());
                    candidate->proofSemantic=plan.frame.sceneGeneration.value();candidate->proofScene=plan.frame.sceneReadToken.value();
                    candidate->resource=resourceEpoch_;candidate->isolate=plan.isolateEraseMasks;
                    if(SkColorGetA(candidate->background)==255&&plan.referenceDrawList.viewportClip==foundation::WorldRect{0,0,metrics.logicalWidth,metrics.logicalHeight})
                        candidate->sources=internal::NavigationSourceSnapshot::capture(plan.referenceDrawList.entries,renderCacheOptions_,resourceBudget_,sourceRetention_);
                    // Keep one previous qualified plane for a resumed gesture.
                    // A narrow sharp zoom-in must not erase all wider coverage.
                    // These are existing admitted images, never another copy or pool.
                    std::shared_ptr<NavigationCache> fallback;
                    const auto consider=[&](const std::shared_ptr<NavigationCache>& old){
                        if(old&&old->valid.count&&old->sameTarget(&provider,plan.frame)&&old->resource==resourceEpoch_&&old->background==candidate->background&&old->isolate==candidate->isolate&&
                           (old->matches(&provider,plan.frame)||old->sources)&&old->transform!=candidate->transform&&
                           old->worldBounds().intersects(candidate->worldBounds())&&
                           (!fallback||old->worldArea()>fallback->worldArea()))fallback=old;
                    };
                    consider(previous);consider(navigationFallback_);
                    navigationFallback_=std::move(fallback);
                    navigationCache_=std::move(candidate);
                }
            }else{navigationCache_.reset();navigationFallback_.reset();}
        }catch(const std::bad_alloc&){navigationCache_.reset();navigationFallback_.reset();}
    }
    if (result.code == BackendSubmissionCode::kAccepted && selectionOverlay != nullptr) {
        G5_TARGET(acquired.frame.surface,true);
    auto* canvas = acquired.frame.surface->getCanvas();
        if (selectionOverlay->creationPreview()) {
            internal::drawTransientObjectToSkCanvas(*canvas, plan, *selectionOverlay->creationPreview(),selectionOverlay->creationRoute());
            internal::drawInkEntriesToSkCanvas(*canvas, plan, images_, &textResources_,cache);
        }
        if (const auto& hover=selectionOverlay->creationHover()) {
            SkPaint ghostPaint;ghostPaint.setAlphaf(0.25F);
            canvas->saveLayer(nullptr,&ghostPaint);
            if(hover->sibling) internal::drawTransientObjectToSkCanvas(*canvas,plan,*hover->sibling);
            internal::drawTransientObjectToSkCanvas(*canvas,plan,hover->primary,hover->route?&*hover->route:nullptr);
            canvas->restore();
        }
        internal::drawEditingOverlayToSkCanvas(*canvas, plan.frame.metrics, *selectionOverlay);
    }
    provider.release();
    if (result.code != BackendSubmissionCode::kAccepted) {clearRenderCache();return result;}
    ++submissions_;
    rasterizations_+=cache?cache->work.rasterizedGroups-beforeRaster:1;
    AXIOM_FRAME_TIMING_SCOPE(presentTiming,provider_present);
    G5_SCOPE(g5Present,g5::Tag::provider,g5::Purpose::none);
    const auto presented=provider.present();
    G5_SCOPE_END(g5Present);
    G5_OBSERVE(g5::Kind::Submit,g5::Tag::provider,true,plan.frame.viewId.value(),plan.frame.frameId.value(),plan.frame.sceneGeneration.value(),plan.frame.sceneReadToken.value(),plan.frame.surfaceGeneration.value(),plan.frame.metricsGeneration.value(),static_cast<unsigned>(presented.code));
    AXIOM_FRAME_TIMING_END(presentTiming);
    if(presented.code!=BackendSubmissionCode::kAccepted)clearRenderCache();
    return presented;
}

BackendSubmissionResult SkiaRenderer::renderNavigation(SkiaSurfaceProvider& provider,
    const CanonicalViewportTransform& camera,const FrameState& current,
    const EditingOverlay* selectionOverlay){
    G5_ROUTE(g5NavRoute,g5::Route::nav_unprepared);
    G5_OBSERVE(g5::Kind::Basis,g5::Tag::navigation_basis,true,current.viewId.value(),current.sceneGeneration.value(),current.sceneReadToken.value(),current.surfaceGeneration.value(),current.metricsGeneration.value(),resourceEpoch_,g5::bits(camera.scale),g5::bits(camera.translationX),g5::bits(camera.translationY));

    serviceResourceRetirements();
    const auto cache=navigationCache_;
    if(!cache || !cache->image || cache->provider!=&provider || !cache->frame ||
       cache->frame->viewId!=current.viewId || cache->frame->sceneGeneration!=current.sceneGeneration ||
       cache->frame->sceneReadToken!=current.sceneReadToken || cache->frame->metrics!=current.metrics ||
       cache->frame->metricsGeneration!=current.metricsGeneration ||
       cache->frame->surfaceGeneration!=current.surfaceGeneration ||
       !std::isfinite(camera.scale)||camera.scale<=0 ||
       !std::isfinite(camera.translationX)||!std::isfinite(camera.translationY))
        return BackendSubmissionResult::retry("navigation source unavailable");
    const auto acquired=provider.acquire();
    if(acquired.code!=SkiaSurfaceAcquireCode::kAcquired || !acquired.frame.surface)
        return BackendSubmissionResult::retry("navigation target unavailable");
    if(acquired.frame.generation!=current.surfaceGeneration.value()){
        provider.release();return BackendSubmissionResult::rejected("stale navigation surface");
    }
    if(!gpuRetirements_)gpuRetirements_=std::make_shared<internal::GpuResourceRetirement>();
    internal::SkiaGpuLifetime gpu(*gpuRetirements_,resourceBudget_,provider,
        GrAsDirectContext(acquired.frame.surface->recordingContext()),resources::kPixelResourceClasses,
        resources::kPixelResourceClasses&~resources::resourceClassMask(resources::ResourceClass::kCanonicalFrame)&
        ~resources::resourceClassMask(resources::ResourceClass::kNavigation));
    if(!gpu.ready()){provider.release();return BackendSubmissionResult::retry("navigation completion unavailable");}
    gpu.touch();
    const auto& old=cache->transform;
    if(old.a<=0 || old.d!=old.a || old.b!=0 || old.c!=0){
        provider.release();return BackendSubmissionResult::retry("navigation transform ineligible");
    }
    const float ratio=static_cast<float>(camera.scale/old.a);
    const float sx=current.metrics.physicalWidth/current.metrics.logicalWidth;
    const float sy=current.metrics.physicalHeight/current.metrics.logicalHeight;
    G5_TARGET(acquired.frame.surface,true);
    auto* canvas=acquired.frame.surface->getCanvas();
    canvas->save();canvas->resetMatrix();canvas->clear(cache->background);
    canvas->translate(static_cast<SkScalar>((camera.translationX-ratio*old.tx)*sx),static_cast<SkScalar>((camera.translationY-ratio*old.ty)*sy));
    canvas->scale(ratio,ratio);
    if(cache->prepared)cache->prepared->markGpuUse();
    (void)cache->lease.markGpuUse();
    G5_ROUTE(g5Retained,g5::Route::nav_retained);
    G5_TARGET(acquired.frame.surface,true);
    canvas->drawImage(cache->image,0,0,SkSamplingOptions(SkFilterMode::kLinear));
    canvas->restore();
    if(selectionOverlay){
#if defined(AXIOM_ENABLE_FRAME_TIMING)
        const auto started=std::chrono::steady_clock::now();
#endif
        internal::drawEditingOverlayToSkCanvas(*canvas,current.metrics,*selectionOverlay);
#if defined(AXIOM_ENABLE_FRAME_TIMING)
        if(navigationTiles_)navigationTiles_->work.overlayNanoseconds+=
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count();
#endif
    }
    provider.release();
    return provider.present();
}
BackendSubmissionResult SkiaRenderer::renderNavigation(SkiaSurfaceProvider& provider,
    const CanonicalViewportTransform& camera,const FrameState& current,const NavigationSourceQuery& query,bool allowPrewarm,
    const EditingOverlay* selectionOverlay,const NavigationBackgroundQuery& backgroundOnly){
    G5_ROUTE(g5NavRoute,g5::Route::nav_unprepared);
    G5_OBSERVE(g5::Kind::Basis,g5::Tag::navigation_basis,true,current.viewId.value(),current.sceneGeneration.value(),current.sceneReadToken.value(),current.surfaceGeneration.value(),current.metricsGeneration.value(),resourceEpoch_,g5::bits(camera.scale),g5::bits(camera.translationX),g5::bits(camera.translationY));

    serviceResourceRetirements();
    if(!resourceBudget_)resourceBudget_=std::make_shared<resources::ResourceBudgetManager>(
        resources::BudgetLimits{renderCacheOptions_.maxBytes,renderCacheOptions_.sharedMaxBytes});
    if(!sourceRetention_)sourceRetention_=std::make_shared<internal::SourceRetentionPool>();
        if(!preparedGeometry_)preparedGeometry_=std::make_shared<internal::PreparedGeometryCache>();
        preparedGeometry_->setRetentionPool(sourceRetention_);
    if(!navigationTiles_)navigationTiles_=std::make_shared<internal::SkiaNavigationTiles>();
    // Keep same-demand exact work;discard only obsolete offscreen preparation.
    // Its leases still wait for actual GPU completion before becoming reusable.
    if(renderCache_)renderCache_->releaseIdleWorkBuffers(current,
        {camera.scale,0,0,camera.scale,camera.translationX,camera.translationY},&provider,resourceEpoch_);
    auto options=renderCacheOptions_;
    internal::RetainedNavigationSource retained,fallback;
    const auto proofStart=std::chrono::steady_clock::now();
    const auto revalidate=[&](const std::shared_ptr<NavigationCache>& cache){
        if(!cache||!cache->sameTarget(&provider,current)||cache->resource!=resourceEpoch_)return;
        if(cache->matches(&provider,current))return;
        // A completed current plane supplies the current background/isolation
        // policy. Without it, a source-only comparison cannot certify pixels.
        const auto basis=navigationCache_;
        if(!basis||!basis->matches(&provider,current)||basis->background!=cache->background||basis->isolate!=cache->isolate)return;
        if(!cache->sources||cache->proofSemantic>current.sceneGeneration.value()||cache->proofScene>current.sceneReadToken.value()||
           !query||!options.navigationMaxQueries||
           std::chrono::steady_clock::now()-proofStart>=std::chrono::microseconds(renderCacheOptions_.navigationMicroseconds))return;
        --options.navigationMaxQueries;++navigationTiles_->work.queries;++navigationTiles_->work.regionalPlaneQueries;
        try{
        std::vector<ReferenceTraversalEntry> entries;NavigationSourceDiagnostics diagnostics;
        const auto start=std::chrono::steady_clock::now();
        const bool queried=query(cache->worldBounds(),entries,diagnostics);
        navigationTiles_->work.queryNanoseconds+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
        const auto reject=[&]{++navigationTiles_->work.regionalPlaneRejects;};
        if(!queried||!diagnostics.sceneStamped||diagnostics.semanticGeneration!=current.sceneGeneration.value()||diagnostics.sceneRevision!=current.sceneReadToken.value()){reject();return;}
        ReferenceDrawList validation{.frame=current,.entries={},.canonicalBytes={},.digest={}};validation.viewportClip=current.viewportClip.isFiniteAndOrdered()?current.viewportClip:foundation::WorldRect{0,0,current.metrics.logicalWidth,current.metrics.logicalHeight};
        validation.entries=entries;
        if(internal::validateReferencePlan(FramePlan{current,std::move(validation)}).code!=BackendSubmissionCode::kAccepted){reject();return;}
        const auto difference=internal::maskPixelDifference(cache->sources->entries,entries);
        if(!difference.compatible){++navigationTiles_->work.regionalPlaneBodyRejects;reject();return;}
        auto regions=cache->valid;
        if(difference.damage){
            const auto density=cache->transform.a*std::min(current.metrics.physicalWidth/current.metrics.logicalWidth,current.metrics.physicalHeight/current.metrics.logicalHeight);
            const auto damage=internal::maskSamplingDamage(*difference.damage,density,cache->worldBounds());
            if(!damage||!regions.subtract(*damage)){reject();return;}
        }
        auto sources=internal::NavigationSourceSnapshot::capture(entries,renderCacheOptions_,resourceBudget_,sourceRetention_);
        if(!sources){++navigationTiles_->work.regionalPlaneAdmissionRejects;reject();return;}
        cache->sources=std::move(sources);cache->valid=regions;
        // Captured FrameState stays immutable. Only this derived region proof
        // advances; it never enters canonical presentation/receipt tracking.
        cache->proofSemantic=current.sceneGeneration.value();cache->proofScene=current.sceneReadToken.value();
        cache->backgroundProofRejected=false;++navigationTiles_->work.regionalPlaneProofs;
        }catch(const std::bad_alloc&){++navigationTiles_->work.regionalPlaneRejects;}
    };
    revalidate(navigationCache_);revalidate(navigationFallback_);
    const auto proofUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-proofStart).count();
    options.navigationMicroseconds=proofUs>=std::int64_t(options.navigationMicroseconds)?0:options.navigationMicroseconds-proofUs;
    navigationTiles_->work.retainedValidRegions=0;
    const auto bindRetained=[&](const std::shared_ptr<NavigationCache>& cache,internal::RetainedNavigationSource& retained){
      if(cache&&cache->matches(&provider,current)){
        const auto& old=cache->transform;
        retained.bounds={float(-old.tx/old.a),float(-old.ty/old.a),
            float((cache->frame->metrics.logicalWidth-old.tx)/old.a),float((cache->frame->metrics.logicalHeight-old.ty)/old.a)};
        retained.regions={cache->valid.areas.data(),cache->valid.count};
        navigationTiles_->work.retainedValidRegions+=cache->valid.count;
        retained.density=old.a*std::min(current.metrics.physicalWidth/current.metrics.logicalWidth,
            current.metrics.physicalHeight/current.metrics.logicalHeight);
        if(cache->valid.full(cache->worldBounds()))retained.extend=[&,cache]{
            const auto& old=cache->transform;
        const double ratio=camera.scale/old.a;
        const double x=camera.translationX-ratio*old.tx,y=camera.translationY-ratio*old.ty;
        if(!std::isfinite(ratio)||ratio<=0||
           !std::isfinite(x)||!std::isfinite(y))return false;
        const double width=current.metrics.logicalWidth,height=current.metrics.logicalHeight;
        const double right=x+cache->frame->metrics.logicalWidth*ratio,bottom=y+cache->frame->metrics.logicalHeight*ratio;
        if(x>0||y>0||right<width||bottom<height){
            if(!backgroundOnly||cache->backgroundProofRejected)return false;
            // Exact retained pixels plus Scene-proven plain background cover
            // the entire demand. Unknown exposure never certifies feedback.
            const auto left=std::clamp(x,0.,width),top=std::clamp(y,0.,height);
            const auto r=std::clamp(right,0.,width),b=std::clamp(bottom,0.,height);
            std::vector<foundation::WorldRect> exposed;
            const auto add=[&](double l,double t,double r,double b){if(r>l&&b>t)exposed.push_back({float(l),float(t),float(r),float(b)});};
            if(r<=left||b<=top)add(0,0,width,height);
            else{add(0,0,left,height);add(r,0,width,height);add(left,0,r,top);add(left,b,r,height);}
            if(exposed.size()>options.navigationMaxQueries)return false;
            const double pad=2*width/current.metrics.physicalWidth;
            for(const auto& e:exposed){
                const foundation::WorldRect world{float((e.left-pad-camera.translationX)/camera.scale),
                    float((e.top-pad-camera.translationY)/camera.scale),float((e.right+pad-camera.translationX)/camera.scale),
                    float((e.bottom+pad-camera.translationY)/camera.scale)};
                --options.navigationMaxQueries;++navigationTiles_->work.queries;
                const auto started=std::chrono::steady_clock::now();const bool empty=backgroundOnly(world);
                navigationTiles_->work.queryNanoseconds+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count();
                if(!empty){cache->backgroundProofRejected=true;return false;}
            }
        }
            return true;
        };
        retained.draw=[cache,camera,current](SkCanvas& canvas,const foundation::WorldRect& area){
            if(cache->prepared)cache->prepared->markGpuUse();
            (void)cache->lease.markGpuUse();
            const auto& old=cache->transform;const float ratio=float(camera.scale/old.a);
            const float sx=current.metrics.physicalWidth/current.metrics.logicalWidth;
            const float sy=current.metrics.physicalHeight/current.metrics.logicalHeight;
            canvas.save();canvas.resetMatrix();
            canvas.clipRect(SkRect::MakeLTRB(float((area.left*camera.scale+camera.translationX)*sx),
                float((area.top*camera.scale+camera.translationY)*sy),float((area.right*camera.scale+camera.translationX)*sx),
                float((area.bottom*camera.scale+camera.translationY)*sy)));
            canvas.drawColor(cache->background,SkBlendMode::kSrc);
            canvas.translate(float((camera.translationX-ratio*old.tx)*sx),float((camera.translationY-ratio*old.ty)*sy));
            canvas.scale(ratio,ratio);canvas.drawImage(cache->image,0,0,SkSamplingOptions(SkFilterMode::kLinear));canvas.restore();
        };
      }
    };
    bindRetained(navigationCache_,retained);bindRetained(navigationFallback_,fallback);
    if(!retained.draw)retained=std::move(fallback);
    else if(fallback.draw)retained.fallback=&fallback;
    const auto drawOverlay=[&](SkCanvas& canvas){
        if(selectionOverlay){
#if defined(AXIOM_ENABLE_FRAME_TIMING)
            const auto started=std::chrono::steady_clock::now();
#endif
            internal::drawEditingOverlayToSkCanvas(canvas,current.metrics,*selectionOverlay);
#if defined(AXIOM_ENABLE_FRAME_TIMING)
            navigationTiles_->work.overlayNanoseconds+=
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count();
#endif
        }
    };
    try{
        if(!gpuRetirements_)gpuRetirements_=std::make_shared<internal::GpuResourceRetirement>();
        AXIOM_PREPARED_TRACE(preparedTrace,preparedGeometry_,"navigation");
        const auto result=navigationTiles_->render(provider,camera,current,query,resourceEpoch_,options,resourceBudget_,allowPrewarm,retained.draw?&retained:nullptr,&textResources_,images_,drawOverlay,sourceRetention_,gpuRetirements_,preparedGeometry_);
        if(result.code!=BackendSubmissionCode::kAccepted)navigationTiles_.reset();
        return result;
    }catch(const std::bad_alloc&){navigationTiles_.reset();return BackendSubmissionResult::retry("world navigation allocation failure");}
}
void SkiaRenderer::cancelNavigationWork() noexcept {if(navigationTiles_)navigationTiles_->cancelWork();}
NavigationWork SkiaRenderer::navigationWork() const noexcept {
    return navigationTiles_?navigationTiles_->work:NavigationWork{};
}
void SkiaRenderer::setRenderCacheOptions(RenderCacheOptions options) {
    clearRenderCache();if(resourceBudget_)resourceBudget_->close();gpuRetirements_.reset();resourceBudget_.reset();sourceRetention_.reset();preparedGeometry_.reset();renderCacheOptions_=options;
}
void SkiaRenderer::clearRenderCache() noexcept {eraseQueries_.reset();partialEraseFrame_.reset();navigationTiles_.reset();navigationCache_.reset();navigationFallback_.reset();if(renderCache_)renderCache_->clear();if(preparedGeometry_)preparedGeometry_->clear();}
SelectionGeometryHit SkiaRenderer::intersectEraseGeometry(const RuntimeSceneRecord& record,const semantic::EraseMaskGeometry& geometry)const noexcept{
    if(!resourceBudget_)return canvas::render::intersectEraseGeometry(record,geometry);
    if(!eraseQueries_)eraseQueries_=std::make_shared<internal::EraseMaskPathCache>();
    const auto capacity=std::min(renderCacheOptions_.maxEntryBytes,renderCacheOptions_.maxBytes/4);
    return eraseQueries_->query(record,geometry,resourceBudget_,capacity);
}
RenderCacheWork SkiaRenderer::renderCacheWork()const noexcept{auto result=renderCache_?renderCache_->work:RenderCacheWork{};internal::copyExecutionCounters(result,executionWork_);
    if(preparedGeometry_){const auto s=preparedGeometry_->statistics();result.preparedBodyBuilds=s.builds;result.preparedBodyHits=s.hits;
        result.preparedIdentityHits=s.identityHits;result.preparedHashBytes=s.fingerprintBytes;result.preparedContentComparisons=s.contentComparisons;
        result.preparedIndexQueries=s.indexLookups;result.preparedNavigationQueries=s.navigationQueries;result.preparedInkQueries=s.retainedInkQueries;
        result.preparedCanonicalQueries=s.canonicalQueries;result.preparedDirectQueries=s.directQueries;
        result.preparedJobAdmissions=s.jobAdmissions;result.preparedAdvances=s.advances;result.preparedSupportKnots=s.supportKnots;
        result.preparedDistanceInputs=s.distanceInputs;result.preparedCompileInputs=s.compileInputs;result.preparedPathPrimitives=s.pathPrimitives;result.preparedMaxAdvanceKnots=s.maxAdvanceKnots;
        result.preparedAliases=s.aliases;result.preparedIndexReservationBytes=s.indexReservationBytes;result.preparedSourceFootprintEvaluations=s.sourceFootprintEvaluations;}
    return result;}
RenderDamage SkiaRenderer::renderDamage()const{return renderCache_?renderCache_->damage:RenderDamage{};}

BackendSubmissionResult SkiaRenderer::renderTransientOverlay(
    SkiaSurfaceProvider& provider, const FramePlan& plan,
    const EditingOverlay* selectionOverlay) {
    G5_SCOPE(g5Render,g5::Tag::material,g5::Purpose::none);
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
    ObservationCounters observationCounters{*this};
#endif
    G5_ROUTE(g5Route,g5::Route::edit_plain);
    G5_OBSERVE(g5::Kind::Basis,g5::Tag::transient,true,plan.frame.viewId.value(),plan.frame.frameId.value(),plan.frame.sceneGeneration.value(),plan.frame.sceneReadToken.value(),plan.frame.camera.generation.value(),plan.frame.surfaceGeneration.value(),plan.frame.metricsGeneration.value(),resourceEpoch_);

    auto previousErase=std::move(partialEraseFrame_);
    serviceResourceRetirements();
    if (selectionOverlay == nullptr) {
        return clearPreview(provider, SurfaceGeneration{provider.generation()});
    }
    const auto acquired = provider.acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired || acquired.frame.surface == nullptr) {
        return BackendSubmissionResult::rejected(acquired.message);
    }
    G5_TARGET(acquired.frame.surface,true);
    auto* canvas = acquired.frame.surface->getCanvas();
    if(acquired.frame.generation!=plan.frame.surfaceGeneration.value()||
       acquired.frame.surface->width()!=int(plan.frame.metrics.physicalWidth)||
       acquired.frame.surface->height()!=int(plan.frame.metrics.physicalHeight)){
        provider.release();return BackendSubmissionResult::rejected("stale transient target");
    }
    const auto validation=internal::validateReferencePlan(plan);
    if(validation.code!=BackendSubmissionCode::kAccepted){provider.release();return validation;}
    internal::RenderExecutionScope execution(internal::ExecutionPurpose::kFullTransientEdit,plan,
        *acquired.frame.surface,&provider,resourceEpoch_,renderCacheOptions_.gpuCoverageCandidate,
        renderCacheOptions_.sharedMaxBytes,executionWork_,provider.describe().kind);
    if(execution.rejected()){provider.release();return BackendSubmissionResult::rejected("provider target kind does not match actual surface");}
    const bool gpuLiveV4=renderCacheOptions_.gpuCoverageCandidate&&canvas->recordingContext()&&std::any_of(plan.referenceDrawList.entries.begin(),plan.referenceDrawList.entries.end(),[](const auto& e){const auto* c=std::get_if<CompactStrokeReferenceCommand>(&e.command);return e.presentationEraseActive&&c&&c->content.get().stroke.config.algorithmVersion==4;});
    if(gpuLiveV4){
        // Retire obsolete masked material before starting a GPU read epoch.
        // Keep navigation planes/tiles and their regional validity witnesses:
        // live feedback does not invalidate pixels outside the changed mask.
        // Mandatory preparation already reclaims navigation on actual denial;
        // optional body layers must yield rather than destroy that coverage.
        if(renderCache_&&!renderCache_->stagedBodies())renderCache_->releaseMaterialSources(!previousErase);
    }
    try{
        if(!resourceBudget_)resourceBudget_=std::make_shared<resources::ResourceBudgetManager>(
            resources::BudgetLimits{renderCacheOptions_.maxBytes,renderCacheOptions_.sharedMaxBytes});
        if(!gpuRetirements_)gpuRetirements_=std::make_shared<internal::GpuResourceRetirement>();
    }catch(const std::bad_alloc&){provider.release();return BackendSubmissionResult::retry("derived completion metadata unavailable");}
    auto* gpuContext=GrAsDirectContext(acquired.frame.surface->recordingContext());
    internal::SkiaGpuLifetime gpu(*gpuRetirements_,resourceBudget_,provider,gpuContext,
        internal::GpuResourceRetirement::canonicalAllocations,internal::GpuResourceRetirement::canonicalReads);
    if(gpuContext&&!gpu.ready()){
        const bool permanent=!provider.gpuCompletionService();
        internal::ExecutionDecision unavailable;
        execution.unavailable(unavailable,permanent?internal::ExecutionReason::kMissingCompletion:internal::ExecutionReason::kAllocationDenied,permanent);
        provider.release();return permanent?BackendSubmissionResult::rejected("GPU completion service unavailable"):BackendSubmissionResult::retry("GPU completion metadata pending");
    }
    const bool cachedGpuSafe=!gpuContext||gpu.ready();
    if(gpu.ready())gpu.touch();
    internal::SkiaRenderCache* cache=nullptr;
    if(!cachedGpuSafe)clearRenderCache();
    if(cachedGpuSafe)try{
        if(!resourceBudget_)resourceBudget_=std::make_shared<resources::ResourceBudgetManager>(
            resources::BudgetLimits{renderCacheOptions_.maxBytes,renderCacheOptions_.sharedMaxBytes});
        if(!renderCache_)renderCache_=std::make_shared<internal::SkiaRenderCache>();
        if(!sourceRetention_)sourceRetention_=std::make_shared<internal::SourceRetentionPool>();
        if(!preparedGeometry_)preparedGeometry_=std::make_shared<internal::PreparedGeometryCache>();
        preparedGeometry_->setRetentionPool(sourceRetention_);
        renderCache_->sourcePool=sourceRetention_;renderCache_->preparedGeometry=preparedGeometry_;
        renderCache_->budget=resourceBudget_;renderCache_->options=renderCacheOptions_;
        renderCache_->begin(plan,&provider,*acquired.frame.surface,resourceEpoch_,
            selectionOverlay?selectionOverlay->selectedObject():foundation::ObjectId{});cache=renderCache_.get();execution.bindTargets(cache->executionTargets());
        if(gpuLiveV4){
            const auto navigation=std::weak_ptr<internal::SkiaNavigationTiles>(navigationTiles_);
            cache->reclaimPreparation=[this,navigation,frame=plan.frame,providerIdentity=&provider](std::size_t bytes){
                if(internal::releasedQuotaSatisfies(*resourceBudget_,renderCacheOptions_.sharedMaxBytes,bytes))return;
                // Live feedback reclaims only after a real admission denial.
                // Prefer disposable tiles to qualified wide-view planes.
                if(const auto tiles=navigation.lock())tiles->reclaimForExact(resourceBudget_,
                    renderCacheOptions_.sharedMaxBytes,bytes,frame,providerIdentity,resourceEpoch_,true);
                if(internal::releasedQuotaSatisfies(*resourceBudget_,renderCacheOptions_.sharedMaxBytes,bytes))return;
                navigationFallback_.reset();
                if(internal::releasedQuotaSatisfies(*resourceBudget_,renderCacheOptions_.sharedMaxBytes,bytes))return;
                navigationCache_.reset();
            };
        }
    }catch(const std::bad_alloc&){clearRenderCache();}
    if(!renderCache_){cache=nullptr;execution.unbindTargets();}
    if(!cache&&renderCacheOptions_.enabled&&renderCacheOptions_.gpuCoverageCandidate&&gpuContext&&
       std::any_of(plan.referenceDrawList.entries.begin(),plan.referenceDrawList.entries.end(),[](const auto& e){const auto* c=std::get_if<CompactStrokeReferenceCommand>(&e.command);return c&&c->content.get().stroke.config.algorithmVersion==4;})){
        provider.release();return BackendSubmissionResult::retry("transient prepared owner unavailable");
    }
    AXIOM_PREPARED_TRACE(preparedTrace,preparedGeometry_,"edit");
    internal::PreparationAllowance allowance;
    if(cache){
        const auto bodies=cache->prepareBodies(*canvas,plan,allowance);
        if(bodies!=internal::SkiaRenderCache::Preparation::kReady){
            provider.release();return bodies==internal::SkiaRenderCache::Preparation::kRejected?BackendSubmissionResult::rejected("transient body cannot fit preparation capacity"):BackendSubmissionResult::retry("transient body preparation pending");
        }
    }
    if(cache&&cache->stagedBodies()){
        const auto staged=cache->prepareWorkingSet(*canvas,plan,allowance,images_,&textResources_,true,selectionOverlay);
        if(staged!=internal::SkiaRenderCache::Preparation::kReady){
            provider.release();return staged==internal::SkiaRenderCache::Preparation::kRejected?BackendSubmissionResult::rejected("transient working set cannot fit staging capacity"):BackendSubmissionResult::retry("transient working set pending");
        }
    }
    // Selection/creation transient presentation is a complete replacement
    // frame for the overlay window.  Drawing only the moved object on a
    // transparent surface leaves the old canonical pixels visible underneath
    // it, which looks like a duplicated object during a drag.  The reference
    // plan already contains any transient transform overrides, so render the
    // whole scene first and then add the editing chrome.
    const auto includeInk=!selectionOverlay->creationPreview().has_value();
    const bool bareOverlay=!selectionOverlay->selectionOutline().visible&&!selectionOverlay->creationPreview()&&
        !selectionOverlay->creationHover()&&!selectionOverlay->textBox&&!selectionOverlay->textCaret&&
        selectionOverlay->textSelection.empty()&&selectionOverlay->readOnlyOutlines.empty()&&
        selectionOverlay->candidateOutlines.empty()&&selectionOverlay->acquisitionOutline.empty()&&
        selectionOverlay->guides().empty()&&selectionOverlay->attachmentPoints().empty();
    const bool compactInkOnly=std::none_of(plan.referenceDrawList.entries.begin(),plan.referenceDrawList.entries.end(),[](const auto& entry){
        return std::holds_alternative<VectorStrokeReferenceCommand>(entry.command)||std::holds_alternative<DabStrokeReferenceCommand>(entry.command)||
            std::holds_alternative<BrushStrokeReferenceCommand>(entry.command)||std::holds_alternative<DabBrushStrokeReferenceCommand>(entry.command);
    });
    const bool persistentErase=compactInkOnly&&cache&&!cache->stagedBodies()&&renderCacheOptions_.enabled&&bareOverlay&&cache->eraseDamage().active&&
        !renderCacheOptions_.tileCandidate&&canvas->getTotalMatrix().isIdentity()&&
        canvas->isClipRect()&&canvas->getDeviceClipBounds()==SkIRect::MakeWH(acquired.frame.surface->width(),acquired.frame.surface->height())&&
        provider.describe().capabilities.supportsPartialUpdate;
    bool incremental=persistentErase&&cache->eraseDamage().continuous&&previousErase&&
        previousErase->matches(provider,*acquired.frame.surface,plan,*cache,resourceEpoch_);
    if(incremental&&!cache->prepareEraseDamage(*canvas,gpuLiveV4)){
        if(gpuLiveV4){provider.release();return BackendSubmissionResult::retry("live erase work surface unavailable");}
        incremental=false;
    }
    // Preserve the full frame's coverage tile origin and dimensions. A narrow
    // clip changes how many tiles are needed, never their AA/noise sampling grid.
    SkIRect originalClip;
    {SkAutoCanvasRestore restore(canvas,true);
        const auto& m=plan.frame.metrics;
        canvas->scale(static_cast<float>(m.physicalWidth)/m.logicalWidth,
                      static_cast<float>(m.physicalHeight)/m.logicalHeight);
        const auto& c=plan.referenceDrawList.viewportClip;canvas->clipRect(SkRect::MakeLTRB(c.left,c.top,c.right,c.bottom));
        originalClip=canvas->getDeviceClipBounds();}
    const internal::CompactDeviceGrid deviceGrid{originalClip.left(),originalClip.top(),originalClip.right(),originalClip.bottom()};
    SkAutoCanvasRestore damageRestore(canvas,false);
    if(incremental) {
        canvas->save();canvas->resetMatrix();
        SkIRect clip=SkIRect::MakeEmpty();
        if(const auto& bounds=cache->eraseDamage().bounds) {
            const auto& v=plan.referenceDrawList.worldToView;const auto& m=plan.frame.metrics;
            const double sx=static_cast<float>(m.physicalWidth)/m.logicalWidth;
            const double sy=static_cast<float>(m.physicalHeight)/m.logicalHeight;
            SkMatrix matrix;matrix.setAll(static_cast<SkScalar>(v.a*sx),static_cast<SkScalar>(v.c*sx),
                static_cast<SkScalar>(v.tx*sx),static_cast<SkScalar>(v.b*sy),
                static_cast<SkScalar>(v.d*sy),static_cast<SkScalar>(v.ty*sy),0,0,1);
            auto area=matrix.mapRect(SkRect::MakeLTRB(bounds->left,bounds->top,bounds->right,bounds->bottom));
            area.outset(3,3); // Include stroke AA and inverse-affine rounding.
            if(!area.isFinite()){damageRestore.restore();incremental=false;}
            else if(area.intersect(SkRect::Make(canvas->getDeviceClipBounds())))area.roundOut(&clip);
        }
        if(incremental)canvas->clipIRect(clip);
    }
    if(cache&&cache->eraseDamage().active) {
        if(incremental){const auto clip=canvas->getDeviceClipBounds();++cache->work.eraseDamageFrames;
            cache->work.eraseDamagePixels+=std::uint64_t(clip.width())*clip.height();}
        else ++cache->work.eraseFullFrames;
    }
    BackendSubmissionResult scene=BackendSubmissionResult::accepted();
    const auto sceneSaveCount=canvas->getSaveCount();
    try {
        if(!incremental||!canvas->getDeviceClipBounds().isEmpty()) {
            if(cache&&cache->stagedBodies()){if(!cache->compositePreparedFrame(*canvas))scene=BackendSubmissionResult::retry("transient staged frame unavailable");}
            else if(cache){if(!cache->drawTransientScene(*canvas,plan,images_,&textResources_,includeInk,incremental,&deviceGrid))
                scene=BackendSubmissionResult::retry("erase feedback allocation unavailable");}
            else scene=internal::drawReferencePlanToSkCanvas(*canvas,plan,images_,&textResources_,includeInk);
        }
    }catch(const std::bad_alloc&){
        canvas->restoreToCount(sceneSaveCount);clearRenderCache();
        scene=BackendSubmissionResult::retry("transient scene allocation unavailable");
    }
    if(execution.pending()||execution.rejected())scene=execution.rejected()?BackendSubmissionResult::rejected("execution target rejected"):BackendSubmissionResult::retry("coverage target pending");
    if (scene.code != BackendSubmissionCode::kAccepted) {
        damageRestore.restore();
        provider.release();
        return scene;
    }
    if (selectionOverlay->creationPreview()&&(!cache||!cache->stagedBodies())) {
        internal::drawTransientObjectToSkCanvas(*canvas, plan,
                                                 *selectionOverlay->creationPreview(),selectionOverlay->creationRoute());
        internal::drawInkEntriesToSkCanvas(*canvas, plan, images_, &textResources_,cache,true);
    }
    if (const auto& hover=selectionOverlay->creationHover()) {
        SkPaint ghostPaint;ghostPaint.setAlphaf(0.25F);
        canvas->saveLayer(nullptr,&ghostPaint);
        if(hover->sibling) internal::drawTransientObjectToSkCanvas(*canvas,plan,*hover->sibling);
        internal::drawTransientObjectToSkCanvas(*canvas,plan,hover->primary,hover->route?&*hover->route:nullptr);
        canvas->restore();
    }
    internal::drawEditingOverlayToSkCanvas(*canvas, plan.frame.metrics, *selectionOverlay);
    damageRestore.restore();
    std::shared_ptr<PartialEraseFrame> candidate;
    if(persistentErase)try {
        candidate=std::make_shared<PartialEraseFrame>(plan);candidate->provider=&provider;
        candidate->surface=acquired.frame.surface;candidate->cache=cache;
        candidate->sequence=cache->frameSequence();candidate->resource=resourceEpoch_;
        candidate->contents=acquired.frame.surface->generationID();
    }catch(const std::bad_alloc&){} // Optional validity; next frame fully replaces.
    provider.release();
    ++submissions_;
    ++rasterizations_;
    G5_SCOPE(g5Present,g5::Tag::provider,g5::Purpose::none);
    const auto presented=provider.present();
    G5_SCOPE_END(g5Present);
    G5_OBSERVE(g5::Kind::Submit,g5::Tag::provider,true,plan.frame.viewId.value(),plan.frame.frameId.value(),plan.frame.sceneGeneration.value(),plan.frame.sceneReadToken.value(),plan.frame.surfaceGeneration.value(),plan.frame.metricsGeneration.value(),static_cast<unsigned>(presented.code));
    if(candidate&&presented.code==BackendSubmissionCode::kAccepted){candidate->presents=provider.presentCount();partialEraseFrame_=std::move(candidate);}
    return presented;
}

BackendSubmissionResult SkiaRenderer::renderPreview(
    SkiaSurfaceProvider& provider, const PreviewGeometry& geometry,
    const PreviewStyleOverride& style) {
    G5_SCOPE(g5Preview,g5::Tag::preview,g5::Purpose::none);
    G5_ROUTE(g5Route,g5::Route::preview_tail);

    if (geometry.revision == 0U || geometry.surfaceGeneration.value() == 0U ||
        !std::isfinite(geometry.viewport.scale) || geometry.viewport.scale <= 0.0F ||
        !std::isfinite(geometry.viewport.translationX) ||
        !std::isfinite(geometry.viewport.translationY) ||
        !std::isfinite(geometry.logicalToPhysicalX) || geometry.logicalToPhysicalX <= 0.0F ||
        !std::isfinite(geometry.logicalToPhysicalY) || geometry.logicalToPhysicalY <= 0.0F ||
        provider.generation() != geometry.surfaceGeneration.value()) {
        return BackendSubmissionResult::rejected("invalid preview geometry or surface generation");
    }
    const auto acquired = provider.acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired || acquired.frame.surface == nullptr) {
        return acquired.code == SkiaSurfaceAcquireCode::kUnavailable
            ? BackendSubmissionResult::retry(acquired.message)
            : BackendSubmissionResult::rejected(acquired.message);
    }
    if (acquired.frame.generation != geometry.surfaceGeneration.value()) {
        provider.release();
        return BackendSubmissionResult::rejected("stale preview surface generation");
    }
    G5_TARGET(acquired.frame.surface,true);
    auto* canvas = acquired.frame.surface->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);
    canvas->save();
    canvas->scale(geometry.logicalToPhysicalX, geometry.logicalToPhysicalY);
    canvas->translate(geometry.viewport.translationX, geometry.viewport.translationY);
    canvas->scale(geometry.viewport.scale, geometry.viewport.scale);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    const auto blend = style.blendMode == 0U ? SkBlendMode::kSrcOver
                                              : static_cast<SkBlendMode>(style.blendMode);
    paint.setBlendMode(blend);
    // The preview contract defines opacityMultiplier as the effective
    // overlay opacity. The RGBA style alpha is descriptive metadata.
    const float alpha = style.overrideOpacity
        ? std::clamp(style.opacityMultiplier, 0.0F, 1.0F)
        : 1.0F;
    // The default PreviewStyleOverride is an amber transient tint;
    // callers such as the partial eraser can still provide an explicit color.
    paint.setColor4f({style.overrideColor ? style.red : 1.0F,
                      style.overrideColor ? style.green : 0.62F,
                      style.overrideColor ? style.blue : 0.04F, alpha}, nullptr);
    auto drawOutline = [&](const auto& outline, const SkPaint& outlinePaint) {
        if (outline.empty()) return;
        canvas->drawPath(buildVectorBrushOutlineSkPath(
            std::span<const std::remove_cvref_t<decltype(outline.front())>>(
                outline.data(), outline.size())), outlinePaint);
    };
    auto drawDabs = [&](const auto& dabs, const SkPaint& dabPaint) {
        if constexpr (std::is_same_v<std::decay_t<decltype(dabs)>,
                                     std::vector<canvas::ink::BrushDab>>) {
            const auto materialRevision = dabs.empty() ? 0U : dabs.front().materialRevision;
            const auto materialMode = dabs.empty() ? 0U : dabs.front().materialMode;
            if (materialMode == 4U) {
                internal::drawPreviewMembraneDabsToSkCanvas(*canvas, dabs,
                    dabPaint.getColor4f().fR, dabPaint.getColor4f().fG,
                    dabPaint.getColor4f().fB, dabPaint.getColor4f().fA);
            } else if (materialRevision >= 3U) {
                internal::drawPreviewChalkDabsToSkCanvas(*canvas, dabs,
                    dabPaint.getColor4f().fR, dabPaint.getColor4f().fG,
                    dabPaint.getColor4f().fB, dabPaint.getColor4f().fA, materialRevision);
            } else {
                internal::drawPreviewDabsToSkCanvas(*canvas, dabs,
                    dabPaint.getColor4f().fR, dabPaint.getColor4f().fG,
                    dabPaint.getColor4f().fB, dabPaint.getColor4f().fA);
            }
        }
    };
    auto drawContour = [&](const PreviewGeometry::Contour& contour) {
        if(contour.compactConfig&&contour.compactChunks){
            auto config=*contour.compactConfig;
            if (!style.preferCapturedPaint) {
                if (style.overrideColor)
                    config.color = {style.red, style.green, style.blue, 1.0};
                // As with outline/dab previews, this is the effective overlay
                // alpha. Do not multiply by the captured stroke opacity again.
                if (style.overrideOpacity) {
                    config.color[3] = 1.0;
                    config.opacity = alpha;
                }
            }
            const auto info=acquired.frame.surface->imageInfo();
            const auto bytes=std::uint64_t(info.width())*info.height()*4;
            if(!compactCache_||compactCache_->generation!=geometry.surfaceGeneration.value()||compactCache_->scale!=geometry.viewport.scale||
               compactCache_->x!=geometry.viewport.translationX||compactCache_->y!=geometry.viewport.translationY||compactCache_->px!=geometry.logicalToPhysicalX||compactCache_->py!=geometry.logicalToPhysicalY||compactCache_->width!=info.width()||compactCache_->height!=info.height()){
                compactCache_=std::make_shared<CompactCache>();compactCache_->generation=geometry.surfaceGeneration.value();
                compactCache_->scale=geometry.viewport.scale;compactCache_->x=geometry.viewport.translationX;compactCache_->y=geometry.viewport.translationY;
                compactCache_->px=geometry.logicalToPhysicalX;compactCache_->py=geometry.logicalToPhysicalY;compactCache_->width=info.width();compactCache_->height=info.height();
            }
            auto& cache=*compactCache_;
            // Persistent coverage is view-bounded and disposable. Large hosts
            // fall back to clipped reconstruction rather than exceed budget.
            const bool gpuMaterial=config.algorithmVersion==5||config.algorithmVersion==6;
            // Keep a 2x 1000x650 view incremental: two view surfaces fit the
            // existing 32MiB aggregate budget. Oversized views stay bounded.
            const std::uint64_t surfaceLimit=gpuMaterial?16*1024*1024:8*1024*1024;
            if(bytes>surfaceLimit){
                std::vector<const ink::BrushPreviewDelta::Chunk*> chunks;
                for(auto c=contour.compactChunks.get();c;c=c->previous.get())chunks.push_back(c);
                std::vector<foundation::InkKnot> knots;
                for(auto i=chunks.rbegin();i!=chunks.rend();++i)knots.insert(knots.end(),(*i)->knots.begin(),(*i)->knots.end());
                compactWork_.generatedKnots+=knots.size();internal::drawCompactStrokeToSkCanvas(*canvas,config,knots,contour.compactSeed);return;
            }
            const auto stableLimit=gpuMaterial?32*1024*1024-bytes:
                cache.scratch?32*1024*1024-2*bytes:24*1024*1024;
            const auto nextCount=cache.entries.size()+(cache.entries.contains(contour.session)?0:1);
            if(bytes*nextCount>stableLimit)cache.entries.clear();
            auto& entry=cache.entries[contour.session];
            auto prefix=contour.compactChunks->previous;
            std::vector<const ink::BrushPreviewDelta::Chunk*> added;
            auto c=prefix;
            while(c&&c!=entry.prefix){added.push_back(c.get());c=c->previous;}
            if(!entry.surface||c!=entry.prefix||entry.config!=config){
                entry.surface=gpuMaterial?acquired.frame.surface->makeSurface(info):SkSurfaces::Raster(info);
                if(!entry.surface)return;
                entry.surface->getCanvas()->clear(SK_ColorBLACK);entry.prefix.reset();added.clear();
                for(auto v=prefix;v;v=v->previous)added.push_back(v.get());
            }
            auto* stable=entry.surface->getCanvas();stable->save();
            stable->scale(geometry.logicalToPhysicalX,geometry.logicalToPhysicalY);
            stable->translate(geometry.viewport.translationX,geometry.viewport.translationY);stable->scale(geometry.viewport.scale,geometry.viewport.scale);
            for(auto i=added.rbegin();i!=added.rend();++i){compactWork_.generatedKnots+=(*i)->knots.size();if(config.algorithmVersion==4)internal::drawCompactContourCoverageToSkCanvas(*stable,config,(*i)->contour);
                else internal::drawCompactCoverageToSkCanvas(*stable,config,(*i)->knots,(*i)->startDistance,contour.compactChunks->totalDistance,contour.compactSeed);}
            stable->restore();entry.prefix=prefix;entry.config=config;
            // Use the target backend, retaining one disposable scratch. A CPU
            // Raster here interprets every procedural shader pixel on Web.
            if(gpuMaterial&&!cache.scratch)cache.scratch=acquired.frame.surface->makeSurface(info);
            auto combined=gpuMaterial?cache.scratch:SkSurfaces::Raster(info);if(!combined)return;
            auto* temporary=combined->getCanvas();temporary->resetMatrix();temporary->clear(SK_ColorBLACK);
            temporary->drawImage(entry.surface->makeImageSnapshot(),0,0);
            temporary->scale(geometry.logicalToPhysicalX,geometry.logicalToPhysicalY);
            temporary->translate(geometry.viewport.translationX,geometry.viewport.translationY);temporary->scale(geometry.viewport.scale,geometry.viewport.scale);
            compactWork_.generatedKnots+=contour.compactChunks->knots.size();
            if(config.algorithmVersion==4)internal::drawCompactContourCoverageToSkCanvas(*temporary,config,contour.compactChunks->contour);
            else internal::drawCompactCoverageToSkCanvas(*temporary,config,contour.compactChunks->knots,contour.compactChunks->startDistance,contour.compactChunks->totalDistance,contour.compactSeed);
            internal::compositeCompactCoverageToSkCanvas(*canvas,*combined->makeImageSnapshot(),config,contour.compactSeed);
            compactWork_.retainedBytes=bytes*cache.entries.size();compactWork_.temporaryBytes=bytes;
            return;
        }
        SkPaint contourPaint = paint;
        if (contour.hasPaint && style.preferCapturedPaint) {
            const float effectiveAlpha = std::clamp(contour.alpha * contour.opacity, 0.0F, 1.0F);
            contourPaint.setColor4f({contour.red, contour.green, contour.blue,
                                     effectiveAlpha}, nullptr);
        }
        if (contour.fadingTrail) {
            drawFadingTrail(*canvas,*contour.fadingTrail,
                {contour.red,contour.green,contour.blue,std::clamp(contour.alpha*contour.opacity,0.0F,1.0F)});
        } else if (contour.roundStroke) {
            const auto& stroke = *contour.roundStroke;
            if (stroke.points.empty() || !std::isfinite(stroke.radius) || stroke.radius <= 0) return;
            contourPaint.setStyle(SkPaint::kStroke_Style);
            contourPaint.setStrokeWidth(static_cast<float>(2 * stroke.radius));
            contourPaint.setStrokeCap(SkPaint::kRound_Cap);
            contourPaint.setStrokeJoin(SkPaint::kRound_Join);
            SkPathBuilder path;
            path.moveTo(static_cast<float>(stroke.points.front().x), static_cast<float>(stroke.points.front().y));
            bool hasLength = false;
            for (const auto& point : stroke.points) {
                path.lineTo(static_cast<float>(point.x), static_cast<float>(point.y));
                hasLength |= point.x != stroke.points.front().x || point.y != stroke.points.front().y;
            }
            if (hasLength) canvas->drawPath(path.detach(), contourPaint);
            else {
                contourPaint.setStyle(SkPaint::kFill_Style);
                canvas->drawCircle(static_cast<float>(stroke.points.front().x),
                    static_cast<float>(stroke.points.front().y), static_cast<float>(stroke.radius), contourPaint);
            }
        } else if (contour.dabs.empty()) drawOutline(contour.outline, contourPaint);
        else drawDabs(contour.dabs, contourPaint);
    };
    if (!geometry.contours.empty()) {
        for (const auto& contour : geometry.contours) drawContour(contour);
    } else {
        if (geometry.dabs.empty()) drawOutline(geometry.outline, paint);
        else drawDabs(geometry.dabs, paint);
    }
    canvas->restore();
    provider.release();
    ++submissions_;
    ++rasterizations_;
    return provider.present();
}

BackendSubmissionResult SkiaRenderer::clearPreview(
    SkiaSurfaceProvider& provider, SurfaceGeneration generation) {
    if (generation.value() == 0U || provider.generation() != generation.value()) {
        return BackendSubmissionResult::rejected("stale preview clear generation");
    }
    const auto acquired = provider.acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired || acquired.frame.surface == nullptr) {
        return BackendSubmissionResult::rejected(acquired.message);
    }
    if (acquired.frame.generation != generation.value()) {
        provider.release();
        return BackendSubmissionResult::rejected("stale preview clear surface generation");
    }
    acquired.frame.surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    provider.release();
    ++submissions_;
    ++rasterizations_;
    return provider.present();
}

BackendSubmissionResult SkiaRenderer::renderPrimitives(
    SkiaSurfaceProvider& provider,
    std::span<const canvas::ink::BrushPrimitive> primitives,
    float scale, float translationX, float translationY) {
    if (!std::isfinite(scale) || scale <= 0.0F || !std::isfinite(translationX) || !std::isfinite(translationY)) {
        return BackendSubmissionResult::rejected("invalid Skia viewport transform");
    }
    const auto acquired = provider.acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired || acquired.frame.surface == nullptr) {
        return BackendSubmissionResult::rejected(acquired.message);
    }
    G5_TARGET(acquired.frame.surface,true);
    auto* canvas = acquired.frame.surface->getCanvas();
    canvas->clear(SK_ColorWHITE);
    canvas->save();
    canvas->translate(translationX, translationY);
    canvas->scale(scale, scale);
    internal::drawBrushPrimitivesToSkCanvas(*canvas, primitives);
    canvas->restore();
    provider.release();
    ++submissions_;
    ++rasterizations_;
    return provider.present();
}

BackendSubmissionResult SkiaRenderer::renderBrushPoints(
    SkiaSurfaceProvider& provider, std::span<const BrushRenderPoint> points,
    float scale, float translationX, float translationY) {
    std::vector<canvas::ink::BrushPrimitive> primitives;
    primitives.reserve(points.size());
    for (const auto& point : points) {
        canvas::ink::BrushPrimitive primitive;
        primitive.x = point.x;
        primitive.y = point.y;
        primitive.size = point.size;
        primitive.rotation = point.rotation;
        primitive.opacity = point.opacity;
        primitive.representation = static_cast<canvas::ink::BrushRepresentation>(point.representation);
        primitives.push_back(primitive);
    }
    return renderPrimitives(provider, primitives, scale, translationX, translationY);
}

BackendSubmissionResult SkiaRenderer::renderStrokes(
    SkiaSurfaceProvider& provider,
    std::span<const std::vector<CanonicalStrokePoint>> strokes,
    float scale, float translationX, float translationY) {
    if (!std::isfinite(scale) || scale <= 0.0F || !std::isfinite(translationX) || !std::isfinite(translationY)) {
        return BackendSubmissionResult::rejected("invalid Skia viewport transform");
    }
    const auto acquired = provider.acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired || acquired.frame.surface == nullptr) {
        return BackendSubmissionResult::rejected(acquired.message);
    }
    G5_TARGET(acquired.frame.surface,true);
    auto* canvas = acquired.frame.surface->getCanvas();
    canvas->clear(SK_ColorWHITE);
    canvas->save();
    canvas->translate(translationX, translationY);
    canvas->scale(scale, scale);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorBLUE);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeCap(SkPaint::kRound_Cap);
    paint.setStrokeJoin(SkPaint::kRound_Join);
    paint.setStrokeWidth(3.0F);
    for (const auto& stroke : strokes) {
        if (stroke.empty()) continue;
        if (stroke.size() == 1U) { canvas->drawCircle(stroke.front().x, stroke.front().y, 1.5F, paint); continue; }
        SkPathBuilder path;
        path.moveTo(stroke.front().x, stroke.front().y);
        if (stroke.size() == 2U) path.lineTo(stroke.back().x, stroke.back().y);
        else for (std::size_t i = 0; i + 1U < stroke.size(); ++i) {
            const auto& p0 = stroke[i == 0U ? i : i - 1U];
            const auto& p1 = stroke[i];
            const auto& p2 = stroke[i + 1U];
            const auto& p3 = stroke[i + 2U < stroke.size() ? i + 2U : i + 1U];
            path.cubicTo(p1.x + (p2.x - p0.x) / 6.0F, p1.y + (p2.y - p0.y) / 6.0F,
                         p2.x - (p3.x - p1.x) / 6.0F, p2.y - (p3.y - p1.y) / 6.0F, p2.x, p2.y);
        }
        canvas->drawPath(path.detach(), paint);
    }
    canvas->restore();
    provider.release();
    ++submissions_;
    ++rasterizations_;
    return provider.present();
}

} // namespace canvas::render
