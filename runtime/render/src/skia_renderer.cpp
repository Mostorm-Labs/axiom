#include "canvas/foundation/internal/frame_timing.hpp"
#include "canvas/render/skia_renderer.hpp"

#include "canvas/render/skia_brush_renderer.hpp"
#include "canvas/render/brush_outline_skia_path.hpp"
#include "canvas/render/skia_ink_backend.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "skia_render_cache_internal.hpp"
#include "skia_navigation_tiles.hpp"
#include "skia_gpu_lifetime.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkBlendMode.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkSurface.h"
#include "include/core/SkImage.h"
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <chrono>

#include <cmath>
#include <algorithm>
#include <type_traits>

namespace canvas::render {
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

struct SkiaRenderer::NavigationCache final {
    const void* provider=nullptr;
    sk_sp<SkImage> image;
    std::unique_ptr<FrameState> frame;
    WorldToViewAffine transform;
    resources::ResourceLease lease;
    std::shared_ptr<const internal::PreparedFrameImage> prepared;
    SkColor background=SK_ColorWHITE;
    std::optional<CanonicalViewportTransform> feedbackCamera;
    bool backgroundProofRejected=false;
};
struct SkiaRenderer::CompactCache final {
    struct Entry {std::shared_ptr<const ink::BrushPreviewDelta::Chunk> prefix;sk_sp<SkSurface> surface;foundation::InkBrushConfig config;};
    std::unordered_map<std::uint64_t,Entry> entries;
    sk_sp<SkSurface> scratch;
    std::uint64_t generation=0;float scale=0,x=0,y=0,px=0,py=0;int width=0,height=0;
};
namespace {
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
        chrome.setStrokeWidth(2.0F);
        chrome.setColor4f({0.16F, 0.72F, 0.82F, 0.95F}, nullptr);
        const auto& outline = overlay.selectionOutline();
        SkPathBuilder path;
        path.moveTo(outline.corners[0].x, outline.corners[0].y);
        path.lineTo(outline.corners[1].x, outline.corners[1].y);
        path.lineTo(outline.corners[2].x, outline.corners[2].y);
        path.lineTo(outline.corners[3].x, outline.corners[3].y);
        path.close();
        canvas.drawPath(path.detach(), chrome);
        chrome.setStyle(SkPaint::kFill_Style);
        for (const auto kind : {HandleKind::kTopLeft, HandleKind::kTop,
                                HandleKind::kTopRight, HandleKind::kRight,
                                HandleKind::kBottomRight, HandleKind::kBottom,
                                HandleKind::kBottomLeft, HandleKind::kLeft,
                                HandleKind::kRotation}) {
            const auto& handle = overlay.handle(kind);
            if (handle.kind == HandleKind::kNone) continue;
            canvas.drawCircle(handle.center.x, handle.center.y,
                               handle.visualRadius, chrome);
        }
    }
    canvas.restore();
}
}

