#include "canvas/foundation/internal/g5_observation.hpp"
namespace g5 = canvas::foundation::internal::g5;
#include "skia_ink_layers.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "skia_geometry_internal.hpp"
#include "skia_erase_mask.hpp"
#include <algorithm>

namespace canvas::render::internal {
void SkiaInkLayers::clear()noexcept{tiles_.clear();sources_.clear();geometry_.clear();bytes_=0;}
void SkiaInkLayers::eraseSource(foundation::ObjectId id){
    for(auto it=tiles_.begin();it!=tiles_.end();)if(it->first.id==id){bytes_-=it->second.bytes;it=tiles_.erase(it);}else ++it;
    sources_.erase(id);
}
void SkiaInkLayers::prune(std::span<const ReferenceTraversalEntry> entries){
    for(auto it=sources_.begin();it!=sources_.end();){const auto id=it->first;++it;
        if(std::none_of(entries.begin(),entries.end(),[&](const auto& e){return e.record.objectId==id;}))eraseSource(id);}
}
bool SkiaInkLayers::reserve(std::size_t bytes,const Key& key,const RenderCacheOptions& limits,
    const std::shared_ptr<resources::ResourceBudgetManager>& budget,resources::ResourceLease& lease){
    if(bytes>limits.maxBytes||bytes>limits.maxEntryBytes)return false;
    for(;;){
        const bool newKey=!tiles_.contains(key);
        const bool room=bytes_<=limits.maxBytes-bytes&&(!newKey||tiles_.size()<limits.maxEntries);
        auto token=room?budget->reserve(resources::ResourceClass::kRaster,bytes,true):std::optional<std::uint64_t>{};
        if(token){lease=resources::ResourceLease(budget,*token);return true;}
        auto victim=tiles_.end();for(auto it=tiles_.begin();it!=tiles_.end();++it)
            if((it->first<key||key<it->first)&&(victim==tiles_.end()||it->second.used<victim->second.used))victim=it;
        if(victim==tiles_.end())return false;
        bytes_-=victim->second.bytes;tiles_.erase(victim);
    }
}
void SkiaInkLayers::drawMask(SkCanvas& c,const MaskPrimitive& primitive){
    std::visit([&](const auto& p){drawErasePrimitive(c,p);},primitive);
}
bool SkiaInkLayers::draw(SkCanvas& destination,SkSurface& scratch,const ReferenceTraversalEntry& entry,
    const SkMatrix& matrix,const SkIRect& viewport,const CompactDeviceGrid* originalGrid,
    const RenderCacheOptions& limits,const std::shared_ptr<resources::ResourceBudgetManager>& budget,
    const std::shared_ptr<SourceRetentionPool>& pool,CompactCoverageTargets& targets,RenderCacheWork& work,bool& painted,bool& resourcePending){
    G5_ROUTE(g5LayersRoute,g5::Route::erase_layers);
    G5_SCOPE(g5Layers,g5::Tag::mask,g5::Purpose::none);

    painted=false;resourcePending=false;
    const auto pending=[&]{resourcePending=true;return false;};
    const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command);
    if(!compact||compact->content.get().stroke.config.algorithmVersion!=4||!budget||!limits.maxEntries||
       limits.maxEntryBytes<std::size_t(tileSize)*std::min(tileSize,scratch.height())*4U)return false;
    const auto maxBody=std::size_t(tileSize)*std::min(tileSize,scratch.height())*4+sizeof(Tile)+sizeof(Key)+128;
    const auto maxResult=std::size_t(tileSize+2*maskGuard)*(std::min(tileSize,scratch.height())+2*maskGuard)*4;
    if(limits.maxEntryBytes<std::max(maxBody,maxResult)||limits.maxBytes<maxBody+maxResult)return false;
    const auto id=entry.record.objectId;
    auto found=sources_.find(id);
    if(found!=sources_.end()&&(found->second.command!=entry.command||found->second.matrix!=matrix||found->second.viewport!=viewport)){
        eraseSource(id);found=sources_.end();
    }
    if(found==sources_.end()){
        if(sources_.size()>=limits.maxEntries)return false;
        geometry_.setRetentionPool(pool);auto prepared=geometry_.get(entry,limits,budget,true);if(!prepared)return false;
        Source source;source.command=entry.command;source.geometry=std::move(prepared);source.matrix=matrix;source.viewport=viewport;
        found=sources_.emplace(id,std::move(source)).first;
    }
    auto& source=found->second;
    std::size_t count=0;for(const auto list:{std::span(entry.record.eraseMasks),std::span(entry.presentationEraseMasks)})
        for(const auto& m:list)count+=std::holds_alternative<semantic::SweptCircleMask>(m.geometry)?std::get<semantic::SweptCircleMask>(m.geometry).segments.size():1;
    // Admit the replacement metadata before copying any history; old source
    // metadata remains charged until the exact prefix proof commits.
    std::size_t metadataBytes=sizeof(Source)+256+count*sizeof(MaskPrimitive)+sourceMetadataBytes(entry);
    if(metadataBytes>limits.maxEntryBytes)return false;
    auto metadata=budget->reserve(resources::ResourceClass::kRecorded,metadataBytes,true);if(!metadata)return pending();
    resources::ResourceLease metadataLease(budget,*metadata);
    std::vector<MaskPrimitive> masks;masks.reserve(count);
    for(const auto list:{std::span(entry.record.eraseMasks),std::span(entry.presentationEraseMasks)})for(const auto& m:list){
        if(const auto* s=std::get_if<semantic::SweptCircleMask>(&m.geometry))for(const auto& segment:s->segments)masks.emplace_back(segment);
        else masks.emplace_back(std::get<semantic::FilledPathMask>(m.geometry));
    }
    if(source.masks.size()>masks.size()||!std::equal(source.masks.begin(),source.masks.end(),masks.begin()))++source.epoch;
    G5_OBSERVE(g5::Kind::Counter,g5::Tag::mask,false,1,source.masks.size(),masks.size(),source.epoch);
    source.masks=std::move(masks);source.metadata=std::move(metadataLease);
    auto demand=destination.getDeviceClipBounds();if(!demand.intersect(viewport))return true;
    const CompactDeviceGrid grid=originalGrid?*originalGrid:CompactDeviceGrid{viewport.left(),viewport.top(),viewport.right(),viewport.bottom()};
    const auto& stroke=compact->content.get().stroke;
    for(int y=grid.top;y<grid.bottom;y+=tileSize)for(int x=grid.left;x<grid.right;x+=tileSize){
        auto crop=SkIRect::MakeLTRB(x,y,std::min(x+tileSize,grid.right),std::min(y+tileSize,grid.bottom));
        if(!SkIRect::Intersects(crop,demand))continue;
        const Key key{id,x,y};auto tile=tiles_.find(key);const auto bytes=std::size_t(crop.width())*crop.height()*4;
        if(tile==tiles_.end()){
            constexpr auto metadataBytes=sizeof(Tile)+sizeof(Key)+128;
            resources::ResourceLease lease;if(!reserve(bytes+metadataBytes,key,limits,budget,lease))return pending();
            auto target=destination.makeSurface(scratch.imageInfo().makeWH(crop.width(),crop.height()));if(!target)return pending();
            G5_TARGET(target,true);
            auto* canvas=target->getCanvas();canvas->clear(SK_ColorTRANSPARENT);
            canvas->translate(float(-x),float(-y));canvas->concat(matrix);
            // Keep the original coverage grid/material origin in local device
            // coordinates. A body target is one physical tile, never a subset
            // image retaining a full-frame scratch allocation.
            const CompactDeviceGrid localGrid{grid.left-x,grid.top-y,grid.right-x,grid.bottom-y};
            CompactRasterWork raster;drawCompactStrokeToSkCanvas(*canvas,stroke.config,stroke.knots,stroke.seed,&raster,nullptr,&localGrid,limits.gpuCoverageCandidate,&targets,nullptr,source.geometry.get(),true);
            if(raster.resourcePending)return pending();
            work.gpuCoverageTiles+=raster.gpuTiles;work.cpuCoverageTiles+=raster.cpuTiles;work.coverageTargetAllocations+=raster.allocatedTiles;work.coverageTargetReuses+=raster.targetReuses;
            Image body{target->makeImageSnapshot(),std::move(lease)};if(!body.pixels)return pending();
            Tile candidate;candidate.body=std::move(body);candidate.bytes=bytes+metadataBytes;candidate.used=++tick_;
            tile=tiles_.emplace(key,std::move(candidate)).first;bytes_+=bytes+metadataBytes;++work.bodyTileBuilds;
        }else ++work.bodyTileHits;
        auto& t=tile->second;t.used=++tick_;
        if(!t.result.pixels||t.epoch!=source.epoch||t.applied!=source.masks.size()){
            const bool prefix=t.result.pixels&&t.epoch==source.epoch;
            const auto first=prefix?t.applied:0;auto base=prefix?t.result.pixels:t.body.pixels;
            // Clear-stroke AA gets a guard around the interior tile. Only the
            // interior is composed, so adjacent tiles never overlap or blend
            // twice. Full device coordinates still bind geometry and material.
            constexpr int guard=maskGuard;
            const auto resultBytes=std::size_t(crop.width()+2*guard)*(crop.height()+2*guard)*4;
            resources::ResourceLease lease;if(!reserve(resultBytes,key,limits,budget,lease))return pending();
            auto target=destination.makeSurface(scratch.imageInfo().makeWH(crop.width()+2*guard,crop.height()+2*guard));if(!target)return pending();
            G5_TARGET(target,true);
            auto* canvas=target->getCanvas();canvas->clear(SK_ColorTRANSPARENT);
            SkPaint copy;copy.setBlendMode(SkBlendMode::kSrc);canvas->drawImage(base,prefix?0:guard,prefix?0:guard,SkSamplingOptions{},&copy);
            canvas->translate(float(guard-x),float(guard-y));canvas->clipIRect(viewport);canvas->concat(matrix);
            for(auto i=first;i<source.masks.size();++i){
                // Convex-hull plus maximum radius is conservative for the
                // shared cubic clear evaluator, including caps/AA/affines.
                if(const auto* s=std::get_if<semantic::EraseCubicSegment>(&source.masks[i])){
                    auto bounds=SkRect::MakeLTRB(float(std::min({s->p0.position.x,s->p1.position.x,s->control1.x,s->control2.x})),float(std::min({s->p0.position.y,s->p1.position.y,s->control1.y,s->control2.y})),float(std::max({s->p0.position.x,s->p1.position.x,s->control1.x,s->control2.x})),float(std::max({s->p0.position.y,s->p1.position.y,s->control1.y,s->control2.y})));
                    bounds.outset(float(std::max(s->p0.radius,s->p1.radius)),float(std::max(s->p0.radius,s->p1.radius)));
                    bounds=matrix.mapRect(bounds);bounds.outset(3,3);
                    if(bounds.isFinite()&&!SkRect::Intersects(bounds,SkRect::Make(crop)))continue;
                }
                drawMask(*canvas,source.masks[i]);++work.maskPrimitiveDraws;
            }
            Image result{target->makeImageSnapshot(),std::move(lease)};if(!result.pixels)return pending();
            if(!t.result.pixels){t.bytes+=resultBytes;bytes_+=resultBytes;}
            t.result=std::move(result);t.epoch=source.epoch;t.applied=source.masks.size();++work.maskTileUpdates;if(!prefix)++work.maskTileResets;
        }
        (void)t.result.lease.markGpuUse();SkPaint opacity;opacity.setAlphaf(std::clamp(entry.presentationOpacity,0.F,1.F));
        painted=true;destination.save();destination.resetMatrix();destination.clipIRect(crop);destination.drawImage(t.result.pixels,float(x-maskGuard),float(y-maskGuard),SkSamplingOptions{},&opacity);destination.restore();
    }
    work.inkLayerBytes=bytes_;return true;
}
}
