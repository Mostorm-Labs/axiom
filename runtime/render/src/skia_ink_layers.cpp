#include "transient_display_state.hpp"
#include "canvas/foundation/internal/g5_observation.hpp"
namespace g5 = canvas::foundation::internal::g5;
#include "skia_ink_layers.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "skia_geometry_internal.hpp"
#include "skia_erase_mask.hpp"
#include <algorithm>
#include <bit>
#include "include/core/SkColorSpace.h"

namespace canvas::render::internal {
void SkiaInkLayers::bindSurfaceBasis(const void*owner,std::uint64_t resource,std::uint64_t surface,std::uint64_t view,std::uint64_t metrics){
 const std::array<std::uint64_t,4> basis{resource,surface,view,metrics};
 if(surfaceOwner_!=owner||surfaceBasis_!=basis){clear();surfaceOwner_=owner;surfaceBasis_=basis;}
}
void SkiaInkLayers::clearDeviceLayers()noexcept{tiles_.clear();sources_.clear();geometry_.reset();histories_.reset();bytes_=0;}
void SkiaInkLayers::clear()noexcept{fullBodies_.clear();navigationBodies_.clear();navigationBudget_.reset();navigationWork_={};clearDeviceLayers();}
std::size_t SkiaInkLayers::retireIdleHistoryPreparation()noexcept{
    if(drawDepth_)return 0;
    std::size_t retired=0;
    for(auto& [id,source]:sources_)if(source.history){
        source.history.reset();++retired;
        source.epoch=source.epoch==UINT64_MAX?1:source.epoch+1;
        for(auto& [key,tile]:tiles_)if(key.id==id){tile.epoch=0;tile.applied=0;}
    }
    return retired;
}
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
        const auto count=std::min(std::size_t(1024),limits.maxEntries);
        const bool room=navigationWork_.bytes<=limits.maxBytes-bytes&&bytes_<=limits.maxBytes-bytes-navigationWork_.bytes&&
            (!newKey||tiles_.size()+navigationWork_.entries<count);
        auto token=room?reserveWithSourcePressure(budget,pressure_,resources::ResourceClass::kRaster,bytes,true):std::optional<std::uint64_t>{};
        if(token){lease=resources::ResourceLease(budget,*token);return true;}
        auto victim=tiles_.end();for(auto it=tiles_.begin();it!=tiles_.end();++it)
            if((it->first<key||key<it->first)&&(victim==tiles_.end()||it->second.used<victim->second.used))victim=it;
        if(victim==tiles_.end()){if(evictNavigationBody())continue;return false;}
        bytes_-=victim->second.bytes;tiles_.erase(victim);
    }
}
void SkiaInkLayers::drawMask(SkCanvas& c,const MaskPrimitive& primitive){
    std::visit([&](const auto& p){drawErasePrimitive(c,p);},primitive);
}
bool SkiaInkLayers::draw(SkCanvas& destination,SkSurface& scratch,const ReferenceTraversalEntry& entry,
    const SkMatrix& matrix,const SkIRect& viewport,const CompactDeviceGrid* originalGrid,
    const RenderCacheOptions& limits,const std::shared_ptr<resources::ResourceBudgetManager>& budget,
    const std::shared_ptr<SourceRetentionPool>& pool,CompactCoverageTargets& targets,RenderCacheWork& work,bool& painted,bool& resourcePending,std::shared_ptr<PreparedGeometryCache> prepared,std::shared_ptr<const PreparedNavigationGeometry> ready,std::shared_ptr<EraseMaskPathCache> histories,PreparationAllowance* allowance){
    struct DrawScope {unsigned& depth;explicit DrawScope(unsigned& value):depth(value){++depth;}~DrawScope(){--depth;}} drawScope{drawDepth_};
    if(prepared)geometry_=std::move(prepared);
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
    const std::array<int,4> gridBasis=originalGrid?std::array<int,4>{originalGrid->left,originalGrid->top,originalGrid->right,originalGrid->bottom}:
        std::array<int,4>{viewport.left(),viewport.top(),viewport.right(),viewport.bottom()};
    auto found=sources_.find(id);
    if(found!=sources_.end()&&(found->second.command!=entry.command||found->second.matrix!=matrix||found->second.viewport!=viewport||
       found->second.context!=destination.recordingContext()||found->second.grid!=gridBasis)){
        eraseSource(id);found=sources_.end();
    }
    if(found==sources_.end()){
        if(sources_.size()>=limits.maxEntries)return false;
        if(!geometry_)return false;
        geometry_->setRetentionPool(pool);auto prepared=ready?std::move(ready):geometry_->get(entry,limits,budget,true,PreparedConsumer::kRetainedInk);if(!prepared)return false;
        Source source;source.command=entry.command;source.geometry=std::move(prepared);source.matrix=matrix;source.viewport=viewport;
        source.context=destination.recordingContext();source.grid=gridBasis;
        found=sources_.emplace(id,std::move(source)).first;
    }
    auto& source=found->second;
    if(histories)histories_=std::move(histories);
    if(!histories_)histories_=std::make_shared<EraseMaskPathCache>();
    histories_->setRetentionPool(pool);const auto beforeHistory=histories_->work().history;
    MaskHistoryAdmission admission=MaskHistoryAdmission::kReady;
    const auto history=histories_->history(entry.record.eraseMasks,budget,
        std::min(limits.maxEntryBytes,limits.maxBytes/4),source.history,&admission);
    const auto afterHistory=histories_->work().history;
    work.historyFlattenedPrimitives+=afterHistory.flattenedPrimitives-beforeHistory.flattenedPrimitives;
    work.historyCopiedBytes+=afterHistory.copiedBytes-beforeHistory.copiedBytes;
    work.historyComparedPrimitives+=afterHistory.valuePrimitives-beforeHistory.valuePrimitives;
    G5_OBSERVE(g5::Kind::Counter,g5::Tag::mask_history,false,1,g5::word(id,0),g5::word(id,1),
        reinterpret_cast<std::uintptr_t>(entry.record.eraseMasks.storage().get()),history?history->identity:0,
        afterHistory.identityHits-beforeHistory.identityHits,afterHistory.valueRecords-beforeHistory.valueRecords,
        afterHistory.valuePrimitives-beforeHistory.valuePrimitives,afterHistory.flattenedPrimitives-beforeHistory.flattenedPrimitives,
        afterHistory.copiedBytes-beforeHistory.copiedBytes,afterHistory.primitiveBuilds-beforeHistory.primitiveBuilds,
        afterHistory.prefixReuses-beforeHistory.prefixReuses,afterHistory.denied-beforeHistory.denied);
    if(!entry.record.eraseMasks.empty()&&!history)
        return admission==MaskHistoryAdmission::kResourcePending?pending():false;
    // Only the replaceable live tail is unshared per-frame metadata. Historical
    // source/path leases are held once by the renderer-owned history plan.
    std::size_t count=0;for(const auto& m:entry.presentationEraseMasks)
        count+=std::holds_alternative<semantic::SweptCircleMask>(m.geometry)?std::get<semantic::SweptCircleMask>(m.geometry).segments.size():1;
    const auto metadataBytes=sizeof(Source)+256+count*sizeof(MaskPrimitive)+sourceMetadataBytes(entry);
    if(metadataBytes>limits.maxEntryBytes)return false;
    auto metadata=reserveWithSourcePressure(budget,pressure_,resources::ResourceClass::kRecorded,metadataBytes,true);if(!metadata)return pending();
    resources::ResourceLease metadataLease(budget,*metadata);
    std::vector<MaskPrimitive> masks;masks.reserve(count);
    for(const auto& m:entry.presentationEraseMasks){
        if(const auto* s=std::get_if<semantic::SweptCircleMask>(&m.geometry))for(const auto& segment:s->segments)masks.emplace_back(segment);
        else masks.emplace_back(std::get<semantic::FilledPathMask>(m.geometry));
    }
    const bool sameHistory=source.history==history;
    const bool historyPrefix=!source.history||(history&&history->prefixIdentity==source.history->identity&&
        history->prefixPrimitives>=source.history->primitives.size());
    // Canonical IDs are sorted, so a newly committed mask may be inserted
    // before an old record. Prove the complete old ordered subsequence, then
    // retain result pixels only on tiles untouched by every inserted shape.
    // Intersecting tiles replay the original full clear order from body pixels.
    std::vector<const PreparedMaskPrimitive*> inserted;
    bool insertionOnly=!sameHistory&&source.history&&history&&source.masks.empty()&&masks.empty();
    if(insertionOnly){
        const auto& old=std::as_const(source.history->records);const auto& next=std::as_const(history->records);
        std::size_t previous=0;
        for(std::size_t i=0;i<next.size();++i){
            if(previous<old.size()&&old[previous]==next[i]){++previous;continue;}
            const auto begin=i?history->primitiveEnds[i-1]:0;
            for(auto j=begin;j<history->primitiveEnds[i];++j)inserted.push_back(history->primitives[j].get());
        }
        insertionOnly=previous==old.size();
    }
    const auto previousEpoch=source.epoch;
    if((!sameHistory&&(!historyPrefix||!source.masks.empty()))||source.masks.size()>masks.size()||
        !std::equal(source.masks.begin(),source.masks.end(),masks.begin()))++source.epoch;
    source.history=history;source.masks=std::move(masks);source.metadata=std::move(metadataLease);
    const auto historical=history?history->primitives.size():0;
    const auto maskCount=historical+source.masks.size();
    auto demand=destination.getDeviceClipBounds();if(!demand.intersect(viewport))return true;
    const CompactDeviceGrid grid=originalGrid?*originalGrid:CompactDeviceGrid{viewport.left(),viewport.top(),viewport.right(),viewport.bottom()};
    const auto& stroke=compact->content.get().stroke;
    sk_sp<SkImage> sharedWhite;SkMatrix whiteBasis;
    if(allowance&&limits.gpuCoverageCandidate&&destination.recordingContext()){
        const auto prepared=prepareNavigationBody(entry,source.geometry,matrix,destination,limits,budget,targets,work,*allowance,sharedWhite,whiteBasis);
        if(prepared==BodyPreparation::kPending)return pending();
    }
    for(int y=grid.top;y<grid.bottom;y+=tileSize)for(int x=grid.left;x<grid.right;x+=tileSize){
        auto crop=SkIRect::MakeLTRB(x,y,std::min(x+tileSize,grid.right),std::min(y+tileSize,grid.bottom));
        if(!SkIRect::Intersects(crop,demand))continue;
        const Key key{id,x,y};auto tile=tiles_.find(key);const auto bytes=std::size_t(crop.width())*crop.height()*4;
        if(allowance&&allowance->exhausted())return pending();
        if(tile==tiles_.end()){
            constexpr auto metadataBytes=sizeof(Tile)+sizeof(Key)+sizeof(CompactStrokeRasterJob)+128;
            resources::ResourceLease lease;if(!reserve(bytes+metadataBytes,key,limits,budget,lease))return pending();
            auto target=destination.makeSurface(scratch.imageInfo().makeWH(crop.width(),crop.height()));if(!target)return pending();
            G5_TARGET(target,true);
            auto* canvas=target->getCanvas();canvas->clear(SK_ColorTRANSPARENT);
            canvas->translate(float(-x),float(-y));canvas->concat(matrix);
            // Keep the original coverage grid/material origin in local device
            // coordinates. A body target is one physical tile, never a subset
            // image retaining a full-frame scratch allocation.
            const CompactDeviceGrid localGrid{grid.left-x,grid.top-y,grid.right-x,grid.bottom-y};
            CompactRasterWork raster;
            if(sharedWhite){
                compositeCompactCoverageToSkCanvas(*canvas,*sharedWhite,stroke.config,stroke.seed,
                    int(std::round(matrix.getTranslateX()-whiteBasis.getTranslateX()))-x,
                    int(std::round(matrix.getTranslateY()-whiteBasis.getTranslateY()))-y);
            }else if(allowance){
                // Canonical preparation consumes the same ready body through
                // the existing cooperative raster leaf; its prepared-use and
                // 32-primitive progress remain visible to the owner contract.
                Tile candidate;candidate.building=std::move(target);candidate.body.lease=std::move(lease);
                candidate.raster=std::make_unique<CompactStrokeRasterJob>(*canvas,stroke.config,stroke.knots,&localGrid,
                    limits.gpuCoverageCandidate,budget,limits.maxEntryBytes,std::function<void(std::size_t)>{},true,source.geometry);
                candidate.bytes=bytes+metadataBytes;candidate.used=++tick_;
                tile=tiles_.emplace(key,std::move(candidate)).first;bytes_+=bytes+metadataBytes;
            }else drawCompactStrokeToSkCanvas(*canvas,stroke.config,stroke.knots,stroke.seed,&raster,nullptr,&localGrid,limits.gpuCoverageCandidate,&targets,nullptr,source.geometry.get(),true);
            if(!allowance||sharedWhite){
                if(raster.resourcePending)return pending();
                work.gpuCoverageTiles+=raster.gpuTiles;work.cpuCoverageTiles+=raster.cpuTiles;work.coverageTargetAllocations+=raster.allocatedTiles;work.coverageTargetReuses+=raster.targetReuses;
                Image body{target->makeImageSnapshot(),std::move(lease)};if(!body.pixels)return pending();
                Tile candidate;candidate.body=std::move(body);candidate.bytes=bytes+metadataBytes;candidate.used=++tick_;
                tile=tiles_.emplace(key,std::move(candidate)).first;bytes_+=bytes+metadataBytes;++work.bodyTileBuilds;
            }
        }else ++work.bodyTileHits;
        auto& t=tile->second;t.used=++tick_;
        if(t.raster){
            bool complete=false;
            do{
                if(allowance&&allowance->exhausted())return pending();
                CompactRasterWork raster;complete=t.raster->step(*t.building->getCanvas(),stroke.config,stroke.knots,
                    stroke.seed,targets,limits.gpuCoverageCandidate,&raster);
                work.gpuCoverageTiles+=raster.gpuTiles;work.cpuCoverageTiles+=raster.cpuTiles;
                work.coverageTargetAllocations+=raster.allocatedTiles;work.coverageTargetReuses+=raster.targetReuses;
                ++work.preparationCoverageSteps;if(allowance)++allowance->steps;
                if(t.raster->resourcePending())return pending();
            }while(!complete);
            t.body.pixels=t.building->makeImageSnapshot();if(!t.body.pixels)return pending();
            t.raster.reset();t.building.reset();++work.bodyTileBuilds;
        }
        if(insertionOnly&&t.result.pixels&&t.epoch==previousEpoch){
            const bool unaffected=std::none_of(inserted.begin(),inserted.end(),[&](const auto* primitive){
                auto bounds=matrix.mapRect(primitive->bounds);bounds.outset(3,3);
                return !bounds.isFinite()||SkRect::Intersects(bounds,SkRect::Make(crop));
            });
            if(unaffected){t.epoch=source.epoch;t.applied=maskCount;}
        }
        if(!t.result.pixels||t.epoch!=source.epoch||t.applied!=maskCount){
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
            for(auto i=first;i<maskCount;++i){
                if(i<historical){
                    const auto& primitive=*history->primitives[i];
                    if(primitive.swept){auto bounds=matrix.mapRect(primitive.bounds);bounds.outset(3,3);
                        if(bounds.isFinite()&&!SkRect::Intersects(bounds,SkRect::Make(crop)))continue;}
                    primitive.draw(*canvas);++work.maskPrimitiveDraws;continue;
                }
                const auto& active=source.masks[i-historical];
                // Keep the same conservative hull and sequential clear for
                // the replaceable active tail, without touching history.
                if(const auto* s=std::get_if<semantic::EraseCubicSegment>(&active)){
                    auto bounds=SkRect::MakeLTRB(float(std::min({s->p0.position.x,s->p1.position.x,s->control1.x,s->control2.x})),float(std::min({s->p0.position.y,s->p1.position.y,s->control1.y,s->control2.y})),float(std::max({s->p0.position.x,s->p1.position.x,s->control1.x,s->control2.x})),float(std::max({s->p0.position.y,s->p1.position.y,s->control1.y,s->control2.y})));
                    bounds.outset(float(std::max(s->p0.radius,s->p1.radius)),float(std::max(s->p0.radius,s->p1.radius)));
                    bounds=matrix.mapRect(bounds);bounds.outset(3,3);
                    if(bounds.isFinite()&&!SkRect::Intersects(bounds,SkRect::Make(crop)))continue;
                }
                drawMask(*canvas,active);++work.maskPrimitiveDraws;
            }
            Image result{target->makeImageSnapshot(),std::move(lease)};if(!result.pixels)return pending();
            if(!t.result.pixels){t.bytes+=resultBytes;bytes_+=resultBytes;}
            t.result=std::move(result);t.epoch=source.epoch;t.applied=maskCount;++work.maskTileUpdates;if(!prefix)++work.maskTileResets;
            if(allowance)++allowance->steps;
        }
        (void)t.result.lease.markGpuUse();SkPaint opacity;opacity.setAlphaf(std::clamp(entry.presentationOpacity,0.F,1.F));
        painted=true;destination.save();destination.resetMatrix();destination.clipIRect(crop);destination.drawImage(t.result.pixels,float(x-maskGuard),float(y-maskGuard),SkSamplingOptions{},&opacity);destination.restore();
        if(transientTileRefused())return pending();
    }
    work.inkLayerBytes=bytes_;return true;
}
}