BackendSubmissionResult SkiaRenderer::renderFrame(
    SkiaSurfaceProvider& provider, const FramePlan& plan,
    const EditingOverlay* selectionOverlay, const RenderPreparationBudget* preparation) {
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
    const bool cachedGpuSafe=!gpuContext||gpu.ready();
    if(gpu.ready())gpu.touch();
    internal::SkiaRenderCache* cache=nullptr;
    if(!cachedGpuSafe)clearRenderCache();
    if(cachedGpuSafe)try {
        if(!resourceBudget_)resourceBudget_=std::make_shared<resources::ResourceBudgetManager>(
            resources::BudgetLimits{renderCacheOptions_.maxBytes,renderCacheOptions_.sharedMaxBytes});
        if (!renderCache_) renderCache_=std::make_shared<internal::SkiaRenderCache>();
        if(!sourceRetention_)sourceRetention_=std::make_shared<internal::SourceRetentionPool>();
        renderCache_->sourcePool=sourceRetention_;
        renderCache_->budget=resourceBudget_;
        renderCache_->options=renderCacheOptions_;
        AXIOM_FRAME_TIMING_SCOPE(beginTiming,cache_begin);
        renderCache_->begin(plan,&provider,*acquired.frame.surface,resourceEpoch_,
            selectionOverlay?selectionOverlay->selectedObject():foundation::ObjectId{});
        cache=renderCache_.get();
    } catch(const std::bad_alloc&) {clearRenderCache();}
    if(preparation && cache){
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
                // Reclaim optional resources only on actual critical admission
                // pressure. Their unfinished GPU reads remain charged.
                navigationCache_.reset();
                if(const auto tiles=navigation.lock())tiles->reclaimForExact(resourceBudget_,
                    renderCacheOptions_.sharedMaxBytes,bytes,frame,providerIdentity,resourceEpoch_,true);
            };
            AXIOM_FRAME_TIMING_SCOPE(prepareTiming,cache_prepare);prepared=cache->prepare(*target,plan,*preparation,images_,&textResources_);
        }
        catch(const std::bad_alloc&){cache->clear();prepared=internal::SkiaRenderCache::Preparation::kResourcePending;}
        target->restore();
        rasterizations_+=cache->work.rasterizedGroups-beforePreparation;
        if(prepared==internal::SkiaRenderCache::Preparation::kRejected){
            provider.release();return BackendSubmissionResult::rejected("canonical work buffers exceed shared resource capacity");
        }
        if(prepared==internal::SkiaRenderCache::Preparation::kPending ||
           prepared==internal::SkiaRenderCache::Preparation::kResourcePending){
            provider.release();return BackendSubmissionResult::retry("exact cache preparation pending");
        }
    }
    const auto beforeRaster=cache?cache->work.rasterizedGroups:0;
    AXIOM_FRAME_TIMING_SCOPE(sceneTiming,scene_draw);
    const auto result = (preparation && cache && cache->compositePreparedFrame(*acquired.frame.surface->getCanvas()))
        ? BackendSubmissionResult::accepted() : internal::drawReferencePlanToSkCanvas(
        *acquired.frame.surface->getCanvas(), plan, images_, &textResources_,true,cache);
    AXIOM_FRAME_TIMING_END(sceneTiming);
    if(result.code==BackendSubmissionCode::kAccepted){
        AXIOM_FRAME_TIMING_SCOPE(snapshotTiming,navigation_snapshot);
        try {
            const auto bytes=std::uint64_t(metrics.physicalWidth)*metrics.physicalHeight*4;
            if(cachedGpuSafe && resourceBudget_ && renderCacheOptions_.enabled && (!selectionOverlay || (!selectionOverlay->creationPreview() && !selectionOverlay->textCaret)) &&
               plan.background.mode==foundation::BackgroundMode::kPlain && !plan.transparentBackground){
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
                    navigationCache_=std::move(candidate);
                }
            }else navigationCache_.reset();
        }catch(const std::bad_alloc&){navigationCache_.reset();}
    }
    if (result.code == BackendSubmissionCode::kAccepted && selectionOverlay != nullptr) {
        auto* canvas = acquired.frame.surface->getCanvas();
        if (selectionOverlay->creationPreview()) {
            internal::drawTransientObjectToSkCanvas(*canvas, plan, *selectionOverlay->creationPreview(),selectionOverlay->creationRoute());
            internal::drawInkEntriesToSkCanvas(*canvas, plan, images_, &textResources_);
        }
        internal::drawEditingOverlayToSkCanvas(*canvas, plan.frame.metrics, *selectionOverlay);
    }
    provider.release();
    if (result.code != BackendSubmissionCode::kAccepted) {clearRenderCache();return result;}
    ++submissions_;
    rasterizations_+=cache?cache->work.rasterizedGroups-beforeRaster:1;
    AXIOM_FRAME_TIMING_SCOPE(presentTiming,provider_present);
    const auto presented=provider.present();
    AXIOM_FRAME_TIMING_END(presentTiming);
    if(presented.code!=BackendSubmissionCode::kAccepted)clearRenderCache();
    return presented;
}

