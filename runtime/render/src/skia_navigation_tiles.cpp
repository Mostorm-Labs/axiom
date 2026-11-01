#include "skia_navigation_tiles.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <unordered_set>

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
    if(t.a!=1||t.b!=0||t.c!=0||t.d!=1||t.tx!=0||t.ty!=0||!e.record.eraseMasks.empty())return false;
    if(std::holds_alternative<CompactStrokeReferenceCommand>(e.command))return true;
    const auto* v=std::get_if<BrushStrokeReferenceCommand>(&e.command);
    return v&&v->content.stroke.snapshot.material_mode==1;
}
}
void SkiaNavigationTiles::cancel(){
    if(job_)(void)scheduler_.cancel(job_->id,1);
    job_.reset();
}
BackendSubmissionResult SkiaNavigationTiles::render(SkiaSurfaceProvider& provider,
    const CanonicalViewportTransform& camera,const FrameState& frame,const NavigationSourceQuery& query,
    std::uint64_t resourceEpoch,const RenderCacheOptions& options,
    std::shared_ptr<resources::ResourceBudgetManager> budget){
    work.presented=false;work.pending=false;work.coverageComplete=false;
    if(!query||!options.enabled||!std::isfinite(camera.scale)||camera.scale<=0||
       !std::isfinite(camera.translationX)||!std::isfinite(camera.translationY)||
       frame.metrics.logicalWidth<=0||frame.metrics.logicalHeight<=0)
        return BackendSubmissionResult::retry("world navigation ineligible");
    if(provider_!=&provider||surface_!=frame.surfaceGeneration.value()||resource_!=resourceEpoch||
       (last_&&(last_->viewId!=frame.viewId||last_->metrics!=frame.metrics||last_->metricsGeneration!=frame.metricsGeneration))){
        cancel();tiles_.clear();sources_.clear();work.retainedBytes=0;last_.reset();dirty_=true;
        provider_=&provider;surface_=frame.surfaceGeneration.value();resource_=resourceEpoch;
    }
    const bool demandChanged=!last_||last_->viewId!=frame.viewId||last_->metrics!=frame.metrics||
        last_->metricsGeneration!=frame.metricsGeneration||scene_!=frame.sceneReadToken.value()||
        camera_.scale!=camera.scale||camera_.translationX!=camera.translationX||camera_.translationY!=camera.translationY;
    if(demandChanged){
        if(scene_!=frame.sceneReadToken.value())cancel();
        dirty_=true;
    }
    const double ratio=frame.metrics.physicalWidth/frame.metrics.logicalWidth;
    const double scale=camera.scale*ratio;
    if(!std::isfinite(scale)||scale<=0||scale<1./65536||scale>65536)
        return BackendSubmissionResult::retry("world navigation scale range");
    int level=std::clamp(int(std::ceil(std::log2(scale))),-16,16);
    const foundation::WorldRect visible{
        -camera.translationX/camera.scale,-camera.translationY/camera.scale,
        (frame.metrics.logicalWidth-camera.translationX)/camera.scale,
        (frame.metrics.logicalHeight-camera.translationY)/camera.scale};
    const auto maximumTiles=std::min(options.maxEntries,options.maxBytes/(tileBytes*3));
    auto keys=grid_.coverage(visible,level,256);
    auto parents=grid_.coverage(visible,level-1,256);
    // Feedback resolution is disposable. Retina metrics and crossing a scale
    // bucket must not turn a bounded navigation opportunity into Direct raster
    // of the whole scene. Coarsen until both planes can reside simultaneously.
    while(level>-16&&(keys.empty()||parents.empty()||keys.size()+parents.size()>maximumTiles)){
        --level;keys=grid_.coverage(visible,level,256);parents=grid_.coverage(visible,level-1,256);
    }
    if(keys.empty()||parents.empty()||keys.size()+parents.size()>maximumTiles)
        return BackendSubmissionResult::retry("world navigation residency cannot converge");
    // Query the complete tile bounds (including filter gutter), never the old
    // viewport snapshot. Queries own Scene traversal and return immutable data.
    std::map<TileKey,std::shared_ptr<const Sources>> desired;
    auto load=[&](TileKey key)->bool {
        if(desired.contains(key))return true;
        if(const auto cached=sources_.find(key);cached!=sources_.end()&&cached->second->scene==frame.sceneReadToken.value()){
            desired.emplace(key,cached->second);return true;
        }
        std::vector<ReferenceTraversalEntry> sources;
        if(!query(expanded(grid_.bounds(key),std::ldexp(double(gutter),-key.scale)),sources))return false;
        if(std::any_of(sources.begin(),sources.end(),[](const auto& e){return !supported(e);}))return false;
        std::unordered_set<foundation::ObjectId,foundation::ObjectIdHash> identities;
        for(const auto& entry:sources)if(entry.record.objectId.isZero()||!identities.insert(entry.record.objectId).second)return false;
        ReferenceDrawList validationList{.frame=frame,.entries={},.canonicalBytes={},.digest={}};validationList.viewportClip={0,0,frame.metrics.logicalWidth,frame.metrics.logicalHeight};
        validationList.entries=sources;
        const FramePlan validationPlan{frame,std::move(validationList)};
        if(validateReferencePlan(validationPlan).code!=BackendSubmissionCode::kAccepted)return false;
        std::size_t bytes=sizeof(ReferenceTraversalEntry)*sources.size();
        for(const auto& entry:sources){
            if(const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command))
                bytes+=compact->content.stroke.knots.size()*sizeof(foundation::InkKnot)*2;
            if(const auto* vector=std::get_if<BrushStrokeReferenceCommand>(&entry.command))
                bytes+=vector->content.stroke.vector_output.outline.size()*sizeof(semantic::Vec2)*2+
                    vector->content.stroke.confirmed_samples.size()*sizeof(semantic::BrushConfirmedSample)*2;
        }
        auto token=budget->reserve(resources::ResourceClass::kRecorded,bytes,true);
        if(!token){
            // Drop only unreferenced source rows; live tiles/jobs retain their
            // shared lease. No canonical data is admitted by this budget.
            for(auto it=sources_.begin();it!=sources_.end()&&!token;){
                if(it->second.use_count()==1){it=sources_.erase(it);token=budget->reserve(resources::ResourceClass::kRecorded,bytes,true);}
                else ++it;
            }
        }
        if(!token)return false;
        if(sources_.size()>=options.maxEntries)sources_.erase(sources_.begin());
        auto entries=std::make_shared<const Sources>(Sources{frame.sceneReadToken.value(),std::move(sources),resources::ResourceLease(budget,*token)});
        sources_.insert_or_assign(key,entries);
        desired.emplace(key,std::move(entries));return true;
    };
    // A single coarser plane supplies complete world coverage before sharpening.
    // Its cells remain world anchored and are independently replaceable.
    if(job_&&std::find(keys.begin(),keys.end(),job_->key)==keys.end()&&
       std::find(parents.begin(),parents.end(),job_->key)==parents.end())cancel();
    std::erase_if(tiles_,[](const auto& row){return !row.second.image;});
    const auto scene=frame.sceneReadToken.value();
    for(auto& [key,tile]:tiles_){
        if(tile.checkedScene==scene)continue;
        if(!load(key))return BackendSubmissionResult::retry("world navigation revalidation failed");
        if(tile.sources->entries!=desired.at(key)->entries){tile.image.reset();tile.sources.reset();tile.lease.reset();work.retainedBytes-=tileBytes*3;dirty_=true;}
        tile.checkedScene=scene;
    }
    std::erase_if(tiles_,[](const auto& row){return !row.second.image;});
    const auto acquired=provider.acquire();
    if(acquired.code!=SkiaSurfaceAcquireCode::kAcquired||!acquired.frame.surface)
        return BackendSubmissionResult::retry("world navigation target unavailable");
    struct Release {SkiaSurfaceProvider& p;bool active=true;~Release(){if(active)p.release();}} release{provider};
    if(acquired.frame.generation!=frame.surfaceGeneration.value()||
       acquired.frame.surface->width()!=int(frame.metrics.physicalWidth)||
       acquired.frame.surface->height()!=int(frame.metrics.physicalHeight))
        return BackendSubmissionResult::rejected("stale world navigation target");
    auto* target=acquired.frame.surface->getCanvas();
    ++tick_;
    auto ready=[&](TileKey key){auto it=tiles_.find(key);return it!=tiles_.end()&&bool(it->second.image);};
    const auto start=std::chrono::steady_clock::now();unsigned steps=0;
    auto stop=[&]{return steps>=std::max<std::size_t>(1,options.navigationMaxSteps)||std::chrono::steady_clock::now()-start>=std::chrono::milliseconds(3);};
    while(!stop()){
        if(!job_){
            std::optional<TileKey> next;
            // Coarse missing coverage first, then exact-level visible cells.
            for(const auto key:parents)if(!ready(key)){next=key;break;}
            if(!next)for(const auto key:keys)if(!ready(key)){next=key;break;}
            if(!next)break;
            if(tileBytes>options.maxEntryBytes||tileBytes*3>options.maxBytes)
                return BackendSubmissionResult::retry("world navigation tile budget");
            while(!tiles_.empty()&&(work.retainedBytes+tileBytes*3>options.maxBytes||tiles_.size()>=options.maxEntries)){
                auto victim=std::min_element(tiles_.begin(),tiles_.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});
                work.retainedBytes-=tileBytes*3;tiles_.erase(victim);++work.evictions;
            }
            auto token=budget->reserve(resources::ResourceClass::kTile,tileBytes*3,true);
            while(!token&&!tiles_.empty()){
                // Never evict required coverage to build another required cell:
                // shared-budget denial must fall back, not cycle forever.
                auto victim=tiles_.end();
                for(auto it=tiles_.begin();it!=tiles_.end();++it){
                    if(std::find(keys.begin(),keys.end(),it->first)!=keys.end()||
                       std::find(parents.begin(),parents.end(),it->first)!=parents.end())continue;
                    if(victim==tiles_.end()||it->second.used<victim->second.used)victim=it;
                }
                if(victim==tiles_.end())break;
                work.retainedBytes-=tileBytes*3;tiles_.erase(victim);++work.evictions;
                token=budget->reserve(resources::ResourceClass::kTile,tileBytes*3,true);
            }
            if(!token)return BackendSubmissionResult::retry("world navigation budget unavailable");
            resources::ResourceLease lease(budget,*token);
            if(!load(*next))return BackendSubmissionResult::retry("world navigation unsupported sources");
            auto job=std::make_unique<Job>();job->key=*next;job->sources=desired.at(*next);
            job->lease=std::move(lease);
            job->surface=target->makeSurface(acquired.frame.surface->imageInfo().makeWH(extent,extent));
            job->coverage=target->makeSurface(SkImageInfo::MakeN32Premul(extent,extent));
            if(!job->surface||!job->coverage)return BackendSubmissionResult::retry("world navigation allocation failed");
            job->surface->getCanvas()->clear(SK_ColorTRANSPARENT);
            const auto b=grid_.bounds(*next);const float s=std::ldexp(1.F,next->scale);
            job->surface->getCanvas()->translate(gutter-b.left*s,gutter-b.top*s);
            job->surface->getCanvas()->scale(s,s);
            job->id=++serial_;
            if(!scheduler_.submit({job->id,1,scheduling::WorkPriority::kVisible,0,{},tileBytes*3})||!scheduler_.take())
                return BackendSubmissionResult::retry("world navigation admission failed");
            job_=std::move(job);
        }
        auto& job=*job_;
        if(job.source>=job.sources->entries.size()){
            auto image=job.surface->makeImageSnapshot();if(!image){cancel();return BackendSubmissionResult::retry("world navigation snapshot failed");}
            const auto key=job.key;
            (void)scheduler_.finish(job.id,1);
            // Keep the conservative lease for surface/coverage/image residency.
            tiles_.emplace(key,Tile{std::move(job.sources),std::move(image),tick_,scene,std::move(job.lease)});
            work.retainedBytes+=tileBytes*3;job_.reset();++work.tilesBuilt;dirty_=true;++steps;continue;
        }
        const auto& entry=job.sources->entries[job.source];
        if(const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command)){
            const auto& stroke=compact->content.stroke;auto* coverage=job.coverage->getCanvas();
            if(job.knot==0){
                job.distance=0;job.totalDistance=ink::compactLength(stroke.knots);
                coverage->resetMatrix();coverage->clear(SK_ColorBLACK);
                coverage->setMatrix(job.surface->getCanvas()->getTotalMatrix());
            }
            const auto end=std::min(job.knot+32,stroke.knots.size());
            drawCompactCoverageRangeToSkCanvas(*coverage,stroke.config,stroke.knots,job.knot,end,job.distance,job.totalDistance);
            for(std::size_t i=std::max(std::size_t(1),job.knot);i<end;++i)
                job.distance+=std::hypot(stroke.knots[i].x-stroke.knots[i-1].x,stroke.knots[i].y-stroke.knots[i-1].y);
            work.knotSteps+=end-job.knot;job.knot=end;++steps;
            if(job.knot<stroke.knots.size())continue;
            compositeCompactCoverageToSkCanvas(*job.surface->getCanvas(),*job.coverage->makeImageSnapshot(),stroke.config,stroke.seed);
        }else drawValidatedIsolatedEntryToSkCanvas(*job.surface->getCanvas(),entry);
        ++job.source;job.knot=0;++work.sourceSteps;++steps;
    }
    work.pending=bool(job_);
    for(const auto key:parents)work.pending|=!ready(key);
    for(const auto key:keys)work.pending|=!ready(key);
    struct Patch {TileKey source;foundation::WorldRect area;};
    std::vector<Patch> patches;
    const auto resolve=[&](auto&& self,TileKey key,unsigned depth)->bool {
        const auto area=grid_.bounds(key);
        if(ready(key)){patches.push_back({key,area});return true;}
        std::optional<TileKey> parent;
        for(const auto& [candidate,tile]:tiles_){(void)tile;
            if(candidate.scale>=key.scale||!intersects(grid_.bounds(candidate),area))continue;
            if(!parent||candidate.scale>parent->scale)parent=candidate;
        }
        if(parent){patches.push_back({*parent,area});++work.fallbackUses;return true;}
        if(depth==3)return false;
        const auto before=patches.size();
        for(int y=0;y<2;++y)for(int x=0;x<2;++x)
            if(!self(self,{key.x*2+x,key.y*2+y,key.scale+1},depth+1)){
                patches.resize(before);return false;
            }
        return true;
    };
    bool completeCoverage=true;
    for(const auto key:keys)if(!resolve(resolve,key,0)){completeCoverage=false;break;}
    work.coverageComplete=completeCoverage;
    // Never replace an existing complete presentation with cold partial cells.
    // The previous feedback remains visible until a complete coarse plane exists.
    if(dirty_&&completeCoverage){
        target->save();target->resetMatrix();target->clear(SK_ColorWHITE);
        const double sx=frame.metrics.physicalWidth/frame.metrics.logicalWidth;
        const double sy=frame.metrics.physicalHeight/frame.metrics.logicalHeight;
        // Each target cell is composed once. Parent/child fallback is clipped
        // to disjoint interiors; gutter is sampled but never double blended.
        auto draw=[&](TileKey sourceKey,const foundation::WorldRect& area){
            auto& tile=tiles_.at(sourceKey);tile.used=tick_;const auto b=grid_.bounds(sourceKey);
            const auto sourceScale=std::ldexp(1.,sourceKey.scale);
            const auto dst=SkRect::MakeLTRB(float((area.left*camera.scale+camera.translationX)*sx),
                float((area.top*camera.scale+camera.translationY)*sy),float((area.right*camera.scale+camera.translationX)*sx),
                float((area.bottom*camera.scale+camera.translationY)*sy));
            const auto src=SkRect::MakeLTRB(float((area.left-b.left)*sourceScale+gutter),float((area.top-b.top)*sourceScale+gutter),
                float((area.right-b.left)*sourceScale+gutter),float((area.bottom-b.top)*sourceScale+gutter));
            target->drawImageRect(tile.image,src,dst,SkSamplingOptions(SkFilterMode::kLinear),nullptr,SkCanvas::kFast_SrcRectConstraint);
        };
        for(const auto& patch:patches)draw(patch.source,patch.area);
        target->restore();
        provider.release();release.active=false;
        const auto present=provider.present();if(present.code!=BackendSubmissionCode::kAccepted)return present;
        work.presented=true;dirty_=false;
    }
    last_=std::make_unique<FrameState>(frame);camera_=camera;scene_=scene;
    return BackendSubmissionResult::accepted();
}
}
