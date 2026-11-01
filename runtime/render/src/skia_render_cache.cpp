#include "skia_render_cache_internal.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkMatrix.h"
#include <algorithm>
#include <cmath>

namespace canvas::render::internal {
void SkiaRenderCache::clear() noexcept {entries_.clear();groups_.clear();frame_.reset();work.retainedBytes=0;work.entries=0;}
void SkiaRenderCache::begin(const FramePlan& plan,const void* target,SkSurface& surface,std::uint64_t resourceEpoch){
    const auto& f=plan.frame;
    const bool changed=!frame_||target_!=target||surface_!=&surface||frame_->viewId!=f.viewId||
        frame_->surfaceGeneration!=f.surfaceGeneration||frame_->metrics!=f.metrics||
        frame_->metricsGeneration!=f.metricsGeneration||transform_!=plan.referenceDrawList.worldToView;
    const auto previous=frame_?frame_->sceneReadToken:foundation::SceneRevision{};
    if(changed){entries_.clear();work.retainedBytes=0;++targetEpoch_;}
    auto updates=groups_.update(plan.referenceDrawList.entries,plan.referenceDrawList.viewportClip,resourceEpoch);
    damage=DamageTracker{}.collect(previous,f.sceneReadToken,updates.contributions,
        changed?FullRedrawReason::kViewChanged:FullRedrawReason::kNone);
    // Removed/cull-out groups never retain an unbounded raster representation.
    for(auto it=entries_.begin();it!=entries_.end();)if(!groups_.find(it->first)){
        work.retainedBytes-=it->second.bytes;it=entries_.erase(it);
    }else ++it;
    target_=target;surface_=&surface;frame_=std::make_unique<FrameState>(f);
    transform_=plan.referenceDrawList.worldToView;++tick_;work.entries=entries_.size();
}
void SkiaRenderCache::evict(std::size_t bytes){
    while(!entries_.empty()&&(work.retainedBytes>options.maxBytes-bytes||entries_.size()>=options.maxEntries)){
        auto victim=std::min_element(entries_.begin(),entries_.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});
        work.retainedBytes-=victim->second.bytes;entries_.erase(victim);++work.evictions;
    }
}
bool SkiaRenderCache::draw(SkCanvas& canvas,const FramePlan& plan,const ReferenceTraversalEntry& source,bool isolated){
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
    auto scratch=canvas.makeSurface(info);if(!scratch)return false;
    auto* target=scratch->getCanvas();target->clear(SK_ColorTRANSPARENT);
    const auto sx=plan.frame.metrics.physicalWidth/plan.frame.metrics.logicalWidth;
    const auto sy=plan.frame.metrics.physicalHeight/plan.frame.metrics.logicalHeight;
    target->scale(sx,sy);
    const auto& clip=plan.referenceDrawList.viewportClip;
    target->clipRect(SkRect::MakeLTRB(clip.left,clip.top,clip.right,clip.bottom));
    target->setMatrix(matrix);
    drawValidatedIsolatedEntryToSkCanvas(*target,source);
    // Own a physically cropped representation. A subset image is allowed to
    // retain its source allocation, which would undercount residency.
    auto cropped=canvas.makeSurface(info.makeWH(crop.width(),crop.height()));
    if(!cropped)return false;
    cropped->getCanvas()->clear(SK_ColorTRANSPARENT);
    auto fullImage=scratch->makeImageSnapshot();if(!fullImage)return false;
    cropped->getCanvas()->drawImage(fullImage,static_cast<float>(-crop.left()),static_cast<float>(-crop.top()));
    auto image=cropped->makeImageSnapshot();if(!image)return false;
    evict(static_cast<std::size_t>(bytes));
    entries_.emplace(source.record.objectId,Entry{validity,image,crop,static_cast<std::size_t>(bytes),tick_});
    work.retainedBytes+=static_cast<std::size_t>(bytes);work.entries=entries_.size();++work.rasterizedGroups;
    canvas.save();canvas.resetMatrix();canvas.drawImage(image,static_cast<float>(crop.left()),static_cast<float>(crop.top()));canvas.restore();
    return true;
}
}