BackendSubmissionResult SkiaRenderer::renderNavigation(SkiaSurfaceProvider& provider,
    const CanonicalViewportTransform& camera,const FrameState& current,
    const EditingOverlay* selectionOverlay){
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
        resources::kPixelResourceClasses&~resources::resourceClassMask(resources::ResourceClass::kCanonicalFrame));
    if(!gpu.ready()){provider.release();return BackendSubmissionResult::retry("navigation completion unavailable");}
    gpu.touch();
    const auto& old=cache->transform;
    if(old.a<=0 || old.d!=old.a || old.b!=0 || old.c!=0){
        provider.release();return BackendSubmissionResult::retry("navigation transform ineligible");
    }
    const float ratio=static_cast<float>(camera.scale/old.a);
    const float sx=current.metrics.physicalWidth/current.metrics.logicalWidth;
    const float sy=current.metrics.physicalHeight/current.metrics.logicalHeight;
    auto* canvas=acquired.frame.surface->getCanvas();
    canvas->save();canvas->resetMatrix();canvas->clear(cache->background);
    canvas->translate(static_cast<SkScalar>((camera.translationX-ratio*old.tx)*sx),static_cast<SkScalar>((camera.translationY-ratio*old.ty)*sy));
    canvas->scale(ratio,ratio);
    if(cache->prepared)cache->prepared->markGpuUse();
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
    serviceResourceRetirements();
    if(!resourceBudget_)resourceBudget_=std::make_shared<resources::ResourceBudgetManager>(
        resources::BudgetLimits{renderCacheOptions_.maxBytes,renderCacheOptions_.sharedMaxBytes});
    if(!sourceRetention_)sourceRetention_=std::make_shared<internal::SourceRetentionPool>();
    if(!navigationTiles_)navigationTiles_=std::make_shared<internal::SkiaNavigationTiles>();
    // Keep same-demand exact work;discard only obsolete offscreen preparation.
    // Its leases still wait for actual GPU completion before becoming reusable.
    if(renderCache_)renderCache_->releaseIdleWorkBuffers(current,
        {camera.scale,0,0,camera.scale,camera.translationX,camera.translationY},&provider,resourceEpoch_);
    auto options=renderCacheOptions_;
    internal::RetainedNavigationSource retained;
    const auto cache=navigationCache_;
    if(cache&&cache->image&&cache->frame&&cache->provider==&provider&&
       cache->frame->viewId==current.viewId&&cache->frame->sceneGeneration==current.sceneGeneration&&
       cache->frame->sceneReadToken==current.sceneReadToken&&cache->frame->metrics==current.metrics&&
       cache->frame->metricsGeneration==current.metricsGeneration&&cache->frame->surfaceGeneration==current.surfaceGeneration&&
       cache->frame->viewportClip==current.viewportClip&&cache->transform.a>0&&std::isfinite(cache->transform.a)&&
       cache->transform.d==cache->transform.a&&cache->transform.b==0&&cache->transform.c==0){
        const auto& old=cache->transform;
        retained.bounds={float(-old.tx/old.a),float(-old.ty/old.a),
            float((cache->frame->metrics.logicalWidth-old.tx)/old.a),float((cache->frame->metrics.logicalHeight-old.ty)/old.a)};
        retained.density=old.a*std::min(current.metrics.physicalWidth/current.metrics.logicalWidth,
            current.metrics.physicalHeight/current.metrics.logicalHeight);
        retained.extend=[&,cache]{
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
        const auto result=navigationTiles_->render(provider,camera,current,query,resourceEpoch_,options,resourceBudget_,allowPrewarm,retained.draw?&retained:nullptr,&textResources_,images_,drawOverlay,sourceRetention_,gpuRetirements_);
        if(result.code!=BackendSubmissionCode::kAccepted)navigationTiles_.reset();
        return result;
    }catch(const std::bad_alloc&){navigationTiles_.reset();return BackendSubmissionResult::retry("world navigation allocation failure");}
}
void SkiaRenderer::cancelNavigationWork() noexcept {if(navigationTiles_)navigationTiles_->cancelWork();}
NavigationWork SkiaRenderer::navigationWork() const noexcept {
    return navigationTiles_?navigationTiles_->work:NavigationWork{};
}
void SkiaRenderer::setRenderCacheOptions(RenderCacheOptions options) {
    clearRenderCache();if(resourceBudget_)resourceBudget_->close();gpuRetirements_.reset();resourceBudget_.reset();sourceRetention_.reset();renderCacheOptions_=options;
}
void SkiaRenderer::clearRenderCache() noexcept {navigationTiles_.reset();navigationCache_.reset();if(renderCache_)renderCache_->clear();}
RenderCacheWork SkiaRenderer::renderCacheWork()const noexcept{return renderCache_?renderCache_->work:RenderCacheWork{};}
RenderDamage SkiaRenderer::renderDamage()const{return renderCache_?renderCache_->damage:RenderDamage{};}

