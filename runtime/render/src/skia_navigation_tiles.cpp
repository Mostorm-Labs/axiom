#include "continuous_zoom_observation.hpp"
#include "navigation_sharing_observation.hpp"
#include "reference_validation_cache.hpp"
#include "canvas/foundation/internal/g5_observation.hpp"
namespace g5 = canvas::foundation::internal::g5;
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
void SkiaNavigationTiles::cancelJob(std::unique_ptr<Job>& job){
    NS_IF(++ns::state.row->transitions);
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
    add(work.retainedBytes);if(geometry_){add(geometry_->preparedBytes());add(geometry_->sourceStatistics().bytes);}
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
    NS_SCOPE(nsPartition,Partition);
    struct Candidate {TileKey key;foundation::WorldRect bounds;double density;const RetainedNavigationSource* retained;bool plainBackground=false;};
    std::vector<Candidate> candidates;candidates.reserve(tiles_.size()+1);
    for(const auto& [key,tile]:tiles_)if((tile.image||tile.plainBackground)&&tile.checkedScene==scene)
        for(std::size_t i=0;i<tile.valid.count;++i)if(intersects(tile.valid.areas[i],visible))
            candidates.push_back({key,tile.valid.areas[i],std::ldexp(1.,key.scale),nullptr,tile.plainBackground});
    // At most two qualified canonical planes. Each keeps its own density and
    // world bounds; the normal disjoint partition still picks sharper pixels.
    auto* source=retained;
    for(unsigned i=0;source&&i<2;++i,source=source->fallback)
        if(source->draw&&source->density>0&&source->bounds.isFiniteAndOrdered()){
            if(source->regions.empty())candidates.push_back({{},source->bounds,source->density,source});
            else for(const auto& r:source->regions)if(intersects(r,visible))candidates.push_back({{},r,source->density,source});
        }
    std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){
        return a.plainBackground!=b.plainBackground?a.plainBackground:(a.density!=b.density?a.density>b.density:a.retained&&!b.retained);});
    std::vector<foundation::WorldRect> holes{visible};patches.clear();
    for(const auto& candidate:candidates){
        std::vector<foundation::WorldRect> next;
        for(const auto& region:holes){
            const foundation::WorldRect overlap{std::max(region.left,candidate.bounds.left),std::max(region.top,candidate.bounds.top),
                std::min(region.right,candidate.bounds.right),std::min(region.bottom,candidate.bounds.bottom)};
            if(overlap.right<=overlap.left||overlap.bottom<=overlap.top){next.push_back(region);continue;}
            patches.push_back({candidate.key,overlap,candidate.density,candidate.retained,candidate.plainBackground});
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
    NS_SCOPE(nsCompose,Compose);
    ZQ_BEGIN();canvas.save();canvas.resetMatrix();canvas.clear(background_);
    double total=0,low=0,retainedArea=0;work.maximumUpscale=0;
    const double sx=frame.metrics.physicalWidth/frame.metrics.logicalWidth;
    const double sy=frame.metrics.physicalHeight/frame.metrics.logicalHeight;
    for(const auto& patch:patches){
        const double area=double(patch.area.right-patch.area.left)*(patch.area.bottom-patch.area.top);
        if(patch.plainBackground){ZQ_PATCH(area,1.);total+=area;continue;}
        const double upscale=camera.scale*std::max(sx,sy)/patch.density;
        ZQ_PATCH(area,upscale);ZQ_CONTENT_PATCH(patch.area,upscale);total+=area;if(upscale>1.25)low+=area;if(patch.retained)retainedArea+=area;
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
    const std::function<void(SkCanvas&)>& drawOverlay,std::shared_ptr<SourceRetentionPool> sourcePool,std::shared_ptr<GpuResourceRetirement> retirements,std::shared_ptr<PreparedGeometryCache> prepared,std::uint32_t background,std::shared_ptr<ReferenceValidationCache> validation,bool opaquePlainBackground,std::shared_ptr<SkiaInkLayers> sharedBodies){
#if defined(AXIOM_NAV_SHARING_OBSERVATION)
    ns::begin(0);ns::End nsEnd;
    if(ns::state.row){auto&r=*ns::state.row;r.view=frame.viewId.value();r.semantic=frame.sceneGeneration.value();r.scene=frame.sceneReadToken.value();r.resource=resourceEpoch;r.surface=frame.surfaceGeneration.value();r.metrics=frame.metricsGeneration.value();r.camera=frame.camera.generation.value();r.frame=frame.frameId.value();r.cameraBits={std::bit_cast<std::uint32_t>(camera.scale),std::bit_cast<std::uint32_t>(camera.translationX),std::bit_cast<std::uint32_t>(camera.translationY)};r.dimensions={frame.metrics.physicalWidth,frame.metrics.physicalHeight};r.budgetNs=options.navigationMicroseconds*1000;r.beforeJobs={ns::job(job_.get()),ns::job(refinement_.get())};}
    const auto nsBefore=work;auto nsSummary=ns::Exit{[&]{if(auto*r=ns::state.row){const auto firstRole=r->afterJobs[0].role,secondRole=r->afterJobs[1].role;r->afterJobs={ns::job(job_.get()),ns::job(refinement_.get())};r->afterJobs[0].role=firstRole;r->afterJobs[1].role=secondRole;r->coverage=work.coverageComplete;r->presented=work.presented;r->built=work.tilesBuilt-nsBefore.tilesBuilt;r->sourceSteps=work.sourceSteps-nsBefore.sourceSteps;r->knotSteps=work.knotSteps-nsBefore.knotSteps;r->contourDrawn=work.contourPrimitivesDrawn-nsBefore.contourPrimitivesDrawn;r->cancelled=work.cancelledJobs-nsBefore.cancelledJobs;r->suspended=work.suspendedRefinements-nsBefore.suspendedRefinements;r->resumed=work.resumedRefinements-nsBefore.resumedRefinements;if(budget){r->used=budget->used();r->navUsed=budget->used(resources::ResourceClass::kNavigationTile);for(unsigned i=0;i<unsigned(resources::ResourceClass::kCount);++i){auto u=budget->snapshot(resources::ResourceClass(i));r->pending+=u.pendingRelease;r->staging+=u.staging;}}}}};
#endif
    validation_=std::move(validation);
    if(prepared)geometry_=std::move(prepared);
    // Standalone private callers retain their existing local service. Production
    // always receives the renderer-owned shared service.
    if(!geometry_)geometry_=std::make_shared<PreparedGeometryCache>();
    if(retirements)gpuRetirements_=std::move(retirements);
    if(!gpuRetirements_)gpuRetirements_=std::make_shared<GpuResourceRetirement>();
    {NS_SCOPE(nsService,Service);gpuRetirements_->service();}
    struct Usage {NavigationWork& w;std::shared_ptr<resources::ResourceBudgetManager> b;~Usage(){if(b){
        const auto u=b->snapshot(resources::ResourceClass::kNavigationTile);w.retainedBytes=u.used;w.stagingBytes=u.staging;w.pendingReleaseBytes=u.pendingRelease;}}} usage{work,budget};
    if(sourcePool)geometry_->setRetentionPool(std::move(sourcePool));
    const auto started=PC_NAV_NOW();
    NS_IF(ns::state.row->origin=std::chrono::duration_cast<std::chrono::nanoseconds>(started.time_since_epoch()).count());
    const auto workBefore=work;
    auto bodyStatistics=ns::Exit{[&]{const auto&w=bodyLayers_->navigationBodyWork();work.bodyCoverageHits=w.hits;work.bodyCoverageMisses=w.misses;work.bodyCoverageBuilds=w.builds;work.bodyCoverageBytes=w.bytes;work.bodyCoverageEntries=w.entries;}};
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
                "\"prepared_builds\":%llu,\"prepared_hits\":%llu,\"prepared_denied\":%llu,\"prepared_bytes\":%llu,\"contour_tested\":%llu,\"contour_drawn\":%llu,\"vector_prepared\":%llu,\"gpu_contours\":%llu,\"coarse_masks\":%llu,\"alpha_masks\":%llu,\"region_compactions\":%llu,\"reclaim_skips_total\":%llu,\"tile_evictions\":%llu,\"partition_ns\":%llu,\"contour_ns\":%llu,\"material_ns\":%llu,\"other_draw_ns\":%llu,\"presented\":%s,\"coverage\":%s,\"maximum_upscale\":%.6f,\"low_resolution_fraction\":%.6f,\"retained_fraction\":%.6f,\"suspended\":%llu,\"resumed\":%llu,\"chunk_candidates\":%llu,\"index_nodes\":%llu,\"index_fallbacks\":%llu,\"source_copy_bytes\":%llu,\"source_aliases\":%llu,\"source_queries_observed\":%llu,\"source_bytes\":%llu,\"source_hits\":%llu,\"budget_available\":%s,\"budget_used_bytes\":%llu,\"budget_recorded_bytes\":%llu,\"navigation_tile_bytes\":%llu,\"navigation_staging_bytes\":%llu,\"navigation_pending_release_bytes\":%llu,\"regional_plane_queries_total\":%llu,\"regional_plane_proofs_total\":%llu,\"regional_plane_rejects_total\":%llu,\"retained_valid_regions\":%zu,\"regional_tile_proofs\":%llu,\"regional_plane_body_rejects_total\":%llu,\"regional_plane_admission_rejects_total\":%llu}\n",
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
                static_cast<unsigned long long>(budget?budget->snapshot(resources::ResourceClass::kNavigationTile).pendingRelease:0),
                static_cast<unsigned long long>(work.regionalPlaneQueries),static_cast<unsigned long long>(work.regionalPlaneProofs),
                static_cast<unsigned long long>(work.regionalPlaneRejects),work.retainedValidRegions,
                static_cast<unsigned long long>(work.regionalTileProofs-before.regionalTileProofs),
                static_cast<unsigned long long>(work.regionalPlaneBodyRejects),static_cast<unsigned long long>(work.regionalPlaneAdmissionRejects));
#endif
        }
    } timing{work,workBefore,started,frame,camera,options,*geometry_,budget};
    const bool externalBodies=bool(sharedBodies);if(sharedBodies)bodyLayers_=std::move(sharedBodies);
    bodyLayers_->bindSurfaceBasis(&provider,resourceEpoch,frame.surfaceGeneration.value(),frame.viewId.value(),frame.metricsGeneration.value());
    work.presented=false;work.pending=false;work.coverageComplete=false;
    if(!query||!options.enabled||!budget||!std::isfinite(camera.scale)||camera.scale<=0||
       !std::isfinite(camera.translationX)||!std::isfinite(camera.translationY)||
       frame.metrics.logicalWidth<=0||frame.metrics.logicalHeight<=0)
        return ns::result(BackendSubmissionResult::retry("world navigation ineligible"));
    if(background_!=background||budgetOwner_.lock()!=budget||provider_!=&provider||surface_!=frame.surfaceGeneration.value()||resource_!=resourceEpoch||
       (last_&&(last_->viewId!=frame.viewId||last_->metrics!=frame.metrics||last_->metricsGeneration!=frame.metricsGeneration))){
        cancel();if(!externalBodies)bodyLayers_->clear();tiles_.clear();sources_.clear();qualityDemands_.clear();schedulingLease_.reset();schedulingAdmitted_=false;displayEstimateValid_=false;checkpointCosts_={};displayCosts_={};qualityAge_=0;work.retainedBytes=0;last_.reset();fallbackFocus_.reset();dirty_=true;level_.reset();
        background_=background;budgetOwner_=budget;provider_=&provider;surface_=frame.surfaceGeneration.value();resource_=resourceEpoch;
    }
    if(!schedulingAdmitted_){const auto token=budget->reserve(resources::ResourceClass::kRecorded,sizeof(checkpointCosts_)+sizeof(displayCosts_)+sizeof(streamingCursor_)+64*(sizeof(TileKey)+sizeof(NavigationRegions)+sizeof(double)*2+64)+256,true);if(!token)return ns::result(BackendSubmissionResult::retry("navigation scheduling metadata unavailable"));schedulingLease_=resources::ResourceLease(budget,*token);schedulingAdmitted_=true;}
    std::optional<ns::Scope> nsPlan;NS_IF(nsPlan.emplace(ns::Planning));
    opaquePlainBackground=opaquePlainBackground&&SkColorGetA(background)==255;
    if(!opaquePlainBackground||(last_&&last_->sceneGeneration!=frame.sceneGeneration)){
        for(auto it=tiles_.begin();it!=tiles_.end();){if(it->second.plainBackground){sources_.erase(it->first);it=tiles_.erase(it);dirty_=true;}else++it;}
        for(auto it=sources_.begin();it!=sources_.end();){if(it->second->emptyProof)it=sources_.erase(it);else++it;}
    }
    const auto scene=frame.sceneReadToken.value();
    const bool demandChanged=!last_||scene_!=scene||camera_.scale!=camera.scale||
        camera_.translationX!=camera.translationX||camera_.translationY!=camera.translationY;
    if(demandChanged){
        if(scene_!=scene){cancel();qualityDemands_.clear();qualityAge_=0;displayCosts_={};displayEstimateValid_=false;}else if(job_&&job_->prewarm)cancelJob(job_);
        dirty_=true;warmed_=0;
    }
    const double scale=camera.scale*std::max(double(frame.metrics.physicalWidth)/frame.metrics.logicalWidth,double(frame.metrics.physicalHeight)/frame.metrics.logicalHeight);
    if(!std::isfinite(scale)||scale<1./65536||scale>65536)return ns::result(BackendSubmissionResult::retry("world navigation scale range"));
    int level=std::clamp(int(std::ceil(std::log2(scale/1.25))),-16,16);
    // Desireddevicequality is not downgraded byresidentcount orhysteresis.
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
    const auto navigationCount=options.maxEntries<512?options.maxEntries:std::size_t(4096);
    const auto maximumTiles=std::min(navigationCount,options.maxBytes>transientBytes
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
    const auto demandLimit=std::size_t(4096);
    auto keys=grid_.coverageWindow(visible,level,std::min(maximumTiles,demandLimit),streamingCursor_[level+16]),parents=coarseKeys(level);
    // A target grid may stream, not allfitresidentwithbootstrap. Reject only
    // structurally unenumerable finite demand; preserve desireddensity.
    if(parents.empty()||maximumTiles<parents.size()+1)return ns::result(BackendSubmissionResult::retry("navigation target streaming capacity unavailable"));
    if(level_&&*level_!=level)++work.levelChanges;
    level_=level;work.selectedLevel=level;
    // Keep one world-tile ring around the visible demand.  This is residency
    // protection only: composition still uses `keys`, so overscan never
    // exposes pixels outside the requested viewport or qualifies a frame.
    const auto tileWorld = std::ldexp(256.0, -level);
    const auto overscan = expanded(visible, tileWorld);
    const auto overscanKeys = grid_.coverage(overscan, level,demandLimit);
    const auto required=[&](TileKey key){
        return std::find(keys.begin(),keys.end(),key)!=keys.end() ||
            std::find(parents.begin(),parents.end(),key)!=parents.end() ||
            (key.scale<level&&key.scale>=parents.front().scale&&intersects(grid_.bounds(key),visible)) ||
            std::find(overscanKeys.begin(),overscanKeys.end(),key)!=overscanKeys.end();
    };
    // A scale level is a request, not the identity of already admitted world
    // work. A critical source still covering visible pixels must finish rather
    // than restart at every coarse-parent transition. Scene/provider/resource
    // invalidation above and real offscreen demand still retire it normally.
    const auto visibleProgress=[&](const Job& job){return
        !job.prewarm&&job.sources->scene==scene&&intersects(job.demand,visible);};
    if(job_&&!required(job_->key)&&!visibleProgress(*job_)&&(!job_->prewarm||demandChanged))cancelJob(job_);
    if(refinement_&&!required(refinement_->key)&&!visibleProgress(*refinement_))cancelJob(refinement_);
    nsPlan.reset();
    auto acquired=[&]{NS_SCOPE(nsAcquire,Acquire);return provider.acquire();}();
    if(acquired.code!=SkiaSurfaceAcquireCode::kAcquired||!acquired.frame.surface)return ns::result(BackendSubmissionResult::retry("world navigation target unavailable"));
    struct Release {SkiaSurfaceProvider& provider;bool active=true;~Release(){if(active)provider.release();}} release{provider};
    const auto valid=[&]{return acquired.frame.generation==frame.surfaceGeneration.value()&&acquired.frame.surface->width()==int(frame.metrics.physicalWidth)&&acquired.frame.surface->height()==int(frame.metrics.physicalHeight);};
    if(!valid())return ns::result(BackendSubmissionResult::rejected("stale world navigation target"));
    auto* target=acquired.frame.surface->getCanvas();++tick_;
    auto* context=GrAsDirectContext(target->recordingContext());
    SkiaGpuLifetime gpu(*gpuRetirements_,budget,provider,context,resources::kPixelResourceClasses,
        resources::kPixelResourceClasses&~resources::resourceClassMask(resources::ResourceClass::kNavigationTile)&
        ~resources::resourceClassMask(resources::ResourceClass::kCanonicalFrame)&
        ~resources::resourceClassMask(resources::ResourceClass::kNavigation),validationPressure(validation_,budget));
    gpu.bindFrame(frame,resourceEpoch);
    if(!gpu.ready())return ns::result(BackendSubmissionResult::retry("world navigation completion unavailable"));
    RetainedNavigationSource retainedValue=retainedSource?*retainedSource:RetainedNavigationSource{};
    const RetainedNavigationSource* retained=retainedSource?&retainedValue:nullptr;
    std::vector<Patch> patches;std::vector<foundation::WorldRect> missing;
    work.coverageComplete=coverage(visible,scene,patches,retained,&missing);
    if(!work.coverageComplete&&retained&&retained->extend&&retained->extend()){
        // The callback has proved the entire extended rectangle, including
        // plain-background exposure. Drop the old invocation-scoped region
        // restriction too; otherwise the proven strips still remain holes.
        // A plane with mask holes never has an extend callback.
        retainedValue.bounds=visible;retainedValue.regions={};
        work.coverageComplete=coverage(visible,scene,patches,retained,&missing);
    }
    // Reserve mandatorydisplaycost under theoneoriginaldeadline. Safe
    // preparation canpublishbeforeouronlylatest-camera present below.
    const auto ready=[&](TileKey key){auto it=tiles_.find(key);return it!=tiles_.end()&&(it->second.image||it->second.plainBackground)&&it->second.checkedScene==scene&&it->second.valid.full(grid_.bounds(key));};
    // Age still-applicable demands once per owner opportunity, even when a
    // time gate prevents selection. Camera/LOD changes alone do not reset age.
    for(auto it=qualityDemands_.begin();it!=qualityDemands_.end();){
        bool applicable=false;const auto bounds=grid_.bounds(it->first);
        for(const auto&p:patches)if(!p.plainBackground&&intersects(p.area,bounds)&&scale/p.density>1.25){applicable=true;break;}
        if(!applicable){it=qualityDemands_.erase(it);continue;}
        it->second.age=std::min(12U,it->second.age+1);++it;
    }
    // A newly exposed frontier owns priority over disposable refinement.
    // Preserve coarse jobs across camera movement inside their world region;
    // GPU batches keep their existing bounded completion/backpressure policy.
    const auto fillsMissing=[&](const Job& job){return visibleProgress(job)&&
        std::any_of(missing.begin(),missing.end(),[&](const auto& area){return intersects(job.demand,area);});};
    if(!work.coverageComplete&&job_&&
       ((retained&&retained->draw)?(!job_->key.region||!fillsMissing(*job_)):(!job_->coveragePriority))){
        // A full legacy quality cell is not a fixed exposure completion unit,
        // even when its AABB overlaps a new hole. Keep its cursor suspended
        // while complete regional frontiers can actually unlock presentation.
        if(!refinement_){refinement_=std::move(job_);++work.suspendedRefinements;}
        else if(refinement_->key.region&&fillsMissing(*refinement_)){
            std::swap(job_,refinement_);++work.resumedRefinements;
        }else cancelJob(job_);
    }
    // Redistribute only the caller remaining query/setup allowance. Outer
    // proof work has already reduced it; neither coverage nor a new tile
    // restores a fresh quota or restarts the original deadline.
    const auto queryLimit=options.navigationMaxQueries;
    const auto initializationLimit=std::max(std::size_t(1),queryLimit);
    NS_IF(ns::state.row->prepare=ns::now();ns::state.row->earlyPresented=work.presented;ns::state.row->maxQueries=queryLimit;ns::state.row->maxInits=initializationLimit;ns::state.row->maxSteps=options.navigationMaxSteps);
    std::size_t steps=0,queries=0,initializations=0,gpuPrimitives=0;bool deferred=false;
    auto nsPrepare=ns::Exit{[&]{NS_IF(ns::state.row->queries=queries;ns::state.row->inits=initializations;ns::state.row->steps=steps;ns::state.row->gpu=gpuPrimitives);}};
    const auto fineGpuLimit=std::max(std::size_t(1),options.navigationMaxGpuContourPrimitives);
    const auto coarseGpuLimit=std::max(std::size_t(1),options.navigationMaxGpuCoarseContourPrimitives);
    const auto gpuLimit=std::max(fineGpuLimit,coarseGpuLimit);
    const auto displayClass=[](std::size_t count){unsigned band=0;while(count>1&&band<13){count=(count+1)/2;++band;}return band;};
    const auto& comparableDisplay=displayCosts_[displayClass(patches.size())];
    const bool displayKnown=comparableDisplay.known;
    const double displayReserve=work.coverageComplete?comparableDisplay.reserve():0;
    const auto checkpoint=[&](const Job* job){
        if(!job)return Checkpoint::Query;
        if(job->source>=job->sources->entries.size())return Checkpoint::Publish;
        if(!std::holds_alternative<CompactStrokeReferenceCommand>(job->sources->entries[job->source].command))return Checkpoint::Other;
        const auto&g=job->sources->geometry[job->source];
        const bool finish=g&&g->isContour()&&job->primitive+(job->gpuCoarse?128:32)>=g->primitiveCount();
        return finish?Checkpoint::CompactFinish:Checkpoint::Compact;
    };
    const auto expired=[&](Checkpoint kind){PC_NAV_TICK();const auto estimate=checkpointCosts_[unsigned(kind)].reserve();
        NS_IF(ns::state.row->displayReserveUs=displayReserve;ns::state.row->checkpointEstimateUs=estimate);
        const auto elapsed=std::chrono::duration<double,std::micro>(PC_NAV_NOW()-started).count();
        return elapsed+displayReserve+estimate>=options.navigationMicroseconds;
    };
    std::map<TileKey,std::shared_ptr<const Sources>> desired;
    auto load=[&](TileKey key)->bool{
        if(desired.contains(key))return true;
        if(const auto cached=sources_.find(key);cached!=sources_.end()&&cached->second->scene==scene){desired.emplace(key,cached->second);return true;}
        if(queries>=queryLimit||expired(Checkpoint::Query)){NS_IF(ns::gate(queries>=queryLimit?ns::QueryCap:ns::TimeLoad,queries,initializations,steps,gpuPrimitives));deferred=true;return false;}
        ++queries;++work.queries;if(key.region)++work.regionQueries;const auto start=std::chrono::steady_clock::now();
        const auto queriedRegion=expanded(grid_.bounds(key),std::ldexp(double(gutter),-key.scale));
        std::vector<ReferenceTraversalEntry> sources;NavigationSourceDiagnostics diagnostics;
        const auto queryCostStarted=PC_NAV_NOW();
        const bool ok=[&]{NS_SCOPE(nsQuery,Query);return query(queriedRegion,sources,diagnostics);}();
        checkpointCosts_[unsigned(Checkpoint::Query)].observe(std::chrono::duration<double,std::micro>(PC_NAV_NOW()-queryCostStarted).count());
        NS_IF(ns::state.row->knownEmptyCandidates+=ok&&sources.empty());
        if(diagnostics.sceneStamped&&(diagnostics.semanticGeneration!=frame.sceneGeneration.value()||diagnostics.sceneRevision!=frame.sceneReadToken.value()))return false;
        work.chunkCandidates+=diagnostics.chunkCandidates;work.indexNodes+=diagnostics.indexNodes;work.indexFallbacks+=diagnostics.indexFallbacks;
        work.payloadCopyBytes+=diagnostics.payloadCopyBytes;work.payloadAliases+=diagnostics.payloadAliases;work.observedSourceQueries+=diagnostics.observed;
        work.queryNanoseconds+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
        if(!ok||std::any_of(sources.begin(),sources.end(),[](const auto& e){return !supported(e); })){NS_GATE(Unsupported);return false;}
        std::unordered_set<foundation::ObjectId,foundation::ObjectIdHash> ids;
        for(const auto& entry:sources)if(entry.record.objectId.isZero()||!ids.insert(entry.record.objectId).second)return false;
        ReferenceDrawList validation{.frame=frame,.entries={},.canonicalBytes={},.digest={}};
        validation.viewportClip={0,0,frame.metrics.logicalWidth,frame.metrics.logicalHeight};validation.entries=sources;
        if([&]{NS_SCOPE(nsValidation,Validation);return validateReferencePlan(FramePlan{frame,std::move(validation)},validation_.get(),diagnostics.sceneStamped?HistoryValidationPolicy::kPublishedImmutable:HistoryValidationPolicy::kUntrusted).code;}()!=BackendSubmissionCode::kAccepted){NS_GATE(InvalidBasis);return false;}
        std::vector<std::shared_ptr<const PreparedNavigationGeometry>> geometry;
        geometry.reserve(sources.size());
        SourceRetentions sourceLeases;
        for(const auto& entry:sources){
            auto retained=geometry_->retainSource(entry,budget,true);if(!retained){NS_GATE(MetadataDenied);deferred=true;return false;}
            sourceLeases.insert(sourceLeases.end(),retained->begin(),retained->end());
            const auto before=geometry_->statistics();
            {NS_SCOPE(nsGeometry,Geometry);geometry.push_back(geometry_->get(entry,options,budget,!work.coverageComplete&&key.scale<level,PreparedConsumer::kNavigation));}
            const auto after=geometry_->statistics();
            work.preparedGeometryBuilds+=after.builds-before.builds;
            work.preparedGeometryHits+=after.hits-before.hits;
            work.preparedGeometryDenied+=after.denied-before.denied;
            if(after.builds>before.builds)
                if(const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command))
                    work.prefixScanKnots+=compact->content.get().stroke.knots.size();
        }
        std::optional<NavigationSceneEmptyProof> emptyProof;
        if(opaquePlainBackground&&diagnostics.sceneStamped&&diagnostics.sceneEmptyProof){const auto&p=*diagnostics.sceneEmptyProof;
            if(p.region==queriedRegion&&p.semanticGeneration==frame.sceneGeneration.value()&&p.sceneRevision==frame.sceneReadToken.value()&&p.view==frame.viewId.value()&&p.surface==frame.surfaceGeneration.value()&&p.metrics==frame.metricsGeneration.value()&&sources.empty())emptyProof=p;
        }
        std::size_t bytes=sizeof(Sources)+(sizeof(ReferenceTraversalEntry)+sizeof(std::shared_ptr<const PreparedNavigationGeometry>))*sources.size();
        for(const auto& entry:sources)bytes+=sourceMetadataBytes(entry);
        bytes+=sourceLeases.capacity()*sizeof(SourceRetentions::value_type);
        auto token=budget->reserve(resources::ResourceClass::kRecorded,bytes,true);
        if(!token&&validation_){validation_->reclaimFor(budget);token=budget->reserve(resources::ResourceClass::kRecorded,bytes,true);}
        for(auto it=sources_.begin();it!=sources_.end()&&!token;){
            if(it->second.use_count()==1){it=sources_.erase(it);token=budget->reserve(resources::ResourceClass::kRecorded,bytes,true);}else ++it;
        }
        if(!token){NS_GATE(MetadataDenied);deferred=true;return false;}
        if(sources_.size()>=maximumTiles)sources_.erase(sources_.begin());
        auto source=std::make_shared<const Sources>(Sources{scene,std::move(sources),std::move(geometry),resources::ResourceLease(budget,*token),std::move(sourceLeases),bytes,diagnostics.sceneStamped,std::move(emptyProof),queriedRegion});
        sources_.insert_or_assign(key,source);desired.emplace(key,std::move(source));return true;
    };
    bool partitionInvalidated=false;
    // Revalidate only possible visible fallback cells, at the same query budget.
    for(auto it=tiles_.begin();it!=tiles_.end();){
        if(it->second.checkedScene==scene||!intersects(grid_.bounds(it->first),visible)){++it;continue;}
        if(!load(it->first)){if(deferred)break;return ns::result(BackendSubmissionResult::retry("world navigation revalidation failed"));}
        auto& tile=it->second;const auto& fresh=desired.at(it->first);
        if(tile.plainBackground){if(!fresh->emptyProof){it=tiles_.erase(it);dirty_=true;partitionInvalidated=true;continue;}tile.sources=fresh;tile.checkedScene=scene;++it;continue;}
        auto validity=tile.valid;
        const auto difference=maskPixelDifference(tile.sources->entries,fresh->entries);
        bool qualified=difference.compatible;
        if(qualified&&difference.damage){
            // Localization needs an explicitly Scene-bound query. Legacy
            // queries retain their old exact-equality revalidation only.
            // The load above validates binding when it is present; fresh
            // Sources carries that evidence for this comparison.
            const auto damage=maskSamplingDamage(*difference.damage,std::ldexp(1.,it->first.scale),grid_.bounds(it->first));
            qualified=fresh->stamped&&damage&&validity.subtract(*damage);
        }
        if(!qualified||!validity.count){it=tiles_.erase(it);dirty_=true;partitionInvalidated=true;}
        else{if(difference.damage){++work.regionalTileProofs;dirty_=true;partitionInvalidated=true;}
            tile.valid=validity;tile.sources=fresh;tile.checkedScene=scene;++it;}
    }
    if(partitionInvalidated){patches.clear();work.coverageComplete=coverage(visible,scene,patches,retained,&missing);}
    const bool gpuBlocked=context&&gpuRetirements_->preparationCount()>=2;
    if(gpuBlocked){NS_GATE(GpuBackpressure);++work.gpuThrottles;}
    const auto protectedTile=[&](TileKey key){return
        std::find(parents.begin(),parents.end(),key)!=parents.end()||
        (job_&&job_->key==key)||(refinement_&&refinement_->key==key)||
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
    bool qualityDemand=false;for(const auto&p:patches)qualityDemand|=!p.plainBackground&&scale/p.density>1.25;
    if(!qualityDemand)qualityAge_=0;else qualityAge_=std::min(qualityAge_+1,12U);
    const auto usefulBefore=work.knotSteps+work.sourceSteps+work.tilesBuilt;
    [[maybe_unused]] const auto needsFine=[&](TileKey key){
        const auto bounds=grid_.bounds(key);const double density=std::ldexp(1.,key.scale);
        return std::any_of(missing.begin(),missing.end(),[&](const auto& area){return intersects(area,bounds);})||
            std::any_of(patches.begin(),patches.end(),[&](const auto& patch){return !patch.plainBackground&&patch.density<density&&intersects(patch.area,bounds);});
    };

    NS_IF(ns::state.row->maxGpu=gpuLimit;ns::state.row->actualPreparationTickets=gpuRetirements_->preparationCount());
    auto nsDemandAfter=ns::Exit{[&]{
    NS_IF(ns::state.row->qualityAge=qualityAge_;ns::state.row->businessEnd=ns::now());
    {NS_SCOPE(nsProbe,ProbeOnly);
    NS_IF(auto&r=*ns::state.row;r.maxGpu=gpuLimit;r.level=level;r.visibleArea=double(visible.right-visible.left)*(visible.bottom-visible.top);r.missingCount=missing.size();r.missingHash=1469598103934665603ULL;
      for(const auto&h:missing){r.missingArea+=double(h.right-h.left)*(h.bottom-h.top);if(r.missingCopied<r.holes.size())r.holes[r.missingCopied++]=ns::rect(h);for(float f:{h.left,h.top,h.right,h.bottom})r.missingHash=(r.missingHash^std::bit_cast<std::uint32_t>(f))*1099511628211ULL;}
      for(const auto&p:patches){if(p.plainBackground)continue;double a=double(p.area.right-p.area.left)*(p.area.bottom-p.area.top),u=scale/p.density,ca=ns::contentArea(ns::rect(p.area));r.contentSupportArea+=ca;r.lowArea+=u>1.25?a:0;r.severeArea+=u>4?a:0;r.debtArea+=a*std::max(0.,u/1.25-1);r.contentLowArea+=u>1.25?ca:0;r.contentSevereArea+=u>4?ca:0;r.contentDebtArea+=ca*std::max(0.,u/1.25-1);}
      const auto role=[&](const Job*j){unsigned a=0;if(!j)return a;if(j->prewarm)return 8U;for(const auto&h:missing)if(intersects(h,j->demand)){a|=1U;break;}for(const auto&p:patches)if(!p.plainBackground&&p.density<std::ldexp(1.,j->key.scale)&&intersects(p.area,j->demand)){a|=2U;break;}return a;};r.afterJobs[0].role=role(job_.get());r.afterJobs[1].role=role(refinement_.get());
      auto nsCandidates=keys;
      for(const auto&k:parents)nsCandidates.push_back(k);
      for(const auto&k:nsCandidates){if(ready(k)||!needsFine(k))continue;ns::Candidate c;c.x=k.x;c.y=k.y;c.scale=k.scale;c.region=k.region;c.targetDensity=std::ldexp(1.,k.scale);const auto bounds=ns::rect(grid_.bounds(k));for(const auto&h:missing)c.missing+=ns::intersection(bounds,ns::rect(h));for(const auto&p:patches)if(!p.plainBackground&&p.density<c.targetDensity){const auto a=ns::intersection(bounds,ns::rect(p.area));c.quality+=a;c.currentDensity=std::max(c.currentDensity,p.density);c.benefit+=a*std::max(0.,scale/p.density-scale/c.targetDensity);}c.role=(c.missing>0?1U:0U)|(c.quality>0?2U:0U);r.frontierCandidates+=c.missing>0;r.qualityCandidates+=c.quality>0;for(auto&top:r.candidates)if(c.benefit+c.missing>top.benefit+top.missing)std::swap(c,top);}
    );}
    }};
    struct QualityChoice {
        TileKey key{};double score=0;unsigned age=0;bool resumable=false;
        NavigationRegions useful; // Original deficient regions, never emptiness proof.
    };
    std::array<QualityChoice,64> qualityChoices{};std::size_t qualityCount=0;
    bool qualityOutsideWorkset=false;[[maybe_unused]] std::size_t publications=0;
    const auto better=[](const QualityChoice&a,const QualityChoice&b){
        const bool dueA=a.age>=11,dueB=b.age>=11;
        if(dueA!=dueB)return dueA;
        if(a.resumable!=b.resumable)return a.resumable;
        if(a.score!=b.score)return a.score>b.score;
        if(a.age!=b.age)return a.age>b.age;
        return a.key<b.key;
    };
    std::optional<TileKey> bestQuality;unsigned bestQualityAge=0;
    if(displayKnown){
                std::map<TileKey,double> choices;
                for(const auto& patch:patches){
                    if(patch.plainBackground||scale/patch.density<=1.25)continue;
                    const auto bridge=std::min(level,std::clamp(int(std::ceil(std::log2(std::max(2*patch.density,scale/2)))),-16,16));
                    const auto consider=[&](foundation::WorldRect content){
                        const auto overlap=foundation::WorldRect{std::max({patch.area.left,visible.left,content.left}),std::max({patch.area.top,visible.top,content.top}),std::min({patch.area.right,visible.right,content.right}),std::min({patch.area.bottom,visible.bottom,content.bottom})};
                        if(overlap.right<=overlap.left||overlap.bottom<=overlap.top)return;
                        for(auto key:grid_.coverageWindow(overlap,bridge,std::min(maximumTiles,demandLimit),streamingCursor_[bridge+16])){if(ready(key))continue;const auto b=grid_.bounds(key);
                            const double area=std::max(0.,double(std::min(b.right,overlap.right)-std::max(b.left,overlap.left)))*std::max(0.,double(std::min(b.bottom,overlap.bottom)-std::max(b.top,overlap.top)));
                            const double benefit=area/((visible.right-visible.left)*(visible.bottom-visible.top))*std::max(0.,scale/patch.density/1.25-scale/std::ldexp(1.,bridge)/1.25);
                            // Max avoids counting overlapping copies as distinct benefit.
                            auto found=choices.find(key);if(found==choices.end()){
                                if(choices.size()>=demandLimit){qualityOutsideWorkset=true;continue;}
                                choices.emplace(key,benefit);
                            }else found->second=std::max(found->second,benefit);
                        }
                    };
                    if(patch.retained){consider(patch.area);}else if(const auto t=tiles_.find(patch.source);t!=tiles_.end()){
                        bool contentOverlap=false;for(const auto&e:t->second.sources->entries){contentOverlap|=intersects(e.record.worldBounds,patch.area);consider(e.record.worldBounds);}
                        // Without an authoritative proof the safe fallback still
                        // paints the unknown region; AABBs do not prove empty.
                        if(!contentOverlap)consider(patch.area);
                    }
                }
                for(const auto&[key,benefit]:choices){
                    auto it=qualityDemands_.find(key);
                    if(it==qualityDemands_.end()){
                        if(qualityDemands_.size()>=demandLimit)continue;
                        const auto token=budget->reserve(resources::ResourceClass::kRecorded,sizeof(QualityDemand)+sizeof(TileKey)+128,true);if(!token)continue;
                        it=qualityDemands_.emplace(key,QualityDemand{0,0,0,resources::ResourceLease(budget,*token)}).first;
                    }
                    auto&d=it->second;if(d.seen!=tick_){if(d.age==0)d.age=1;d.seen=tick_;}
                    const double score=benefit*(1+std::min(d.age,12U)/12.)/std::max(d.costUs>0?d.costUs:checkpointCosts_[unsigned(Checkpoint::Compact)].reserve(),1.);
                    QualityChoice candidate{key,score,d.age,(job_&&job_->key==key)||(refinement_&&refinement_->key==key),{}};
                    const auto b=grid_.bounds(key);candidate.useful.reset({std::max(b.left,visible.left),std::max(b.top,visible.top),std::min(b.right,visible.right),std::min(b.bottom,visible.bottom)});
                    if(qualityCount<qualityChoices.size())qualityChoices[qualityCount++]=std::move(candidate);
                    else{
                        qualityOutsideWorkset=true;auto worst=std::max_element(qualityChoices.begin(),qualityChoices.end(),better);
                        if(better(candidate,*worst))*worst=std::move(candidate);
                    }
                }
                for(auto it=qualityDemands_.begin();it!=qualityDemands_.end();)if(it->second.seen!=tick_&&(!job_||job_->key!=it->first)&&(!refinement_||refinement_->key!=it->first))it=qualityDemands_.erase(it);else++it;
                std::sort(qualityChoices.begin(),qualityChoices.begin()+qualityCount,better);
                if(qualityCount){bestQuality=qualityChoices[0].key;bestQualityAge=qualityChoices[0].age;}
                NS_IF(ns::state.row->worksetSize=qualityCount;ns::state.row->worksetRemaining=qualityCount);
            }
    const auto selectQuality=[&]()->std::optional<TileKey>{
        while(qualityCount){
            const auto candidate=qualityChoices[0];bool useful=!ready(candidate.key)&&intersects(grid_.bounds(candidate.key),visible);
            useful=useful&&candidate.useful.count>0;
            if(useful){bestQualityAge=candidate.age;NS_IF(ns::state.row->worksetRemaining=qualityCount;ns::state.row->reselected+=publications>0);return candidate.key;}
            std::move(qualityChoices.begin()+1,qualityChoices.begin()+qualityCount,qualityChoices.begin());--qualityCount;
        }
        NS_IF(ns::state.row->worksetRemaining=0);return std::nullopt;
    };
    const auto qualityPublished=[&](TileKey key,const NavigationRegions& valid){
        for(std::size_t n=0;n<qualityCount;++n){auto& candidate=qualityChoices[n];
            if(key.scale<candidate.key.scale)continue;
            for(std::size_t i=0;i<valid.count&&candidate.useful.count;++i)
                if(!candidate.useful.subtract(valid.areas[i]))qualityOutsideWorkset=true;
        }
    };
    // A due quality checkpoint can run alongside mandatory frontier under
    // the same origin/limits. Preserve the single suspended world job.
    bool usefulQualityThisCall=false;
    if((work.coverageComplete||!retained||!retained->draw)&&bestQualityAge>=11&&refinement_&&(!job_||!fillsMissing(*job_))){
        std::swap(job_,refinement_);if(!job_)job_=std::move(refinement_);++work.resumedRefinements;
    }
    std::optional<ns::Scope> nsPreparation;NS_IF(nsPreparation.emplace(ns::Preparation));
    const auto nsExpired=[&]{const bool value=expired(checkpoint(job_.get()));if(value)NS_IF(ns::gate(work.presented?ns::TimeAfterDisplay:ns::TimeBeforePrepare,queries,initializations,steps,gpuPrimitives));return value;};
    while((!work.coverageComplete||displayKnown)&&!deferred&&!gpuBlocked&&gpuPrimitives<gpuLimit&&steps<std::max<std::size_t>(1,options.navigationMaxSteps)&&!nsExpired()){
        if(!job_&&work.coverageComplete&&refinement_){job_=std::move(refinement_);++work.resumedRefinements;}
        if(!job_){
            bestQuality=selectQuality();
            std::optional<TileKey> next;bool prewarm=false;
            if(work.coverageComplete&&bestQualityAge>=11&&!usefulQualityThisCall&&bestQuality&&!ready(*bestQuality))next=bestQuality;
            if(!next&&!work.coverageComplete){
                for(const auto parent:parents){
                    if(retained&&retained->draw){for(unsigned n=1;n<=16;++n){auto key=parent;key.region=std::uint8_t(n);
                        if(!ready(key)&&std::any_of(missing.begin(),missing.end(),[&](const auto&area){return intersects(grid_.bounds(key),area);})){next=key;break;}}
                    }else if(!ready(parent)&&std::any_of(missing.begin(),missing.end(),[&](const auto&area){return intersects(grid_.bounds(parent),area);}))next=parent;
                    if(next)break;
                }
            }
            if(!next&&bestQuality&&!ready(*bestQuality))next=bestQuality;
            if(!next&&!refinement_&&allowPrewarm&&warmed_<options.navigationPrewarmTiles){
                // One-cell overscan and one adjacent finer level, bounded total.
                std::vector<TileKey> near;
                if(level<16){const float cx=(visible.left+visible.right)*.5F,cy=(visible.top+visible.bottom)*.5F;
                    auto center=grid_.coverage({cx,cy,cx+1,cy+1},level+1,4);near.insert(near.end(),center.begin(),center.end());}
                for(const auto key:keys)for(const auto& d:std::vector<std::pair<int,int>>{{-1,0},{1,0},{0,-1},{0,1}}){
                    TileKey candidate{key.x+d.first,key.y+d.second,key.scale};if(!required(candidate))near.push_back(candidate);}
                for(const auto key:near)if(!ready(key)){next=key;prewarm=true;break;}
            }
            if(!next){
                const bool remainingQuality=std::any_of(patches.begin(),patches.end(),[&](const auto&p){return !p.plainBackground&&scale/p.density>1.25;});
                if(qualityOutsideWorkset||remainingQuality){NS_GATE(BoundedWorksetYield);deferred=true;}
                else NS_GATE(NoEligible);
                break;
            }
            NS_IF(auto&c=ns::state.row->selected;c.x=next->x;c.y=next->y;c.scale=next->scale;c.region=next->region;c.targetDensity=std::ldexp(1.,next->scale);c.role=prewarm?8U:(!work.coverageComplete?1U:2U));
            if(initializations>=initializationLimit){NS_GATE(InitCap,queries,initializations,steps,gpuPrimitives);deferred=true;break;}
            if(!load(*next)){if(deferred)break;return ns::result(BackendSubmissionResult::retry("world navigation unsupported sources"));}
            if(expired(Checkpoint::Setup)){NS_GATE(TimeAfterSetup,queries,initializations,steps,gpuPrimitives);deferred=true;break;}
            if(desired.at(*next)->emptyProof&&opaquePlainBackground){
                const auto bytes=sizeof(Tile)+sizeof(TileKey)+128;const auto token=budget->reserve(resources::ResourceClass::kRecorded,bytes,true);
                if(!token){NS_GATE(MetadataDenied);deferred=true;break;}
                resources::ResourceLease metadata(budget,*token);if(tiles_.size()>=maximumTiles&&!evict(prewarm)){NS_GATE(ProtectedOrResident);deferred=true;break;}
                ValidRegions region;region.reset(grid_.bounds(*next));tiles_.insert_or_assign(*next,Tile{desired.at(*next),nullptr,tick_,scene,std::move(metadata),region,true});
                qualityPublished(*next,region);++initializations;++work.initializations;++steps;dirty_=true;NS_IF(++ns::state.row->proofPublishes);patches.clear();work.coverageComplete=coverage(visible,scene,patches,retained,&missing);continue;
            }
            if(tileBytes>options.maxEntryBytes||(tileBytes*3+tileMetadataBytes)>options.maxBytes)return ns::result(BackendSubmissionResult::retry("world navigation tile budget"));
            const auto renderExtent=next->region?68:extent;
            const auto renderBytes=std::size_t(renderExtent)*renderExtent*4;
            const auto peak=renderBytes*3+tileMetadataBytes;
            // Keep room for one replacement peak during optional prewarm.
            const auto navLimit=prewarm&&options.maxBytes>=peak?options.maxBytes-peak:options.maxBytes;
            const auto fits=[&]{const auto used=budget->used(resources::ResourceClass::kNavigationTile);
                return peak<=navLimit&&used<=navLimit-peak&&tiles_.size()<maximumTiles;};
            while(!fits()){
                const auto before=budget->used();if(!evict(prewarm))break;
                // An unfinished image remains charged after logical eviction.
                // Yield for completion instead of cascading through the cache.
                if(budget->used()>=before)break;
            }
            if(!fits()){NS_GATE(ProtectedOrResident);deferred=true;break;}
            auto token=budget->reserve(resources::ResourceClass::kNavigationTile,peak,true,renderBytes+tileMetadataBytes);
            if(!token&&validation_){validation_->reclaimFor(budget);token=budget->reserve(resources::ResourceClass::kNavigationTile,peak,true,renderBytes+tileMetadataBytes);}
            while(!token){const auto before=budget->used();if(!evict(prewarm)||budget->used()>=before)break;
                token=budget->reserve(resources::ResourceClass::kNavigationTile,peak,true,renderBytes+tileMetadataBytes);}
            if(!token){NS_GATE(PixelDenied);deferred=true;break;}
            NS_SCOPE(nsInit,Initialization);
            const auto initStart=std::chrono::steady_clock::now();
            const auto setupStart=PC_NAV_NOW();auto setupCost=ns::Exit{[&]{checkpointCosts_[unsigned(Checkpoint::Setup)].observe(std::chrono::duration<double,std::micro>(PC_NAV_NOW()-setupStart).count());}};
            auto job=std::make_unique<Job>();job->pixelBytes=renderBytes;job->key=*next;job->sources=desired.at(*next);job->prewarm=prewarm;
            job->coveragePriority=!work.coverageComplete;
            job->lease=resources::ResourceLease(budget,*token);
            gpu.touch(true);job->surface=target->makeSurface(acquired.frame.surface->imageInfo().makeWH(renderExtent,renderExtent));
            const auto coverageInfo=SkImageInfo::MakeN32Premul(renderExtent,renderExtent);
            const bool coarseV4=next->scale<level&&std::all_of(job->sources->entries.begin(),job->sources->entries.end(),[](const auto& entry){
                const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command);
                return !compact||compact->content.get().stroke.config.algorithmVersion==4;
            });
            // Ready v4 paths use the same qualified SrcOver GPU coverage with
            // or without historical masks. Completeness still gates present;
            // existing command/time/memory ceilings bound this bootstrap.
            job->gpuCoarse=next->scale<level&&context&&options.gpuCoverageCandidate&&
                std::any_of(job->sources->geometry.begin(),job->sources->geometry.end(),[](const auto&g){return g&&g->isContour();});
            // Unprepared, raster-only and explicit CPU-policy work
            // retain the established bounded CPU coarse path.
            job->cpuCoverage=coarseV4&&!job->gpuCoarse;
            // Reuse canonical's qualified single-channel encoding for v4
            // white CPU masks. Geometry/order/grid stay shared and unchanged;
            // unsupported materials/targets retain the original N32 fallback.
            const bool alpha=job->cpuCoverage&&std::all_of(job->sources->entries.begin(),job->sources->entries.end(),[](const auto& entry){
                const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command);
                return !compact||compact->content.get().stroke.config.materialVersion<3;
            });
            if(alpha)job->coverage=SkSurfaces::Raster(SkImageInfo::MakeA8(renderExtent,renderExtent));
            if(!job->coverage)job->coverage=job->cpuCoverage?SkSurfaces::Raster(coverageInfo):target->makeSurface(coverageInfo);
            if(!job->surface||!job->coverage){NS_GATE(PixelDenied);deferred=true;break;}
            G5_ROUTE(g5NavTarget,job->cpuCoverage?g5::Route::nav_coarse_cpu:(job->gpuCoarse?g5::Route::nav_coarse_gpu:g5::Route::nav_unprepared));
            G5_TARGET(job->coverage,!job->cpuCoverage);
            job->surface->getCanvas()->clear(SK_ColorTRANSPARENT);
            const auto b=grid_.bounds(*next);const float s=std::ldexp(1.F,next->scale);
            auto demand=b;
            const auto old=tiles_.find(*next);
            const bool reusable=old!=tiles_.end()&&old->second.checkedScene==scene;
            const bool zoomExposure=retained&&retained->draw&&
                (retained->bounds.right-retained->bounds.left!=visible.right-visible.left||
                 retained->bounds.bottom-retained->bounds.top!=visible.bottom-visible.top);
            if(zoomExposure){
            // Prepare all actual holes in this cell in one source pass. A
            // rectangle around their union is only a lifecycle/culling hint;
            // its unknown inner corners never become valid or visible pixels.
            job->paint.reset(b);
            if(!work.coverageComplete&&next->scale<level&&!job->sources->entries.empty()){
                job->paint.count=0;
                for(const auto& hole:missing)if(intersects(hole,b)){
                    auto r=foundation::WorldRect{std::max(b.left,hole.left),std::max(b.top,hole.top),
                        std::min(b.right,hole.right),std::min(b.bottom,hole.bottom)};
                    r={std::max(b.left,b.left+std::floor((r.left-b.left)*s)/s),
                       std::max(b.top,b.top+std::floor((r.top-b.top)*s)/s),
                       std::min(b.right,b.left+std::ceil((r.right-b.left)*s)/s),
                       std::min(b.bottom,b.top+std::ceil((r.bottom-b.top)*s)/s)};
                    // The critical job leads only observed camera movement.
                    // A fixed64-coarse-texel lead could add512world units to a
                    // first18world-unit exposure and import unrelated ink.
                    const auto prior=previousVisible.value_or(visible);
                    const auto lead=[&](float edge,float previous){return float(std::clamp(
                        2.*std::abs(double(edge)-previous),1./s,64./s));};
                    if(r.left<=visible.left)r.left=std::max(b.left,r.left-lead(visible.left,prior.left));
                    if(r.right>=visible.right)r.right=std::min(b.right,r.right+lead(visible.right,prior.right));
                    if(r.top<=visible.top)r.top=std::max(b.top,r.top-lead(visible.top,prior.top));
                    if(r.bottom>=visible.bottom)r.bottom=std::min(b.bottom,r.bottom+lead(visible.bottom,prior.bottom));
                    if(!job->paint.add(r)){
                        // Bounded metadata fallback repaints the whole cell;
                        // no unknown corner is certified by a bounding box.
                        job->paint.reset(b);break;
                    }
                }
                if(job->paint.count){demand=job->paint.areas[0];for(std::size_t i=1;i<job->paint.count;++i){const auto& r=job->paint.areas[i];
                    demand={std::min(demand.left,r.left),std::min(demand.top,r.top),std::max(demand.right,r.right),std::max(demand.bottom,r.bottom)};}}
            }
            }else{
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
                job->paint.reset(demand);
            }
            if(next->region){
                // Region identity is a fixed64 interior plus2texel gutter.
                // A publication and its immutable body witness cover that
                // complete basis, independent of the current exposed strip.
                demand=b;job->paint.reset(b);
            }
            if(reusable){(void)old->second.lease.markGpuUse();job->valid=old->second.valid;}
            bool admitted=true;
            for(std::size_t i=0;i<job->paint.count&&admitted;++i)admitted=job->valid.add(job->paint.areas[i]);
            if(!admitted){
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
                job->valid.reset(repaint);job->paint.reset(repaint);demand=repaint;
            }
            job->demand=demand;
            const auto deviceRect=[&](foundation::WorldRect r){return SkRect::MakeLTRB(
                gutter+(r.left-b.left)*s-gutter,gutter+(r.top-b.top)*s-gutter,
                gutter+(r.right-b.left)*s+gutter,gutter+(r.bottom-b.top)*s+gutter);};
            if(reusable)job->surface->getCanvas()->drawImage(old->second.image,0,0);
            auto* material=job->surface->getCanvas();
            material->clipRect(deviceRect(demand));
            if(zoomExposure&&!next->region)job->coverage->getCanvas()->clipRect(deviceRect(demand));
            // Repaint the complete new strip, including its sampling gutter.
            // Src clear prevents double application over old gutter pixels.
            // A rectangular scissor also avoids complex difference-clip GPU
            // programs on the critical preparation path.
            material->clear(SK_ColorTRANSPARENT);
            job->surface->getCanvas()->translate(gutter-b.left*s,gutter-b.top*s);job->surface->getCanvas()->scale(s,s);
            job->id=++serial_;
            if(!scheduler_.submit({job->id,1,prewarm?scheduling::WorkPriority::kPrefetch:scheduling::WorkPriority::kVisible,0,{},(tileBytes*3+tileMetadataBytes)})||!scheduler_.take())return ns::result(BackendSubmissionResult::retry("world navigation admission failed"));
            job_=std::move(job);NS_IF(++ns::state.row->transitions);++initializations;++work.initializations;
            work.initializationNanoseconds+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-initStart).count();

            if(expired(checkpoint(job_.get()))){NS_GATE(TimeAfterSetup,queries,initializations,steps,gpuPrimitives);deferred=true;break;}
        }
        auto& job=*job_;(void)job.lease.markGpuUse();
        const auto quantumStarted=PC_NAV_NOW();const auto quantumKey=job.key;const auto quantumClass=checkpoint(&job);
        const auto quantumBefore=work.knotSteps+work.sourceSteps+work.tilesBuilt+work.contourPrimitivesDrawn;
        bool quantumQuality=false;[[maybe_unused]] bool quantumFrontier=false;const auto quantumBounds=grid_.bounds(job.key);
        for(const auto&p:patches)if(!p.plainBackground&&p.density<std::ldexp(1.,job.key.scale)&&intersects(p.area,quantumBounds))
            for(const auto&e:job.sources->entries)quantumQuality|=intersects(e.record.worldBounds,p.area)&&intersects(e.record.worldBounds,job.demand);
        for(const auto&h:missing)quantumFrontier|=intersects(h,quantumBounds);
        auto quantumRecord=ns::Exit{[&]{if(work.knotSteps+work.sourceSteps+work.tilesBuilt+work.contourPrimitivesDrawn>quantumBefore){usefulQualityThisCall|=quantumQuality;NS_IF(ns::state.row->qualityAdvances+=quantumQuality;ns::state.row->frontierAdvances+=quantumFrontier);const double us=std::chrono::duration<double,std::micro>(PC_NAV_NOW()-quantumStarted).count();checkpointCosts_[unsigned(quantumClass)].observe(us);quantumEwmaUs_=checkpointCosts_[unsigned(quantumClass)].ewma;const auto found=qualityDemands_.find(quantumKey);if(found!=qualityDemands_.end()){found->second.age=0;found->second.costUs=found->second.costUs>0?.75*found->second.costUs+.25*us:us;}}}};

        G5_ROUTE(g5NavJob,job.cpuCoverage?g5::Route::nav_coarse_cpu:(job.gpuCoarse?g5::Route::nav_coarse_gpu:g5::Route::nav_unprepared));
        G5_TARGET(job.coverage,!job.cpuCoverage);
        G5_TARGET(job.surface,true);
        if(job.source>=job.sources->entries.size()){
            // A fixed ticket can retire at most eight publications. Defer
            // additional cells; never allocate unbounded callback payloads.
            gpu.touch(true);auto image=job.surface->makeImageSnapshot();if(!image)return ns::result(BackendSubmissionResult::retry("world navigation snapshot failed"));
            if(!gpu.publish(job.lease,job.pixelBytes+tileMetadataBytes)){NS_GATE(PublishCap);deferred=true;break;}
            (void)scheduler_.finish(job.id,1);
            if(job.prewarm){++warmed_;++work.prewarmTiles;}
            NS_IF(if(!job.sources->entries.empty()){auto&r=*ns::state.row;if(r.contentPublications<r.publishedContentKeys.size())r.publishedContentKeys[r.contentPublications]={job.key.x,job.key.y,job.key.scale,job.key.region};++r.contentPublications;});
            tiles_.insert_or_assign(job.key,Tile{std::move(job.sources),std::move(image),tick_,scene,std::move(job.lease),job.valid});
            qualityPublished(job.key,tiles_.at(job.key).valid);
            if(job.key.region)++work.regionPublications;job_.reset();dirty_=true;++work.tilesBuilt;++steps;++publications;
            // Partial publication changes the missing frontier even when the
            // cell is not full. Do not select the same completed strip again
            // for the rest of this opportunity.
            patches.clear();work.coverageComplete=coverage(visible,scene,patches,retained,&missing);
            continue;
        }
    const auto& entry=job.sources->entries[job.source];
        // Scene queried the full block plus sampling gutter. Never use a
        // narrower interior AABB to omit a relevant edge source.
        if(!job.sourceInitialized&&job.key.region&&!intersects(entry.record.worldBounds,job.sources->queried)){
            ++job.source;++work.sourceSteps;++steps;continue;
        }
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
                draw->resetMatrix();draw->clear(job.coverage->imageInfo().colorType()==kAlpha_8_SkColorType?SK_ColorTRANSPARENT:SK_ColorBLACK);
                draw->setMatrix(job.surface->getCanvas()->getTotalMatrix());
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
                if(job.geometry&&job.geometry->isContour()){
                    job.projectedBody=bodyLayers_->projectNavigationBody(job.geometry,*draw,options,budget);
                    if(!job.projectedBody)job.sharedBody=bodyLayers_->navigationBody(job.geometry,*draw,options,budget);
                    if(job.projectedBody||job.sharedBody){job.knot=range.second;job.primitive=job.geometry->primitiveCount();}
                }
            }
            const auto range = compactRange(entry);
            std::size_t end=std::min(job.knot+32,range.second);
            if(job.projectedBody){end=range.second;
            }else if(job.sharedBody){
                draw->save();draw->resetMatrix();draw->drawImage(job.sharedBody,0,0);draw->restore();end=range.second;
            }else if(stroke.config.algorithmVersion==4 && job.geometry&&job.geometry->isContour()){
                const bool gpuCoverage=context&&!job.cpuCoverage;
                const auto limit=job.gpuCoarse?coarseGpuLimit:fineGpuLimit;
                if(gpuCoverage&&gpuPrimitives>=limit){NS_GATE(GpuCommandCap,queries,initializations,steps,gpuPrimitives);deferred=true;break;}
                // Prepared coarse paths have fixed-size ribbon primitives. A
                // 32-index checkpoint spent the64step ceiling mostly on culled
                // ribbons, yielding below1ms before its GPU quota was reached.
                // Keep fine/CPU checkpoints unchanged and the same2ms deadline.
                const auto quantum=std::size_t(job.gpuCoarse?128:32);
                const auto count=gpuCoverage?std::min(quantum,limit-gpuPrimitives):std::size_t(32);
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
            if(job.key.region&&job.geometry&&job.geometry->isContour()&&!job.sharedBody&&!job.projectedBody)
                bodyLayers_->rememberNavigationBody(job.geometry,*draw,job.coverage->makeImageSnapshot(),options,budget);
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
        ++job.source;job.knot=0;job.sourceInitialized=false;job.geometry.reset();job.sharedBody.reset();job.projectedBody=false;++work.sourceSteps;
    }
    NS_IF(if(!deferred&&!gpuBlocked){if(gpuPrimitives>=gpuLimit)ns::gate(ns::GpuCommandCap,queries,initializations,steps,gpuPrimitives);else if(steps>=std::max<std::size_t>(1,options.navigationMaxSteps))ns::gate(ns::StepCap,queries,initializations,steps,gpuPrimitives);});
    if(deferred||expired(checkpoint(job_.get())))++work.budgetYields;
    nsPreparation.reset();
    // Cold start may acquire complete coverage in this opportunity. At most
    // one feedback present per call, including refinements completed above.
    // Publications already updated the partition; do not recompute unchanged dependencies.
    work.pending=bool(job_)||bool(refinement_)||!work.coverageComplete||qualityOutsideWorkset;
    for(const auto&p:patches)if(!p.plainBackground&&scale/p.density>1.25)work.pending=true;
    if(dirty_&&work.coverageComplete&&!work.presented){
        const auto displayStarted=PC_NAV_NOW();
        gpu.touch();
        G5_ROUTE(g5NavCompose,g5::Route::nav_retained);
        G5_TARGET(acquired.frame.surface,true);
        compose(*target,patches,camera,frame);
        if(drawOverlay)drawOverlay(*target);
        provider.release();release.active=false;
        if(![&]{NS_SCOPE(nsFlush,FlushSubmit);return gpu.finish();}())return ns::result(BackendSubmissionResult::rejected("world navigation GPU submission failed"));
        const auto result=[&]{NS_SCOPE(nsPresent,Present);return provider.present();}();
        G5_OBSERVE(g5::Kind::Submit,g5::Tag::navigation,true,frame.viewId.value(),scene,frame.sceneGeneration.value(),frame.surfaceGeneration.value(),frame.metricsGeneration.value(),g5::bits(camera.scale),g5::bits(camera.translationX),g5::bits(camera.translationY),static_cast<unsigned>(result.code),work.coverageComplete);
        if(result.code!=BackendSubmissionCode::kAccepted)return ns::result(result);
        NS_IF(ns::state.row->presentSequence=++ns::state.present);
        ZQ_PRESENT(&provider,frame,camera,resourceEpoch,"navigation",budget,true);
        const double displayUs=std::chrono::duration<double,std::micro>(PC_NAV_NOW()-displayStarted).count();
        displayCosts_[displayClass(patches.size())].observe(displayUs);
        displayLastUs_=displayUs;displayEwmaUs_=displayEstimateValid_?.75*displayEwmaUs_+.25*displayUs:displayUs;displayEstimateValid_=true;
        work.presented=true;dirty_=false;
    }
    // Do not report convergence before completed finer pixels reach present.
    if(work.knotSteps+work.sourceSteps+work.tilesBuilt>usefulBefore)qualityAge_=0;
    work.pending|=dirty_;
    for(auto&cursor:streamingCursor_)cursor+=std::min(maximumTiles,demandLimit);
    last_=std::make_unique<FrameState>(frame);fallbackFocus_=focus;camera_=camera;scene_=scene;
    if(![&]{NS_SCOPE(nsFlush,FlushSubmit);return gpu.finish();}())return ns::result(BackendSubmissionResult::rejected("navigation preparation GPU submission failed"));
    return ns::result(BackendSubmissionResult::accepted());
}
}
