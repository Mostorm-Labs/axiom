#include "render_execution.hpp"
#include "transient_display_state.hpp"
#include "canvas/foundation/internal/g5_observation.hpp"
namespace g5 = canvas::foundation::internal::g5;
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
#if defined(AXIOM_TRANSIENT_DISPLAY_TESTS) || defined(AXIOM_G5_UR0_DIAGNOSTICS)
#define OI_NOTE(...) do { __VA_ARGS__; } while(false)
#else
#define OI_NOTE(...) ((void)0)
#endif
void SkiaRenderCache::invalidateStaticScene([[maybe_unused]] bool count) noexcept {
    OI_NOTE(if(count && staticScene_) ++objectWork.invalidations);
    staticScene_.reset();
    OI_NOTE(objectWork.leaseBytes=0);
}
bool SkiaRenderCache::staticSceneBasisMatches(const FramePlan& plan,
    std::size_t activeIndex,std::size_t activeEnd) const noexcept {
    if (!staticScene_ || !staticScene_->valid() || activeIndex>=plan.referenceDrawList.entries.size()) return false;
    if (staticScene_->target!=target_ || staticScene_->resourceEpoch!=resourceEpoch_ ||
        staticScene_->targetEpoch!=targetEpoch_ || staticScene_->surface!=surface_ ||
        staticScene_->context!=surface_->recordingContext() ||
        staticScene_->frame.viewId!=plan.frame.viewId || staticScene_->frame.camera!=plan.frame.camera ||
        staticScene_->frame.viewportClip!=plan.frame.viewportClip ||
        staticScene_->frame.sceneGeneration!=plan.frame.sceneGeneration ||
        staticScene_->frame.sceneReadToken!=plan.frame.sceneReadToken || staticScene_->frame.metrics!=plan.frame.metrics ||
        staticScene_->frame.surfaceGeneration!=plan.frame.surfaceGeneration || staticScene_->frame.metricsGeneration!=plan.frame.metricsGeneration ||
        staticScene_->frame.worldViewport!=plan.frame.worldViewport || staticScene_->transform!=plan.referenceDrawList.worldToView ||
        staticScene_->background!=plan.background || staticScene_->transparent!=plan.transparentBackground ||
        staticScene_->isolate!=plan.isolateEraseMasks || staticScene_->active!=activeSource_ ||
        staticScene_->activeIndex!=activeIndex || staticScene_->activeEnd!=activeEnd ||
        staticScene_->stamps.size()!=plan.referenceDrawList.entries.size()) return false;
    for (std::size_t i=0;i<staticScene_->stamps.size();++i) {
        const auto& entry=plan.referenceDrawList.entries[i];
        if (staticScene_->stamps[i].id!=entry.record.objectId) return false;
        if (i>=activeIndex && i<activeEnd) continue;
        const auto* group=groups_.find(entry.record.objectId);
        if (!group || staticScene_->stamps[i]!=StaticSceneStamp{entry.record.objectId,group->revisions,
            entry.presentationOpacity,entry.contributesPixels,entry.record.visualBounds,entry.record.worldBounds}) return false;
    }
    return true;
}
bool SkiaRenderCache::buildStaticScene(SkCanvas& canvas,const FramePlan& plan,
    std::size_t activeIndex,std::size_t activeEnd,ImageResourceResolver* images,SkiaTextResources* text,
    const CompactDeviceGrid* grid) {
    if(!budget || !sourceAdmission_ || plan.referenceDrawList.entries.size()>options.maxEntries) return false;
    const auto info=surface_->imageInfo(); const auto bytes=std::size_t(info.width())*info.height()*4;
    const bool needSuffix=activeEnd<plan.referenceDrawList.entries.size();
    const auto metadataBytes=sizeof(StaticSceneCache)+plan.referenceDrawList.entries.size()*sizeof(StaticSceneStamp)+256U;
    const auto planes=std::size_t(1+needSuffix);
    if(metadataBytes>options.maxBytes || bytes>(options.maxBytes-metadataBytes)/planes) {
        OI_NOTE(++objectWork.denials); return false;
    }
    const auto total=bytes*planes;
    auto metadata=budget->reserve(resources::ResourceClass::kRecorded,metadataBytes,true);
    if(!metadata){OI_NOTE(++objectWork.denials);return false;}
    resources::ResourceLease metadataLease(budget,*metadata);
    auto token=budget->reserve(resources::ResourceClass::kRaster,total,true);
    if(!token){OI_NOTE(++objectWork.denials);return false;}
    resources::ResourceLease lease(budget,*token);
    const bool marked=lease.markGpuUse(); (void)marked; OI_NOTE(objectWork.gpuLeaseMarks+=marked);
    auto candidate=std::make_shared<StaticSceneCache>(target_,surface_->recordingContext(),resourceEpoch_,targetEpoch_,plan.frame,
        plan.referenceDrawList.worldToView,plan.background,plan.transparentBackground,plan.isolateEraseMasks,
        true,activeSource_,activeIndex);
    candidate->surface=surface_;candidate->activeEnd=activeEnd;
    candidate->pixelLease=std::move(lease);candidate->metadataLease=std::move(metadataLease);
    candidate->leaseBytes=total+metadataBytes;
    candidate->stamps.reserve(plan.referenceDrawList.entries.size());
    for(const auto& entry:plan.referenceDrawList.entries){
        const auto* group=groups_.find(entry.record.objectId);if(!group)return false;
        candidate->stamps.push_back({entry.record.objectId,group->revisions,entry.presentationOpacity,
            entry.contributesPixels,entry.record.visualBounds,entry.record.worldBounds});
    }
    // Both ranges retain the original full plan's isolation and device grid.
    auto prefix=canvas.makeSurface(info);if(!prefix)return false;
    drawReferenceBackgroundToSkCanvas(*prefix->getCanvas(),plan);
    drawValidatedReferenceRangeToSkCanvas(*prefix->getCanvas(),plan,0,activeIndex,images,text,true,this,grid);
    if(damageFailed_)return false;
    candidate->prefix=prefix->makeImageSnapshot();if(!candidate->prefix)return false;
    if(needSuffix){
        auto suffix=canvas.makeSurface(info);if(!suffix)return false;
        suffix->getCanvas()->clear(SK_ColorTRANSPARENT);
        drawValidatedReferenceRangeToSkCanvas(*suffix->getCanvas(),plan,activeEnd,
            plan.referenceDrawList.entries.size(),images,text,true,this,grid);
        if(damageFailed_)return false;
        candidate->suffix=suffix->makeImageSnapshot();if(!candidate->suffix)return false;
    }
    if(auto* execution=RenderExecutionScope::current();execution&&(execution->pending()||execution->rejected()))return false;
    staticScene_=std::move(candidate);
    OI_NOTE(++objectWork.builds;objectWork.leaseBytes=staticScene_->leaseBytes);
    return true;
}
void SkiaRenderCache::drawHistoricalMasks(SkCanvas& canvas,const ReferenceTraversalEntry& entry){
    AXIOM_FRAME_TIMING_SCOPE(historyTiming,historical_mask_draw);
#if defined(AXIOM_ENABLE_FRAME_TIMING)
    for(const auto& mask:entry.record.eraseMasks)AXIOM_FRAME_TIMING_ADD(history_draw_primitives,
        std::holds_alternative<semantic::SweptCircleMask>(mask.geometry)?std::get<semantic::SweptCircleMask>(mask.geometry).segments.size():1);
#endif
    if(maskHistory&&!entry.record.eraseMasks.empty()){
        [[maybe_unused]]const auto before=maskHistory->work().history;
        auto plan=maskHistory->history(entry.record.eraseMasks,budget,std::min(options.maxEntryBytes,options.maxBytes/4));
        [[maybe_unused]]const auto after=maskHistory->work().history;
        G5_OBSERVE(g5::Kind::Counter,g5::Tag::mask_history,false,3,g5::word(entry.record.objectId,0),g5::word(entry.record.objectId,1),
            reinterpret_cast<std::uintptr_t>(entry.record.eraseMasks.storage().get()),plan?plan->identity:0,
            after.identityHits-before.identityHits,after.valueRecords-before.valueRecords,after.valuePrimitives-before.valuePrimitives,
            after.flattenedPrimitives-before.flattenedPrimitives,after.copiedBytes-before.copiedBytes,
            after.primitiveBuilds-before.primitiveBuilds,after.prefixReuses-before.prefixReuses,after.denied-before.denied);
        if(plan){for(const auto& primitive:plan->primitives)primitive->draw(canvas);return;}
    }
    // Optional preparation denial preserves the original sequential Skia mask
    // evaluator and target. This is not a CPU coverage or whole-frame fallback.
    drawEraseMasksToSkCanvas(canvas,entry);
}
bool SkiaRenderCache::canRetainCompact(const SkCanvas& canvas,const ReferenceTraversalEntry& entry) const noexcept {
    // GPU v4 SrcOver material can be retained after an object affine. Keep
    // CPU Direct's exact quantization and permanent destination-clear masks.
    const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command);
    return options.gpuCoverageCandidate&&canvas.recordingContext()&&compact&&
        compact->content.get().stroke.config.algorithmVersion==4&&entry.record.eraseMasks.empty();
}
void SkiaRenderCache::releaseMaterialSources(bool releaseWorkSurfaces)noexcept{
    bodyPixels_.reset();work.bodyPixelBytes=0;
    entries_.clear();coverage_.clear();runs_.clear();tileEntries_.clear();resetPreparation();
    if(releaseWorkSurfaces){coverageTargets_.clear();scratch_.reset();scratchLease_.reset();}
    work.retainedBytes=0;work.entries=0;work.coverageBytes=0;work.retainedRunBytes=0;
}
void SkiaRenderCache::clear() noexcept {bodyPixels_.reset();work.bodyPixelBytes=0;invalidateStaticScene(false);bodyDependencies_.clear();stagedBodies_=false;stagedMaterial_=false;stagedDemand_=0;stagedCreation_.reset();stagedCreationRoute_.reset();stagedCreationLease_.reset();inkLayers_.clear();maskHistory.reset();work.inkLayerBytes=0;entries_.clear();coverage_.clear();runs_.clear();work.retainedRunBytes=0;resetPreparation();coverageTargets_.clear();scratch_.reset();scratchLease_.reset();tiles_.reset();tileEntries_.clear();groups_.clear();eraseDamage_={};pendingErase_={};hasPendingErase_=false;groupSourceLeases_.clear();groupMetadataLease_.reset();frame_.reset();work.retainedBytes=0;work.coverageBytes=0;work.entries=0;}
bool SkiaRenderCache::releaseIdleWorkBuffers(const FrameState& current,const WorldToViewAffine& camera,
    const void* target,std::uint64_t resourceEpoch)noexcept{
    const bool matching=frame_&&target_==target&&resourceEpoch_==resourceEpoch&&
        frame_->viewId==current.viewId&&frame_->sceneGeneration==current.sceneGeneration&&
        frame_->sceneReadToken==current.sceneReadToken&&frame_->metrics==current.metrics&&
        frame_->metricsGeneration==current.metricsGeneration&&frame_->surfaceGeneration==current.surfaceGeneration&&
        frame_->viewportClip==current.viewportClip&&transform_==camera;
    if(pending_&&matching)return false;
    if(!matching&&(pending_||composite_))resetPreparation();
    scratch_.reset();scratchLease_.reset();coverageTargets_.clear();return true;
}
void SkiaRenderCache::begin(const FramePlan& plan,const void* target,SkSurface& surface,std::uint64_t resourceEpoch,foundation::ObjectId activeSource,bool transient){
    const auto& f=plan.frame;
    movingEraseBodies_=transient&&std::any_of(plan.referenceDrawList.entries.begin(),plan.referenceDrawList.entries.end(),[](const auto& entry){
        const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command);
        return entry.presentationEraseActive&&!entry.presentationEraseMasks.empty()&&compact&&compact->content.get().stroke.config.algorithmVersion==4;
    });
    if(resourceEpoch_!=resourceEpoch){bodyPixels_.reset();work.bodyPixelBytes=0;invalidateStaticScene();inkLayers_.clear();work.inkLayerBytes=0;}
    resourceEpoch_=resourceEpoch;
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
        clear();if(preparedGeometry)while(preparedGeometry->reclaimUnused()){}
        sourceAdmission_=retain();
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
    if(changed || !transient || activeSource_!=activeSource) invalidateStaticScene();
    if(changed){bodyPixels_.reset();work.bodyPixelBytes=0;inkLayers_.clear();work.inkLayerBytes=0;}
    else inkLayers_.prune(plan.referenceDrawList.entries);
    coverageTargets_.budget=budget;
    if(targetChanged){coverageTargets_.clear();coverage_.clear();scratch_.reset();scratchLease_.reset();work.coverageBytes=0;}
    const auto previous=frame_?frame_->sceneReadToken:foundation::SceneRevision{};
    tiledFrame_=false;
    if(changed){tileEntries_.clear();if(tiles_)tiles_->invalidate();entries_.clear();work.retainedBytes=0;++targetEpoch_;}
    AXIOM_FRAME_TIMING_SCOPE(groupTiming,group_update);
    auto updates=groups_.update(plan.referenceDrawList.entries,plan.referenceDrawList.viewportClip,resourceEpoch);
    if(stagedBodies_&&(!updates.erase.continuous||updates.erase.bounds))resetPreparation();
    eraseDamage_=updates.erase;
    if(transient){
        if(changed||!hasPendingErase_)pendingErase_=updates.erase;
        else {
            pendingErase_.active=updates.erase.active;
            pendingErase_.continuous&=updates.erase.continuous;
            if(const auto& area=updates.erase.bounds){
                if(!pendingErase_.bounds)pendingErase_.bounds=area;
                else {auto& b=*pendingErase_.bounds;b.left=std::min(b.left,area->left);b.top=std::min(b.top,area->top);b.right=std::max(b.right,area->right);b.bottom=std::max(b.bottom,area->bottom);}
            }
        }
        hasPendingErase_=true;eraseDamage_=pendingErase_;
    }else acknowledgeTransient();
    AXIOM_FRAME_TIMING_END(groupTiming);
    // Keep prior source leases until every retained group has rebound. Both
    // generations remain charged throughout this owner-thread transaction.
    for(const auto& entry:plan.referenceDrawList.entries){
        const auto found=bodyDependencies_.find(entry.record.objectId);if(found==bodyDependencies_.end())continue;
        if(!sameBodyIdentity(found->second.identity,preparedBodyIdentity(entry.command)))bodyDependencies_.erase(found);
        else found->second.seen=tick_+1;
    }
    std::erase_if(bodyDependencies_,[&](const auto& item){return item.second.seen!=tick_+1;});
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
        auto removed=it++;eraseEntry(removed);
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
    while(!entries_.empty()&&(work.retainedBytes+work.bodyPixelBytes>options.maxBytes-bytes||entries_.size()+std::size_t(bool(bodyPixels_))>=options.maxEntries)){
        auto victim=std::min_element(entries_.begin(),entries_.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});
        eraseEntry(victim);++work.evictions;
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
    G5_ROUTE(g5RecordRoute,g5::Route::recording);
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
    if(g5::enabled())g5::observeTarget(reinterpret_cast<std::uintptr_t>(canvas),3,0,0,0,options.gpuCoverageCandidate);