namespace canvas::render::internal {
SkiaInkLayers::NavigationBodyKey SkiaInkLayers::navigationKey(const PreparedNavigationGeometry&body,const SkImageInfo&info,const void*context,const SkMatrix&matrix,SkIRect clip){
 NavigationBodyKey key;key.body=body.identity();key.context=context;SkScalar values[9];matrix.get9(values);
 for(unsigned i=0;i<9;++i)key.matrix[i]=std::bit_cast<std::uint32_t>(values[i]==0?0.F:values[i]);
 key.crop={info.width(),info.height(),clip.left(),clip.top(),clip.right(),clip.bottom()};key.colorType=int(info.colorType());key.alphaType=int(info.alphaType());key.colorSpace=info.colorSpace()?info.colorSpace()->hash():0;return key;
}
SkiaInkLayers::NavigationBodyKey SkiaInkLayers::navigationKey(const PreparedNavigationGeometry&body,SkCanvas&canvas){
 return navigationKey(body,canvas.imageInfo(),canvas.recordingContext(),canvas.getTotalMatrix(),canvas.getDeviceClipBounds());
}
void SkiaInkLayers::bindNavigationBudget(const std::shared_ptr<resources::ResourceBudgetManager>&budget){
 if(navigationBudget_.lock()==budget)return;navigationBodies_.clear();fullBodies_.clear();navigationWork_={};navigationBudget_=budget;
}
bool SkiaInkLayers::evictNavigationBody(){
 auto a=navigationBodies_.end();for(auto it=navigationBodies_.begin();it!=navigationBodies_.end();++it)if(a==navigationBodies_.end()||it->second.used<a->second.used)a=it;
 auto b=fullBodies_.end();for(auto it=fullBodies_.begin();it!=fullBodies_.end();++it)if(it->second.pixels&&(b==fullBodies_.end()||it->second.used<b->second.used))b=it;
 if(a==navigationBodies_.end()&&b==fullBodies_.end())return false;
 if(b==fullBodies_.end()||(a!=navigationBodies_.end()&&a->second.used<b->second.used)){navigationWork_.bytes-=a->second.bytes;navigationBodies_.erase(a);}
 else {navigationWork_.bytes-=b->second.bytes;fullBodies_.erase(b);}
 ++navigationWork_.evictions;navigationWork_.entries=navigationBodies_.size()+fullBodies_.size();return true;
}
sk_sp<SkImage> SkiaInkLayers::navigationBody(const std::shared_ptr<const PreparedNavigationGeometry>&body,SkCanvas&canvas,
 const RenderCacheOptions&,const std::shared_ptr<resources::ResourceBudgetManager>&budget){
 if(!body||!body->ready()||!body->isContour()||!budget)return {};
 bindNavigationBudget(budget);
 const auto key=navigationKey(*body,canvas);auto it=navigationBodies_.find(key);
 if(it==navigationBodies_.end()){++navigationWork_.misses;return {};}
 ++navigationWork_.hits;it->second.used=++tick_;(void)it->second.lease.markGpuUse();return it->second.pixels;
}
void SkiaInkLayers::rememberNavigationBody(const std::shared_ptr<const PreparedNavigationGeometry>&body,SkCanvas&canvas,sk_sp<SkImage>pixels,
 const RenderCacheOptions&options,const std::shared_ptr<resources::ResourceBudgetManager>&budget){
 if(!body||!pixels||!budget)return;const auto key=navigationKey(*body,canvas);
 bindNavigationBudget(budget);
 if(navigationBodies_.contains(key))return;const auto count=std::min(std::size_t(1024),options.maxEntries);if(!count)return;
 const auto bytes=std::size_t(pixels->width())*pixels->height()*4+sizeof(NavigationBodyKey)+sizeof(NavigationBody)+256;
 if(bytes>options.maxEntryBytes||bytes>options.maxBytes)return;
 std::optional<std::uint64_t> token;
 while(!token){
  if(navigationBodies_.size()+fullBodies_.size()+tiles_.size()<count&&navigationWork_.bytes+bytes_<=options.maxBytes-bytes)token=budget->reserve(resources::ResourceClass::kRaster,bytes,true);
  if(token)break;if(!evictNavigationBody())return;
 }
 resources::ResourceLease lease(budget,*token);(void)lease.markGpuUse();navigationBodies_.emplace(key,NavigationBody{body,std::move(pixels),std::move(lease),++tick_,bytes});navigationWork_.bytes+=bytes;++navigationWork_.builds;navigationWork_.entries=navigationBodies_.size()+fullBodies_.size();
}
}

