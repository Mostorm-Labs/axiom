#include "skia_navigation_tiles.hpp"
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
    // Nonidentity erased ink retains its existing fail-closed fallback until
    // its isolated-mask realization is qualified. Plain affine ink is safe.
    if(std::holds_alternative<CompactStrokeReferenceCommand>(e.command))return identity||e.record.eraseMasks.empty();
    const auto* v=std::get_if<BrushStrokeReferenceCommand>(&e.command);
    if(v&&v->content.stroke.snapshot.material_mode==1)return identity;
    return std::holds_alternative<ShapeReferenceCommand>(e.command) ||
        std::holds_alternative<ImageReferenceCommand>(e.command) ||
        std::holds_alternative<StickyReferenceCommand>(e.command) ||
        std::holds_alternative<TableReferenceCommand>(e.command) ||
        std::holds_alternative<RichTextReferenceCommand>(e.command);
}

std::pair<std::size_t, std::size_t> compactRange(const ReferenceTraversalEntry& entry) {
    const auto* compact = std::get_if<CompactStrokeReferenceCommand>(&entry.command);
    if (compact == nullptr || !entry.navigationChunkSpan) return {0, compact == nullptr ? 0U : compact->content.stroke.knots.size()};
    const auto& knots = compact->content.stroke.knots;
    // V4 locality selects original paired contour primitives from prepared
    // geometry. A raw knot subspan would change its stateful anchor history.
    if (compact->content.stroke.config.algorithmVersion == 4U)
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
void SkiaNavigationTiles::cancel(){
    if(job_){
        (void)scheduler_.cancel(job_->id,1);
        ++work.cancelledJobs;work.discardedKnotSteps+=job_->steps;
    }
    job_.reset();
}
bool SkiaNavigationTiles::coverage(std::span<const TileKey> keys,std::uint64_t scene,std::vector<Patch>& patches){
    const auto ready=[&](TileKey key){auto it=tiles_.find(key);return it!=tiles_.end()&&it->second.image&&it->second.checkedScene==scene;};
    const auto resolve=[&](auto&& self,TileKey key,unsigned depth)->bool{
        const auto area=grid_.bounds(key);
        if(ready(key)){patches.push_back({key,area});return true;}
        std::optional<TileKey> parent;
        for(const auto& [candidate,tile]:tiles_){
            if(!ready(candidate)||candidate.scale>=key.scale||!intersects(grid_.bounds(candidate),area))continue;
            if(!parent||candidate.scale>parent->scale)parent=candidate;
        }
        if(parent){patches.push_back({*parent,area});++work.fallbackUses;return true;}
        if(depth==3)return false;
        const auto before=patches.size();
        for(int y=0;y<2;++y)for(int x=0;x<2;++x)
            if(!self(self,{key.x*2+x,key.y*2+y,key.scale+1},depth+1)){patches.resize(before);return false;}
        return true;
    };
    for(const auto key:keys)if(!resolve(resolve,key,0)){patches.clear();return false;}
    return true;
}
void SkiaNavigationTiles::compose(SkCanvas& canvas,std::span<const Patch> patches,
    const CanonicalViewportTransform& camera,const FrameState& frame){
    canvas.save();canvas.resetMatrix();canvas.clear(SK_ColorWHITE);
    const double sx=frame.metrics.physicalWidth/frame.metrics.logicalWidth;
    const double sy=frame.metrics.physicalHeight/frame.metrics.logicalHeight;
    for(const auto& patch:patches){
        auto& tile=tiles_.at(patch.source);tile.used=tick_;const auto b=grid_.bounds(patch.source);
        const auto sourceScale=std::ldexp(1.,patch.source.scale);const auto& a=patch.area;
        const auto dst=SkRect::MakeLTRB(float((a.left*camera.scale+camera.translationX)*sx),
            float((a.top*camera.scale+camera.translationY)*sy),float((a.right*camera.scale+camera.translationX)*sx),
            float((a.bottom*camera.scale+camera.translationY)*sy));
        const auto src=SkRect::MakeLTRB(float((a.left-b.left)*sourceScale+gutter),float((a.top-b.top)*sourceScale+gutter),
            float((a.right-b.left)*sourceScale+gutter),float((a.bottom-b.top)*sourceScale+gutter));
        canvas.drawImageRect(tile.image,src,dst,SkSamplingOptions(SkFilterMode::kLinear),nullptr,SkCanvas::kFast_SrcRectConstraint);
    }
    canvas.restore();
}
BackendSubmissionResult SkiaNavigationTiles::render(SkiaSurfaceProvider& provider,
    const CanonicalViewportTransform& camera,const FrameState& frame,const NavigationSourceQuery& query,
    std::uint64_t resourceEpoch,const RenderCacheOptions& options,
    std::shared_ptr<resources::ResourceBudgetManager> budget,bool allowPrewarm,const std::function<bool()>& retainedFeedback, SkiaTextResources* textResources, ImageResourceResolver* images,
    const std::function<void(SkCanvas&)>& drawOverlay){
    const auto started=std::chrono::steady_clock::now();
    const auto before=work;
    struct Timing {
        NavigationWork& work;const NavigationWork before;std::chrono::steady_clock::time_point started;
        const FrameState& frame;const CanonicalViewportTransform& camera;const RenderCacheOptions& options;
        PreparedGeometryCache& geometry;
        ~Timing(){
            work.preparedGeometryBytes=geometry.statistics().residentBytes;
            const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count();
            work.opportunityNanoseconds+=elapsed;
            if(std::uint64_t(elapsed)>options.navigationMicroseconds*1000)++work.overBudget;
#if defined(AXIOM_ENABLE_FRAME_TIMING)
            std::fprintf(stderr,"AXIOM_NAVIGATION_TIMING {\"version\":2,\"scene\":\"%llu\",\"surface\":\"%llu\",\"scale\":%.6f,\"level\":%d,\"elapsed_ns\":%llu,\"query_ns\":%llu,\"initialization_ns\":%llu,\"queries\":%llu,\"initialized\":%llu,\"knots\":%llu,\"cancelled\":%llu,\"discarded_knots\":%llu,\"throttled\":%llu,\"prewarm\":%llu,\"built\":%llu,\"fallback\":%llu,\"prefix_knots\":%llu,\"overlay_ns\":%llu,"
                "\"prepared_builds\":%llu,\"prepared_hits\":%llu,\"prepared_denied\":%llu,\"prepared_bytes\":%llu,\"contour_tested\":%llu,\"contour_drawn\":%llu,\"vector_prepared\":%llu,\"gpu_contours\":%llu,\"coarse_masks\":%llu,\"presented\":%s,\"coverage\":%s}\n",
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
                work.presented?"true":"false",work.coverageComplete?"true":"false");
#endif
        }
    } timing{work,before,started,frame,camera,options,geometry_};
    work.presented=false;work.pending=false;work.coverageComplete=false;
    if(!query||!options.enabled||!budget||!std::isfinite(camera.scale)||camera.scale<=0||
       !std::isfinite(camera.translationX)||!std::isfinite(camera.translationY)||
       frame.metrics.logicalWidth<=0||frame.metrics.logicalHeight<=0)
        return BackendSubmissionResult::retry("world navigation ineligible");
    if(provider_!=&provider||surface_!=frame.surfaceGeneration.value()||resource_!=resourceEpoch||
       (last_&&(last_->viewId!=frame.viewId||last_->metrics!=frame.metrics||last_->metricsGeneration!=frame.metricsGeneration))){
        cancel();tiles_.clear();sources_.clear();geometry_.clear();gpuBatches_.clear();work.retainedBytes=0;last_.reset();dirty_=true;level_.reset();
        provider_=&provider;surface_=frame.surfaceGeneration.value();resource_=resourceEpoch;
    }
    const auto scene=frame.sceneReadToken.value();
    const bool demandChanged=!last_||scene_!=scene||camera_.scale!=camera.scale||
        camera_.translationX!=camera.translationX||camera_.translationY!=camera.translationY;
    if(demandChanged){if(scene_!=scene||(job_&&job_->prewarm))cancel();dirty_=true;warmed_=0;}
    const double scale=camera.scale*frame.metrics.physicalWidth/frame.metrics.logicalWidth;
    if(!std::isfinite(scale)||scale<1./65536||scale>65536)return BackendSubmissionResult::retry("world navigation scale range");
    int level=std::clamp(int(std::ceil(std::log2(scale))),-16,16);
    if(level_&&scale>=std::ldexp(.85,*level_-1)&&scale<=std::ldexp(1.15,*level_))level=*level_;
    const foundation::WorldRect visible{-camera.translationX/camera.scale,-camera.translationY/camera.scale,
        (frame.metrics.logicalWidth-camera.translationX)/camera.scale,(frame.metrics.logicalHeight-camera.translationY)/camera.scale};
    const auto maximumTiles=std::min(options.maxEntries,options.maxBytes/(tileBytes*3));
    // Prefer the nearest coarse level that covers the viewport in four cells.
    // This bounds cold coverage work without querying an unnecessarily large
    // world region or depending on an incomplete retained screenshot.
    const auto coarseKeys=[&](int detail){
        int coarse=std::max(-16,detail-1);auto cover=grid_.coverage(visible,coarse,256);
        while(coarse>-16&&(cover.empty()||cover.size()>4)){--coarse;cover=grid_.coverage(visible,coarse,256);}
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
    if(job_&&!required(job_->key)&&(!job_->prewarm||demandChanged))cancel();
    auto acquired=provider.acquire();
    if(acquired.code!=SkiaSurfaceAcquireCode::kAcquired||!acquired.frame.surface)return BackendSubmissionResult::retry("world navigation target unavailable");
    struct Release {SkiaSurfaceProvider& provider;bool active=true;~Release(){if(active)provider.release();}} release{provider};
    const auto valid=[&]{return acquired.frame.generation==frame.surfaceGeneration.value()&&acquired.frame.surface->width()==int(frame.metrics.physicalWidth)&&acquired.frame.surface->height()==int(frame.metrics.physicalHeight);};
    if(!valid())return BackendSubmissionResult::rejected("stale world navigation target");
    auto* target=acquired.frame.surface->getCanvas();++tick_;
    auto* context=GrAsDirectContext(target->recordingContext());
    if(context)context->checkAsyncWorkCompletion();
    std::erase_if(gpuBatches_,[](const auto& p){return p->done.load();});
    std::vector<Patch> patches;
    work.coverageComplete=coverage(keys,scene,patches);
    // Feedback goes out before a single new Scene query or allocation.
    if(dirty_&&work.coverageComplete){
        compose(*target,patches,camera,frame);
        if(drawOverlay)drawOverlay(*target);
        provider.release();release.active=false;
        const auto result=provider.present();if(result.code!=BackendSubmissionCode::kAccepted)return result;
        work.presented=true;dirty_=false;++work.feedbackFirst;
    }else if(dirty_&&retainedFeedback){
        provider.release();release.active=false;
        if(retainedFeedback()){work.presented=true;++work.feedbackFirst;}
    }
    if(!release.active){
        acquired=provider.acquire();if(acquired.code!=SkiaSurfaceAcquireCode::kAcquired||!acquired.frame.surface)return BackendSubmissionResult::retry("navigation reacquire failed");
        release.active=true;if(!valid())return BackendSubmissionResult::rejected("stale navigation reacquire");target=acquired.frame.surface->getCanvas();
    }
    const auto ready=[&](TileKey key){auto it=tiles_.find(key);return it!=tiles_.end()&&it->second.image&&it->second.checkedScene==scene;};
    // A newly exposed frontier owns priority over disposable refinement.
    // Preserve coarse jobs across camera movement inside their world region;
    // GPU batches keep their existing bounded completion/backpressure policy.
    if(!work.coverageComplete&&job_&&job_->key.scale>parents.front().scale)cancel();
    // Before first complete coverage, admit several cheap cells within the
    // same wall-time allowance. Once feedback exists, keep setup to one unit.
    const auto queryLimit=work.presented?std::min(std::size_t(1),options.navigationMaxQueries):options.navigationMaxQueries;
    const auto initializationLimit=std::max(std::size_t(1),queryLimit);
    std::size_t steps=0,queries=0,initializations=0,gpuPrimitives=0;bool deferred=false,submitted=false;
    const auto gpuLimit=std::max(std::size_t(1),options.navigationMaxGpuContourPrimitives);
    const auto expired=[&]{return std::chrono::steady_clock::now()-started>=std::chrono::microseconds(options.navigationMicroseconds);};
    std::map<TileKey,std::shared_ptr<const Sources>> desired;
    auto load=[&](TileKey key)->bool{
        if(desired.contains(key))return true;
        if(const auto cached=sources_.find(key);cached!=sources_.end()&&cached->second->scene==scene){desired.emplace(key,cached->second);return true;}
        if(queries>=queryLimit||expired()){deferred=true;return false;}
        ++queries;++work.queries;const auto start=std::chrono::steady_clock::now();
        std::vector<ReferenceTraversalEntry> sources;
        const bool ok=query(expanded(grid_.bounds(key),std::ldexp(double(gutter),-key.scale)),sources);
        work.queryNanoseconds+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
        if(!ok||std::any_of(sources.begin(),sources.end(),[](const auto& e){return !supported(e);}))return false;
        std::unordered_set<foundation::ObjectId,foundation::ObjectIdHash> ids;
        for(const auto& entry:sources)if(entry.record.objectId.isZero()||!ids.insert(entry.record.objectId).second)return false;
        ReferenceDrawList validation{.frame=frame,.entries={},.canonicalBytes={},.digest={}};
        validation.viewportClip={0,0,frame.metrics.logicalWidth,frame.metrics.logicalHeight};validation.entries=sources;
        if(validateReferencePlan(FramePlan{frame,std::move(validation)}).code!=BackendSubmissionCode::kAccepted)return false;
        std::vector<std::shared_ptr<const PreparedNavigationGeometry>> geometry;
        geometry.reserve(sources.size());
        for(const auto& entry:sources){
            const auto before=geometry_.statistics();
            geometry.push_back(geometry_.get(entry,options,budget));
            const auto after=geometry_.statistics();
            work.preparedGeometryBuilds+=after.builds-before.builds;
            work.preparedGeometryHits+=after.hits-before.hits;
            work.preparedGeometryDenied+=after.denied-before.denied;
            if(after.builds>before.builds)
                if(const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command))
                    work.prefixScanKnots+=compact->content.stroke.knots.size();
        }
        std::size_t bytes=(sizeof(ReferenceTraversalEntry)+sizeof(std::shared_ptr<const PreparedNavigationGeometry>))*sources.size();
        for(const auto& entry:sources){
            if(const auto* c=std::get_if<CompactStrokeReferenceCommand>(&entry.command))bytes+=c->content.stroke.knots.size()*sizeof(foundation::InkKnot)*2;
            if(const auto* v=std::get_if<BrushStrokeReferenceCommand>(&entry.command))bytes+=v->content.stroke.vector_output.outline.size()*sizeof(semantic::Vec2)*2+v->content.stroke.confirmed_samples.size()*sizeof(semantic::BrushConfirmedSample)*2;
        }
        auto token=budget->reserve(resources::ResourceClass::kRecorded,bytes,true);
        for(auto it=sources_.begin();it!=sources_.end()&&!token;){
            if(it->second.use_count()==1){it=sources_.erase(it);token=budget->reserve(resources::ResourceClass::kRecorded,bytes,true);}else ++it;
        }
        if(!token){deferred=true;return false;}
        if(sources_.size()>=options.maxEntries)sources_.erase(sources_.begin());
        auto source=std::make_shared<const Sources>(Sources{scene,std::move(sources),std::move(geometry),resources::ResourceLease(budget,*token)});
        sources_.insert_or_assign(key,source);desired.emplace(key,std::move(source));return true;
    };
    // Revalidate only possible visible fallback cells, at the same query budget.
    for(auto it=tiles_.begin();it!=tiles_.end();){
        if(it->second.checkedScene==scene||!intersects(grid_.bounds(it->first),visible)){++it;continue;}
        if(!load(it->first)){if(deferred)break;return BackendSubmissionResult::retry("world navigation revalidation failed");}
        if(it->second.sources->entries!=desired.at(it->first)->entries){work.retainedBytes-=tileBytes*3;it=tiles_.erase(it);dirty_=true;}
        else{it->second.checkedScene=scene;++it;}
    }
    const bool gpuBlocked=context&&gpuBatches_.size()>=2;
    if(gpuBlocked)++work.gpuThrottles;
    const auto protectedTile=[&](TileKey key){return required(key)||std::any_of(patches.begin(),patches.end(),[&](const auto& p){return p.source==key;});};
    const auto evict=[&]()->bool{
        auto victim=tiles_.end();
        for(auto it=tiles_.begin();it!=tiles_.end();++it)if(!protectedTile(it->first)&&(victim==tiles_.end()||it->second.used<victim->second.used))victim=it;
        if(victim==tiles_.end())return false;
        work.retainedBytes-=tileBytes*3;tiles_.erase(victim);++work.evictions;return true;
    };
    while(!deferred&&!gpuBlocked&&gpuPrimitives<gpuLimit&&steps<std::max<std::size_t>(1,options.navigationMaxSteps)&&!expired()){
        if(!job_){
            std::optional<TileKey> next;bool prewarm=false;
            for(const auto key:parents)if(!ready(key)){next=key;break;}
            if(!next)for(const auto key:keys)if(!ready(key)){next=key;break;}
            if(!next&&allowPrewarm&&warmed_<options.navigationPrewarmTiles){
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
            if(tileBytes>options.maxEntryBytes||tileBytes*3>options.maxBytes)return BackendSubmissionResult::retry("world navigation tile budget");
            while((work.retainedBytes+tileBytes*3>options.maxBytes||tiles_.size()>=options.maxEntries)&&evict()){}
            if(work.retainedBytes+tileBytes*3>options.maxBytes||tiles_.size()>=options.maxEntries){deferred=true;break;}
            auto token=budget->reserve(resources::ResourceClass::kTile,tileBytes*3,true);
            while(!token&&evict())token=budget->reserve(resources::ResourceClass::kTile,tileBytes*3,true);
            if(!token){deferred=true;break;}
            const auto initStart=std::chrono::steady_clock::now();
            auto job=std::make_unique<Job>();job->key=*next;job->sources=desired.at(*next);job->prewarm=prewarm;
            job->lease=resources::ResourceLease(budget,*token);
            job->surface=target->makeSurface(acquired.frame.surface->imageInfo().makeWH(extent,extent));
            // Coarse cells collect whole strokes, including many overlapping
            // Lighten ribbons. Rasterize this bounded coverage mask in Skia
            // CPU memory and upload the completed mask; otherwise a small CPU
            // opportunity can enqueue enough GPU path work to stall RAF itself.
            // The material, scene tile and final presentation stay on the GPU.
            const auto coverageInfo=SkImageInfo::MakeN32Premul(extent,extent);
            job->cpuCoverage=next->scale<level&&std::all_of(job->sources->entries.begin(),job->sources->entries.end(),[](const auto& entry){
                const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command);
                return !compact||compact->content.stroke.config.algorithmVersion==4;
            });
            job->coverage=job->cpuCoverage?SkSurfaces::Raster(coverageInfo):target->makeSurface(coverageInfo);
            if(!job->surface||!job->coverage){deferred=true;break;}
            job->surface->getCanvas()->clear(SK_ColorTRANSPARENT);
            const auto b=grid_.bounds(*next);const float s=std::ldexp(1.F,next->scale);
            job->surface->getCanvas()->translate(gutter-b.left*s,gutter-b.top*s);job->surface->getCanvas()->scale(s,s);
            job->id=++serial_;
            if(!scheduler_.submit({job->id,1,prewarm?scheduling::WorkPriority::kPrefetch:scheduling::WorkPriority::kVisible,0,{},tileBytes*3})||!scheduler_.take())return BackendSubmissionResult::retry("world navigation admission failed");
            job_=std::move(job);++initializations;++work.initializations;
            work.initializationNanoseconds+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-initStart).count();
            submitted=true;
            if(expired()){deferred=true;break;}
        }
        auto& job=*job_;
        if(job.source>=job.sources->entries.size()){
            auto image=job.surface->makeImageSnapshot();if(!image)return BackendSubmissionResult::retry("world navigation snapshot failed");
            (void)scheduler_.finish(job.id,1);
            if(job.prewarm){++warmed_;++work.prewarmTiles;}
            tiles_.emplace(job.key,Tile{std::move(job.sources),std::move(image),tick_,scene,std::move(job.lease)});
            work.retainedBytes+=tileBytes*3;job_.reset();dirty_=true;++work.tilesBuilt;++steps;continue;
        }
    const auto& entry=job.sources->entries[job.source];submitted=true;
        if(const auto* c=std::get_if<CompactStrokeReferenceCommand>(&entry.command)){
            const auto& stroke=c->content.stroke;auto* draw=job.coverage->getCanvas();
            if(!job.sourceInitialized){
                if(job.cpuCoverage)++work.coarseMaskStarts;
                const auto range = compactRange(entry);
                if (range.second <= range.first) { ++job.source; continue; }
                job.knot = range.first;
                draw->resetMatrix();draw->clear(SK_ColorBLACK);draw->setMatrix(job.surface->getCanvas()->getTotalMatrix());
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
                job.totalDistance = !distances.empty()?distances.back():ink::compactLength(stroke.knots);
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
                const bool gpu=context&&!job.cpuCoverage;
                const auto count=gpu?std::min(std::size_t(32),gpuLimit-gpuPrimitives):std::size_t(32);
                const auto drawn=job.geometry->drawContour(*draw,job.primitive,count);
                work.contourPrimitivesTested+=drawn.primitivesTested;
                work.contourPrimitivesDrawn+=drawn.primitivesDrawn;
                if(gpu){work.gpuContourPrimitives+=drawn.primitivesDrawn;gpuPrimitives+=drawn.primitivesDrawn;}
                job.primitive=std::min(job.primitive+count,job.geometry->primitiveCount());
                if(job.primitive<job.geometry->primitiveCount()){++steps;continue;}
                end = range.second;
            } else if(job.contour){
                ink::CompactContour contours;job.contour->append({stroke.knots.data()+job.knot,end-job.knot},job.totalDistance,contours);
                if(end==stroke.knots.size())job.contour->finish(job.totalDistance,contours);
                drawCompactContourCoverageToSkCanvas(*draw,stroke.config,contours);
            }else drawCompactCoverageRangeToSkCanvas(*draw,stroke.config,stroke.knots,job.knot,end,job.distance,job.totalDistance,stroke.seed);
            const auto distances=job.geometry?job.geometry->distances():std::span<const double>{};
            if(end<distances.size())job.distance=distances[end];
            else for(auto i=std::max(std::size_t(1),job.knot);i<end;++i)
                job.distance+=std::hypot(stroke.knots[i].x-stroke.knots[i-1].x,
                                         stroke.knots[i].y-stroke.knots[i-1].y);
            work.knotSteps+=end-job.knot;job.steps+=end-job.knot;job.knot=end;++steps;
            if(job.knot<range.second)continue;
            auto* material=job.surface->getCanvas();material->save();
            const auto& t=entry.record.transform;
            material->concat(SkMatrix::MakeAll(float(t.a),float(t.c),float(t.tx),float(t.b),float(t.d),float(t.ty),0,0,1));
            compositeCompactCoverageToSkCanvas(*material,*job.coverage->makeImageSnapshot(),stroke.config,stroke.seed);
            if (!entry.record.eraseMasks.empty())
                drawEraseMasksToSkCanvas(*material, entry);
            material->restore();
        }else{
            const auto& geometry=job.sources->geometry[job.source];
            const auto* path=geometry?geometry->vectorPath():nullptr;
            drawNavigationEntry(*job.surface->getCanvas(),entry,textResources,images,path);
            if(path)++work.vectorPreparedUses;
        }
        ++job.source;job.knot=0;job.sourceInitialized=false;job.geometry.reset();++work.sourceSteps;
    }
    if(context&&submitted){
        auto completion=std::make_shared<Completion>();
        auto* ticket=new std::shared_ptr<Completion>(completion);
        GrFlushInfo info;info.fFinishedContext=ticket;
        info.fFinishedProc=[](void* raw){auto* p=static_cast<std::shared_ptr<Completion>*>(raw);(*p)->done.store(true);delete p;};
        gpuBatches_.push_back(std::move(completion));context->flush(info);(void)context->submit(GrSyncCpu::kNo);
    }
    if(deferred||expired())++work.budgetYields;
    work.pending=bool(job_);
    for(const auto key:parents)work.pending|=!ready(key);
    for(const auto key:keys)work.pending|=!ready(key);
    // Cold start may acquire complete coverage in this opportunity. At most
    // one feedback present per call; new refinements wait for the next frame.
    patches.clear();work.coverageComplete=coverage(keys,scene,patches);
    if(dirty_&&work.coverageComplete&&!work.presented){
        compose(*target,patches,camera,frame);
        if(drawOverlay)drawOverlay(*target);
        provider.release();release.active=false;
        const auto result=provider.present();if(result.code!=BackendSubmissionCode::kAccepted)return result;
        work.presented=true;dirty_=false;
    }
    last_=std::make_unique<FrameState>(frame);camera_=camera;scene_=scene;
    return BackendSubmissionResult::accepted();
}
}