#endif
    // Recording has no GPU context. Carry the target realization explicitly;
    // white GPU coverage uses fixed-function SrcOver rather than destination-
    // reading Lighten. CPU recordings keep the pinned evaluator unchanged.
    drawCompactCoverageToSkCanvas(*canvas,stroke.config,stroke.knots,0,-1,stroke.seed,true,true,
        options.gpuCoverageCandidate&&surface_->recordingContext());
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
void SkiaRenderCache::eraseEntry(std::unordered_map<foundation::ObjectId,Entry,foundation::ObjectIdHash>::iterator it){
    if(!it->second.material||it->second.material.use_count()==1)work.retainedBytes-=it->second.bytes;
    entries_.erase(it);work.entries=entries_.size();
}
bool SkiaRenderCache::equivalentMaterial(const ReferenceTraversalEntry& a,const ReferenceTraversalEntry& b){
    if(a.geometryChunked||b.geometryChunked||a.record.transform!=b.record.transform||a.record.properties!=b.record.properties||
       a.record.eraseMasks.size()!=b.record.eraseMasks.size())return false;
    const auto body=readyBody(a,PreparedConsumer::kCanonical);
    if(!body||body!=readyBody(b,PreparedConsumer::kCanonical))return false;
    for(std::size_t i=0;i<a.record.eraseMasks.size();++i)
        if(a.record.eraseMasks[i].geometry!=b.record.eraseMasks[i].geometry)return false;
    return true;
}
bool SkiaRenderCache::aliasMaterial(const ReferenceTraversalEntry& source,const RenderCacheValidity& validity,const SkIRect& crop){
    if(entries_.contains(source.record.objectId)||source.geometryChunked)return false;
    if(entries_.size()+std::size_t(bool(bodyPixels_))>=options.maxEntries){bodyPixels_.reset();work.bodyPixelBytes=0;}
    if(entries_.size()>=options.maxEntries)return false;
    const auto body=readyBody(source,PreparedConsumer::kCanonical);if(!body)return false;
    for(const auto& [id,resident]:entries_){
        const auto& material=resident.material;
        // A ready body is pinned by the prepared owner after full immutable
        // content/config/seed comparison. Per-object revision numbers are not
        // an equivalence proof. All other pixel dependencies are checked here.
        const auto* group=groups_.find(id);
        if(!group||RenderSourceResolver::resolve(resident.validity,{group->revisions,targetEpoch_,false})!=RenderSourceKind::kRasterCache||
           !material||material->body!=body||material->targetEpoch!=targetEpoch_||material->resourceEpoch!=resourceEpoch_||resident.bounds!=crop||
           material->transform!=source.record.transform||material->properties!=source.record.properties||
           material->masks.size()!=source.record.eraseMasks.size())continue;
        bool masks=true;for(std::size_t i=0;i<material->masks.size();++i)
            if(material->masks[i].geometry!=source.record.eraseMasks[i].geometry){masks=false;break;}
        if(!masks)continue;
        const auto token=budget->reserve(resources::ResourceClass::kRecorded,sizeof(Entry)+128,true);if(!token)return false;
        Entry alias{validity,resident.image,crop,resident.bytes,tick_,material,resources::ResourceLease(budget,*token)};
        entries_.emplace(source.record.objectId,std::move(alias));AXIOM_FRAME_TIMING_ADD(material_alias_hits,1);++work.hits;work.entries=entries_.size();
        if(auto* execution=RenderExecutionScope::current())execution->reusePixels();return true;
    }
    return false;
}
SkiaRenderCache::Preparation SkiaRenderCache::prepareTransientMaterial(SkCanvas& canvas,const FramePlan& plan,
    PreparationAllowance& allowance,ImageResourceResolver* images,SkiaTextResources* text){
    // Preparation ignores presentation masks; the transient scene applies them
    // only to complete immutable canonical material. It never publishes scratch.
    SkAutoCanvasRestore restore(&canvas,true);
    canvas.scale(plan.frame.metrics.physicalWidth/plan.frame.metrics.logicalWidth,
        plan.frame.metrics.physicalHeight/plan.frame.metrics.logicalHeight);
    const auto& clip=plan.referenceDrawList.viewportClip;
    canvas.clipRect(SkRect::MakeLTRB(clip.left,clip.top,clip.right,clip.bottom));
    const auto result=prepare(canvas,plan,allowance.limits,images,text,&allowance);
    if(streamFrame_){
        // Canonical streaming includes no live masks. Use the existing complete
        // transient working-set evaluator when all material cannot reside;
        // never synchronously redraw nonresident sources after preparation.
        stagedBodies_=true;stagedMaterial_=true;
        stagedDemand_=std::count_if(plan.referenceDrawList.entries.begin(),plan.referenceDrawList.entries.end(),
            [&](const auto& entry){return needsPreparedBody(entry);});
        resetPreparation();return Preparation::kPending;
    }
    return result;
}
bool SkiaRenderCache::restoreBodyPixels([[maybe_unused]] SkCanvas& canvas,const ReferenceTraversalEntry& source,const SkIRect& crop){
    if(!bodyPixels_||source.geometryChunked)return false;
    const auto body=readyBody(source,PreparedConsumer::kCanonical);const auto& pixels=*bodyPixels_;const auto& basis=*pixels.material;
    // This witness pins the prepared body's fully validated immutable content,
    // config and seed. Mask identities never enter body-pixel qualification.
    if(!body||body!=basis.body||basis.targetEpoch!=targetEpoch_||basis.resourceEpoch!=resourceEpoch_||pixels.crop!=crop||
       basis.transform!=source.record.transform||basis.properties!=source.record.properties)return false;
    (void)basis.pixels.markGpuUse();auto* target=scratch_->getCanvas();target->resetMatrix();target->clear(SK_ColorTRANSPARENT);
    SkPaint copy;copy.setBlendMode(SkBlendMode::kSrc);target->drawImage(pixels.image,float(crop.left()),float(crop.top()),SkSamplingOptions{},&copy);
    ++work.bodyPixelHits;if(auto* execution=RenderExecutionScope::current())execution->reusePixels();return true;
}
void SkiaRenderCache::retainBodyPixels(SkCanvas& canvas,const ReferenceTraversalEntry& source,const SkIRect& crop){
    if(source.geometryChunked||!scratch_)return;
    const auto body=readyBody(source,PreparedConsumer::kCanonical);if(!body)return;
    const auto bytes=std::size_t(crop.width())*crop.height()*4;
    const auto optionalOther=work.retainedBytes+work.coverageBytes+work.retainedRunBytes+work.inkLayerBytes;
    if(!bytes||bytes>options.maxEntryBytes||bytes>options.maxBytes||optionalOther>options.maxBytes-bytes||
       entries_.size()>=options.maxEntries)return;
    // The old artifact is optional. Retirement stays charged through its last
    // real GPU read; a denied replacement simply keeps cooperative raster.
    bodyPixels_.reset();work.bodyPixelBytes=0;
    auto token=budget->reserve(resources::ResourceClass::kRaster,bytes,true);if(!token)return;
    resources::ResourceLease lease(budget,*token);
    auto metadata=budget->reserve(resources::ResourceClass::kRecorded,sizeof(BodyPixels)+sizeof(Material)+sourceMetadataBytes(source)+256,true);if(!metadata)return;
    resources::ResourceLease meta(budget,*metadata);auto bodySource=source;bodySource.record.eraseMasks.clear();bodySource.presentationEraseMasks.clear();
    auto sources=sourcePool->get(bodySource,budget,true);if(!sources)return;
    auto target=canvas.makeSurface(surface_->imageInfo().makeWH(crop.width(),crop.height()));if(!target)return;
    target->getCanvas()->clear(SK_ColorTRANSPARENT);auto image=scratch_->makeImageSnapshot();if(!image)return;
    target->getCanvas()->drawImage(image,float(-crop.left()),float(-crop.top()));image=target->makeImageSnapshot();if(!image)return;
    auto material=std::make_shared<Material>();material->body=body;material->command=source.command;
    material->transform=source.record.transform;material->properties=source.record.properties;material->sources=std::move(*sources);
    material->pixels=std::move(lease);material->metadata=std::move(meta);material->targetEpoch=targetEpoch_;material->resourceEpoch=resourceEpoch_;
    bodyPixels_=BodyPixels{std::move(image),crop,std::move(material),bytes};work.bodyPixelBytes=bytes;++work.bodyPixelBuilds;
}
bool SkiaRenderCache::hasPixels(const ReferenceTraversalEntry& entry)const{
    const auto* group=groups_.find(entry.record.objectId);if(!group)return false;
    const auto found=entries_.find(entry.record.objectId);
    return found!=entries_.end()&&RenderSourceResolver::resolve(found->second.validity,{group->revisions,targetEpoch_,false})==RenderSourceKind::kRasterCache;
}
bool SkiaRenderCache::needsPreparedBody(const ReferenceTraversalEntry& entry)const noexcept{
    const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command);
    return options.enabled&&options.gpuCoverageCandidate&&surface_&&surface_->recordingContext()&&preparedGeometry&&
        compact&&compact->content.get().stroke.config.algorithmVersion==4&&entry.contributesPixels;
}
SkiaRenderCache::Preparation SkiaRenderCache::prepareBody(const ReferenceTraversalEntry& entry,PreparationAllowance& allowance){
    if(!needsPreparedBody(entry))return Preparation::kReady;
    auto found=bodyDependencies_.find(entry.record.objectId);
    const auto identity=preparedBodyIdentity(entry.command);
    if(found!=bodyDependencies_.end()&&!sameBodyIdentity(found->second.identity,identity)){
        bodyDependencies_.erase(found);found=bodyDependencies_.end();
    }
    // Copies share one immutable body. A ready lookup neither builds geometry
    // nor scans its history. Keep a charged representative dependency alive;
    // the other aliases do not require a second per-object body reservation.
    std::shared_ptr<const PreparedNavigationGeometry> resident;
    if(movingEraseBodies_){
        resident=preparedGeometry->lookup(entry,PreparedConsumer::kDirect);
        if(resident&&std::any_of(bodyDependencies_.begin(),bodyDependencies_.end(),[&](const auto& held){return held.second.body==resident;}))return Preparation::kReady;
    }
    if(found==bodyDependencies_.end()){
        if(bodyDependencies_.size()>=options.maxEntries)return Preparation::kResourcePending;
        constexpr auto charge=sizeof(BodyDependency)+sizeof(foundation::ObjectId)+128;
        auto token=budget?budget->reserve(resources::ResourceClass::kRecorded,charge,true):std::optional<std::uint64_t>{};
        while(budget&&!token&&preparedGeometry->reclaimUnused())token=budget->reserve(resources::ResourceClass::kRecorded,charge,true);
        if(!token)return Preparation::kResourcePending;
        resources::ResourceLease metadata(budget,*token);
        auto body=resident?std::move(resident):preparedGeometry->request(entry,options,budget);
        if(!body)return preparedGeometry->requestRejected()?Preparation::kRejected:Preparation::kResourcePending;
        found=bodyDependencies_.emplace(entry.record.objectId,BodyDependency{identity,std::move(body),std::move(metadata)}).first;
    }
    const auto& body=found->second.body;
    while(!body->ready()){
        if(body->failed()||!allowance.limits.maxCoverageSteps||!allowance.limits.maxMicroseconds)return Preparation::kRejected;
        if(allowance.exhausted()){++work.preparationYields;return Preparation::kPending;}
        const auto before=allowance.steps;preparedGeometry->advance(body,allowance);
        // A removed/cancelled owner cannot advance this job. Re-admit through
        // the same bounded path on the next dirty retry, never a legacy leaf.
        if(allowance.steps==before){bodyDependencies_.erase(found);return Preparation::kPending;}
    }
    return Preparation::kReady;
}
SkiaRenderCache::Preparation SkiaRenderCache::prepareBodies(SkCanvas& canvas,const FramePlan& plan,PreparationAllowance& allowance){
    if(!options.enabled||!options.gpuCoverageCandidate||!canvas.recordingContext()||!preparedGeometry)return Preparation::kReady;
    if(!sourceAdmission_)return Preparation::kResourcePending;
    const auto demands=std::count_if(plan.referenceDrawList.entries.begin(),plan.referenceDrawList.entries.end(),
        [&](const auto& e){return needsPreparedBody(e)&&(!hasPixels(e)||e.presentationEraseActive);});
    // A strictly smaller demand gets one normal-residency admission attempt.
    // Same-demand retries retain their cooperative staged progress and latch.
    if(stagedBodies_&&std::size_t(demands)<stagedDemand_){
        stagedBodies_=false;stagedMaterial_=false;stagedDemand_=0;bodyDependencies_.clear();resetPreparation();
    }
    if(std::size_t(demands)>options.maxEntries&&!stagedBodies_&&!movingEraseBodies_){
        stagedDemand_=demands;stagedBodies_=true;bodyDependencies_.clear();resetPreparation();
    }
    // An oversized unique body set remains staged. Re-admitting the full
    // prefix before every staged step discards the same job indefinitely.
    if(stagedBodies_&&(stagedMaterial_||!movingEraseBodies_||std::size_t(demands)>options.maxEntries))return Preparation::kReady;
    for(const auto& entry:plan.referenceDrawList.entries){
        if(!needsPreparedBody(entry)||(hasPixels(entry)&&!entry.presentationEraseActive))continue;
        const auto result=prepareBody(entry,allowance);
        if(result==Preparation::kResourcePending){
            // Optional whole-demand residency is not a document limit. The
            // existing ordered staging consumes/reclaims one body at a time.
            stagedDemand_=demands;stagedBodies_=true;bodyDependencies_.clear();resetPreparation();return Preparation::kReady;
        }
        if(result!=Preparation::kReady)return result;
    }
    if(stagedBodies_&&movingEraseBodies_){
        // Same-demand staging is a pixel-residency decision, not a permanent
        // veto on an affordable active body set. All current bodies are now
        // ready and pinned under the existing quota. Use the existing ordered
        // damage renderer and independent body/history mask tiles. Obsolete
        // composite pixels cannot represent the new active prefix.
        stagedBodies_=false;stagedDemand_=0;resetPreparation();
    }
    return Preparation::kReady;
}
SkiaRenderCache::Preparation SkiaRenderCache::prepareWorkingSet(SkCanvas& canvas,const FramePlan& plan,
    PreparationAllowance& allowance,ImageResourceResolver* images,SkiaTextResources* text,bool direct,const EditingOverlay* overlay){
    if(!stagedBodies_)return Preparation::kReady;
    if(!allowance.limits.maxGroups||!allowance.limits.maxCoverageSteps||!allowance.limits.maxMicroseconds)return Preparation::kRejected;
    const auto* creation=overlay&&overlay->creationPreview()?&*overlay->creationPreview():nullptr;
    const auto* route=creation?overlay->creationRoute():nullptr;
    const bool creationChanged=bool(creation)!=bool(stagedCreation_)||
        (creation&&*creation!=*stagedCreation_)||bool(route)!=bool(stagedCreationRoute_)||
        (route&&*route!=*stagedCreationRoute_);
    if(creationChanged){
        resetPreparation();stagedCreation_.reset();stagedCreationRoute_.reset();stagedCreationLease_.reset();
        if(creation){
            // One derived creation object, charged before copying it. Reuse
            // the existing UI-owned value and drawing helper; no projection
            // or alternate semantic/brush owner is introduced here.
            std::size_t bytes=sizeof(semantic::ObjectRecord)+4096+sourceContentBytes(creation->content)+
                creation->properties.entries.capacity()*sizeof(semantic::PropertyEntry)+
                creation->erase_masks.capacity()*sizeof(semantic::EraseMaskRecord);
            for(const auto& mask:creation->erase_masks)std::visit([&](const auto& g){
                if constexpr(requires{g.segments;})bytes+=g.segments.capacity()*sizeof(semantic::EraseCubicSegment);
                if constexpr(requires{g.path.commands;})bytes+=g.path.commands.capacity()*sizeof(typename std::remove_cvref_t<decltype(g.path.commands)>::value_type);
            },mask.geometry);
            for(const auto& p:creation->properties.entries)if(const auto* v=std::get_if<semantic::StrokeStyleValue>(&p.value))
                if(const auto* stroke=std::get_if<semantic::SolidStroke>(v))if(const auto* dash=std::get_if<semantic::DashPattern>(&stroke->dash))bytes+=dash->segments.capacity()*sizeof(double);
            if(route)bytes+=sizeof(*route)+route->route.capacity()*sizeof(foundation::WorldPoint)+route->sketchObstacles.capacity()*sizeof(foundation::WorldRect);
            const auto token=budget?budget->reserve(resources::ResourceClass::kRecorded,bytes,true):std::optional<std::uint64_t>{};
            if(!token)return Preparation::kResourcePending;
            stagedCreationLease_=resources::ResourceLease(budget,*token);stagedCreation_=*creation;if(route)stagedCreationRoute_=*route;
        }
    }
    if(compositeReady_)return Preparation::kReady;
    SkAutoCanvasRestore restore(&canvas,true);
    canvas.scale(plan.frame.metrics.physicalWidth/plan.frame.metrics.logicalWidth,
        plan.frame.metrics.physicalHeight/plan.frame.metrics.logicalHeight);
    const auto& clip=plan.referenceDrawList.viewportClip;
    canvas.clipRect(SkRect::MakeLTRB(clip.left,clip.top,clip.right,clip.bottom));
    const auto frameBytes=std::size_t(surface_->width())*surface_->height()*4;
    const auto tileBytes=std::size_t(std::min(256,surface_->width()))*std::min(256,surface_->height())*4;
    if(tileBytes>options.sharedMaxBytes||frameBytes>(options.sharedMaxBytes-tileBytes)/2)return Preparation::kRejected;
    if(!ensureComposite(canvas,plan))return allocationFailure_;
    const auto yield=[&]{flushPreparation();++work.preparationYields;return Preparation::kPending;};
    std::size_t built=0;
    const bool nonInk=requiresInkIsolation(plan);
    const auto entries=plan.referenceDrawList.entries.size(),total=creation?entries*2+1:entries;
    for(;preparationCursor_<total;++preparationCursor_){
        if(built>=allowance.limits.maxGroups||allowance.exhausted())return yield();
        if(creation&&preparationCursor_==entries){
            drawTransientObjectToSkCanvas(*composite_->getCanvas(),plan,*creation,route);++built;++allowance.steps;continue;
        }
        const auto index=creation&&preparationCursor_>entries?preparationCursor_-entries-1:preparationCursor_;
        const auto& entry=plan.referenceDrawList.entries[index];
        if(!entry.contributesPixels||(creation&&(isInkReference(entry)!=(preparationCursor_>entries))))continue;
        if(!needsPreparedBody(entry)){
            drawValidatedReferenceRangeToSkCanvas(*composite_->getCanvas(),plan,index,index+1,images,text);
            ++built;++allowance.steps;continue;
        }
        const auto prepared=prepareBody(entry,allowance);
        if(prepared!=Preparation::kReady){if(prepared==Preparation::kResourcePending&&reclaimPreparation)reclaimPreparation(tileBytes);return prepared;}
        if(allowance.exhausted())return yield();
        const bool isolated=requiresInkEntryIsolation(entry,nonInk)||canRetainCompact(canvas,entry)||entry.presentationEraseActive||
            !entry.presentationEraseMasks.empty()||entry.presentationOpacity!=1.0F||
            (plan.isolateEraseMasks&&!entry.record.eraseMasks.empty());
        if(isolated&&!ensureScratch(canvas,true))return allocationFailure_;
        auto* target=(isolated?scratch_:composite_)->getCanvas();
        const auto* group=groups_.find(entry.record.objectId);if(!group)return Preparation::kRejected;
        const RenderCacheValidity validity{group->revisions,targetEpoch_,false};
        const auto& stroke=std::get<CompactStrokeReferenceCommand>(entry.command).content.get().stroke;
        if(!pending_){
            if(isolated){target->resetMatrix();target->clear(SK_ColorTRANSPARENT);}
            configureScratch(*target,plan,entryMatrix(canvas,plan,entry));
            auto job=std::make_unique<CompactStrokeRasterJob>(*target,stroke.config,stroke.knots,nullptr,
                options.gpuCoverageCandidate,budget,std::min(options.maxEntryBytes,options.maxBytes),reclaimRecorded,true,
                readyBody(entry,direct?PreparedConsumer::kDirect:PreparedConsumer::kCanonical),direct);
            target->restore();pending_=std::make_unique<Pending>(Pending{entry.record.objectId,validity,{},std::move(job)});
        }
        bool complete=false;
        do{
            configureScratch(*target,plan,entryMatrix(canvas,plan,entry));CompactRasterWork rasterWork;
            try{complete=pending_->raster->step(*target,stroke.config,stroke.knots,stroke.seed,coverageTargets_,options.gpuCoverageCandidate,&rasterWork);}
            catch(...){target->restore();throw;}
            target->restore();noteCoverage(rasterWork);++allowance.steps;++work.preparationCoverageSteps;
            if(pending_->raster->resourcePending()){
                // Prefix and current body/job survive quota retry. Reclaim only
                // unreferenced prepared cache entries and existing optional work.
                while(preparedGeometry->reclaimUnused()){}
                if(reclaimPreparation)reclaimPreparation(tileBytes);
                flushPreparation();return Preparation::kResourcePending;
            }
            if(!complete&&allowance.exhausted())return yield();
        }while(!complete);
        configureScratch(*target,plan,entryMatrix(canvas,plan,entry));
        drawHistoricalMasks(*target,entry);drawPresentationEraseMasksToSkCanvas(*target,entry);target->restore();
        if(isolated){
            SkPaint paint;paint.setAlphaf(std::clamp(entry.presentationOpacity,0.0F,1.0F));
            composite_->getCanvas()->drawImage(scratch_->makeImageSnapshot(),0,0,SkSamplingOptions{},&paint);
        }
        pending_.reset();bodyDependencies_.erase(entry.record.objectId);++built;++work.rasterizedGroups;
    }
    if(direct&&overlay)drawTransientChrome(*composite_->getCanvas(),plan,*overlay);
    if(!finishComposite())return Preparation::kResourcePending;
    flushPreparation();return Preparation::kReady;
}
SkiaRenderCache::Preparation SkiaRenderCache::prepare(
    SkCanvas& canvas,const FramePlan& plan,const RenderPreparationBudget& slice,ImageResourceResolver* images,SkiaTextResources* text,PreparationAllowance* shared){
    G5_SCOPE(g5Prepare,g5::Tag::prepare,g5::Purpose::none);
    G5_ROUTE(g5PreparedRoute,g5::Route::canonical_prepared);

    if(!sourceAdmission_)return Preparation::kResourcePending;
    if(!options.enabled||options.tileCandidate||!slice.maxGroups||!slice.maxMicroseconds||!slice.maxCoverageSteps||
       !options.maxEntries)return Preparation::kFallback;
    PreparationAllowance local(slice);auto& allowance=shared?*shared:local;
    // Standalone private cache consumers use this same admission entry point.
    // A ready miss resumes the existing job; it cannot just yield forever.
    const auto bodies=prepareBodies(canvas,plan,allowance);
    if(bodies!=Preparation::kReady)return bodies;
    if(stagedBodies_){
        SkAutoCanvasRestore restore(&canvas,true);canvas.resetMatrix();
        return prepareWorkingSet(canvas,plan,allowance,images,text,false);
    }
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
            // V4 masks affect completed material, not the pinned body evaluator.
            // Keep its body cooperative; applying masks via Direct here made
            // an individual entry bypass the owner's coverage quantum.
            if(!compact||!entry.contributesPixels||(!entry.record.eraseMasks.empty()&&
               compact->content.get().stroke.config.algorithmVersion!=4)){
                admissions_.insert_or_assign(entry.record.objectId,Admission{{},AdmissionKind::kDirect});
                AXIOM_FRAME_TIMING_ADD(prepare_direct,1);
                ++work.preparationDirectEntries;continue;
            }
            auto body=readyBody(entry,PreparedConsumer::kCanonical);
            SkRect support=body?body->support():SkRect::MakeEmpty();
            if(!body&&!hasPixels(entry)&&needsPreparedBody(entry))return Preparation::kPending;
            if(!body&&!hasPixels(entry)){
                AXIOM_FRAME_TIMING_ADD(prepare_knots,compact->content.get().stroke.knots.size());
                for(const auto& k:compact->content.get().stroke.knots){
                    if(auto* execution=RenderExecutionScope::current())execution->sourceWork(1,0);
                    const auto radius=ink::compactRadius(compact->content.get().stroke.config,k.pressure);
                    support.join(SkRect::MakeLTRB(float(k.x-radius),float(k.y-radius),float(k.x+radius),float(k.y+radius)));
                }
            }
            auto device=hasPixels(entry)?SkRect::Make(entries_.at(entry.record.objectId).bounds):entryMatrix(canvas,plan,entry).mapRect(support);
            if(!hasPixels(entry))device.outset(2,2);
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
            if(!requiresInkEntryIsolation(entry,isolatedInk)&&!canRetainCompact(canvas,entry)&&!(plan.isolateEraseMasks&&!entry.record.eraseMasks.empty())&&
               (t.a!=1||t.b!=0||t.c!=0||t.d!=1||t.tx!=0||t.ty!=0)){
                // Direct transformed ink-only material blends onto the actual
                // backdrop. A transparent per-object cache adds a quantization
                // pass. Stream into the bounded full-frame composite instead.
                admissions_.insert_or_assign(entry.record.objectId,Admission{crop,AdmissionKind::kComposite});
                streamFrame_=true;continue;
            }
            const auto allocation=std::uint64_t(crop.width())*crop.height()*4;
            // Count each completed isolated material once, not once per object.
            // Admission is still bounded by maxEntries for all aliases.
            const bool shareable=needsPreparedBody(entry)&&(plan.isolateEraseMasks||
                requiresInkEntryIsolation(entry,isolatedInk)||canRetainCompact(canvas,entry));
            const bool alias=shareable&&std::any_of(plan.referenceDrawList.entries.begin(),plan.referenceDrawList.entries.end(),[&](const auto& prior){
                const auto found=admissions_.find(prior.record.objectId);
                return prior.record.objectId!=entry.record.objectId&&found!=admissions_.end()&&
                    found->second.kind==AdmissionKind::kRaster&&found->second.crop==crop&&equivalentMaterial(entry,prior);
            });
            if(allocation>options.maxEntryBytes||allocation>options.maxBytes||(!alias&&bytes>options.maxBytes-allocation)||admitted>=options.maxEntries){
                admissions_.insert_or_assign(entry.record.objectId,Admission{crop,AdmissionKind::kDirect});
                AXIOM_FRAME_TIMING_ADD(prepare_budget,1);
                ++work.preparationBudgetEntries;streamFrame_=true;continue;
            }
            if(!alias)bytes+=allocation;++admitted;admissions_.insert_or_assign(entry.record.objectId,Admission{crop,AdmissionKind::kRaster});
        }
        // Admission is deterministic for the entire demand. Drop ineligible
        // and stale residents before filling it; never evict a prepared prefix.
        for(auto it=entries_.begin();it!=entries_.end();){
            const auto admissionEntry=admissions_.find(it->first);const auto* group=groups_.find(it->first);
            if(admissionEntry==admissions_.end()||admissionEntry->second.kind!=AdmissionKind::kRaster||!group||
               RenderSourceResolver::resolve(it->second.validity,{group->revisions,targetEpoch_,false})!=RenderSourceKind::kRasterCache){
                auto removed=it++;eraseEntry(removed);
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
    std::size_t built=0;
    const auto exhausted=[&]{return built>=slice.maxGroups||allowance.exhausted();};
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
                noteDirect();++built;++allowance.steps;
            }
            continue;
        }
        const auto* group=groups_.find(entry.record.objectId);if(!group)return Preparation::kFallback;
        const RenderCacheValidity validity{group->revisions,targetEpoch_,false};
        if(raster&&!pending_&&(plan.isolateEraseMasks||requiresInkEntryIsolation(entry,requiresInkIsolation(plan))||canRetainCompact(canvas,entry)))aliasMaterial(entry,validity,admission->second.crop);
        const auto found=entries_.find(entry.record.objectId);
        if(raster&&found!=entries_.end()&&RenderSourceResolver::resolve(found->second.validity,validity)==RenderSourceKind::kRasterCache){
            if(found->second.material)(void)found->second.material->pixels.markGpuUse();
            captureRunEntry(plan,preparationCursor_,found->second.image,found->second.bounds.left(),found->second.bounds.top());
            if(streamFrame_){
                if(exhausted())return yield();
                composite_->getCanvas()->drawImage(found->second.image,float(found->second.bounds.left()),float(found->second.bounds.top()));
                found->second.used=tick_;++work.hits;if(auto* execution=RenderExecutionScope::current())execution->reusePixels();++allowance.steps;
            }
            continue;
        }
        if(exhausted())return yield();
        // Reuse the existing device body/history tiles for canonical isolated
        // v4 sources too. Partial work stays on disposable scratch; each retry
        // clears scratch and resumes already qualified tiles, never a provider.
        const auto* layered=std::get_if<CompactStrokeReferenceCommand>(&entry.command);
        const auto layerBytes=std::uint64_t(admission->second.crop.width()+2*SkiaInkLayers::tileSize)*
            (admission->second.crop.height()+2*SkiaInkLayers::tileSize)*10;
        const auto inkCount=std::count_if(plan.referenceDrawList.entries.begin(),plan.referenceDrawList.entries.end(),[](const auto& e){return std::holds_alternative<CompactStrokeReferenceCommand>(e.command);});
        const auto tileCount=std::size_t((admission->second.crop.width()+SkiaInkLayers::tileSize-1)/SkiaInkLayers::tileSize+1)*
            ((admission->second.crop.height()+SkiaInkLayers::tileSize-1)/SkiaInkLayers::tileSize+1);
        const bool layerFits=inkCount&&layerBytes<=options.maxBytes/std::size_t(inkCount)&&
            tileCount<=options.maxEntries/std::size_t(inkCount);
        if(raster&&!pending_&&!entry.presentationEraseActive&&entry.presentationEraseMasks.empty()&&entry.presentationOpacity==1.0F&&layered&&layered->content.get().stroke.config.algorithmVersion==4&&
           options.gpuCoverageCandidate&&canvas.recordingContext()&&layerFits&&
           (plan.isolateEraseMasks||requiresInkEntryIsolation(entry,requiresInkIsolation(plan)))){
            if(!ensureScratch(canvas,true))return allocationFailure_;
            auto* target=scratch_->getCanvas();target->resetMatrix();target->clear(SK_ColorTRANSPARENT);
            const auto& clip=plan.referenceDrawList.viewportClip;const auto& metrics=plan.frame.metrics;
            SkIRect viewport;SkMatrix scale=SkMatrix::Scale(metrics.physicalWidth/metrics.logicalWidth,metrics.physicalHeight/metrics.logicalHeight);
            scale.mapRect(SkRect::MakeLTRB(clip.left,clip.top,clip.right,clip.bottom)).roundOut(&viewport);
            viewport.intersect(SkIRect::MakeWH(surface_->width(),surface_->height()));
            target->save();target->clipIRect(admission->second.crop);
            bool painted=false,resourcePending=false;
            const bool complete=inkLayers_.draw(*target,*scratch_,entry,entryMatrix(canvas,plan,entry),viewport,nullptr,
                options,budget,sourcePool,coverageTargets_,work,painted,resourcePending,preparedGeometry,
                readyBody(entry,PreparedConsumer::kCanonical),maskHistory,&allowance);
            target->restore();
            if(!complete){if(resourcePending)return yield();}
            else {
                const bool stored=store(canvas,entry,validity,admission->second.crop,false,true);
                if(!stored)return yield();
                const auto& resident=entries_.at(entry.record.objectId);
                captureRunEntry(plan,preparationCursor_,resident.image,resident.bounds.left(),resident.bounds.top());
                if(streamFrame_)composite_->getCanvas()->drawImage(resident.image,float(resident.bounds.left()),float(resident.bounds.top()));
                ++built;continue;
            }
        }
        const bool bodyReusable=!directComposite&&needsPreparedBody(entry)&&
            (plan.isolateEraseMasks||requiresInkEntryIsolation(entry,requiresInkIsolation(plan))||canRetainCompact(canvas,entry));
        if(!directComposite&&!ensureScratch(canvas,true))return allocationFailure_;
        const bool bodyHit=bodyReusable&&!pending_&&restoreBodyPixels(canvas,entry,admission->second.crop);
        auto* target=(directComposite?composite_:scratch_)->getCanvas();
        if(!bodyHit){
            if(!pending_){
                if(!directComposite&&!ensureScratch(canvas,true))return allocationFailure_;
                const auto admissionEntry=admissions_.find(entry.record.objectId);if(admissionEntry==admissions_.end())return Preparation::kFallback;
                auto* target=(directComposite?composite_:scratch_)->getCanvas();
                if(!directComposite){target->resetMatrix();target->clear(SK_ColorTRANSPARENT);}
                configureScratch(*target,plan,entryMatrix(canvas,plan,entry));
                const auto& stroke=std::get<CompactStrokeReferenceCommand>(entry.command).content.get().stroke;
                auto rasterJob=std::make_unique<CompactStrokeRasterJob>(*target,stroke.config,stroke.knots,nullptr,options.gpuCoverageCandidate,budget,
                    std::min(options.maxEntryBytes,options.maxBytes),reclaimRecorded,true,readyBody(entry,PreparedConsumer::kCanonical));
                target->restore();
                pending_=std::make_unique<Pending>(Pending{entry.record.objectId,validity,admissionEntry->second.crop,std::move(rasterJob)});
                ++work.misses;
            }
            if(pending_->id!=entry.record.objectId||RenderSourceResolver::resolve(pending_->validity,validity)!=RenderSourceKind::kRasterCache){
                pending_.reset();return Preparation::kFallback;
            }
            const auto& stroke=std::get<CompactStrokeReferenceCommand>(entry.command).content.get().stroke;
            bool complete=false;
            do{
                configureScratch(*target,plan,entryMatrix(canvas,plan,entry));
                CompactRasterWork rasterWork;
                try{complete=pending_->raster->step(*target,stroke.config,stroke.knots,stroke.seed,
                        coverageTargets_,options.gpuCoverageCandidate,&rasterWork);}
                catch(...){target->restore();pending_.reset();throw;}
                target->restore();noteCoverage(rasterWork);++allowance.steps;++work.preparationCoverageSteps;
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
            if(movingEraseBodies_&&transientTileRefused())return yield();
            if(bodyReusable)retainBodyPixels(canvas,entry,admission->second.crop);
        }
        // Match Direct's original destination and complete viewport clip.
        // Isolated entries clear scratch only; legacy transformed ink-only
        // entries clear the ordered composite, preserving existing semantics.
        if(!entry.record.eraseMasks.empty()){
            configureScratch(*target,plan,entryMatrix(canvas,plan,entry));
            drawHistoricalMasks(*target,entry);target->restore();
        }
        const auto crop=admission->second.crop;pending_.reset();
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
        if(entry.presentationOpacity!=1.0F||!entry.presentationEraseMasks.empty()||!group||run.stamps[i-run.begin]!=RunStamp{entry.record.objectId,group->revisions})return false;
    }
    return true;
}
SkiaRenderCache::InkDraw SkiaRenderCache::drawDamagedInk(SkCanvas& canvas,const FramePlan& plan,
    const ReferenceTraversalEntry& source,ImageResourceResolver* images,SkiaTextResources* text,const CompactDeviceGrid* grid) {
    if(transientObjectRefused()){damageFailed_=true;return InkDraw::kRetry;}
    const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&source.command);
    if(needsPreparedBody(source)&&!hasPixels(source)&&!readyBody(source,PreparedConsumer::kDirect)){
        if(auto* execution=RenderExecutionScope::current())execution->unavailable({},ExecutionReason::kAllocationDenied);
        damageFailed_=true;return InkDraw::kRetry;
    }
    if(hasPixels(source)&&source.presentationEraseMasks.empty()&&source.presentationOpacity==1.0F){
        if(draw(canvas,plan,source,true,true,false))return InkDraw::kDrawn;
    }
    const bool gpuLive=options.enabled&&options.gpuCoverageCandidate&&canvas.recordingContext()&&
        (damageOnly_||source.presentationEraseActive)&&compact&&compact->content.get().stroke.config.algorithmVersion==4;
    if(gpuLive&&!ensureScratch(canvas,true)){damageFailed_=true;return InkDraw::kRetry;}
    constexpr auto edge=SkiaInkLayers::tileSize,guard=SkiaInkLayers::maskGuard;
    const auto layerTileBytes=std::size_t(edge)*std::min(edge,surface_->height())*4U;
    const auto inkSources=std::count_if(plan.referenceDrawList.entries.begin(),plan.referenceDrawList.entries.end(),[](const auto& e){return std::holds_alternative<CompactStrokeReferenceCommand>(e.command);});
    const auto original=grid?SkIRect::MakeLTRB(grid->left,grid->top,grid->right,grid->bottom):canvas.getDeviceClipBounds();
    auto demand=canvas.getDeviceClipBounds();const bool visible=demand.intersect(original);
    const auto columns=visible?std::size_t((demand.right()-1-original.left())/edge-(demand.left()-original.left())/edge+1):0;
    const auto rows=visible?std::size_t((demand.bottom()-1-original.top())/edge-(demand.top()-original.top())/edge+1):0;
    const auto tiles=columns*rows;
    // Evicted images remain charged until GPU completion. A synchronous live
    // frame cannot stream a working set larger than optional residency: it
    // would restart the same incomplete frame on every RAF. Keep the existing
    // narrow direct path when this demand cannot retain all affected sources.
    const auto tilePeak=layerTileBytes+std::size_t(edge+2*guard)*(std::min(edge,surface_->height())+2*guard)*4U+1024;
    const auto used=budget?budget->used():options.sharedMaxBytes;
    const auto nonLayers=used-std::min(used,work.inkLayerBytes);
    const auto layerCapacity=std::min(options.maxBytes,options.sharedMaxBytes-std::min(options.sharedMaxBytes,nonLayers));
    const bool fits=tiles&&tiles<=layerCapacity/tilePeak&&std::size_t(inkSources)<=layerCapacity/(tilePeak*tiles);
    if(gpuLive&&!hasPixels(source)&&sourceAdmission_&&options.maxEntryBytes>=layerTileBytes&&fits){
        // Body layers have no permanent/transient masks in their identity.
        // All masks retain the original full viewport/device grid.
        const auto transform=entryMatrix(canvas,plan,source);
        const auto& v=plan.referenceDrawList.viewportClip;const auto& m=plan.frame.metrics;
        SkIRect viewport;
        if(grid)viewport=SkIRect::MakeLTRB(grid->left,grid->top,grid->right,grid->bottom);
        else {SkMatrix scale=SkMatrix::Scale(float(m.physicalWidth)/m.logicalWidth,float(m.physicalHeight)/m.logicalHeight);
            scale.mapRect(SkRect::MakeLTRB(v.left,v.top,v.right,v.bottom)).roundOut(&viewport);viewport.intersect(SkIRect::MakeWH(surface_->width(),surface_->height()));}
        bool painted=false,resourcePending=false;
        if(inkLayers_.draw(canvas,*scratch_,source,transform,viewport,grid,options,budget,sourcePool,coverageTargets_,work,painted,resourcePending,preparedGeometry,readyBody(source,PreparedConsumer::kRetainedInk),maskHistory))return InkDraw::kDrawn;
        // An allocation miss after any composite must reject this entire
        // disposable frame; replaying Direct would blend already drawn ink.
        // Reuse denial is otherwise safe because the provider is not presented.
        if(painted||resourcePending){
            if(resourcePending&&reclaimPreparation)reclaimPreparation(tilePeak);
            damageFailed_=true;return InkDraw::kRetry;
        }
    }
    if((!damageOnly_&&!gpuLive)||!scratch_)return InkDraw::kSkipped;
    auto* isolated=scratch_->getCanvas();isolated->resetMatrix();isolated->clear(SK_ColorTRANSPARENT);
    const auto transform=entryMatrix(canvas,plan,source);
    // Reuse a complete resident source when available. A miss never enters the
    // LRU: only the small damage coverage is evaluated on the existing scratch.
    isolated->save();isolated->clipIRect(canvas.getDeviceClipBounds());
    // This canvas is scratch_, in physical pixel coordinates. A miss must not
    // enter draw's builder: it would omit the destination DPR transform and
    // composite its result back into the same scratch surface a second time.
    const bool resident=draw(*isolated,plan,source,true,true,false);isolated->restore();
    configureScratch(*isolated,plan,transform);
    if(!resident) {
        G5_ROUTE(g5EraseMiss,g5::Route::erase_miss);
        G5_CANVAS(*isolated,true);
        isolated->save();isolated->resetMatrix();isolated->clipIRect(canvas.getDeviceClipBounds());isolated->setMatrix(transform);
        // A nonresident body is still part of the live GPU presentation path.
        // Preserve the qualified v4 coverage policy instead of silently using
        // the CPU Direct oracle on every damage frame under cache pressure.
        const bool gpuV4=options.gpuCoverageCandidate&&compact&&compact->content.get().stroke.config.algorithmVersion==4&&isolated->recordingContext();
        CompactRasterWork rasterWork;
        drawInkBodyToSkCanvas(*isolated,source,images,text,grid,&rasterWork,gpuV4,gpuV4?&coverageTargets_:nullptr,true,readyBody(source,PreparedConsumer::kDirect).get());
        noteCoverage(rasterWork);isolated->restore();
        if(rasterWork.resourcePending){
            isolated->restore();if(reclaimPreparation)reclaimPreparation(256U*256U*4U);
            damageFailed_=true;return InkDraw::kRetry;
        }
        // Narrowing this clip changes Skia stroke-mask AA at existing edges.
        // Keep both mask evaluators on their full original device surface.
        drawHistoricalMasks(*isolated,source);noteDirect();
    }
    drawPresentationEraseMasksToSkCanvas(*isolated,source);isolated->restore();
    SkPaint feedback;feedback.setAlphaf(std::clamp(source.presentationOpacity,0.0F,1.0F));
    auto image=scratch_->makeImageSnapshot();
    if(!image){damageFailed_=true;return InkDraw::kRetry;} // Reject this frame; never present missing ink.
    canvas.save();canvas.resetMatrix();canvas.drawImage(image,0,0,SkSamplingOptions{},&feedback);
    canvas.restore();return InkDraw::kDrawn;
}
void SkiaRenderCache::planRuns(SkCanvas& canvas,const FramePlan& plan){
    // Small scenes already fit the per-object cache. Runs are a bounded
    // composition representation for pressure, not another unbounded residency.
    if(!options.enabled||plan.referenceDrawList.entries.size()<=options.maxEntries)return;
    const auto eligible=[&](const ReferenceTraversalEntry& entry){
        const auto& t=entry.record.transform;
        // Identity compact entries have destination-independent isolation in
        // the shared mapping. Legacy destination-clear never enters a run.
        return entry.presentationOpacity==1.0F&&entry.presentationEraseMasks.empty()&&entry.record.objectId!=activeSource_&&std::holds_alternative<CompactStrokeReferenceCommand>(entry.command)&&
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
bool SkiaRenderCache::drawTransientScene(SkCanvas& canvas,const FramePlan& plan,ImageResourceResolver* images,SkiaTextResources* text,bool includeInk,bool damageOnly,const CompactDeviceGrid* grid,bool objectTransform){
    struct Restore {bool& value;bool old;~Restore(){value=old;}} restore{damageOnly_,damageOnly_};
    damageOnly_=damageOnly;damageFailed_=false;
    // A transient source may include permanent masks that cooperative exact
    // preparation routes Direct. Reuse/build its bounded isolated cache here;
    // never keep an exact-frame admission that forbids that source reuse.
    if(pending_||composite_)resetPreparation();
    const auto& entries=plan.referenceDrawList.entries;
    const auto active=std::find_if(entries.begin(),entries.end(),[&](const auto& e){return e.record.objectId==activeSource_;});
    const bool supported=active!=entries.end() && active->contributesPixels &&
        (active->record.kind==semantic::ObjectKind::kShape || active->record.kind==semantic::ObjectKind::kSticky ||
         active->record.kind==semantic::ObjectKind::kTable || active->record.kind==semantic::ObjectKind::kImage);
    bool eligible=objectTransform && options.enabled && sourceAdmission_ && !stagedBodies_ &&
        includeInk && !damageOnly && supported && canvas.getTotalMatrix().isIdentity() && canvas.isClipRect() &&
        canvas.getDeviceClipBounds()==SkIRect::MakeWH(surface_->width(),surface_->height());
    std::size_t first=std::size_t(std::distance(entries.begin(),active)),end=first+1;
    const bool hasNonInk=requiresInkIsolation(plan);
    if(eligible)for(std::size_t i=0;i<entries.size();++i){
        const auto& e=entries[i];
        if(e.presentationEraseActive || !e.presentationEraseMasks.empty())eligible=false;
        if(i!=first && !e.record.eraseMasks.empty() &&
           !requiresInkEntryIsolation(e,hasNonInk) && !plan.isolateEraseMasks)eligible=false;
        if(i!=first && std::find(e.record.directDependencies.begin(),e.record.directDependencies.end(),activeSource_)!=e.record.directDependencies.end())eligible=false;
        if(e.record.placement.parent_id && *e.record.placement.parent_id==activeSource_){
            if(i!=end)eligible=false;else ++end;
        }
    }
    if(eligible){
        const bool hit=staticSceneBasisMatches(plan,first,end);
        if(!hit){
            invalidateStaticScene();
            if(buildStaticScene(canvas,plan,first,end,images,text,grid)){
                OI_NOTE(++objectWork.fullSceneDraws);
            }else if(damageFailed_ || (RenderExecutionScope::current() &&
                (RenderExecutionScope::current()->pending() || RenderExecutionScope::current()->rejected())))return false;
        }
        const auto retained=staticScene_;
        if(retained){
            const bool marked=retained->pixelLease.markGpuUse();(void)marked;
            OI_NOTE(objectWork.gpuLeaseMarks+=marked;objectWork.hits+=hit;++objectWork.activeDraws);
            if(auto* execution=RenderExecutionScope::current())execution->reusePixels();
            {SkAutoCanvasRestore restoreCanvas(&canvas,true);canvas.resetMatrix();
             SkPaint paint;paint.setBlendMode(SkBlendMode::kSrc);
             canvas.drawImage(retained->prefix,0,0,SkSamplingOptions{},&paint);}
            drawValidatedReferenceRangeToSkCanvas(canvas,plan,first,end,images,text,true,this,grid);
            if(damageFailed_)return false;
            if(retained->suffix){
                (void)retained->pixelLease.markGpuUse();
                SkAutoCanvasRestore restoreCanvas(&canvas,true);canvas.resetMatrix();
                SkPaint paint;paint.setBlendMode(SkBlendMode::kSrcOver);
                canvas.drawImage(retained->suffix,0,0,SkSamplingOptions{},&paint);
            }
            return true;
        }
    }else invalidateStaticScene();
    OI_NOTE(++objectWork.fullSceneDraws);
    drawReferenceBackgroundToSkCanvas(canvas,plan);
    std::size_t begin=0;
    if(includeInk)for(std::size_t i=0;i<plan.referenceDrawList.entries.size();++i){
        const auto found=std::find_if(runs_.begin(),runs_.end(),[&](const auto& run){return run.image&&run.begin==i&&validRun(run,plan);});
        if(found==runs_.end())continue;
        drawValidatedReferenceRangeToSkCanvas(canvas,plan,begin,i,images,text,true,this,grid);
        if(damageFailed_)return false;
        (void)compositeRetainedRun(canvas,plan,i);begin=i+1;
    }
    drawValidatedReferenceRangeToSkCanvas(canvas,plan,begin,plan.referenceDrawList.entries.size(),images,text,includeInk,this,grid);
    return !damageFailed_;
}
void SkiaRenderCache::resetPreparation(){
    AXIOM_FRAME_TIMING_ADD(prepare_resets,1);
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
        bodyPixels_.reset();work.bodyPixelBytes=0;entries_.clear();coverage_.clear();runs_.clear();inkLayers_.clear();work.inkLayerBytes=0;
        work.retainedBytes=0;work.entries=0;work.coverageBytes=0;work.retainedRunBytes=0;
        if(preparedGeometry)while(preparedGeometry->reclaimUnused()){}
        if(reclaimPreparation)reclaimPreparation(bytes);
        token=budget->reserve(kind,bytes,true);
    }
    if(budget&&!token){
        if(!stagedBodies_&&bodyDependencies_.size()>1){stagedDemand_=bodyDependencies_.size();stagedBodies_=true;bodyDependencies_.clear();resetPreparation();}
        return false;
    }
    resources::ResourceLease lease(budget,token.value_or(0));
    auto candidate=canvas.makeSurface(info);
    if(!candidate){allocationFailure_=Preparation::kRejected;return false;}
    target=std::move(candidate);targetLease=std::move(lease);++work.scratchAllocations;
    return true;
}
void SkiaRenderCache::flushPreparation(){
    AXIOM_FRAME_TIMING_SCOPE(flushTiming,coverage_flush);
    // Submit bounded offscreen work without presenting or a CPU fence. Leaving
    // all GL work queued until the final present merely moves the long stall.
    if(auto* context=GrAsDirectContext(surface_->recordingContext())){
        context->flush();(void)context->submit(GrSyncCpu::kNo);
    }
}
bool SkiaRenderCache::compositePreparedFrame(SkCanvas& canvas){
    G5_SCOPE(g5Composite,g5::Tag::material,g5::Purpose::none);
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
bool SkiaRenderCache::draw(SkCanvas& canvas,const FramePlan& plan,const ReferenceTraversalEntry& source,bool isolated,bool composite,bool allowBuild){
    G5_SCOPE(g5Lookup,g5::Tag::lookup,g5::Purpose::none);

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
        G5_ROUTE(g5Resident,source.presentationEraseActive?g5::Route::erase_resident:g5::context().route);
        G5_CANVAS(canvas,true);
        G5_OBSERVE(g5::Kind::Stage,g5::Tag::cached_pixels,true,g5::word(source.record.objectId,0),g5::word(source.record.objectId,1),targetEpoch_,plan.frame.sceneGeneration.value(),plan.frame.sceneReadToken.value(),plan.frame.camera.generation.value());
        if(composite){canvas.save();canvas.resetMatrix();canvas.drawImage(found->second.image,static_cast<float>(found->second.bounds.left()),static_cast<float>(found->second.bounds.top()));canvas.restore();}
        if(found->second.material)(void)found->second.material->pixels.markGpuUse();
        found->second.used=tick_;++work.hits;if(auto* execution=RenderExecutionScope::current())execution->reusePixels();return true;
    }
    // Damage feedback can reuse a complete resident source, but a miss is
    // painted directly under the small destination clip. Never cycle the LRU
    // or create clipped images that could later masquerade as full sources.
    if(damageOnly_||!allowBuild)return false;
    ++work.misses;
    if(found!=entries_.end()){eraseEntry(found);}
    const auto& r=source.record.transform;
    const auto& v=plan.referenceDrawList.worldToView;
    SkMatrix world;world.setAll(static_cast<float>(v.a),static_cast<float>(v.c),static_cast<float>(v.tx),static_cast<float>(v.b),static_cast<float>(v.d),static_cast<float>(v.ty),0,0,1);
    SkMatrix object;object.setAll(static_cast<float>(r.a),static_cast<float>(r.c),static_cast<float>(r.tx),static_cast<float>(r.b),static_cast<float>(r.d),static_cast<float>(r.ty),0,0,1);
    const auto matrix=SkMatrix::Concat(canvas.getTotalMatrix(),SkMatrix::Concat(world,object));
    const auto body=readyBody(source,PreparedConsumer::kDirect);
    if(!body&&needsPreparedBody(source))return false;
    SkRect support=body?body->support():SkRect::MakeEmpty();
    if(!body)for(const auto& k:compact->content.get().stroke.knots){
        if(auto* execution=RenderExecutionScope::current())execution->sourceWork(1,0);
        const auto radius=ink::compactRadius(compact->content.get().stroke.config,k.pressure);
        support.join(SkRect::MakeLTRB(static_cast<float>(k.x-radius),static_cast<float>(k.y-radius),static_cast<float>(k.x+radius),static_cast<float>(k.y+radius)));}
    const auto commands=body?sk_sp<SkPicture>{}:coverage(source,*group,support);
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
    if(compact->content.get().stroke.config.algorithmVersion==4&&!source.record.eraseMasks.empty()){
        // The existing v4 command mapping has no range/picture override here:
        // keep its same prepared body/material/target, then consume history.
        drawInkBodyToSkCanvas(*target,source,nullptr,nullptr,nullptr,&rasterWork,options.gpuCoverageCandidate,&coverageTargets_,false,body.get());
        drawHistoricalMasks(*target,source);
    }else drawValidatedIsolatedEntryToSkCanvas(*target,source,commands.get(),nullptr,&rasterWork,options.gpuCoverageCandidate,&coverageTargets_,nullptr,nullptr,nullptr,body.get());
    noteCoverage(rasterWork);}
    target->restore();
    return store(canvas,source,validity,crop,composite);
}
bool SkiaRenderCache::store(SkCanvas& canvas,const ReferenceTraversalEntry& source,
    const RenderCacheValidity& validity,const SkIRect& crop,bool composite,bool preservePrepared){
    const auto info=surface_->imageInfo();const auto bytes=std::size_t(crop.width())*crop.height()*4;
    const auto old=entries_.find(source.record.objectId);
    if(old!=entries_.end()){eraseEntry(old);}
    // Own a physically cropped representation. A subset image is allowed to
    // retain its source allocation, which would undercount residency.
    if(bodyPixels_&&(work.retainedBytes+work.bodyPixelBytes>options.maxBytes-bytes||
       entries_.size()+1>=options.maxEntries)){bodyPixels_.reset();work.bodyPixelBytes=0;}
    if(!preservePrepared)evict(static_cast<std::size_t>(bytes));
    if(work.retainedBytes+work.bodyPixelBytes>options.maxBytes-bytes||
       entries_.size()+std::size_t(bool(bodyPixels_))>=options.maxEntries){
        // Complete material has priority over its optional reusable body.
        // Dropping the owner does not free an in-flight GPU pixel reservation.
        bodyPixels_.reset();work.bodyPixelBytes=0;
    }
    auto reserved=budget?budget->reserve(resources::ResourceClass::kRaster,bytes,true):std::optional<std::uint64_t>{};
    if(budget&&!reserved&&bodyPixels_){
        bodyPixels_.reset();work.bodyPixelBytes=0;
        reserved=budget->reserve(resources::ResourceClass::kRaster,bytes,true);
    }
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
    auto sources=sourcePool?sourcePool->get(source,budget,true):std::nullopt;if(!sources)return false;
    const auto charge=sizeof(Material)+sourceMetadataBytes(source)+256;
    const auto ownerToken=budget->reserve(resources::ResourceClass::kRecorded,charge,true);
    if(!ownerToken)return false;resources::ResourceLease ownerLease(budget,*ownerToken);
    const auto aliasToken=budget->reserve(resources::ResourceClass::kRecorded,sizeof(Entry)+128,true);
    if(!aliasToken)return false;resources::ResourceLease aliasLease(budget,*aliasToken);
    auto material=std::make_shared<Material>();material->body=readyBody(source,PreparedConsumer::kCanonical);
    material->command=source.command;material->masks=source.record.eraseMasks;material->transform=source.record.transform;
    material->properties=source.record.properties;material->sources=std::move(*sources);material->pixels=std::move(lease);
    material->metadata=std::move(ownerLease);material->targetEpoch=targetEpoch_;material->resourceEpoch=resourceEpoch_;
    entries_.emplace(source.record.objectId,Entry{validity,image,crop,static_cast<std::size_t>(bytes),tick_,std::move(material),std::move(aliasLease)});
    work.retainedBytes+=static_cast<std::size_t>(bytes);work.entries=entries_.size();++work.rasterizedGroups;
    if(composite){canvas.save();canvas.resetMatrix();canvas.drawImage(image,static_cast<float>(crop.left()),static_cast<float>(crop.top()));canvas.restore();}
    return true;
}
}