namespace canvas::render::internal {
SkiaInkLayers::BodyPreparation SkiaInkLayers::prepareNavigationBody(const ReferenceTraversalEntry&entry,
 const std::shared_ptr<const PreparedNavigationGeometry>&body,const SkMatrix&matrix,SkCanvas&canvas,
 const RenderCacheOptions&options,const std::shared_ptr<resources::ResourceBudgetManager>&budget,
 CompactCoverageTargets&targets,RenderCacheWork&work,PreparationAllowance&allowance,sk_sp<SkImage>&out,SkMatrix&basis){
 const auto*compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command);
 if(!compact||compact->content.get().stroke.config.algorithmVersion!=4||!body||!body->ready()||!body->isContour()||!budget||!canvas.recordingContext()||matrix.hasPerspective())return BodyPreparation::kUnavailable;
 auto bounds=matrix.mapRect(body->support());bounds.outset(4,4);
 if(!bounds.isFinite()||bounds.width()>16384||bounds.height()>16384||std::max({std::abs(bounds.left()),std::abs(bounds.top()),std::abs(bounds.right()),std::abs(bounds.bottom())})>1048576)return BodyPreparation::kUnavailable;
 SkIRect crop;bounds.roundOut(&crop);if(crop.isEmpty())return BodyPreparation::kUnavailable;
 const auto bytes=std::size_t(crop.width())*crop.height()*4;
 const auto metadata=sizeof(FullBody)+sizeof(NavigationBodyKey)+sizeof(CompactStrokeRasterJob)+512;
 if(bytes*2+metadata>options.maxEntryBytes||bytes*2+metadata>options.maxBytes)return BodyPreparation::kUnavailable;
 bindNavigationBudget(budget);
 SkMatrix normalized;normalized.setConcat(SkMatrix::Translate(float(-crop.left()),float(-crop.top())),matrix);
 // V4 white coverage is a scalar encoded in N32 channels, independent of
 // the final target's color space. Match canonical CompactCoverageTargets
 // and navigation scratch instead of inheriting Web provider sRGB metadata.
 const auto info=SkImageInfo::MakeN32Premul(crop.width(),crop.height());
 const auto key=navigationKey(*body,info,canvas.recordingContext(),normalized,SkIRect::MakeWH(crop.width(),crop.height()));
 auto found=fullBodies_.find(key);
 if(found==fullBodies_.end()){
  const auto count=std::min(std::size_t(1024),options.maxEntries);if(!count)return BodyPreparation::kUnavailable;
  const auto charge=bytes*2+metadata;std::optional<std::uint64_t>token;
  while(!token){
   if(fullBodies_.size()+navigationBodies_.size()+tiles_.size()<count&&navigationWork_.bytes+bytes_<=options.maxBytes-charge)token=budget->reserve(resources::ResourceClass::kRaster,bytes+metadata,true);
   if(token)break;if(!evictNavigationBody())return BodyPreparation::kPending;
  }
  resources::ResourceLease resident(budget,*token);
  const auto temporary=budget->reserve(resources::ResourceClass::kScratch,bytes,true,0);if(!temporary)return BodyPreparation::kPending;
  FullBody item;item.geometry=body;item.matrix=normalized;item.lease=std::move(resident);item.staging=resources::ResourceLease(budget,*temporary);item.bytes=charge;item.used=++tick_;
  item.building=canvas.makeSurface(info);if(!item.building)return BodyPreparation::kPending;
  auto*draw=item.building->getCanvas();draw->clear(SK_ColorBLACK);draw->setMatrix(normalized);
  const auto&stroke=compact->content.get().stroke;
  item.raster=std::make_unique<CompactStrokeRasterJob>(*draw,stroke.config,stroke.knots,nullptr,options.gpuCoverageCandidate,budget,options.maxEntryBytes,std::function<void(std::size_t)>{},true,body,false,true);
  found=fullBodies_.emplace(key,std::move(item)).first;navigationWork_.bytes+=charge;navigationWork_.entries=fullBodies_.size()+navigationBodies_.size();
 }
 auto&item=found->second;item.used=++tick_;(void)item.lease.markGpuUse();
 while(item.raster){
  if(allowance.exhausted())return BodyPreparation::kPending;
  CompactRasterWork raster;const auto&stroke=compact->content.get().stroke;
  const bool done=item.raster->step(*item.building->getCanvas(),stroke.config,stroke.knots,stroke.seed,targets,options.gpuCoverageCandidate,&raster);
  ++allowance.steps;++work.preparationCoverageSteps;work.gpuCoverageTiles+=raster.gpuTiles;work.cpuCoverageTiles+=raster.cpuTiles;
  if(item.raster->resourcePending())return BodyPreparation::kPending;
  if(!done)continue;
  item.pixels=item.building->makeImageSnapshot();if(!item.pixels)return BodyPreparation::kPending;
  item.raster.reset();item.building.reset();
  // Temporary snapshot/COW allowance is released into its real active GPU
  // epoch, not counted as resident pixels and not freed before completion.
  item.staging.reset();item.bytes-=bytes;navigationWork_.bytes-=bytes;++navigationWork_.builds;
 }
 out=item.pixels;basis=item.matrix;++navigationWork_.hits;return BodyPreparation::kReady;
}
bool SkiaInkLayers::projectNavigationBody(const std::shared_ptr<const PreparedNavigationGeometry>&body,SkCanvas&canvas,
 const RenderCacheOptions&,const std::shared_ptr<resources::ResourceBudgetManager>&budget){
 if(!body||!budget)return false;bindNavigationBudget(budget);
 const auto info=canvas.imageInfo();const auto cs=info.colorSpace()?info.colorSpace()->hash():0;
 bool hasBody=false;
 for(auto&[key,item]:fullBodies_)if(item.geometry==body&&item.pixels){
  hasBody=true;
  if(key.context!=canvas.recordingContext()){++navigationWork_.projectionContextMiss;continue;}
  if(key.colorType!=int(info.colorType())||key.alphaType!=int(info.alphaType())||key.colorSpace!=cs){++navigationWork_.projectionFormatMiss;continue;}
  SkMatrix inverse;if(!item.matrix.invert(&inverse))continue;
  SkMatrix imageToDestination;imageToDestination.setConcat(canvas.getTotalMatrix(),inverse);
  // Projection may downsample a complete known-density source. Upsampling
  // cannot satisfy a sharper target; retain the existing exact raster path.
  if(imageToDestination.hasPerspective()||imageToDestination.getMaxScale()>1.000001F){++navigationWork_.projectionDensityMiss;continue;}
  (void)item.lease.markGpuUse();item.used=++tick_;canvas.save();canvas.resetMatrix();canvas.concat(imageToDestination);
  canvas.drawImage(item.pixels,0,0,SkSamplingOptions(SkFilterMode::kLinear));canvas.restore();++navigationWork_.hits;return true;
 }
 if(!hasBody)++navigationWork_.projectionBodyMiss;++navigationWork_.misses;return false;
}
}