BackendSubmissionResult SkiaRenderer::renderTransientOverlay(
    SkiaSurfaceProvider& provider, const FramePlan& plan,
    const EditingOverlay* selectionOverlay) {
    serviceResourceRetirements();
    if (selectionOverlay == nullptr) {
        return clearPreview(provider, SurfaceGeneration{provider.generation()});
    }
    const auto acquired = provider.acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired || acquired.frame.surface == nullptr) {
        return BackendSubmissionResult::rejected(acquired.message);
    }
    auto* canvas = acquired.frame.surface->getCanvas();
    if(acquired.frame.generation!=plan.frame.surfaceGeneration.value()||
       acquired.frame.surface->width()!=int(plan.frame.metrics.physicalWidth)||
       acquired.frame.surface->height()!=int(plan.frame.metrics.physicalHeight)){
        provider.release();return BackendSubmissionResult::rejected("stale transient target");
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
    const bool cachedGpuSafe=!gpuContext||gpu.ready();
    if(gpu.ready())gpu.touch();
    internal::SkiaRenderCache* cache=nullptr;
    if(!cachedGpuSafe)clearRenderCache();
    if(cachedGpuSafe)try{
        if(!resourceBudget_)resourceBudget_=std::make_shared<resources::ResourceBudgetManager>(
            resources::BudgetLimits{renderCacheOptions_.maxBytes,renderCacheOptions_.sharedMaxBytes});
        if(!renderCache_)renderCache_=std::make_shared<internal::SkiaRenderCache>();
        if(!sourceRetention_)sourceRetention_=std::make_shared<internal::SourceRetentionPool>();
        renderCache_->sourcePool=sourceRetention_;
        renderCache_->budget=resourceBudget_;renderCache_->options=renderCacheOptions_;
        renderCache_->begin(plan,&provider,*acquired.frame.surface,resourceEpoch_,
            selectionOverlay?selectionOverlay->selectedObject():foundation::ObjectId{});cache=renderCache_.get();
    }catch(const std::bad_alloc&){clearRenderCache();}
    // Selection/creation transient presentation is a complete replacement
    // frame for the overlay window.  Drawing only the moved object on a
    // transparent surface leaves the old canonical pixels visible underneath
    // it, which looks like a duplicated object during a drag.  The reference
    // plan already contains any transient transform overrides, so render the
    // whole scene first and then add the editing chrome.
    const auto includeInk=!selectionOverlay->creationPreview().has_value();
    BackendSubmissionResult scene;
    if(cache){cache->drawTransientScene(*canvas,plan,images_,&textResources_,includeInk);scene=BackendSubmissionResult::accepted();}
    else scene=internal::drawReferencePlanToSkCanvas(*canvas,plan,images_,&textResources_,includeInk);
    if (scene.code != BackendSubmissionCode::kAccepted) {
        provider.release();
        return scene;
    }
    if (selectionOverlay->creationPreview()) {
        internal::drawTransientObjectToSkCanvas(*canvas, plan,
                                                 *selectionOverlay->creationPreview(),selectionOverlay->creationRoute());
        internal::drawInkEntriesToSkCanvas(*canvas, plan, images_, &textResources_,cache,true);
    }
    canvas->save();
    canvas->scale(plan.frame.metrics.physicalWidth / plan.frame.metrics.logicalWidth,
                  plan.frame.metrics.physicalHeight / plan.frame.metrics.logicalHeight);
    drawAcquisition(*canvas,*selectionOverlay);
    SkPaint guidePaint;
    guidePaint.setAntiAlias(true);
    guidePaint.setStyle(SkPaint::kStroke_Style);
    guidePaint.setStrokeWidth(1.0F);
    guidePaint.setColor4f({0.18F, 0.72F, 0.86F, 0.78F}, nullptr);
    for (const auto& guide : selectionOverlay->guides()) {
        canvas->drawLine(guide.start.x, guide.start.y, guide.end.x, guide.end.y,
                         guidePaint);
    }
    SkPaint attachmentPaint;
    attachmentPaint.setAntiAlias(true);
    attachmentPaint.setColor4f({0.95F, 0.58F, 0.12F, 0.95F}, nullptr);
    for (const auto& point : selectionOverlay->attachmentPoints()) {
        if (point.active) {
            attachmentPaint.setColor4f(point.hovered
                ? SkColor4f{0.35F, 0.72F, 1.0F, 0.38F}
                : SkColor4f{0.95F, 0.58F, 0.12F, 0.95F}, nullptr);
            canvas->drawCircle(point.center.x, point.center.y, point.hovered ? 8.0F : 4.0F,
                               attachmentPaint);
        }
    }
    if (selectionOverlay->selectionOutline().visible) {
        SkPaint chrome;
        chrome.setAntiAlias(true);
        chrome.setStyle(SkPaint::kStroke_Style);
        chrome.setStrokeWidth(2.0F);
        chrome.setColor4f({0.16F, 0.72F, 0.82F, 0.95F}, nullptr);
        const auto& outline = selectionOverlay->selectionOutline();
        SkPathBuilder path;
        path.moveTo(outline.corners[0].x, outline.corners[0].y);
        path.lineTo(outline.corners[1].x, outline.corners[1].y);
        path.lineTo(outline.corners[2].x, outline.corners[2].y);
        path.lineTo(outline.corners[3].x, outline.corners[3].y);
        path.close();
        canvas->drawPath(path.detach(), chrome);
        chrome.setStyle(SkPaint::kFill_Style);
        for (const auto kind : {HandleKind::kTopLeft, HandleKind::kTop,
                                HandleKind::kTopRight, HandleKind::kRight,
                                HandleKind::kBottomRight, HandleKind::kBottom,
                                HandleKind::kBottomLeft, HandleKind::kLeft,
                                HandleKind::kRotation}) {
            const auto& handle = selectionOverlay->handle(kind);
            if (handle.kind == HandleKind::kNone) continue;
            canvas->drawCircle(handle.center.x, handle.center.y,
                               handle.visualRadius, chrome);
        }
    }
    canvas->restore();
    provider.release();
    ++submissions_;
    ++rasterizations_;
    return provider.present();
}

BackendSubmissionResult SkiaRenderer::renderPreview(
    SkiaSurfaceProvider& provider, const PreviewGeometry& geometry,
    const PreviewStyleOverride& style) {
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
        if (contour.dabs.empty()) drawOutline(contour.outline, contourPaint);
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
