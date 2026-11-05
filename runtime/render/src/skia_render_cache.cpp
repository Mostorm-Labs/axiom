#include "canvas/foundation/internal/frame_timing.hpp"
#include "skia_render_cache_internal.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPictureRecorder.h"
#include "include/core/SkPaint.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrRecordingContext.h"
#include <algorithm>
#include <cmath>
#include <chrono>

namespace canvas::render::internal {
void SkiaRenderCache::clear() noexcept {entries_.clear();coverage_.clear();runs_.clear();work.retainedRunBytes=0;resetPreparation();coverageTargets_.clear();scratch_.reset();scratchLease_.reset();tiles_.reset();tileEntries_.clear();groups_.clear();groupSourceLeases_.clear();groupMetadataLease_.reset();frame_.reset();work.retainedBytes=0;work.coverageBytes=0;work.entries=0;}
bool SkiaRenderCache::releaseIdleWorkBuffers()noexcept{
    if(pending_)return false;
    scratch_.reset();scratchLease_.reset();coverageTargets_.clear();return true;
}
void SkiaRenderCache::begin(const FramePlan& plan,const void* target,SkSurface& surface,std::uint64_t resourceEpoch,foundation::ObjectId activeSource){
    const auto& f=plan.frame;
    const auto beforeRevision=frame_?frame_->sceneReadToken:foundation::SceneRevision{};
    if(!sourcePool)sourcePool=std::make_shared<SourceRetentionPool>();
    SourceRetentions candidateSources;resources::ResourceLease candidateMetadata;
    const auto retain=[&]()->bool{
        SourceRetentions holds;std::size_t bytes=0;
        for(const auto& source:plan.referenceDrawList.entries){
            auto retained=sourcePool->get(source,budget,true);if(!retained)return false;
            holds.insert(holds.end(),retained->begin(),retained->end());
            bytes+=sizeof(ReferenceTraversalEntry)+sourceMetadataBytes(source)+128;
        }
        bytes+=holds.capacity()*sizeof(SourceRetentions::value_type);
        const auto token=budget?budget->reserve(resources::ResourceClass::kRecorded,bytes,true):std::optional<std::uint64_t>{};
        if(!token)return false;
        candidateMetadata=resources::ResourceLease(budget,*token);candidateSources=std::move(holds);return true;
    };
    sourceAdmission_=retain();
    if(!sourceAdmission_){
        clear();sourceAdmission_=retain();
        if(!sourceAdmission_){
            // No unbudgeted source snapshots survive a failed admission. The
            // caller still owns this plan and executes permanent complete Direct.
            damage={beforeRevision,f.sceneReadToken,true,FullRedrawReason::kUnknownExtent,{},0,0,0};
            target_=target;surface_=&surface;frame_=std::make_unique<FrameState>(f);
            transform_=plan.referenceDrawList.worldToView;++targetEpoch_;return;
        }
    }
    const bool targetChanged=!frame_||target_!=target||surface_!=&surface||frame_->viewId!=f.viewId||
        frame_->surfaceGeneration!=f.surfaceGeneration||frame_->metrics!=f.metrics||
        frame_->metricsGeneration!=f.metricsGeneration;
    const bool changed=targetChanged||transform_!=plan.referenceDrawList.worldToView;
    coverageTargets_.budget=budget;
    if(targetChanged){coverageTargets_.clear();coverage_.clear();scratch_.reset();scratchLease_.reset();work.coverageBytes=0;}
    const auto previous=frame_?frame_->sceneReadToken:foundation::SceneRevision{};
    tiledFrame_=false;
    if(changed){tileEntries_.clear();if(tiles_)tiles_->invalidate();entries_.clear();work.retainedBytes=0;++targetEpoch_;}
    AXIOM_FRAME_TIMING_SCOPE(groupTiming,group_update);
    auto updates=groups_.update(plan.referenceDrawList.entries,plan.referenceDrawList.viewportClip,resourceEpoch);
    AXIOM_FRAME_TIMING_END(groupTiming);
    // Keep prior source leases until every retained group has rebound. Both
    // generations remain charged throughout this owner-thread transaction.
    groupSourceLeases_=std::move(candidateSources);groupMetadataLease_=std::move(candidateMetadata);
    if(changed||preparationBackground_!=plan.background||preparationTransparent_!=plan.transparentBackground||
       preparationIsolate_!=plan.isolateEraseMasks){runs_.clear();work.retainedRunBytes=0;}
    const auto runCount=runs_.size();
    std::erase_if(runs_,[&](const auto& run){
        return !validRun(run,plan)||(activeSource_!=activeSource&&
            std::any_of(run.stamps.begin(),run.stamps.end(),[&](const auto& stamp){return stamp.id==activeSource;}));
    });
    if(runCount!=runs_.size())resetPreparation();
    activeSource_=activeSource;
    work.retainedRunBytes=0;for(const auto& run:runs_)work.retainedRunBytes+=run.bytes;
    if(changed||!updates.contributions.empty()||preparationBackground_!=plan.background||
       preparationTransparent_!=plan.transparentBackground||preparationIsolate_!=plan.isolateEraseMasks){
        resetPreparation();
    }
    preparationBackground_=plan.background;preparationTransparent_=plan.transparentBackground;
    preparationIsolate_=plan.isolateEraseMasks;
    AXIOM_FRAME_TIMING_SCOPE(damageTiming,damage);
    damage=DamageTracker{}.collect(previous,f.sceneReadToken,updates.contributions,
        changed?FullRedrawReason::kViewChanged:FullRedrawReason::kNone);
    AXIOM_FRAME_TIMING_END(damageTiming);
    // Removed/cull-out groups never retain an unbounded raster representation.
    for(auto it=entries_.begin();it!=entries_.end();)if(!groups_.find(it->first)){
        work.retainedBytes-=it->second.bytes;it=entries_.erase(it);
    }else ++it;
    for(auto it=coverage_.begin();it!=coverage_.end();)if(!groups_.find(it->first)){
        work.coverageBytes-=it->second.bytes;it=coverage_.erase(it);
    }else ++it;
    if(options.tileCandidate){
        if(tileBackground_!=plan.background || tileClip_!=plan.referenceDrawList.viewportClip){
            tileEntries_.clear();if(tiles_)tiles_->invalidate();
            tileBackground_=plan.background;tileClip_=plan.referenceDrawList.viewportClip;
        }
        if(!tiles_)tiles_=std::make_unique<TileGrid>(options.tileWorldSize);
        tiles_->setEnabled(true);
        if(targetChanged)tiles_->invalidate();
        for(const auto& contribution:updates.contributions){
            if(contribution.before)tiles_->markDirty(contribution.before->bounds);
            if(contribution.after)tiles_->markDirty(contribution.after->bounds);
        }
        // Planning only: no unproved tiled source may qualify canonical pixels.
        work.tileRequests+=tiles_->requests(f.worldViewport,options.tileScale,64).size();
    }
    target_=target;surface_=&surface;frame_=std::make_unique<FrameState>(f);
    transform_=plan.referenceDrawList.worldToView;++tick_;work.entries=entries_.size();
}
void SkiaRenderCache::evict(std::size_t bytes){
    while(!entries_.empty()&&(work.retainedBytes>options.maxBytes-bytes||entries_.size()>=options.maxEntries)){
        auto victim=std::min_element(entries_.begin(),entries_.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});
        work.retainedBytes-=victim->second.bytes;entries_.erase(victim);++work.evictions;
    }
}
sk_sp<SkPicture> SkiaRenderCache::coverage(const ReferenceTraversalEntry& source,
    const RenderGroup& group,const SkRect& support){
    // V3 AA width depends on the actual device matrix. A picture recorded at
    // identity cannot be used as a camera-independent exact coverage source.
    if(std::get<CompactStrokeReferenceCommand>(source.command).content.get().stroke.config.algorithmVersion==3)return nullptr;
    AXIOM_FRAME_TIMING_SCOPE(coverageTiming,coverage_record);
    const auto& r=group.revisions;
    auto found=coverage_.find(source.record.objectId);
    if(found!=coverage_.end()){
        const auto& old=found->second.revisions;
        if(old.content==r.content && old.resource==r.resource){
            found->second.used=tick_;++work.coverageHits;return found->second.picture;
        }
        work.coverageBytes-=found->second.bytes;coverage_.erase(found);
    }
    const auto& stroke=std::get<CompactStrokeReferenceCommand>(source.command).content.get().stroke;
    // Admission bounds shader/recording construction before allocating it.
    const auto estimate=stroke.knots.size()*1024ULL+4096;
    if(estimate>options.maxEntryBytes || estimate>options.maxBytes || !support.isFinite())return nullptr;
    auto token=budget?budget->reserve(resources::ResourceClass::kRecorded,estimate,false):std::optional<std::uint64_t>{};
    if(budget&&!token)return nullptr;
    resources::ResourceLease lease(budget,token.value_or(0));
    if(workId_==UINT64_MAX)return nullptr;
    const auto job=++workId_;
    if(!scheduler_.submit({job,1,scheduling::WorkPriority::kVisible,0,{},static_cast<std::size_t>(estimate)}))return nullptr;
    const auto admitted=scheduler_.take();if(!admitted||admitted->id!=job)return nullptr;
    SkPictureRecorder recorder;
    auto* canvas=recorder.beginRecording(support.makeOutset(2,2));
    drawCompactCoverageToSkCanvas(*canvas,stroke.config,stroke.knots,0,-1,stroke.seed);
    auto picture=recorder.finishRecordingAsPicture();(void)scheduler_.finish(job,1);if(!picture)return nullptr;
    const auto bytes=std::max<std::size_t>(picture->approximateBytesUsed(),estimate);
    if(bytes>options.maxEntryBytes || bytes>options.maxBytes)return nullptr;
    if(bytes>estimate&&budget){lease.reset();token=budget->reserve(resources::ResourceClass::kRecorded,bytes,false);if(!token)return nullptr;lease=resources::ResourceLease(budget,*token);}
    while(!coverage_.empty() && (work.coverageBytes>options.maxBytes-bytes || coverage_.size()>=options.maxEntries)){
        auto victim=std::min_element(coverage_.begin(),coverage_.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});
        work.coverageBytes-=victim->second.bytes;coverage_.erase(victim);++work.evictions;
    }
    coverage_.emplace(source.record.objectId,Coverage{r,picture,bytes,tick_,std::move(lease)});
    work.coverageBytes+=bytes;++work.coverageBuilds;return picture;
}
bool SkiaRenderCache::drawTiles(SkCanvas& canvas,const FramePlan& plan){
    if(plan.transparentBackground || plan.background.color[3]!=1.0F ||
       plan.background.mode!=foundation::BackgroundMode::kPlain)return false;
    for(const auto& entry:plan.referenceDrawList.entries){const auto& t=entry.record.transform;
        if(!std::holds_alternative<CompactStrokeReferenceCommand>(entry.command)||!entry.record.eraseMasks.empty()||
           t.a!=1||t.b!=0||t.c!=0||t.d!=1||t.tx!=0||t.ty!=0)return false;
    }
    if(!tiles_||transform_.b!=0||transform_.c!=0||transform_.a<=0||transform_.a!=transform_.d)return false;
    const double px=plan.frame.metrics.physicalWidth/plan.frame.metrics.logicalWidth;
    const double py=plan.frame.metrics.physicalHeight/plan.frame.metrics.logicalHeight;
    const auto clip=canvas.getDeviceClipBounds();
    const foundation::WorldRect world{float((clip.left()/px-transform_.tx)/transform_.a),
        float((clip.top()/py-transform_.ty)/transform_.d),float((clip.right()/px-transform_.tx)/transform_.a),
        float((clip.bottom()/py-transform_.ty)/transform_.d)};
    const auto keys=tiles_->coverage(world,options.tileScale,256);if(keys.empty())return false;
    std::vector<SkIRect> rects;
    for(const auto& key:keys){const auto b=tiles_->bounds(key);
        const double x0=(b.left*transform_.a+transform_.tx)*px,x1=(b.right*transform_.a+transform_.tx)*px;
        const double y0=(b.top*transform_.d+transform_.ty)*py,y1=(b.bottom*transform_.d+transform_.ty)*py;
        if(std::floor(x0)!=x0||std::floor(x1)!=x1||std::floor(y0)!=y0||std::floor(y1)!=y1)return false;
        if(std::max({std::abs(x0),std::abs(x1),std::abs(y0),std::abs(y1)})>1e8)return false;
        auto rect=SkIRect::MakeLTRB(int(x0),int(y0),int(x1),int(y1));if(!rect.intersect(clip))return false;rects.push_back(rect);
    }
    const auto requests=tiles_->requests(world,options.tileScale,256);work.tileRequests+=requests.size();
    for(const auto& request:requests)tileEntries_.erase(request.key);
    const auto info=surface_->imageInfo();const auto scratchBytes=std::size_t(info.width())*info.height()*4;
    if(!scratch_){auto token=budget?budget->reserve(resources::ResourceClass::kScratch,scratchBytes,true):std::optional<std::uint64_t>{};
        if(budget&&!token)return false;
        resources::ResourceLease lease(budget,token.value_or(0));
        scratch_=canvas.makeSurface(info);if(!scratch_)return false;scratchLease_=std::move(lease);++work.scratchAllocations;}
    const CompactDeviceGrid grid{clip.left(),clip.top(),clip.right(),clip.bottom()};
    for(std::size_t i=0;i<keys.size();++i){const auto key=keys[i];const auto rect=rects[i];
        if(tileEntries_.contains(key)){++work.tileHits;continue;}
        const auto request=std::find_if(requests.begin(),requests.end(),[&](const auto& r){return r.key==key;});
        if(request==requests.end())return false;
        if(workId_==UINT64_MAX)return false;
        const auto job=++workId_;
        if(!scheduler_.submit({job,request->generation,scheduling::WorkPriority::kVisible,0,{},0}))return false;
        const auto admitted=scheduler_.take();if(!admitted||admitted->id!=job)return false;
        struct JobRelease {scheduling::RuntimeScheduler& scheduler;std::uint64_t id,generation;~JobRelease(){(void)scheduler.cancel(id,generation);}} jobRelease{scheduler_,job,request->generation};
        const auto bytes=std::size_t(rect.width())*rect.height()*4;
        if(bytes>options.maxEntryBytes||bytes>options.maxBytes)return false;
        auto token=budget?budget->reserve(resources::ResourceClass::kTile,bytes,true):std::optional<std::uint64_t>{};
        if(budget&&!token)return false;
        resources::ResourceLease lease(budget,token.value_or(0));
        auto* draw=scratch_->getCanvas();draw->save();draw->resetMatrix();draw->clear(SK_ColorTRANSPARENT);draw->clipIRect(rect);
        // Same absolute matrix and original coverage grid; raster only this
        // tile's shading region. No local-origin shader rebasing.
        auto tileList=plan.referenceDrawList;const auto worldBounds=tiles_->bounds(key);
        std::erase_if(tileList.entries,[&](const auto& e){const auto& b=e.record.worldBounds;return b.right<worldBounds.left||b.left>worldBounds.right||b.bottom<worldBounds.top||b.top>worldBounds.bottom;});
        const FramePlan tilePlan{plan.frame,std::move(tileList),plan.background,plan.transparentBackground,plan.isolateEraseMasks};
        const auto result=drawReferencePlanToSkCanvas(*draw,tilePlan,nullptr,nullptr,true,nullptr,&grid);
        draw->restore();if(result.code!=BackendSubmissionCode::kAccepted)return false;
        auto crop=canvas.makeSurface(info.makeWH(rect.width(),rect.height()));if(!crop)return false;
        crop->getCanvas()->clear(SK_ColorTRANSPARENT);crop->getCanvas()->drawImage(scratch_->makeImageSnapshot(),static_cast<SkScalar>(-rect.left()),static_cast<SkScalar>(-rect.top()));
        auto image=crop->makeImageSnapshot();if(!image||!tiles_->complete(*request))return false;
        tileEntries_.emplace(key,TileEntry{request->generation,image,rect,std::move(lease)});(void)scheduler_.finish(job,request->generation);++work.tileRasters;
    }
    canvas.save();canvas.resetMatrix();for(const auto& key:keys){const auto& e=tileEntries_.at(key);canvas.drawImage(e.image,static_cast<SkScalar>(e.device.left()),static_cast<SkScalar>(e.device.top()));}canvas.restore();
    return true;
}
SkiaRenderCache::Preparation SkiaRenderCache::prepare(
    SkCanvas& canvas,const FramePlan& plan,const RenderPreparationBudget& slice,ImageResourceResolver* images,SkiaTextResources* text){
    if(!sourceAdmission_)return Preparation::kResourcePending;
    if(!options.enabled||options.tileCandidate||!slice.maxGroups||!slice.maxMicroseconds||!slice.maxCoverageSteps||
       !options.maxEntries)return Preparation::kFallback;
    // Admission is reused only while begin() has verified all group revisions,
    // target/metrics/resource stamps and camera. Never retain a plan pointer.
    if(!admissionReady_){
        AXIOM_FRAME_TIMING_SCOPE(scanTiming,prepare_scan);
        std::uint64_t bytes=0;std::size_t admitted=0;
        const bool isolatedInk=requiresInkIsolation(plan);
        for(const auto& entry:plan.referenceDrawList.entries){
            const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command);
            // entryMatrix already contains the object affine. A moved stroke
            // needs the same bounded preparation as identity ink; routing it
            // Direct makes every camera update synchronously raster it again.
            if(!compact||!entry.contributesPixels||!entry.record.eraseMasks.empty()){
                admissions_.insert_or_assign(entry.record.objectId,Admission{{},AdmissionKind::kDirect});
                AXIOM_FRAME_TIMING_ADD(prepare_direct,1);
                ++work.preparationDirectEntries;continue;
            }
            SkRect support=SkRect::MakeEmpty();
            AXIOM_FRAME_TIMING_ADD(prepare_knots,compact->content.get().stroke.knots.size());
            for(const auto& k:compact->content.get().stroke.knots){
                const auto radius=ink::compactRadius(compact->content.get().stroke.config,k.pressure);
                support.join(SkRect::MakeLTRB(float(k.x-radius),float(k.y-radius),float(k.x+radius),float(k.y+radius)));
            }
            auto device=entryMatrix(canvas,plan,entry).mapRect(support);device.outset(2,2);
            if(!device.isFinite()){
                admissions_.insert_or_assign(entry.record.objectId,Admission{{},AdmissionKind::kDirect});
                AXIOM_FRAME_TIMING_ADD(prepare_budget,1);
                ++work.preparationDirectEntries;continue;
            }
            SkIRect crop;device.roundOut(&crop);
            if(!crop.intersect(canvas.getDeviceClipBounds())){
                admissions_.insert_or_assign(entry.record.objectId,Admission{SkIRect::MakeEmpty(),AdmissionKind::kEmpty});
                continue;
            }
            const auto& t=entry.record.transform;
            if(!isolatedInk&&(t.a!=1||t.b!=0||t.c!=0||t.d!=1||t.tx!=0||t.ty!=0)){
                // Direct transformed ink-only material blends onto the actual
                // backdrop. A transparent per-object cache adds a quantization
                // pass. Stream into the bounded full-frame composite instead.
                admissions_.insert_or_assign(entry.record.objectId,Admission{crop,AdmissionKind::kComposite});
                streamFrame_=true;continue;
            }
            const auto allocation=std::uint64_t(crop.width())*crop.height()*4;
            if(allocation>options.maxEntryBytes||allocation>options.maxBytes||bytes>options.maxBytes-allocation||admitted>=options.maxEntries){
                admissions_.insert_or_assign(entry.record.objectId,Admission{crop,AdmissionKind::kDirect});
                AXIOM_FRAME_TIMING_ADD(prepare_budget,1);
                ++work.preparationBudgetEntries;streamFrame_=true;continue;
            }
            bytes+=allocation;++admitted;admissions_.insert_or_assign(entry.record.objectId,Admission{crop,AdmissionKind::kRaster});
        }
        // Admission is deterministic for the entire demand. Drop ineligible
        // and stale residents before filling it; never evict a prepared prefix.
        for(auto it=entries_.begin();it!=entries_.end();){
            const auto admissionEntry=admissions_.find(it->first);const auto* group=groups_.find(it->first);
            if(admissionEntry==admissions_.end()||admissionEntry->second.kind!=AdmissionKind::kRaster||!group||
               RenderSourceResolver::resolve(it->second.validity,{group->revisions,targetEpoch_,false})!=RenderSourceKind::kRasterCache){
                work.retainedBytes-=it->second.bytes;it=entries_.erase(it);
            }else ++it;
        }
        admissionReady_=true;
    }
    planRuns(canvas,plan);
    // A streamed frame needs both an ordered destination and an isolated ink
    // target. Optional cache capacity is not the admission cap for this work.
    // Reject structurally impossible requests instead of retrying forever or
    // escaping into an unbounded synchronous renderer.
    const auto frameBytes=std::size_t(surface_->width())*surface_->height()*4;
    const bool needsScratch=std::any_of(admissions_.begin(),admissions_.end(),[](const auto& a){
        return !a.second.crop.isEmpty()&&a.second.kind!=AdmissionKind::kComposite;
    });
    const auto buffers=std::size_t(streamFrame_)+std::size_t(needsScratch);
    const bool needsCoverage=std::any_of(admissions_.begin(),admissions_.end(),[](const auto& a){
        return !a.second.crop.isEmpty();
    });
    const auto coverageBytes=std::size_t(std::min(256,surface_->width()))*std::min(256,surface_->height())*4;
    const bool hybrid=options.gpuCoverageCandidate&&canvas.recordingContext()&&
        std::any_of(plan.referenceDrawList.entries.begin(),plan.referenceDrawList.entries.end(),[](const auto& e){
            const auto* c=std::get_if<CompactStrokeReferenceCommand>(&e.command);
            return c&&c->content.get().stroke.config.algorithmVersion==3;
        });
    const auto coverageReserve=needsCoverage?coverageBytes*(hybrid?2:1):0;
    if(coverageReserve>options.sharedMaxBytes||
        (buffers&&frameBytes>(options.sharedMaxBytes-coverageReserve)/buffers))return Preparation::kRejected;
    if(compositeReady_)return Preparation::kReady;
    if(streamFrame_&&!ensureComposite(canvas,plan))return allocationFailure_;
    ++work.preparationSlices;
    const auto start=std::chrono::steady_clock::now();std::size_t built=0,steps=0;
    const auto exhausted=[&]{return built>=slice.maxGroups||steps>=slice.maxCoverageSteps||
        (steps&&std::chrono::steady_clock::now()-start>=std::chrono::microseconds(slice.maxMicroseconds));};
    const auto yield=[&]{flushPreparation();++work.preparationYields;return Preparation::kPending;};
    for(;preparationCursor_<plan.referenceDrawList.entries.size();++preparationCursor_){
        const auto& entry=plan.referenceDrawList.entries[preparationCursor_];
        const auto admission=admissions_.find(entry.record.objectId);
        if(admission==admissions_.end())return Preparation::kFallback;
        if(admission->second.kind==AdmissionKind::kEmpty){captureRunEntry(plan,preparationCursor_,nullptr);continue;}
        const bool raster=admission->second.kind==AdmissionKind::kRaster;
        const bool directComposite=admission->second.kind==AdmissionKind::kComposite;
        const bool streamedCompact=streamFrame_&&!raster&&!admission->second.crop.isEmpty();
        if(!raster&&!streamedCompact){
            if(streamFrame_){
                if(exhausted())return yield();
                drawValidatedReferenceRangeToSkCanvas(*composite_->getCanvas(),plan,
                    preparationCursor_,preparationCursor_+1,images,text);
                noteDirect();++built;++steps;
            }
            continue;
        }
        const auto* group=groups_.find(entry.record.objectId);if(!group)return Preparation::kFallback;
        const auto found=entries_.find(entry.record.objectId);
        const RenderCacheValidity validity{group->revisions,targetEpoch_,false};
        if(raster&&found!=entries_.end()&&RenderSourceResolver::resolve(found->second.validity,validity)==RenderSourceKind::kRasterCache){
            captureRunEntry(plan,preparationCursor_,found->second.image,found->second.bounds.left(),found->second.bounds.top());
            if(streamFrame_){
                if(exhausted())return yield();
                composite_->getCanvas()->drawImage(found->second.image,float(found->second.bounds.left()),float(found->second.bounds.top()));
                found->second.used=tick_;++work.hits;++steps;
            }
            continue;
        }
        if(exhausted())return yield();
        if(!pending_){
            if(!directComposite&&!ensureScratch(canvas,true))return allocationFailure_;
            const auto admissionEntry=admissions_.find(entry.record.objectId);if(admissionEntry==admissions_.end())return Preparation::kFallback;
            auto* target=(directComposite?composite_:scratch_)->getCanvas();
            if(!directComposite){target->resetMatrix();target->clear(SK_ColorTRANSPARENT);}
            configureScratch(*target,plan,entryMatrix(canvas,plan,entry));
            const auto& stroke=std::get<CompactStrokeReferenceCommand>(entry.command).content.get().stroke;
            auto rasterJob=std::make_unique<CompactStrokeRasterJob>(*target,stroke.config,stroke.knots,nullptr,options.gpuCoverageCandidate,budget,
                std::min(options.maxEntryBytes,options.maxBytes),reclaimRecorded,true);
            target->restore();
            pending_=std::make_unique<Pending>(Pending{entry.record.objectId,validity,admissionEntry->second.crop,std::move(rasterJob)});
            ++work.misses;
        }
        if(pending_->id!=entry.record.objectId||RenderSourceResolver::resolve(pending_->validity,validity)!=RenderSourceKind::kRasterCache){
            pending_.reset();return Preparation::kFallback;
        }
        auto* target=(directComposite?composite_:scratch_)->getCanvas();const auto& stroke=std::get<CompactStrokeReferenceCommand>(entry.command).content.get().stroke;
        bool complete=false;
        do{
            configureScratch(*target,plan,entryMatrix(canvas,plan,entry));
            CompactRasterWork rasterWork;
            try{complete=pending_->raster->step(*target,stroke.config,stroke.knots,stroke.seed,
                    coverageTargets_,options.gpuCoverageCandidate,&rasterWork);}
            catch(...){target->restore();pending_.reset();throw;}
            target->restore();noteCoverage(rasterWork);++steps;++work.preparationCoverageSteps;
            if(pending_->raster->resourcePending()){
                // Admission failure is an expected resumable state. Never
                // throw across a no-exception WASM Runtime boundary, discard
                // the job, or publish its partial coverage.
#if defined(AXIOM_ENABLE_FRAME_TIMING)
                std::fprintf(stderr,"AXIOM_RENDER_RESOURCE_RETRY {\"stage\":\"coverage\",\"used_bytes\":%llu,\"hard_bytes\":%llu}\n",
                    static_cast<unsigned long long>(budget?budget->used():0),static_cast<unsigned long long>(options.sharedMaxBytes));
#endif
                if(reclaimPreparation)reclaimPreparation(coverageBytes*(hybrid?2:1));
                flushPreparation();++work.preparationYields;return Preparation::kResourcePending;
            }
            if(!complete&&exhausted())return yield();
        }while(!complete);
        const auto crop=pending_->crop;pending_.reset();
        if(directComposite){++work.rasterizedGroups;++built;continue;}
        const bool stored=raster&&store(canvas,entry,validity,crop,false,true);
        // Consume the already materialized source; never run coverage again to
        // warm a retained layer. Its device grid and shader origin stay intact.
        if(stored){const auto& resident=entries_.at(entry.record.objectId);
            captureRunEntry(plan,preparationCursor_,resident.image,resident.bounds.left(),resident.bounds.top());
        }else captureRunEntry(plan,preparationCursor_,scratch_->makeImageSnapshot());
        if(streamFrame_){
            auto* compositeTarget=composite_->getCanvas();
            if(stored){const auto& cached=entries_.at(entry.record.objectId);
                compositeTarget->drawImage(cached.image,float(cached.bounds.left()),float(cached.bounds.top()));
            }else{
                compositeTarget->save();compositeTarget->clipIRect(crop);
                compositeTarget->drawImage(scratch_->makeImageSnapshot(),0,0);compositeTarget->restore();
                ++work.rasterizedGroups;
            }
        }else if(!stored){
            // Global shared-budget pressure can deny a previously admitted
            // object. Restart as streamed composition instead of accumulating
            // unbounded synchronous work at the final target submission.
            admissions_.at(entry.record.objectId).kind=AdmissionKind::kDirect;
            ++work.preparationBudgetEntries;streamFrame_=true;preparationCursor_=0;
            pending_.reset();return yield();
        }
        AXIOM_FRAME_TIMING_ADD(built_groups,1);++built;
    }
    if(streamFrame_&&!finishComposite())return Preparation::kResourcePending;
    flushPreparation();
    return Preparation::kReady;
}
bool SkiaRenderCache::validRun(const RetainedRun& run,const FramePlan& plan)const {
    if(run.end>plan.referenceDrawList.entries.size()||run.end-run.begin!=run.stamps.size())return false;
    for(std::size_t i=run.begin;i<run.end;++i){
        const auto& entry=plan.referenceDrawList.entries[i];const auto* group=groups_.find(entry.record.objectId);
        if(!group||run.stamps[i-run.begin]!=RunStamp{entry.record.objectId,group->revisions})return false;
    }
    return true;
}
void SkiaRenderCache::planRuns(SkCanvas& canvas,const FramePlan& plan){
    // Small scenes already fit the per-object cache. Runs are a bounded
    // composition representation for pressure, not another unbounded residency.
    if(!options.enabled||plan.referenceDrawList.entries.size()<=options.maxEntries)return;
    const auto eligible=[&](const ReferenceTraversalEntry& entry){
        const auto& t=entry.record.transform;
        // Identity compact entries have destination-independent isolation in
        // the shared mapping. Legacy destination-clear never enters a run.
        return entry.record.objectId!=activeSource_&&std::holds_alternative<CompactStrokeReferenceCommand>(entry.command)&&
            entry.contributesPixels&&entry.record.eraseMasks.empty()&&t.a==1&&t.b==0&&t.c==0&&t.d==1&&t.tx==0&&t.ty==0;
    };
    std::erase_if(runs_,[&](const auto& run){return !validRun(run,plan);});
    work.retainedRunBytes=0;for(const auto& run:runs_)work.retainedRunBytes+=run.bytes;
    const auto bytes=std::size_t(surface_->width())*surface_->height()*4;
    for(std::size_t i=0;i<plan.referenceDrawList.entries.size();){
        if(!eligible(plan.referenceDrawList.entries[i])){++i;continue;}
        const auto begin=i;while(i<plan.referenceDrawList.entries.size()&&eligible(plan.referenceDrawList.entries[i]))++i;
        if(i-begin<2||std::any_of(runs_.begin(),runs_.end(),[&](const auto& r){return r.begin==begin&&r.end==i;}))continue;
        const auto allocation=bytes*(begin==0?2:1);
        if(runs_.size()>=8||allocation>options.maxBytes||work.retainedRunBytes>options.maxBytes-allocation)continue;
        auto token=budget?budget->reserve(resources::ResourceClass::kRaster,allocation,true):std::optional<std::uint64_t>{};
        if(budget&&!token)continue;
        RetainedRun run;run.begin=begin;run.end=i;run.next=begin;run.bytes=allocation;
        run.lease=resources::ResourceLease(budget,token.value_or(0));
        run.building=canvas.makeSurface(surface_->imageInfo());if(!run.building)continue;
        run.building->getCanvas()->clear(SK_ColorTRANSPARENT);
        if(begin==0){
            run.prefix=canvas.makeSurface(surface_->imageInfo());if(!run.prefix)continue;
            drawReferenceBackgroundToSkCanvas(*run.prefix->getCanvas(),plan);
        }
        for(std::size_t j=begin;j<i;++j){const auto id=plan.referenceDrawList.entries[j].record.objectId;
            run.stamps.push_back({id,groups_.find(id)->revisions});}
        runs_.push_back(std::move(run));work.retainedRunBytes+=allocation;
    }
}
void SkiaRenderCache::captureRunEntry(const FramePlan& plan,std::size_t index,const sk_sp<SkImage>& image,int x,int y){
    for(auto& run:runs_){
        if(run.image||!run.building||index<run.begin||index>=run.end||index!=run.next)continue;
        (void)plan;
        if(image){run.building->getCanvas()->drawImage(image,float(x),float(y));
            if(run.prefix)run.prefix->getCanvas()->drawImage(image,float(x),float(y));}
        if(++run.next==run.end){
            run.image=run.building->makeImageSnapshot();run.building.reset();
            if(run.prefix){run.prefixImage=run.prefix->makeImageSnapshot();run.prefix.reset();}
            ++work.retainedRunBuilds;
        }
    }
}
bool SkiaRenderCache::compositeRetainedRun(SkCanvas& canvas,const FramePlan& plan,std::size_t& index,bool inkPass){
    for(const auto& run:runs_){
        if(!run.image||index!=run.begin||!validRun(run,plan))continue;
        canvas.save();canvas.resetMatrix();
        SkPaint paint;
        if(!inkPass&&run.prefixImage)paint.setBlendMode(SkBlendMode::kSrc);
        canvas.drawImage(!inkPass&&run.prefixImage?run.prefixImage:run.image,0,0,SkSamplingOptions{},&paint);canvas.restore();
        index=run.end-1;++work.retainedRunHits;return true;
    }
    return false;
}
void SkiaRenderCache::drawTransientScene(SkCanvas& canvas,const FramePlan& plan,ImageResourceResolver* images,SkiaTextResources* text,bool includeInk){
    drawReferenceBackgroundToSkCanvas(canvas,plan);
    std::size_t begin=0;
    if(includeInk)for(std::size_t i=0;i<plan.referenceDrawList.entries.size();++i){
        const auto found=std::find_if(runs_.begin(),runs_.end(),[&](const auto& run){return run.image&&run.begin==i&&validRun(run,plan);});
        if(found==runs_.end())continue;
        drawValidatedReferenceRangeToSkCanvas(canvas,plan,begin,i,images,text,true,this);
        (void)compositeRetainedRun(canvas,plan,i);begin=i+1;
    }
    drawValidatedReferenceRangeToSkCanvas(canvas,plan,begin,plan.referenceDrawList.entries.size(),images,text,includeInk,this);
}
void SkiaRenderCache::resetPreparation(){
    pending_.reset();admissions_.clear();admissionReady_=false;preparationCursor_=0;
    composite_.reset();compositeLease_.reset();preparedFrame_.reset();streamFrame_=false;compositeReady_=false;
}
bool SkiaRenderCache::ensureComposite(SkCanvas& canvas,const FramePlan& plan){
    if(composite_){(void)compositeLease_.markGpuUse();return true;}
    if(!acquireWorkSurface(canvas,composite_,compositeLease_,true,resources::ResourceClass::kCanonicalFrame))return false;
    drawReferenceBackgroundToSkCanvas(*composite_->getCanvas(),plan);return true;
}
bool SkiaRenderCache::finishComposite(){
    const auto bytes=sizeof(PreparedFrameImage)+128;
    auto token=budget?budget->reserve(resources::ResourceClass::kRecorded,bytes,true):std::optional<std::uint64_t>{};
    if(budget&&!token){
        if(reclaimPreparation)reclaimPreparation(bytes);
        return false;
    }
    resources::ResourceLease metadata(budget,token.value_or(0));
    auto candidate=std::make_shared<PreparedFrameImage>();
    candidate->image=composite_->makeImageSnapshot();if(!candidate->image)return false;
    candidate->pixels=std::move(compositeLease_);candidate->metadata=std::move(metadata);
    preparedFrame_=std::move(candidate);composite_.reset();compositeReady_=true;return true;
}
bool SkiaRenderCache::acquireWorkSurface(SkCanvas& canvas,sk_sp<SkSurface>& target,
    resources::ResourceLease& targetLease,bool mandatory,resources::ResourceClass kind){
    if(target)return true;
    const auto info=surface_->imageInfo();const auto bytes=std::size_t(info.width())*info.height()*4;
    allocationFailure_=Preparation::kResourcePending;
    if(bytes>(mandatory?options.sharedMaxBytes:options.maxBytes)){
        allocationFailure_=Preparation::kRejected;return false;
    }
    auto token=budget?budget->reserve(kind,bytes,true):std::optional<std::uint64_t>{};
    if(budget&&!token&&mandatory){
        // Completed prefix pixels are already in composite_; optional resident
        // sources can be rebuilt. Never drop the active raster job or its grid.
        entries_.clear();coverage_.clear();runs_.clear();
        work.retainedBytes=0;work.entries=0;work.coverageBytes=0;work.retainedRunBytes=0;
        if(reclaimPreparation)reclaimPreparation(bytes);
        token=budget->reserve(kind,bytes,true);
    }
    if(budget&&!token)return false;
    resources::ResourceLease lease(budget,token.value_or(0));
    auto candidate=canvas.makeSurface(info);
    if(!candidate){allocationFailure_=Preparation::kRejected;return false;}
    target=std::move(candidate);targetLease=std::move(lease);++work.scratchAllocations;
    return true;
}
void SkiaRenderCache::flushPreparation(){
    // Submit bounded offscreen work without presenting or a CPU fence. Leaving
    // all GL work queued until the final present merely moves the long stall.
    if(auto* context=GrAsDirectContext(surface_->recordingContext())){
        context->flush();(void)context->submit(GrSyncCpu::kNo);
    }
}
bool SkiaRenderCache::compositePreparedFrame(SkCanvas& canvas){
    if(!compositeReady_||!preparedFrame_)return false;
    preparedFrame_->markGpuUse();
    SkPaint paint;paint.setBlendMode(SkBlendMode::kSrc);
    canvas.save();canvas.resetMatrix();canvas.drawImage(preparedFrame_->image,0,0,SkSamplingOptions{},&paint);canvas.restore();
    return true;
}
SkMatrix SkiaRenderCache::entryMatrix(SkCanvas& canvas,const FramePlan& plan,const ReferenceTraversalEntry& source)const{
    const auto& r=source.record.transform;const auto& v=plan.referenceDrawList.worldToView;
    SkMatrix world;world.setAll(float(v.a),float(v.c),float(v.tx),float(v.b),float(v.d),float(v.ty),0,0,1);
    SkMatrix object;object.setAll(float(r.a),float(r.c),float(r.tx),float(r.b),float(r.d),float(r.ty),0,0,1);
    return SkMatrix::Concat(canvas.getTotalMatrix(),SkMatrix::Concat(world,object));
}
bool SkiaRenderCache::ensureScratch(SkCanvas& canvas,bool mandatory){
    const auto info=surface_->imageInfo();const auto bytes=std::size_t(info.width())*info.height()*4;
    if(!acquireWorkSurface(canvas,scratch_,scratchLease_,mandatory))return false;
    work.scratchPeakBytes=std::max(work.scratchPeakBytes,bytes);return true;
}
void SkiaRenderCache::configureScratch(SkCanvas& target,const FramePlan& plan,const SkMatrix& matrix){
    target.save();target.resetMatrix();
    target.scale(plan.frame.metrics.physicalWidth/plan.frame.metrics.logicalWidth,
        plan.frame.metrics.physicalHeight/plan.frame.metrics.logicalHeight);
    const auto& clip=plan.referenceDrawList.viewportClip;
    target.clipRect(SkRect::MakeLTRB(clip.left,clip.top,clip.right,clip.bottom));target.setMatrix(matrix);
}
void SkiaRenderCache::noteCoverage(const CompactRasterWork& rasterWork){
    work.gpuCoverageTiles+=rasterWork.gpuTiles;work.cpuCoverageTiles+=rasterWork.cpuTiles;
    work.coverageTargetAllocations+=rasterWork.allocatedTiles;work.coverageTargetReuses+=rasterWork.targetReuses;
    work.gpuInteriorPixels+=rasterWork.gpuInteriorPixels;work.cpuEdgePixels+=rasterWork.cpuEdgePixels;
}
bool SkiaRenderCache::draw(SkCanvas& canvas,const FramePlan& plan,const ReferenceTraversalEntry& source,bool isolated,bool composite){
    AXIOM_FRAME_TIMING_SCOPE(drawTiming,cache_draw);
    if(!sourceAdmission_)return false;
    // A synchronous draw may overwrite the scratch used by suspended work.
    // Its next cooperative demand must start from a fresh offscreen target.
    if(pending_||composite_)resetPreparation();
    if(options.enabled && options.tileCandidate && !plan.referenceDrawList.entries.empty()){
        if(tiledFrame_)return true;
        if(source.record.objectId==plan.referenceDrawList.entries.front().record.objectId&&drawTiles(canvas,plan)){tiledFrame_=true;return true;}
        return false;
    }
    const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&source.command);
    // SrcOver isolated groups are independent of the destination. Unknown
    // effects and destination-dependent erase stay on the Direct path.
    if(!options.enabled||!compact||!isolated||!source.contributesPixels||!options.maxEntries)return false;
    // begin() invalidates these admission results with camera/clip/content.
    // A conservative Scene hit outside the acquired clip contributes nothing.
    if(admissionReady_){
        const auto admission=admissions_.find(source.record.objectId);
        if(admission!=admissions_.end()){
            if(admission->second.kind==AdmissionKind::kEmpty)return true;
            if(admission->second.kind==AdmissionKind::kDirect)return false;
        }
    }
    const auto* group=groups_.find(source.record.objectId);if(!group)return false;
    const RenderCacheValidity validity{group->revisions,targetEpoch_,false};
    auto found=entries_.find(source.record.objectId);
    if(found!=entries_.end()&&RenderSourceResolver::resolve(found->second.validity,validity)==RenderSourceKind::kRasterCache){
        if(composite){canvas.save();canvas.resetMatrix();canvas.drawImage(found->second.image,static_cast<float>(found->second.bounds.left()),static_cast<float>(found->second.bounds.top()));canvas.restore();}
        found->second.used=tick_;++work.hits;return true;
    }
    ++work.misses;
    if(found!=entries_.end()){work.retainedBytes-=found->second.bytes;entries_.erase(found);}
    const auto& r=source.record.transform;
    const auto& v=plan.referenceDrawList.worldToView;
    SkMatrix world;world.setAll(static_cast<float>(v.a),static_cast<float>(v.c),static_cast<float>(v.tx),static_cast<float>(v.b),static_cast<float>(v.d),static_cast<float>(v.ty),0,0,1);
    SkMatrix object;object.setAll(static_cast<float>(r.a),static_cast<float>(r.c),static_cast<float>(r.tx),static_cast<float>(r.b),static_cast<float>(r.d),static_cast<float>(r.ty),0,0,1);
    const auto matrix=SkMatrix::Concat(canvas.getTotalMatrix(),SkMatrix::Concat(world,object));
    SkRect support=SkRect::MakeEmpty();
    for(const auto& k:compact->content.get().stroke.knots){const auto radius=ink::compactRadius(compact->content.get().stroke.config,k.pressure);
        support.join(SkRect::MakeLTRB(static_cast<float>(k.x-radius),static_cast<float>(k.y-radius),static_cast<float>(k.x+radius),static_cast<float>(k.y+radius)));}
    const auto commands=coverage(source,*group,support);
    support=matrix.mapRect(support);support.outset(2,2);
    if(!support.isFinite())return false;
    SkIRect crop;support.roundOut(&crop);if(!crop.intersect(canvas.getDeviceClipBounds()))return false;
    const auto bytes=std::uint64_t(crop.width())*crop.height()*4;
    if(bytes>options.maxEntryBytes||bytes>options.maxBytes)return false;
    // Preserve the original device clip, grid anchor, shader coordinates and
    // color type. Only the snapshot is cropped; rasterization is never rebased.
    // Full-size scratch is bounded and transient, not a huge-group texture.
    if(!ensureScratch(canvas))return false;
    auto* target=scratch_->getCanvas();target->resetMatrix();target->clear(SK_ColorTRANSPARENT);
    configureScratch(*target,plan,matrix);
    {AXIOM_FRAME_TIMING_SCOPE(scratchTiming,scratch_draw);
    CompactRasterWork rasterWork;
    drawValidatedIsolatedEntryToSkCanvas(*target,source,commands.get(),nullptr,&rasterWork,options.gpuCoverageCandidate,&coverageTargets_);
    noteCoverage(rasterWork);}
    target->restore();
    return store(canvas,source,validity,crop,composite);
}
bool SkiaRenderCache::store(SkCanvas& canvas,const ReferenceTraversalEntry& source,
    const RenderCacheValidity& validity,const SkIRect& crop,bool composite,bool preservePrepared){
    const auto info=surface_->imageInfo();const auto bytes=std::size_t(crop.width())*crop.height()*4;
    const auto old=entries_.find(source.record.objectId);
    if(old!=entries_.end()){work.retainedBytes-=old->second.bytes;entries_.erase(old);}
    // Own a physically cropped representation. A subset image is allowed to
    // retain its source allocation, which would undercount residency.
    if(!preservePrepared)evict(static_cast<std::size_t>(bytes));
    auto reserved=budget?budget->reserve(resources::ResourceClass::kRaster,bytes,true):std::optional<std::uint64_t>{};
    if(budget&&!reserved&&preservePrepared)return false;
    if(budget&&!reserved){work.evictions+=entries_.size();entries_.clear();work.retainedBytes=0;reserved=budget->reserve(resources::ResourceClass::kRaster,bytes,true);if(!reserved)return false;}
    resources::ResourceLease lease(budget,reserved.value_or(0));
    AXIOM_FRAME_TIMING_SCOPE(snapshotTiming,raster_snapshot);
    auto cropped=canvas.makeSurface(info.makeWH(crop.width(),crop.height()));
    if(!cropped)return false;
    cropped->getCanvas()->clear(SK_ColorTRANSPARENT);
    auto fullImage=scratch_->makeImageSnapshot();if(!fullImage)return false;
    cropped->getCanvas()->drawImage(fullImage,static_cast<float>(-crop.left()),static_cast<float>(-crop.top()));
    auto image=cropped->makeImageSnapshot();if(!image)return false;
    AXIOM_FRAME_TIMING_END(snapshotTiming);
    entries_.emplace(source.record.objectId,Entry{validity,image,crop,static_cast<std::size_t>(bytes),tick_,std::move(lease)});
    work.retainedBytes+=static_cast<std::size_t>(bytes);work.entries=entries_.size();++work.rasterizedGroups;
    if(composite){canvas.save();canvas.resetMatrix();canvas.drawImage(image,static_cast<float>(crop.left()),static_cast<float>(crop.top()));canvas.restore();}
    return true;
}
}
