#include "skia_navigation_tiles.hpp"
#include "resource_reclaim.hpp"
#include "skia_gpu_lifetime.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrRecordingContext.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <unordered_set>
#include <cstdio>

namespace canvas::render::internal {
namespace {
#if defined(AXIOM_ENABLE_FRAME_TIMING)
struct DiagnosticTiming {
    std::uint64_t* result;std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
    explicit DiagnosticTiming(std::uint64_t& value):result(&value){}
    void finish(){if(result){*result+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count();result=nullptr;}}
    ~DiagnosticTiming(){finish();}
};
#endif
constexpr int pixels=256,gutter=2,extent=pixels+2*gutter;
constexpr std::size_t tileBytes=extent*extent*4;
foundation::WorldRect expanded(foundation::WorldRect r,double amount){
    return {float(r.left-amount),float(r.top-amount),float(r.right+amount),float(r.bottom+amount)};
}
bool intersects(const foundation::WorldRect& a,const foundation::WorldRect& b){
    return a.left<b.right&&a.right>b.left&&a.top<b.bottom&&a.bottom>b.top;
}
bool supported(const ReferenceTraversalEntry& e){
    const auto& t=e.record.transform;
    // Tile sources are rendered in world space.  The shared draw helper
    // applies the record transform below for ordinary shapes.  Stroke
    // coverage jobs use the same object affine when building their masks.
    if(!std::isfinite(t.a)||!std::isfinite(t.b)||!std::isfinite(t.c)||
       !std::isfinite(t.d)||!std::isfinite(t.tx)||!std::isfinite(t.ty))return false;
    // Erase masks are object-local render effects. They are safe in a world
    // tile because the tile source is rendered into an isolated transparent
    // surface and the shared Skia command path applies the mask before the
    // tile is composed. Rejecting them here forced every pan/zoom after a
    // partial erase through exact full-scene preparation.
    if(!e.record.eraseMasks.empty() &&
       !std::holds_alternative<CompactStrokeReferenceCommand>(e.command) &&
       !std::holds_alternative<BrushStrokeReferenceCommand>(e.command))return false;
    const bool identity=t.a==1&&t.b==0&&t.c==0&&t.d==1&&t.tx==0&&t.ty==0;
    // Compact coverage and its local masks share the same object affine in
    // the isolated source below. Translation/rotation/scale/shear therefore
    // remain eligible after erasing too; rejecting them forced camera motion
    // into repeated synchronous whole-stroke canonical rendering.
    if(std::holds_alternative<CompactStrokeReferenceCommand>(e.command))return true;
    const auto* v=std::get_if<BrushStrokeReferenceCommand>(&e.command);
    if(v&&v->content.get().stroke.snapshot.material_mode==1)return identity;
    return std::holds_alternative<ShapeReferenceCommand>(e.command) ||
        std::holds_alternative<ImageReferenceCommand>(e.command) ||
        std::holds_alternative<StickyReferenceCommand>(e.command) ||
        std::holds_alternative<TableReferenceCommand>(e.command) ||
        std::holds_alternative<RichTextReferenceCommand>(e.command);
}

std::pair<std::size_t, std::size_t> compactRange(const ReferenceTraversalEntry& entry) {
    const auto* compact = std::get_if<CompactStrokeReferenceCommand>(&entry.command);
    if (compact == nullptr || !entry.navigationChunkSpan) return {0, compact == nullptr ? 0U : compact->content.get().stroke.knots.size()};
    const auto& knots = compact->content.get().stroke.knots;
    // V4 locality selects original paired contour primitives from prepared
    // geometry. A raw knot subspan would change its stateful anchor history.
    if (compact->content.get().stroke.config.algorithmVersion == 4U)
        return {0, knots.size()};
    const auto begin = std::min<std::size_t>(entry.navigationChunkSpan->pointOffset, knots.size());
    const auto end = std::min<std::size_t>(begin + entry.navigationChunkSpan->pointCount, knots.size());
    if (end <= begin) return {0, 0};
    // Keep the next neighbor on the right. The material helpers themselves
    // read the preceding knot when needed, while the distance seed below
    // preserves the full-stroke arc-length phase.
    return {begin, std::min(knots.size(), end + 1)};
}

void drawNavigationEntry(SkCanvas& canvas,const ReferenceTraversalEntry& entry,
                         SkiaTextResources* textResources, ImageResourceResolver* images,
                         const SkPath* preparedVectorPath=nullptr){
    SkAutoCanvasRestore isolatedMask(&canvas,false);
    if(!entry.record.eraseMasks.empty())canvas.saveLayer(nullptr,nullptr);
    const auto& t=entry.record.transform;
    const bool identity=t.a==1.0&&t.b==0.0&&t.c==0.0&&t.d==1.0&&t.tx==0.0&&t.ty==0.0;
    if(identity){drawValidatedIsolatedEntryToSkCanvas(canvas,entry,nullptr,nullptr,nullptr,false,nullptr,images,textResources,preparedVectorPath);return;}
    canvas.save();
    canvas.concat(SkMatrix::MakeAll(static_cast<SkScalar>(t.a),static_cast<SkScalar>(t.c),
                                    static_cast<SkScalar>(t.tx),static_cast<SkScalar>(t.b),
                                    static_cast<SkScalar>(t.d),static_cast<SkScalar>(t.ty),
                                    0.0F,0.0F,1.0F));
    drawValidatedIsolatedEntryToSkCanvas(canvas,entry,nullptr,nullptr,nullptr,false,nullptr,images,textResources,preparedVectorPath);
    canvas.restore();
}
}
bool SkiaNavigationTiles::ValidRegions::add(foundation::WorldRect r){
    // Merge only exact rectangular unions; never fill the unrendered inside
    // corner of an L-shaped frontier. Revisit after a merge enables another.
    for(std::size_t i=0;i<count;){
        const auto a=areas[i];
        if(a.left<=r.left&&a.top<=r.top&&a.right>=r.right&&a.bottom>=r.bottom)return true;
        const bool contains=r.left<=a.left&&r.top<=a.top&&r.right>=a.right&&r.bottom>=a.bottom;
        const bool horizontal=a.top==r.top&&a.bottom==r.bottom&&a.left<=r.right&&r.left<=a.right;
        const bool vertical=a.left==r.left&&a.right==r.right&&a.top<=r.bottom&&r.top<=a.bottom;
        if(contains||horizontal||vertical){
            r={std::min(a.left,r.left),std::min(a.top,r.top),std::max(a.right,r.right),std::max(a.bottom,r.bottom)};
            areas[i]=areas[--count];i=0;
        }else ++i;
    }
    if(count==areas.size())return false;
    areas[count++]=r;return true;
}
void SkiaNavigationTiles::cancelJob(std::unique_ptr<Job>& job){
    if(job){(void)scheduler_.cancel(job->id,1);++work.cancelledJobs;work.discardedKnotSteps+=job->steps;}
    job.reset();
}
void SkiaNavigationTiles::cancel(){cancelJob(job_);cancelJob(refinement_);}
bool SkiaNavigationTiles::usefulFallback(TileKey key,const Tile& tile,
    const foundation::WorldRect& focus,int level,std::uint64_t scene)const{
    const auto span=std::ldexp(256.,-key.scale);
    if(!tile.image||tile.checkedScene!=scene||key.scale>=level||
       span<std::max(focus.right-focus.left,focus.bottom-focus.top))return false;
    const auto near=expanded(focus,span);
    for(std::size_t i=0;i<tile.valid.count;++i){const auto& r=tile.valid.areas[i];
        if(intersects(r,near)&&!(r.left>=focus.left&&r.right<=focus.right&&r.top>=focus.top&&r.bottom<=focus.bottom))return true;
    }
    return false;
}
void SkiaNavigationTiles::reclaimForExact(const std::shared_ptr<resources::ResourceBudgetManager>& budget,
    std::size_t limit,std::size_t requested,const FrameState& frame,const SkiaSurfaceProvider* provider,std::uint64_t resourceEpoch,bool mandatory){
    if(!budget||budgetOwner_.lock()!=budget||requested>limit)return;
    const auto charged=[&]{return mandatory?chargeAfterRetirement(*budget):budget->used();};
    if(charged()<=limit-requested)return;
    // Optional picture recording cannot reclaim mandatory canonical buffers.
    // If those alone exceed its soft limit, throwing away completed navigation
    // regions still cannot admit the picture. Keep that reusable coverage and
    // let the existing checkpoint fallback complete exact preparation instead.
    // Bound everything this owner could release, including Recorded metadata.
    // Shared source lists/payloads may still be pinned by other consumers;
    // counting aliases again overestimates reclaimability, so skipping is safe.
    const auto used=charged();std::size_t reclaimable=0;
    const auto add=[&](std::size_t bytes){reclaimable+=std::min(bytes,used-reclaimable);};
    add(work.retainedBytes);add(geometry_.preparedBytes());add(geometry_.sourceStatistics().bytes);
    for(const auto& [key,source]:sources_){(void)key;add(source->admittedBytes);}
    for(const auto& [key,tile]:tiles_){(void)key;add(tile.sources->admittedBytes);}
    for(const auto* job:{job_.get(),refinement_.get()})if(job){
        add(tileBytes*3+tileMetadataBytes);add(job->sources->admittedBytes);
    }
    if(used-reclaimable>limit-requested){++work.skippedReclaims;return;}
    cancelWork();
    for(auto it=sources_.begin();it!=sources_.end();)if(!tiles_.contains(it->first))it=sources_.erase(it);else ++it;
    foundation::WorldRect visible{};
    if(last_&&camera_.scale>0)visible={-camera_.translationX/camera_.scale,-camera_.translationY/camera_.scale,
        (last_->metrics.logicalWidth-camera_.translationX)/camera_.scale,(last_->metrics.logicalHeight-camera_.translationY)/camera_.scale};
    const auto focus=fallbackFocus_.value_or(visible);
    const bool sameTarget=provider_==provider&&resource_==resourceEpoch&&last_&&last_->viewId==frame.viewId&&last_->metrics==frame.metrics&&
        last_->metricsGeneration==frame.metricsGeneration&&last_->surfaceGeneration==frame.surfaceGeneration&&
        last_->sceneGeneration==frame.sceneGeneration&&last_->sceneReadToken==frame.sceneReadToken&&last_->viewportClip==frame.viewportClip;
    while(charged()>limit-requested&&!tiles_.empty()){
        auto victim=tiles_.end();
        for(auto it=tiles_.begin();it!=tiles_.end();++it){
            if(!mandatory&&sameTarget&&usefulFallback(it->first,it->second,focus,level_.value_or(it->first.scale),scene_))continue;
            if(victim==tiles_.end()){victim=it;continue;}
            const bool a=intersects(grid_.bounds(it->first),visible),b=intersects(grid_.bounds(victim->first),visible);
            if((!a&&b)||(a==b&&(it->first.scale<victim->first.scale||
                (it->first.scale==victim->first.scale&&it->second.used<victim->second.used))))victim=it;
        }
        // Optional picture recording yields to already completed frontier work.
        // The canonical checkpoint path remains available at the same budget.
        if(victim==tiles_.end())break;
        const auto before=charged();const auto key=victim->first;tiles_.erase(victim);sources_.erase(key);
        ++work.evictions;work.coverageComplete=false;dirty_=true;
        work.retainedBytes=budget->used(resources::ResourceClass::kNavigationTile);if(charged()>=before)break;
    }
}
bool SkiaNavigationTiles::coverage(const foundation::WorldRect& visible,std::uint64_t scene,
    std::vector<Patch>& patches,const RetainedNavigationSource* retained,
    std::vector<foundation::WorldRect>* missing){
#if defined(AXIOM_ENABLE_FRAME_TIMING)
    DiagnosticTiming partitionTiming(work.partitionNanoseconds);
#endif
    struct Candidate {TileKey key;foundation::WorldRect bounds;double density;const RetainedNavigationSource* retained;};
    std::vector<Candidate> candidates;candidates.reserve(tiles_.size()+1);
    for(const auto& [key,tile]:tiles_)if(tile.image&&tile.checkedScene==scene)
        for(std::size_t i=0;i<tile.valid.count;++i)if(intersects(tile.valid.areas[i],visible))
            candidates.push_back({key,tile.valid.areas[i],std::ldexp(1.,key.scale),nullptr});
    // At most two qualified canonical planes. Each keeps its own density and
    // world bounds; the normal disjoint partition still picks sharper pixels.
    auto* source=retained;
    for(unsigned i=0;source&&i<2;++i,source=source->fallback)
        if(source->draw&&source->density>0&&source->bounds.isFiniteAndOrdered())
            candidates.push_back({{},source->bounds,source->density,source});
    std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){
        return a.density!=b.density?a.density>b.density:a.retained&&!b.retained;});
    std::vector<foundation::WorldRect> holes{visible};patches.clear();
    for(const auto& candidate:candidates){
        std::vector<foundation::WorldRect> next;
        for(const auto& region:holes){
            const foundation::WorldRect overlap{std::max(region.left,candidate.bounds.left),std::max(region.top,candidate.bounds.top),
                std::min(region.right,candidate.bounds.right),std::min(region.bottom,candidate.bounds.bottom)};
            if(overlap.right<=overlap.left||overlap.bottom<=overlap.top){next.push_back(region);continue;}
            patches.push_back({candidate.key,overlap,candidate.density,candidate.retained});
            if(!candidate.retained&&candidate.key.scale<level_.value_or(candidate.key.scale))++work.fallbackUses;
            const auto add=[&](float l,float t,float r,float b){if(r>l&&b>t)next.push_back({l,t,r,b});};
            add(region.left,region.top,overlap.left,region.bottom);add(overlap.right,region.top,region.right,region.bottom);
            add(overlap.left,region.top,overlap.right,overlap.top);add(overlap.left,overlap.bottom,overlap.right,region.bottom);
            // Derived display partition only. Denial holds the previous complete
            // presentation instead of allowing pathological fragment growth.
            if(patches.size()+next.size()>4096){patches.clear();if(missing)*missing={visible};return false;}
        }
        holes=std::move(next);if(holes.empty())break;
    }
    if(missing)*missing=holes;
    return holes.empty();
}
void SkiaNavigationTiles::compose(SkCanvas& canvas,std::span<const Patch> patches,
    const CanonicalViewportTransform& camera,const FrameState& frame){
    canvas.save();canvas.resetMatrix();canvas.clear(SK_ColorWHITE);
    double total=0,low=0,retainedArea=0;work.maximumUpscale=0;
    const double sx=frame.metrics.physicalWidth/frame.metrics.logicalWidth;
    const double sy=frame.metrics.physicalHeight/frame.metrics.logicalHeight;
    for(const auto& patch:patches){
        const double area=double(patch.area.right-patch.area.left)*(patch.area.bottom-patch.area.top);
        const double upscale=camera.scale*std::max(sx,sy)/patch.density;
        total+=area;if(upscale>1.25)low+=area;if(patch.retained)retainedArea+=area;
        work.maximumUpscale=std::max(work.maximumUpscale,upscale);
        if(patch.retained){patch.retained->draw(canvas,patch.area);continue;}
        auto& tile=tiles_.at(patch.source);(void)tile.lease.markGpuUse();tile.used=tick_;const auto b=grid_.bounds(patch.source);
        const auto sourceScale=std::ldexp(1.,patch.source.scale);const auto& a=patch.area;
        const auto dst=SkRect::MakeLTRB(float((a.left*camera.scale+camera.translationX)*sx),
            float((a.top*camera.scale+camera.translationY)*sy),float((a.right*camera.scale+camera.translationX)*sx),
            float((a.bottom*camera.scale+camera.translationY)*sy));
        const auto src=SkRect::MakeLTRB(float((a.left-b.left)*sourceScale+gutter),float((a.top-b.top)*sourceScale+gutter),
            float((a.right-b.left)*sourceScale+gutter),float((a.bottom-b.top)*sourceScale+gutter));
        canvas.drawImageRect(tile.image,src,dst,SkSamplingOptions(SkFilterMode::kLinear),nullptr,SkCanvas::kFast_SrcRectConstraint);
    }
    work.lowResolutionFraction=total?low/total:0;work.retainedFraction=total?retainedArea/total:0;
    canvas.restore();
}
BackendSubmissionResult SkiaNavigationTiles::render(SkiaSurfaceProvider& provider,
    const CanonicalViewportTransform& camera,const FrameState& frame,const NavigationSourceQuery& query,
    std::uint64_t resourceEpoch,const RenderCacheOptions& options,
    std::shared_ptr<resources::ResourceBudgetManager> budget,bool allowPrewarm,const RetainedNavigationSource* retainedSource, SkiaTextResources* textResources, ImageResourceResolver* images,
    const std::function<void(SkCanvas&)>& drawOverlay,std::shared_ptr<SourceRetentionPool> sourcePool,std::shared_ptr<GpuResourceRetirement> retirements){
    if(retirements)gpuRetirements_=std::move(retirements);
    if(!gpuRetirements_)gpuRetirements_=std::make_shared<GpuResourceRetirement>();
    gpuRetirements_->service();
    struct Usage {NavigationWork& w;std::shared_ptr<resources::ResourceBudgetManager> b;~Usage(){if(b){
        const auto u=b->snapshot(resources::ResourceClass::kNavigationTile);w.retainedBytes=u.used;w.stagingBytes=u.staging;w.pendingReleaseBytes=u.pendingRelease;}}} usage{work,budget};
    if(sourcePool)geometry_.setRetentionPool(std::move(sourcePool));
    const auto started=std::chrono::steady_clock::now();
    const auto workBefore=work;
    struct Timing {
        NavigationWork& work;const NavigationWork before;std::chrono::steady_clock::time_point started;
        const FrameState& frame;const CanonicalViewportTransform& camera;const RenderCacheOptions& options;
        PreparedGeometryCache& geometry;const std::shared_ptr<resources::ResourceBudgetManager>& budget;
        ~Timing(){
            work.preparedGeometryBytes=geometry.preparedBytes();
            work.sourcePayloadBytes=geometry.sourceStatistics().bytes;work.sourcePayloadHits=geometry.sourceStatistics().hits;
            const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count();
            work.opportunityNanoseconds+=elapsed;
            if(std::uint64_t(elapsed)>options.navigationMicroseconds*1000)++work.overBudget;
#if defined(AXIOM_ENABLE_FRAME_TIMING)
            std::fprintf(stderr,"AXIOM_NAVIGATION_TIMING {\"version\":5,\"start_ns\":\"%llu\",\"scene\":\"%llu\",\"surface\":\"%llu\",\"scale\":%.6f,\"level\":%d,\"elapsed_ns\":%llu,\"query_ns\":%llu,\"initialization_ns\":%llu,\"queries\":%llu,\"initialized\":%llu,\"knots\":%llu,\"cancelled\":%llu,\"discarded_knots\":%llu,\"throttled\":%llu,\"prewarm\":%llu,\"built\":%llu,\"fallback\":%llu,\"prefix_knots\":%llu,\"overlay_ns\":%llu,"
                "\"prepared_builds\":%llu,\"prepared_hits\":%llu,\"prepared_denied\":%llu,\"prepared_bytes\":%llu,\"contour_tested\":%llu,\"contour_drawn\":%llu,\"vector_prepared\":%llu,\"gpu_contours\":%llu,\"coarse_masks\":%llu,\"alpha_masks\":%llu,\"region_compactions\":%llu,\"reclaim_skips_total\":%llu,\"tile_evictions\":%llu,\"partition_ns\":%llu,\"contour_ns\":%llu,\"material_ns\":%llu,\"other_draw_ns\":%llu,\"presented\":%s,\"coverage\":%s,\"maximum_upscale\":%.6f,\"low_resolution_fraction\":%.6f,\"retained_fraction\":%.6f,\"suspended\":%llu,\"resumed\":%llu,\"chunk_candidates\":%llu,\"index_nodes\":%llu,\"index_fallbacks\":%llu,\"source_copy_bytes\":%llu,\"source_aliases\":%llu,\"source_queries_observed\":%llu,\"source_bytes\":%llu,\"source_hits\":%llu,\"budget_available\":%s,\"budget_used_bytes\":%llu,\"budget_recorded_bytes\":%llu,\"navigation_tile_bytes\":%llu,\"navigation_staging_bytes\":%llu,\"navigation_pending_release_bytes\":%llu}\n",
                static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(started.time_since_epoch()).count()),
                static_cast<unsigned long long>(frame.sceneReadToken.value()),static_cast<unsigned long long>(frame.surfaceGeneration.value()),
                double(camera.scale),work.selectedLevel,static_cast<unsigned long long>(elapsed),
                static_cast<unsigned long long>(work.queryNanoseconds-before.queryNanoseconds),static_cast<unsigned long long>(work.initializationNanoseconds-before.initializationNanoseconds),
                static_cast<unsigned long long>(work.queries-before.queries),static_cast<unsigned long long>(work.initializations-before.initializations),
                static_cast<unsigned long long>(work.knotSteps-before.knotSteps),static_cast<unsigned long long>(work.cancelledJobs-before.cancelledJobs),
                static_cast<unsigned long long>(work.discardedKnotSteps-before.discardedKnotSteps),static_cast<unsigned long long>(work.gpuThrottles-before.gpuThrottles),
                static_cast<unsigned long long>(work.prewarmTiles-before.prewarmTiles),
                static_cast<unsigned long long>(work.tilesBuilt-before.tilesBuilt),
                static_cast<unsigned long long>(work.fallbackUses-before.fallbackUses),
                static_cast<unsigned long long>(work.prefixScanKnots-before.prefixScanKnots),
                static_cast<unsigned long long>(work.overlayNanoseconds-before.overlayNanoseconds),
                static_cast<unsigned long long>(work.preparedGeometryBuilds-before.preparedGeometryBuilds),
                static_cast<unsigned long long>(work.preparedGeometryHits-before.preparedGeometryHits),
                static_cast<unsigned long long>(work.preparedGeometryDenied-before.preparedGeometryDenied),
                static_cast<unsigned long long>(work.preparedGeometryBytes),
                static_cast<unsigned long long>(work.contourPrimitivesTested-before.contourPrimitivesTested),
                static_cast<unsigned long long>(work.contourPrimitivesDrawn-before.contourPrimitivesDrawn),
                static_cast<unsigned long long>(work.vectorPreparedUses-before.vectorPreparedUses),
                static_cast<unsigned long long>(work.gpuContourPrimitives-before.gpuContourPrimitives),
                static_cast<unsigned long long>(work.coarseMaskStarts-before.coarseMaskStarts),
                static_cast<unsigned long long>(work.alphaMaskStarts-before.alphaMaskStarts),
                static_cast<unsigned long long>(work.regionCompactions-before.regionCompactions),
                static_cast<unsigned long long>(work.skippedReclaims),
                static_cast<unsigned long long>(work.evictions-before.evictions),
                static_cast<unsigned long long>(work.partitionNanoseconds-before.partitionNanoseconds),
                static_cast<unsigned long long>(work.contourNanoseconds-before.contourNanoseconds),
                static_cast<unsigned long long>(work.materialNanoseconds-before.materialNanoseconds),
                static_cast<unsigned long long>(work.otherDrawNanoseconds-before.otherDrawNanoseconds),
                work.presented?"true":"false",work.coverageComplete?"true":"false",
                work.maximumUpscale,work.lowResolutionFraction,work.retainedFraction,
                static_cast<unsigned long long>(work.suspendedRefinements-before.suspendedRefinements),
                static_cast<unsigned long long>(work.resumedRefinements-before.resumedRefinements),
                static_cast<unsigned long long>(work.chunkCandidates-before.chunkCandidates),static_cast<unsigned long long>(work.indexNodes-before.indexNodes),
                static_cast<unsigned long long>(work.indexFallbacks-before.indexFallbacks),static_cast<unsigned long long>(work.payloadCopyBytes-before.payloadCopyBytes),
                static_cast<unsigned long long>(work.payloadAliases-before.payloadAliases),static_cast<unsigned long long>(work.observedSourceQueries-before.observedSourceQueries),
                static_cast<unsigned long long>(work.sourcePayloadBytes),static_cast<unsigned long long>(work.sourcePayloadHits-before.sourcePayloadHits),
                budget?"true":"false",static_cast<unsigned long long>(budget?budget->used():0),
                static_cast<unsigned long long>(budget?budget->used(resources::ResourceClass::kRecorded):0),
                static_cast<unsigned long long>(budget?budget->used(resources::ResourceClass::kNavigationTile):0),
                static_cast<unsigned long long>(budget?budget->snapshot(resources::ResourceClass::kNavigationTile).staging:0),
                static_cast<unsigned long long>(budget?budget->snapshot(resources::ResourceClass::kNavigationTile).pendingRelease:0));
#endif
        }
    } timing{work,workBefore,started,frame,camera,options,geometry_,budget};
    work.presented=false;work.pending=false;work.coverageComplete=false;
    if(!query||!options.enabled||!budget||!std::isfinite(camera.scale)||camera.scale<=0||
       !std::isfinite(camera.translationX)||!std::isfinite(camera.translationY)||
       frame.metrics.logicalWidth<=0||frame.metrics.logicalHeight<=0)
        return BackendSubmissionResult::retry("world navigation ineligible");
    if(budgetOwner_.lock()!=budget||provider_!=&provider||surface_!=frame.surfaceGeneration.value()||resource_!=resourceEpoch||
       (last_&&(last_->viewId!=frame.viewId||last_->metrics!=frame.metrics||last_->metricsGeneration!=frame.metricsGeneration))){
        cancel();tiles_.clear();sources_.clear();geometry_.clear();work.retainedBytes=0;last_.reset();fallbackFocus_.reset();dirty_=true;level_.reset();
        budgetOwner_=budget;provider_=&provider;surface_=frame.surfaceGeneration.value();resource_=resourceEpoch;
    }
    const auto scene=frame.sceneReadToken.value();
    const bool demandChanged=!last_||scene_!=scene||camera_.scale!=camera.scale||
        camera_.translationX!=camera.translationX||camera_.translationY!=camera.translationY;
    if(demandChanged){
        if(scene_!=scene)cancel();else if(job_&&job_->prewarm)cancelJob(job_);
        dirty_=true;warmed_=0;
    }
    const double scale=camera.scale*frame.metrics.physicalWidth/frame.metrics.logicalWidth;
    if(!std::isfinite(scale)||scale<1./65536||scale>65536)return BackendSubmissionResult::retry("world navigation scale range");
    int level=std::clamp(int(std::ceil(std::log2(scale))),-16,16);
    if(level_&&scale>=std::ldexp(.85,*level_-1)&&scale<=std::ldexp(1.15,*level_))level=*level_;
    const foundation::WorldRect visible{-camera.translationX/camera.scale,-camera.translationY/camera.scale,
        (frame.metrics.logicalWidth-camera.translationX)/camera.scale,(frame.metrics.logicalHeight-camera.translationY)/camera.scale};
    const auto focus=retainedSource&&retainedSource->draw&&retainedSource->bounds.isFiniteAndOrdered()
        ?retainedSource->bounds:visible;
    std::optional<foundation::WorldRect> previousVisible;
    if(last_&&scene_==scene&&camera_.scale>0)
        previousVisible=foundation::WorldRect{-camera_.translationX/camera_.scale,-camera_.translationY/camera_.scale,
            (frame.metrics.logicalWidth-camera_.translationX)/camera_.scale,(frame.metrics.logicalHeight-camera_.translationY)/camera_.scale};
    else if(retainedSource&&retainedSource->draw&&retainedSource->bounds.isFiniteAndOrdered())previousVisible=retainedSource->bounds;
    // Completed tiles retain one image; only an active job needs two extra
    // staging buffers. Reserve that next-job peak here,then let fits()/the
    // shared allocator enforce actual overlapping GPU lifetimes and yields.
    const auto transientBytes=2*tileBytes;
    const auto maximumTiles=std::min(options.maxEntries,options.maxBytes>transientBytes
        ?(options.maxBytes-transientBytes)/(tileBytes+tileMetadataBytes):std::size_t(0));
    // Choose coarse density from viewport extent, not its grid alignment.
    // A cell at least as wide/tall as the viewport needs at most2x2 cells at
    // every origin. Counting only the current cells toggled coarse levels on
    // diagonal pans, cancelling/rebuilding critical coverage while input moved.
    const auto coarseKeys=[&](int detail){
        int coarse=std::max(-16,detail-1);auto cover=grid_.coverage(visible,coarse,256);
        const auto span=std::max(visible.right-visible.left,visible.bottom-visible.top);
        while(coarse>-16&&(std::ldexp(256.,-coarse)<span||cover.empty()||cover.size()>4)){
            --coarse;cover=grid_.coverage(visible,coarse,256);
        }
        return cover;
    };
    auto keys=grid_.coverage(visible,level,256),parents=coarseKeys(level);
    while(level>-16&&(keys.empty()||parents.empty()||keys.size()+parents.size()>maximumTiles)){
        --level;keys=grid_.coverage(visible,level,256);parents=coarseKeys(level);
    }
    if(keys.empty()||parents.empty()||keys.size()+parents.size()>maximumTiles)return BackendSubmissionResult::retry("world navigation residency cannot converge");
    if(level_&&*level_!=level)++work.levelChanges;
    level_=level;work.selectedLevel=level;
    // Keep one world-tile ring around the visible demand.  This is residency
    // protection only: composition still uses `keys`, so overscan never
    // exposes pixels outside the requested viewport or qualifies a frame.
    const auto tileWorld = std::ldexp(256.0, -level);
    const auto overscan = expanded(visible, tileWorld);
    const auto overscanKeys = grid_.coverage(overscan, level, 256);
    const auto required=[&](TileKey key){
        return std::find(keys.begin(),keys.end(),key)!=keys.end() ||
            std::find(parents.begin(),parents.end(),key)!=parents.end() ||
            std::find(overscanKeys.begin(),overscanKeys.end(),key)!=overscanKeys.end();
    };
    if(job_&&!required(job_->key)&&(!job_->prewarm||demandChanged))cancelJob(job_);
    if(refinement_&&!required(refinement_->key))cancelJob(refinement_);
    auto acquired=provider.acquire();
    if(acquired.code!=SkiaSurfaceAcquireCode::kAcquired||!acquired.frame.surface)return BackendSubmissionResult::retry("world navigation target unavailable");
    struct Release {SkiaSurfaceProvider& provider;bool active=true;~Release(){if(active)provider.release();}} release{provider};
    const auto valid=[&]{return acquired.frame.generation==frame.surfaceGeneration.value()&&acquired.frame.surface->width()==int(frame.metrics.physicalWidth)&&acquired.frame.surface->height()==int(frame.metrics.physicalHeight);};
    if(!valid())return BackendSubmissionResult::rejected("stale world navigation target");
    auto* target=acquired.frame.surface->getCanvas();++tick_;
    auto* context=GrAsDirectContext(target->recordingContext());
    SkiaGpuLifetime gpu(*gpuRetirements_,budget,provider,context,resources::kPixelResourceClasses,
        resources::kPixelResourceClasses&~resources::resourceClassMask(resources::ResourceClass::kNavigationTile)&
        ~resources::resourceClassMask(resources::ResourceClass::kCanonicalFrame)&
        ~resources::resourceClassMask(resources::ResourceClass::kNavigation));
    if(!gpu.ready())return BackendSubmissionResult::retry("world navigation completion unavailable");
    RetainedNavigationSource retainedValue=retainedSource?*retainedSource:RetainedNavigationSource{};
    const RetainedNavigationSource* retained=retainedSource?&retainedValue:nullptr;
    std::vector<Patch> patches;std::vector<foundation::WorldRect> missing;
    work.coverageComplete=coverage(visible,scene,patches,retained,&missing);
    if(!work.coverageComplete&&retained&&retained->extend&&retained->extend()){
        retainedValue.bounds=visible;work.coverageComplete=coverage(visible,scene,patches,retained,&missing);
    }
    // Compose the highest-density valid sources in disjoint regions. A complete
    // coarse plane must never overwrite sharper valid pixels in the overlap.
    if(dirty_&&work.coverageComplete){
        gpu.touch();compose(*target,patches,camera,frame);
        if(drawOverlay)drawOverlay(*target);
        provider.release();release.active=false;
        const auto result=provider.present();if(result.code!=BackendSubmissionCode::kAccepted)return result;
        work.presented=true;dirty_=false;++work.feedbackFirst;
    }
    if(!release.active){
        acquired=provider.acquire();if(acquired.code!=SkiaSurfaceAcquireCode::kAcquired||!acquired.frame.surface)return BackendSubmissionResult::retry("navigation reacquire failed");
        release.active=true;if(!valid())return BackendSubmissionResult::rejected("stale navigation reacquire");target=acquired.frame.surface->getCanvas();
    }
    const auto ready=[&](TileKey key){auto it=tiles_.find(key);return it!=tiles_.end()&&it->second.image&&it->second.checkedScene==scene&&it->second.valid.full(grid_.bounds(key));};
    // A newly exposed frontier owns priority over disposable refinement.
    // Preserve coarse jobs across camera movement inside their world region;
    // GPU batches keep their existing bounded completion/backpressure policy.
    if(!work.coverageComplete&&job_&&job_->key.scale>parents.front().scale){
        if(!refinement_){refinement_=std::move(job_);++work.suspendedRefinements;}
        else cancelJob(job_);
    }
    // Before first complete coverage, admit several cheap cells within the
    // same wall-time allowance. Once feedback exists, keep setup to one unit.
    const auto queryLimit=work.presented?std::min(std::size_t(1),options.navigationMaxQueries):options.navigationMaxQueries;
    const auto initializationLimit=std::max(std::size_t(1),queryLimit);
    std::size_t steps=0,queries=0,initializations=0,gpuPrimitives=0;bool deferred=false;
    const auto gpuLimit=std::max(std::size_t(1),options.navigationMaxGpuContourPrimitives);
    const auto expired=[&]{return std::chrono::steady_clock::now()-started>=std::chrono::microseconds(options.navigationMicroseconds);};
    std::map<TileKey,std::shared_ptr<const Sources>> desired;
    auto load=[&](TileKey key)->bool{
        if(desired.contains(key))return true;
        if(const auto cached=sources_.find(key);cached!=sources_.end()&&cached->second->scene==scene){desired.emplace(key,cached->second);return true;}
        if(queries>=queryLimit||expired()){deferred=true;return false;}
        ++queries;++work.queries;const auto start=std::chrono::steady_clock::now();
        std::vector<ReferenceTraversalEntry> sources;NavigationSourceDiagnostics diagnostics;
        const bool ok=query(expanded(grid_.bounds(key),std::ldexp(double(gutter),-key.scale)),sources,diagnostics);
        work.chunkCandidates+=diagnostics.chunkCandidates;work.indexNodes+=diagnostics.indexNodes;work.indexFallbacks+=diagnostics.indexFallbacks;
        work.payloadCopyBytes+=diagnostics.payloadCopyBytes;work.payloadAliases+=diagnostics.payloadAliases;work.observedSourceQueries+=diagnostics.observed;
        work.queryNanoseconds+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
        if(!ok||std::any_of(sources.begin(),sources.end(),[](const auto& e){return !supported(e);}))return false;
        std::unordered_set<foundation::ObjectId,foundation::ObjectIdHash> ids;
        for(const auto& entry:sources)if(entry.record.objectId.isZero()||!ids.insert(entry.record.objectId).second)return false;
        ReferenceDrawList validation{.frame=frame,.entries={},.canonicalBytes={},.digest={}};
        validation.viewportClip={0,0,frame.metrics.logicalWidth,frame.metrics.logicalHeight};validation.entries=sources;
        if(validateReferencePlan(FramePlan{frame,std::move(validation)}).code!=BackendSubmissionCode::kAccepted)return false;
        std::vector<std::shared_ptr<const PreparedNavigationGeometry>> geometry;
        geometry.reserve(sources.size());
        SourceRetentions sourceLeases;
        for(const auto& entry:sources){
            auto retained=geometry_.retainSource(entry,budget,true);if(!retained){deferred=true;return false;}
            sourceLeases.insert(sourceLeases.end(),retained->begin(),retained->end());
            const auto before=geometry_.statistics();
            geometry.push_back(geometry_.get(entry,options,budget,!work.coverageComplete&&key.scale<level));
            const auto after=geometry_.statistics();
            work.preparedGeometryBuilds+=after.builds-before.builds;
            work.preparedGeometryHits+=after.hits-before.hits;
            work.preparedGeometryDenied+=after.denied-before.denied;
            if(after.builds>before.builds)
                if(const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command))
                    work.prefixScanKnots+=compact->content.get().stroke.knots.size();
        }
        std::size_t bytes=sizeof(Sources)+(sizeof(ReferenceTraversalEntry)+sizeof(std::shared_ptr<const PreparedNavigationGeometry>))*sources.size();
        for(const auto& entry:sources)bytes+=sourceMetadataBytes(entry);
        bytes+=sourceLeases.capacity()*sizeof(SourceRetentions::value_type);
        auto token=budget->reserve(resources::ResourceClass::kRecorded,bytes,true);
        for(auto it=sources_.begin();it!=sources_.end()&&!token;){
            if(it->second.use_count()==1){it=sources_.erase(it);token=budget->reserve(resources::ResourceClass::kRecorded,bytes,true);}else ++it;
        }
        if(!token){deferred=true;return false;}
        if(sources_.size()>=options.maxEntries)sources_.erase(sources_.begin());
        auto source=std::make_shared<const Sources>(Sources{scene,std::move(sources),std::move(geometry),resources::ResourceLease(budget,*token),std::move(sourceLeases),bytes});
        sources_.insert_or_assign(key,source);desired.emplace(key,std::move(source));return true;
    };
    // Revalidate only possible visible fallback cells, at the same query budget.
    for(auto it=tiles_.begin();it!=tiles_.end();){
        if(it->second.checkedScene==scene||!intersects(grid_.bounds(it->first),visible)){++it;continue;}
        if(!load(it->first)){if(deferred)break;return BackendSubmissionResult::retry("world navigation revalidation failed");}
        if(it->second.sources->entries!=desired.at(it->first)->entries){it=tiles_.erase(it);dirty_=true;}
        else{it->second.checkedScene=scene;++it;}
    }
    const bool gpuBlocked=context&&gpuRetirements_->preparationCount()>=2;
    if(gpuBlocked)++work.gpuThrottles;
    const auto protectedTile=[&](TileKey key){return
        std::find(keys.begin(),keys.end(),key)!=keys.end()||std::find(parents.begin(),parents.end(),key)!=parents.end()||
        std::any_of(patches.begin(),patches.end(),[&](const auto& p){return !p.retained&&p.source==key;});};
    const auto evict=[&](bool optional)->bool{
        auto victim=tiles_.end();
        const auto rank=[&](const auto& row){return usefulFallback(row.first,row.second,focus,level,scene)?2:
            std::find(overscanKeys.begin(),overscanKeys.end(),row.first)!=overscanKeys.end()?1:0;};
        for(auto it=tiles_.begin();it!=tiles_.end();++it)if(!protectedTile(it->first)&&(!optional||rank(*it)<2)&&
            (victim==tiles_.end()||rank(*it)<rank(*victim)||(rank(*it)==rank(*victim)&&it->second.used<victim->second.used)))victim=it;
        if(victim==tiles_.end())return false;
        tiles_.erase(victim);++work.evictions;return true;
    };
    const auto needsFine=[&](TileKey key){
        const auto bounds=grid_.bounds(key);const double density=std::ldexp(1.,key.scale);
        return std::any_of(missing.begin(),missing.end(),[&](const auto& area){return intersects(area,bounds);})||
            std::any_of(patches.begin(),patches.end(),[&](const auto& patch){return patch.density<density&&intersects(patch.area,bounds);});
    };
    while(!deferred&&!gpuBlocked&&gpuPrimitives<gpuLimit&&steps<std::max<std::size_t>(1,options.navigationMaxSteps)&&!expired()){
        if(!job_&&work.coverageComplete&&refinement_){job_=std::move(refinement_);++work.resumedRefinements;}
        if(!job_){
            std::optional<TileKey> next;bool prewarm=false;
            if(!work.coverageComplete)for(const auto key:parents)if(!ready(key)&&
                std::any_of(missing.begin(),missing.end(),[&](const auto& area){return intersects(grid_.bounds(key),area);})){next=key;break;}
            if(!next){
                // Service the largest visible resolution debt, not grid order.
                // A tiny edge sliver must not starve a mostly coarse viewport.
                double largest=0;
                for(const auto key:keys)if(!ready(key)&&needsFine(key)){
                    const auto bounds=grid_.bounds(key);double debt=0;
                    const auto area=[&](const foundation::WorldRect& r){return
                        std::max(0.,double(std::min(r.right,bounds.right)-std::max(r.left,bounds.left)))*
                        std::max(0.,double(std::min(r.bottom,bounds.bottom)-std::max(r.top,bounds.top)));};
                    for(const auto& r:missing)debt+=area(r);
                    for(const auto& patch:patches)if(patch.density<std::ldexp(1.,key.scale))debt+=area(patch.area);
                    if(debt>largest){largest=debt;next=key;}
                }
            }
            if(!next&&!refinement_&&allowPrewarm&&warmed_<options.navigationPrewarmTiles){
                // One-cell overscan and one adjacent finer level, bounded total.
                std::vector<TileKey> near;
                if(level<16){const float cx=(visible.left+visible.right)*.5F,cy=(visible.top+visible.bottom)*.5F;
                    auto center=grid_.coverage({cx,cy,cx+1,cy+1},level+1,4);near.insert(near.end(),center.begin(),center.end());}
                for(const auto key:keys)for(const auto& d:std::vector<std::pair<int,int>>{{-1,0},{1,0},{0,-1},{0,1}}){
                    TileKey candidate{key.x+d.first,key.y+d.second,key.scale};if(!required(candidate))near.push_back(candidate);}
                for(const auto key:near)if(!ready(key)){next=key;prewarm=true;break;}
            }
            if(!next)break;
            if(initializations>=initializationLimit){deferred=true;break;}
            if(!load(*next)){if(deferred)break;return BackendSubmissionResult::retry("world navigation unsupported sources");}
            if(expired()){deferred=true;break;}
            if(tileBytes>options.maxEntryBytes||(tileBytes*3+tileMetadataBytes)>options.maxBytes)return BackendSubmissionResult::retry("world navigation tile budget");
            const auto peak=tileBytes*3+tileMetadataBytes;
            // Keep room for one replacement peak during optional prewarm.
            const auto navLimit=prewarm&&options.maxBytes>=peak?options.maxBytes-peak:options.maxBytes;
            const auto fits=[&]{const auto used=budget->used(resources::ResourceClass::kNavigationTile);
                return peak<=navLimit&&used<=navLimit-peak&&tiles_.size()<options.maxEntries;};
            while(!fits()){
                const auto before=budget->used();if(!evict(prewarm))break;
                // An unfinished image remains charged after logical eviction.
                // Yield for completion instead of cascading through the cache.
                if(budget->used()>=before)break;
            }
            if(!fits()){deferred=true;break;}
            auto token=budget->reserve(resources::ResourceClass::kNavigationTile,peak,true,tileBytes+tileMetadataBytes);
            while(!token){const auto before=budget->used();if(!evict(prewarm)||budget->used()>=before)break;
                token=budget->reserve(resources::ResourceClass::kNavigationTile,peak,true,tileBytes+tileMetadataBytes);}
            if(!token){deferred=true;break;}
            const auto initStart=std::chrono::steady_clock::now();
            auto job=std::make_unique<Job>();job->key=*next;job->sources=desired.at(*next);job->prewarm=prewarm;
            job->lease=resources::ResourceLease(budget,*token);
            gpu.touch(true);job->surface=target->makeSurface(acquired.frame.surface->imageInfo().makeWH(extent,extent));
            // Coarse cells collect whole strokes, including many overlapping
            // Lighten ribbons. Rasterize this bounded coverage mask in Skia
            // CPU memory and upload the completed mask; otherwise a small CPU
            // opportunity can enqueue enough GPU path work to stall RAF itself.
            // The material, scene tile and final presentation stay on the GPU.
            const auto coverageInfo=SkImageInfo::MakeN32Premul(extent,extent);
            job->cpuCoverage=next->scale<level&&std::all_of(job->sources->entries.begin(),job->sources->entries.end(),[](const auto& entry){
                const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command);
                return !compact||compact->content.get().stroke.config.algorithmVersion==4;
            });
            // Reuse canonical's qualified single-channel encoding for v4
            // white CPU masks. Geometry/order/grid stay shared and unchanged;
            // unsupported materials/targets retain the original N32 fallback.
            const bool alpha=job->cpuCoverage&&std::all_of(job->sources->entries.begin(),job->sources->entries.end(),[](const auto& entry){
                const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command);
                return !compact||compact->content.get().stroke.config.materialVersion<3;
            });
            if(alpha)job->coverage=SkSurfaces::Raster(SkImageInfo::MakeA8(extent,extent));
            if(!job->coverage)job->coverage=job->cpuCoverage?SkSurfaces::Raster(coverageInfo):target->makeSurface(coverageInfo);
            if(!job->surface||!job->coverage){deferred=true;break;}
            job->surface->getCanvas()->clear(SK_ColorTRANSPARENT);
            const auto b=grid_.bounds(*next);const float s=std::ldexp(1.F,next->scale);
            auto demand=b;
            const auto old=tiles_.find(*next);
            const bool reusable=old!=tiles_.end()&&old->second.checkedScene==scene;
            // Coverage is needed only in the exposed frontier. Do not make a
            // narrow strip wait for every contour in a whole coarse cell.
            // Publish explicit valid regions, one missing strip at a time.
            // Unfinished pixels never enter coverage().
            if(!work.coverageComplete&&next->scale<level&&!job->sources->entries.empty()){
                for(const auto& hole:missing)if(intersects(hole,b)){
                    auto r=foundation::WorldRect{std::max(b.left,hole.left),std::max(b.top,hole.top),
                        std::min(b.right,hole.right),std::min(b.bottom,hole.bottom)};
                    // Snap outward to this cell's texel grid; adjacent updates
                    // preserve the same absolute raster/sample phase.
                    r={std::max(b.left,b.left+std::floor((r.left-b.left)*s)/s),
                       std::max(b.top,b.top+std::floor((r.top-b.top)*s)/s),
                       std::min(b.right,b.left+std::ceil((r.right-b.left)*s)/s),
                       std::min(b.bottom,b.top+std::ceil((r.bottom-b.top)*s)/s)};
                    // A strip ending exactly at the latest camera is already
                    // obsolete when its bounded job completes. Keep a small
                    // lead at every exposed viewport edge, within the admitted
                    // cell. This amortizes source setup across successive
                    // camera updates without making a whole cell critical.
                    // Fixed transverse prefetch made a tiny visible strip
                    // depend on dense offscreen ink. Lead the short axis by
                    // observed demand movement; preserve the long-axis guard
                    // so successive strips still merge without L-shaped holes.
                    const auto speculative=foundation::WorldRect{
                        r.left<=visible.left?std::max(b.left,r.left-64/s):r.left,
                        r.top<=visible.top?std::max(b.top,r.top-64/s):r.top,
                        r.right>=visible.right?std::min(b.right,r.right+64/s):r.right,
                        r.bottom>=visible.bottom?std::min(b.bottom,r.bottom+64/s):r.bottom};
                    const auto deviceDemand=[&](foundation::WorldRect a){return SkRect::MakeLTRB(
                        gutter+(a.left-b.left)*s-gutter,gutter+(a.top-b.top)*s-gutter,
                        gutter+(a.right-b.left)*s+gutter,gutter+(a.bottom-b.top)*s+gutter);};
                    const auto actualClip=deviceDemand(r),speculativeClip=deviceDemand(speculative);
                    bool extraDependency=false;
                    auto* probe=job->coverage->getCanvas();
                    for(std::size_t i=0;i<job->sources->entries.size();++i){
                        const auto& geometry=job->sources->geometry[i];
                        if(!geometry||!geometry->isContour())continue;
                        probe->save();probe->translate(gutter-b.left*s,gutter-b.top*s);probe->scale(s,s);
                        const auto& t=job->sources->entries[i].record.transform;
                        probe->concat(SkMatrix::MakeAll(float(t.a),float(t.c),float(t.tx),float(t.b),float(t.d),float(t.ty),0,0,1));
                        extraDependency=geometry->nextVisiblePrimitive(*probe,0,&actualClip)==geometry->primitiveCount()&&
                            geometry->nextVisiblePrimitive(*probe,0,&speculativeClip)<geometry->primitiveCount();
                        probe->restore();if(extraDependency)break;
                    }
                    const bool vertical=r.right-r.left<r.bottom-r.top;
                    const auto lead=[&](float edge,float previous,bool transverse){
                        if(!extraDependency||!transverse||!previousVisible)return 64.F/s;
                        return float(std::clamp(2.*std::abs(double(edge)-previous),1./s,64./s));
                    };
                    const auto prior=previousVisible.value_or(visible);
                    if(r.left<=visible.left)r.left=std::max(b.left,r.left-lead(visible.left,prior.left,vertical));
                    if(r.right>=visible.right)r.right=std::min(b.right,r.right+lead(visible.right,prior.right,vertical));
                    if(r.top<=visible.top)r.top=std::max(b.top,r.top-lead(visible.top,prior.top,!vertical));
                    if(r.bottom>=visible.bottom)r.bottom=std::min(b.bottom,r.bottom+lead(visible.bottom,prior.bottom,!vertical));
                    demand=r;break;
                }
            }
            if(reusable){(void)old->second.lease.markGpuUse();job->valid=old->second.valid;}
            if(!job->valid.add(demand)){
                ++work.regionCompactions;
                auto repaint=demand;
                for(std::size_t i=0;i<job->valid.count;++i){const auto& area=job->valid.areas[i];
                    repaint={std::min(repaint.left,area.left),std::min(repaint.top,area.top),
                        std::max(repaint.right,area.right),std::max(repaint.bottom,area.bottom)};
                }
#if defined(AXIOM_ENABLE_FRAME_TIMING)
                std::fprintf(stderr,"AXIOM_NAVIGATION_REGION_COMPACT {\"x\":%lld,\"y\":%lld,\"scale\":%d,\"regions\":%zu,\"left\":%.3f,\"top\":%.3f,\"right\":%.3f,\"bottom\":%.3f}\n",
                    static_cast<long long>(next->x),static_cast<long long>(next->y),next->scale,job->valid.count,
                    double(repaint.left),double(repaint.top),double(repaint.right),double(repaint.bottom));
#endif
                // The rectangle includes unknown inside corners: repaint all
                // sources there before publishing validity. Metadata exhaustion
                // must not make this strip depend on the rest of the cell.
                job->valid.reset(repaint);demand=repaint;
            }
            const auto deviceRect=[&](foundation::WorldRect r){return SkRect::MakeLTRB(
                gutter+(r.left-b.left)*s-gutter,gutter+(r.top-b.top)*s-gutter,
                gutter+(r.right-b.left)*s+gutter,gutter+(r.bottom-b.top)*s+gutter);};
            if(reusable)job->surface->getCanvas()->drawImage(old->second.image,0,0);
            auto* material=job->surface->getCanvas();
            material->clipRect(deviceRect(demand));
            // Repaint the complete new strip, including its sampling gutter.
            // Src clear prevents double application over old gutter pixels.
            // A rectangular scissor also avoids complex difference-clip GPU
            // programs on the critical preparation path.
            material->clear(SK_ColorTRANSPARENT);
            job->surface->getCanvas()->translate(gutter-b.left*s,gutter-b.top*s);job->surface->getCanvas()->scale(s,s);
            job->id=++serial_;
            if(!scheduler_.submit({job->id,1,prewarm?scheduling::WorkPriority::kPrefetch:scheduling::WorkPriority::kVisible,0,{},(tileBytes*3+tileMetadataBytes)})||!scheduler_.take())return BackendSubmissionResult::retry("world navigation admission failed");
            job_=std::move(job);++initializations;++work.initializations;
            work.initializationNanoseconds+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-initStart).count();

            if(expired()){deferred=true;break;}
        }
        auto& job=*job_;(void)job.lease.markGpuUse();
        if(job.source>=job.sources->entries.size()){
            // A fixed ticket can retire at most eight publications. Defer
            // additional cells; never allocate unbounded callback payloads.
            gpu.touch(true);auto image=job.surface->makeImageSnapshot();if(!image)return BackendSubmissionResult::retry("world navigation snapshot failed");
            if(!gpu.publish(job.lease,tileBytes+tileMetadataBytes)){deferred=true;break;}
            (void)scheduler_.finish(job.id,1);
            if(job.prewarm){++warmed_;++work.prewarmTiles;}
            tiles_.insert_or_assign(job.key,Tile{std::move(job.sources),std::move(image),tick_,scene,std::move(job.lease),job.valid});
            job_.reset();dirty_=true;++work.tilesBuilt;++steps;
            // Partial publication changes the missing frontier even when the
            // cell is not full. Do not select the same completed strip again
            // for the rest of this opportunity.
            patches.clear();work.coverageComplete=coverage(visible,scene,patches,retained,&missing);
            continue;
        }
    const auto& entry=job.sources->entries[job.source];
        if(!job.cpuCoverage||!std::holds_alternative<CompactStrokeReferenceCommand>(entry.command))gpu.touch(true);
        if(const auto* c=std::get_if<CompactStrokeReferenceCommand>(&entry.command)){
#if defined(AXIOM_ENABLE_FRAME_TIMING)
            DiagnosticTiming contourTiming(work.contourNanoseconds);
#endif
            const auto& stroke=c->content.get().stroke;auto* draw=job.coverage->getCanvas();
            if(!job.sourceInitialized){
                if(job.cpuCoverage)++work.coarseMaskStarts;
                if(job.coverage->imageInfo().colorType()==kAlpha_8_SkColorType)++work.alphaMaskStarts;
                const auto range = compactRange(entry);
                if (range.second <= range.first) { ++job.source; continue; }
                job.knot = range.first;
                draw->resetMatrix();draw->clear(job.coverage->imageInfo().colorType()==kAlpha_8_SkColorType?SK_ColorTRANSPARENT:SK_ColorBLACK);draw->setMatrix(job.surface->getCanvas()->getTotalMatrix());
                const auto& t=entry.record.transform;
                draw->concat(SkMatrix::MakeAll(float(t.a),float(t.c),float(t.tx),float(t.b),float(t.d),float(t.ty),0,0,1));
                // The range renderer backs up one shared neighbor before
                // drawing a chunk, so the distance is the length through the
                // knot immediately before the requested span.
                job.geometry=job.sources->geometry[job.source];
                job.primitive=0;
                const auto distances=job.geometry?job.geometry->distances():std::span<const double>{};
                job.distance = job.knot < distances.size()
                    ? distances[job.knot] : ink::compactLength(std::span<const foundation::InkKnot>(
                        stroke.knots.data(), job.knot));
                job.totalDistance = job.geometry?job.geometry->totalDistance():ink::compactLength(stroke.knots);
                if(!job.geometry)work.prefixScanKnots+=stroke.knots.size();
                if (stroke.config.algorithmVersion==4 && job.geometry&&job.geometry->isContour()) {
                    job.contour.reset();
                } else if(stroke.config.algorithmVersion==4) {
                    job.contour.emplace(stroke.config);
                } else job.contour.reset();
                job.sourceInitialized = true;
            }
            const auto range = compactRange(entry);
            std::size_t end=std::min(job.knot+32,range.second);
            if(stroke.config.algorithmVersion==4 && job.geometry&&job.geometry->isContour()){
                const bool gpuCoverage=context&&!job.cpuCoverage;
                const auto count=gpuCoverage?std::min(std::size_t(32),gpuLimit-gpuPrimitives):std::size_t(32);
                const auto demand=SkRect::Make(job.surface->getCanvas()->getDeviceClipBounds());
                job.primitive=job.geometry->nextVisiblePrimitive(*draw,job.primitive,&demand);
                const auto drawn=job.geometry->drawContour(*draw,job.primitive,count,&demand);
                work.contourPrimitivesTested+=drawn.primitivesTested;
                work.contourPrimitivesDrawn+=drawn.primitivesDrawn;
                if(gpuCoverage){work.gpuContourPrimitives+=drawn.primitivesDrawn;gpuPrimitives+=drawn.primitivesDrawn;}
                job.primitive=std::min(job.primitive+count,job.geometry->primitiveCount());
                if(job.primitive<job.geometry->primitiveCount()){++steps;continue;}
                end = range.second;
            } else if(job.contour){
                ink::CompactContour contours;job.contour->append({stroke.knots.data()+job.knot,end-job.knot},job.totalDistance,contours);
                if(end==stroke.knots.size())job.contour->finish(job.totalDistance,contours);
                drawCompactContourCoverageToSkCanvas(*draw,stroke.config,contours);
            }else drawCompactCoverageRangeToSkCanvas(*draw,stroke.config,stroke.knots,job.knot,end,job.distance,job.totalDistance,stroke.seed);
            const auto distances=job.geometry?job.geometry->distances():std::span<const double>{};
            if(stroke.config.algorithmVersion==4&&job.geometry&&job.geometry->isContour())job.distance=job.totalDistance;
            else if(end<distances.size())job.distance=distances[end];
            else for(auto i=std::max(std::size_t(1),job.knot);i<end;++i)
                job.distance+=std::hypot(stroke.knots[i].x-stroke.knots[i-1].x,
                                         stroke.knots[i].y-stroke.knots[i-1].y);
            work.knotSteps+=end-job.knot;job.steps+=end-job.knot;job.knot=end;++steps;
            if(job.knot<range.second)continue;
#if defined(AXIOM_ENABLE_FRAME_TIMING)
            contourTiming.finish();DiagnosticTiming materialTiming(work.materialNanoseconds);
#endif
            gpu.touch(true);auto* material=job.surface->getCanvas();material->save();
            const auto& t=entry.record.transform;
            material->concat(SkMatrix::MakeAll(float(t.a),float(t.c),float(t.tx),float(t.b),float(t.d),float(t.ty),0,0,1));
            // Erase this stroke's isolated coverage before material composition.
            // Clearing the accumulated tile would also remove shapes/images
            // already underneath the stroke and expose the white tile backdrop.
            if (!entry.record.eraseMasks.empty())
                drawEraseMasksToSkCanvas(*draw, entry);
            compositeCompactCoverageToSkCanvas(*material,*job.coverage->makeImageSnapshot(),stroke.config,stroke.seed);
            material->restore();
        }else{
#if defined(AXIOM_ENABLE_FRAME_TIMING)
            DiagnosticTiming otherTiming(work.otherDrawNanoseconds);
#endif
            const auto& geometry=job.sources->geometry[job.source];
            const auto* path=geometry?geometry->vectorPath():nullptr;
            drawNavigationEntry(*job.surface->getCanvas(),entry,textResources,images,path);
            if(path)++work.vectorPreparedUses;
            ++steps;
        }
        ++job.source;job.knot=0;job.sourceInitialized=false;job.geometry.reset();++work.sourceSteps;
    }
    if(deferred||expired())++work.budgetYields;
    // Cold start may acquire complete coverage in this opportunity. At most
    // one feedback present per call; new refinements wait for the next frame.
    patches.clear();work.coverageComplete=coverage(visible,scene,patches,retained,&missing);
    work.pending=bool(job_)||bool(refinement_)||!work.coverageComplete;
    for(const auto key:keys)work.pending|=!ready(key)&&needsFine(key);
    if(dirty_&&work.coverageComplete&&!work.presented){
        gpu.touch();compose(*target,patches,camera,frame);
        if(drawOverlay)drawOverlay(*target);
        provider.release();release.active=false;
        const auto result=provider.present();if(result.code!=BackendSubmissionCode::kAccepted)return result;
        work.presented=true;dirty_=false;
    }
    // A ready refinement produced after the early feedback present still
    // needs its own owner opportunity. Do not report convergence before that
    // sharper plane has actually reached present.
    work.pending|=dirty_;
    last_=std::make_unique<FrameState>(frame);fallbackFocus_=focus;camera_=camera;scene_=scene;
    return BackendSubmissionResult::accepted();
}
}
