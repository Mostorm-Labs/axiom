#include "skia_render_cache_internal.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPictureRecorder.h"
#include <algorithm>
#include <cmath>

namespace canvas::render::internal {
void SkiaRenderCache::clear() noexcept {entries_.clear();coverage_.clear();scratch_.reset();scratchLease_.reset();tiles_.reset();tileEntries_.clear();groups_.clear();frame_.reset();work.retainedBytes=0;work.coverageBytes=0;work.entries=0;}
void SkiaRenderCache::begin(const FramePlan& plan,const void* target,SkSurface& surface,std::uint64_t resourceEpoch){
    const auto& f=plan.frame;
    const bool targetChanged=!frame_||target_!=target||surface_!=&surface||frame_->viewId!=f.viewId||
        frame_->surfaceGeneration!=f.surfaceGeneration||frame_->metrics!=f.metrics||
        frame_->metricsGeneration!=f.metricsGeneration;
    const bool changed=targetChanged||transform_!=plan.referenceDrawList.worldToView;
    if(targetChanged){coverage_.clear();scratch_.reset();scratchLease_.reset();work.coverageBytes=0;}
    const auto previous=frame_?frame_->sceneReadToken:foundation::SceneRevision{};
    tiledFrame_=false;
    if(changed){tileEntries_.clear();if(tiles_)tiles_->invalidate();entries_.clear();work.retainedBytes=0;++targetEpoch_;}
    auto updates=groups_.update(plan.referenceDrawList.entries,plan.referenceDrawList.viewportClip,resourceEpoch);
    damage=DamageTracker{}.collect(previous,f.sceneReadToken,updates.contributions,
        changed?FullRedrawReason::kViewChanged:FullRedrawReason::kNone);
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
    const auto& r=group.revisions;
    auto found=coverage_.find(source.record.objectId);
    if(found!=coverage_.end()){
        const auto& old=found->second.revisions;
        if(old.content==r.content && old.resource==r.resource){
            found->second.used=tick_;++work.coverageHits;return found->second.picture;
        }
        work.coverageBytes-=found->second.bytes;coverage_.erase(found);
    }
    const auto& stroke=std::get<CompactStrokeReferenceCommand>(source.command).content.stroke;
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
    drawCompactCoverageToSkCanvas(*canvas,stroke.config,stroke.knots);
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
        crop->getCanvas()->clear(SK_ColorTRANSPARENT);crop->getCanvas()->drawImage(scratch_->makeImageSnapshot(),-rect.left(),-rect.top());
        auto image=crop->makeImageSnapshot();if(!image||!tiles_->complete(*request))return false;
        tileEntries_.emplace(key,TileEntry{request->generation,image,rect,std::move(lease)});(void)scheduler_.finish(job,request->generation);++work.tileRasters;
    }
    canvas.save();canvas.resetMatrix();for(const auto& key:keys){const auto& e=tileEntries_.at(key);canvas.drawImage(e.image,float(e.device.left()),float(e.device.top()));}canvas.restore();
    return true;
}
bool SkiaRenderCache::draw(SkCanvas& canvas,const FramePlan& plan,const ReferenceTraversalEntry& source,bool isolated){
    if(options.enabled && options.tileCandidate && !plan.referenceDrawList.entries.empty()){
        if(tiledFrame_)return true;
        if(source.record.objectId==plan.referenceDrawList.entries.front().record.objectId&&drawTiles(canvas,plan)){tiledFrame_=true;return true;}
        return false;
    }
    const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&source.command);
    // SrcOver isolated groups are independent of the destination. Unknown
    // effects and destination-dependent erase stay on the Direct path.
    if(!options.enabled||!compact||!isolated||!source.contributesPixels||!options.maxEntries)return false;
    const auto* group=groups_.find(source.record.objectId);if(!group)return false;
    const RenderCacheValidity validity{group->revisions,targetEpoch_,false};
    auto found=entries_.find(source.record.objectId);
    if(found!=entries_.end()&&RenderSourceResolver::resolve(found->second.validity,validity)==RenderSourceKind::kRasterCache){
        canvas.save();canvas.resetMatrix();canvas.drawImage(found->second.image,static_cast<float>(found->second.bounds.left()),static_cast<float>(found->second.bounds.top()));canvas.restore();
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
    for(const auto& k:compact->content.stroke.knots){const auto radius=ink::compactRadius(compact->content.stroke.config,k.pressure);
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
    const auto info=surface_->imageInfo();
    if(std::uint64_t(info.width())*info.height()*4>options.maxBytes)return false;
    if(!scratch_){
        auto reserved=budget?budget->reserve(resources::ResourceClass::kScratch,std::size_t(info.width())*info.height()*4,true):std::optional<std::uint64_t>{};
        if(budget&&!reserved)return false;
        resources::ResourceLease lease(budget,reserved.value_or(0));
        scratch_=canvas.makeSurface(info);if(!scratch_)return false;
        scratchLease_=std::move(lease);++work.scratchAllocations;
    }
    work.scratchPeakBytes=std::max(work.scratchPeakBytes,std::size_t(info.width())*info.height()*4);
    auto scratch=scratch_;
    scratch->getCanvas()->resetMatrix();
    auto* target=scratch->getCanvas();target->save();target->clear(SK_ColorTRANSPARENT);
    const auto sx=plan.frame.metrics.physicalWidth/plan.frame.metrics.logicalWidth;
    const auto sy=plan.frame.metrics.physicalHeight/plan.frame.metrics.logicalHeight;
    target->scale(sx,sy);
    const auto& clip=plan.referenceDrawList.viewportClip;
    target->clipRect(SkRect::MakeLTRB(clip.left,clip.top,clip.right,clip.bottom));
    target->setMatrix(matrix);
    drawValidatedIsolatedEntryToSkCanvas(*target,source,commands.get());
    target->restore();
    // Own a physically cropped representation. A subset image is allowed to
    // retain its source allocation, which would undercount residency.
    evict(static_cast<std::size_t>(bytes));
    auto reserved=budget?budget->reserve(resources::ResourceClass::kRaster,bytes,true):std::optional<std::uint64_t>{};
    if(budget&&!reserved){entries_.clear();work.retainedBytes=0;reserved=budget->reserve(resources::ResourceClass::kRaster,bytes,true);if(!reserved)return false;}
    resources::ResourceLease lease(budget,reserved.value_or(0));
    auto cropped=canvas.makeSurface(info.makeWH(crop.width(),crop.height()));
    if(!cropped)return false;
    cropped->getCanvas()->clear(SK_ColorTRANSPARENT);
    auto fullImage=scratch->makeImageSnapshot();if(!fullImage)return false;
    cropped->getCanvas()->drawImage(fullImage,static_cast<float>(-crop.left()),static_cast<float>(-crop.top()));
    auto image=cropped->makeImageSnapshot();if(!image)return false;
    entries_.emplace(source.record.objectId,Entry{validity,image,crop,static_cast<std::size_t>(bytes),tick_,std::move(lease)});
    work.retainedBytes+=static_cast<std::size_t>(bytes);work.entries=entries_.size();++work.rasterizedGroups;
    canvas.save();canvas.resetMatrix();canvas.drawImage(image,static_cast<float>(crop.left()),static_cast<float>(crop.top()));canvas.restore();
    return true;
}
}
