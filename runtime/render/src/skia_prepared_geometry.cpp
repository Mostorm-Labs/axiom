#include "skia_prepared_geometry.hpp"
#include "compact_contour_skia_path.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "canvas/render/brush_outline_skia_path.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace canvas::render::internal {
namespace {
SkRect capBounds(const ink::ContourCap& cap){
    // The draw uses a float center/radius. Bound that exact representation,
    // rounding outward: double bounds cast to float may collapse a tiny cap
    // at a large world origin even though a zoomed Skia circle is visible.
    const auto x=float(cap.x),y=float(cap.y),r=float(cap.radius);
    const auto inf=std::numeric_limits<float>::infinity();
    return SkRect::MakeLTRB(std::nextafter(x-r,-inf),std::nextafter(y-r,-inf),
                           std::nextafter(x+r,inf),std::nextafter(y+r,inf));
}
std::optional<SkRect> contourClip(SkCanvas& canvas,const SkRect* deviceDemand=nullptr){
    auto clip=canvas.getLocalClipBounds();SkMatrix inverse;
    bool cull=!canvas.getTotalMatrix().hasPerspective()&&canvas.getTotalMatrix().invert(&inverse)&&clip.isFinite();
    // Selection demand must not change Skia's raster clip: analytic AA of a
    // clipped curve can differ from full-cell rasterization even away from the
    // clip edge. Cull complete paths, then raster them on the original grid.
    if(cull&&deviceDemand){
        const auto demand=inverse.mapRect(*deviceDemand);
        if(!demand.isFinite())cull=false;
        else if(!clip.intersect(demand))clip=SkRect::MakeEmpty();
    }
    if(cull){
        const auto& matrix=canvas.getTotalMatrix();
        const auto scale=std::max(std::hypot(matrix.getScaleX(),matrix.getSkewY()),
                                  std::hypot(matrix.getSkewX(),matrix.getScaleY()));
        const auto origin=std::max({std::abs(clip.left()),std::abs(clip.top()),
                                    std::abs(clip.right()),std::abs(clip.bottom())});
        // Use the existing coverage numeric domain for culling hints. Extreme
        // origins/scales still reuse the paths, with complete Direct drawing.
        if(origin>65536||scale<1./64||scale>64||!std::isfinite(scale))cull=false;
        const auto margin=2*std::max(std::hypot(inverse.getScaleX(),inverse.getSkewY()),
                                    std::hypot(inverse.getSkewX(),inverse.getScaleY()));
        cull=cull&&std::isfinite(margin);if(cull)clip.outset(float(margin),float(margin));
    }
    return cull?std::optional<SkRect>(clip):std::nullopt;
}
bool visibleContour(const std::optional<SkRect>& clip,const SkRect& b){
    return !clip||b.isEmpty()||!b.isFinite()||SkRect::Intersects(*clip,b);
}
void drawCap(SkCanvas& canvas,const foundation::InkBrushConfig& config,const ink::ContourCap& cap,const SkPaint& paint){
    canvas.save();canvas.translate(float(cap.x),float(cap.y));
    if(config.family==foundation::InkFamily::kMarker){canvas.rotate(float(config.markerAngle*57.29577951308232));canvas.scale(1,float(config.markerFlatness));}
    canvas.drawCircle(0,0,float(cap.radius),paint);canvas.restore();
}

}
PreparedNavigationGeometry::~PreparedNavigationGeometry(){if(usage_)usage_->bytes-=bytes_;}
void PreparedNavigationGeometry::build(ink::CompactContour* staged){
    if(const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&std::as_const(source_))){
        const auto& s=compact->content.get().stroke;
        distances_.resize(s.knots.size()+1,0);
        for(std::size_t i=2;i<distances_.size();++i)
            distances_[i]=distances_[i-1]+std::hypot(s.knots[i-1].x-s.knots[i-2].x,s.knots[i-1].y-s.knots[i-2].y);
        if(s.config.algorithmVersion!=4)return;
        contour_=true;
        auto contour=staged?std::move(*staged):ink::compileCompactContour(s.config,s.knots);
        ribbons_.reserve(contour.patches.empty()?0:contour.patches.size()-1);
        for(std::size_t i=0;i+1<contour.patches.size();++i)
            ribbons_.push_back(compactContourRibbon(std::span(contour.patches).subspan(i,2)));
        caps_=std::move(contour.caps);
        const auto groups=[](std::size_t count,auto bounds){
            std::vector<Group> result;result.reserve((count+31)/32);
            for(std::size_t first=0;first<count;first+=32){
                Group group{SkRect::MakeEmpty(),first,std::min(first+32,count)};
                for(auto i=group.begin;i<group.end;++i){
                    const auto b=bounds(i);
                    if(b.isEmpty()||!b.isFinite()){
                        const auto maximum=std::numeric_limits<float>::max();
                        group.bounds=SkRect::MakeLTRB(-maximum,-maximum,maximum,maximum);
                    }else group.bounds.join(b);
                }
                result.push_back(group);
            }return result;
        };
        ribbonGroups_=groups(ribbons_.size(),[&](auto i){return ribbons_[i].getBounds();});
        capGroups_=groups(caps_.size(),[&](auto i){return capBounds(caps_[i]);});
    }else if(const auto* vector=std::get_if<BrushStrokeReferenceCommand>(&std::as_const(source_))){
        const auto& output=vector->content.get().stroke.vector_output;
        vector_=buildVectorBrushOutlineSkPath(std::span(output.outline),output.fill_rule,output.closed);
    }
}
void PreparedNavigationGeometry::buildReplay(){
    const auto& stroke=std::get<CompactStrokeReferenceCommand>(source_).content.get().stroke;
    contour_=true;replayDistance_=ink::compactLength(stroke.knots);
    replay_.reserve((stroke.knots.size()+replayBlockKnots-1)/replayBlockKnots);
    replayOutput_.patches.reserve(replayBlockKnots+replayBlockExtras);replayOutput_.caps.reserve(replayBlockKnots+replayBlockExtras);
    ink::CompactContourCompiler compiler(stroke.config);std::size_t primitive=0;
    for(std::size_t first=0;first<stroke.knots.size();first+=replayBlockKnots){
        const auto count=std::min(replayBlockKnots,stroke.knots.size()-first);
        auto checkpoint=compiler;replayOutput_.patches.clear();replayOutput_.caps.clear();
        compiler.append({stroke.knots.data()+first,count},replayDistance_,replayOutput_);
        if(first+count==stroke.knots.size())compiler.finish(replayDistance_,replayOutput_);
        auto bounds=SkRect::MakeEmpty();
        const auto join=[&](SkRect b){if(b.isEmpty()||!b.isFinite()){
            const auto m=std::numeric_limits<float>::max();bounds=SkRect::MakeLTRB(-m,-m,m,m);
        }else bounds.join(b);};
        for(std::size_t i=0;i+1<replayOutput_.patches.size();++i)join(compactContourRibbon(std::span(replayOutput_.patches).subspan(i,2)).getBounds());
        for(const auto& cap:replayOutput_.caps)join(capBounds(cap));
        const auto emitted=(replayOutput_.patches.empty()?0:replayOutput_.patches.size()-1)+replayOutput_.caps.size();
        replay_.push_back({std::move(checkpoint),first,count,primitive,primitive+emitted,bounds});primitive+=emitted;
    }
    replayOutput_.patches.clear();replayOutput_.caps.clear();
}
std::size_t PreparedNavigationGeometry::nextVisiblePrimitive(SkCanvas& canvas,std::size_t first,const SkRect* deviceDemand)const{
    first=std::min(first,primitiveCount());const auto clip=contourClip(canvas,deviceDemand);
    if(!clip)return first;
    if(!replay_.empty()){
        for(const auto& block:replay_)if(block.last>first&&visibleContour(clip,block.bounds))return std::max(first,block.first);
    }else{
        for(const auto& group:ribbonGroups_)if(group.end>first&&visibleContour(clip,group.bounds))return std::max(first,group.begin);
        for(const auto& group:capGroups_)if(group.end+ribbons_.size()>first&&visibleContour(clip,group.bounds))return std::max(first,group.begin+ribbons_.size());
    }
    return primitiveCount();
}
std::size_t PreparedNavigationGeometry::footprint()const{
    std::size_t bytes=sizeof(*this)+512+distances_.capacity()*sizeof(double)+
        ribbons_.capacity()*sizeof(SkPath)+caps_.capacity()*sizeof(ink::ContourCap)+
        (ribbonGroups_.capacity()+capGroups_.capacity())*sizeof(Group);
    for(const auto& path:ribbons_)bytes+=path.approximateBytesUsed();
    if(vector_)bytes+=vector_->approximateBytesUsed();
    bytes+=replay_.capacity()*(sizeof(ReplayBlock)+512)+replayOutput_.patches.capacity()*sizeof(ink::ContourPatch)+replayOutput_.caps.capacity()*sizeof(ink::ContourCap);
    return bytes;
}
PreparedGeometryDraw PreparedNavigationGeometry::drawContour(SkCanvas& canvas,std::size_t first,std::size_t count,const SkRect* deviceDemand)const{
    PreparedGeometryDraw work;if(!contour_)return work;
    first=std::min(first,primitiveCount());const auto last=first+std::min(count,primitiveCount()-first);
    const auto& config=std::get<CompactStrokeReferenceCommand>(source_).content.get().stroke.config;
    const auto clip=contourClip(canvas,deviceDemand);
    const auto visible=[&](const SkRect& b){return visibleContour(clip,b);};
    SkPaint paint;paint.setAntiAlias(true);paint.setBlendMode(SkBlendMode::kLighten);paint.setColor(SK_ColorWHITE);
    if(!replay_.empty()){
        const auto& stroke=std::get<CompactStrokeReferenceCommand>(source_).content.get().stroke;
        for(const auto& block:replay_){
            if(block.last<=first)continue;if(block.first>=last)break;
            ++work.chunksVisited;if(!visible(block.bounds))continue;
            replayOutput_.patches.clear();replayOutput_.caps.clear();auto compiler=block.compiler;
            compiler.append({stroke.knots.data()+block.knot,block.count},replayDistance_,replayOutput_);
            if(block.knot+block.count==stroke.knots.size())compiler.finish(replayDistance_,replayOutput_);
            const auto ribbons=replayOutput_.patches.empty()?0:replayOutput_.patches.size()-1;
            for(auto i=std::max(first,block.first);i<std::min(last,block.last);++i){
                ++work.primitivesTested;const auto local=i-block.first;
                if(local<ribbons){
                    auto path=compactContourRibbon(std::span(replayOutput_.patches).subspan(local,2));
                    if(visible(path.getBounds())){canvas.drawPath(path,paint);++work.primitivesDrawn;}
                }else{const auto& cap=replayOutput_.caps[local-ribbons];
                    if(visible(capBounds(cap))){drawCap(canvas,config,cap,paint);++work.primitivesDrawn;}}
            }
        }
        return work;
    }
    for(auto g=first/32;g<ribbonGroups_.size()&&ribbonGroups_[g].begin<last;++g){
        const auto& group=ribbonGroups_[g];
        ++work.chunksVisited;if(!visible(group.bounds))continue;
        for(auto i=std::max(group.begin,first);i<std::min(group.end,last);++i){++work.primitivesTested;
            if(visible(ribbons_[i].getBounds())){
                canvas.drawPath(ribbons_[i],paint);
                ++work.primitivesDrawn;}}
    }
    const auto capFirst=first>ribbons_.size()?first-ribbons_.size():0;
    const auto capLast=last>ribbons_.size()?last-ribbons_.size():0;
    for(auto g=capFirst/32;g<capGroups_.size()&&capGroups_[g].begin<capLast;++g){
        const auto& group=capGroups_[g];
        ++work.chunksVisited;if(!visible(group.bounds))continue;
        for(auto i=std::max(group.begin,capFirst);i<std::min(group.end,capLast);++i){++work.primitivesTested;const auto& cap=caps_[i];
            if(!visible(capBounds(cap)))continue;
            drawCap(canvas,config,cap,paint);++work.primitivesDrawn;
        }
    }
    return work;
}
bool PreparedGeometryCache::evictUnused(){
    auto victim=entries_.end();
    for(auto i=entries_.begin();i!=entries_.end();++i)
        if(i->second.geometry.use_count()==1&&(victim==entries_.end()||i->second.used<victim->second.used))victim=i;
    if(victim==entries_.end())return false;
    entries_.erase(victim);++work_.evictions;return true;
}
std::shared_ptr<const PreparedNavigationGeometry> PreparedGeometryCache::get(
    const ReferenceTraversalEntry& entry,const RenderCacheOptions& limits,
    const std::shared_ptr<resources::ResourceBudgetManager>& budget,bool criticalReplay){
    const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command);
    const auto* vector=std::get_if<BrushStrokeReferenceCommand>(&entry.command);
    if(!compact&&(!vector||vector->content.get().stroke.snapshot.material_mode!=1))return {};
    const auto denied=[&]()->std::shared_ptr<const PreparedNavigationGeometry>{++work_.denied;return {};};
    if(!budget||!limits.maxEntries||!limits.maxBytes||!limits.maxEntryBytes)return denied();
    if(owner_.lock()!=budget){clear();owner_=budget;}
    auto retained=retainSource(entry,budget,false);if(!retained)return denied();
    std::size_t sourceBytes=0;for(const auto& lease:*retained)sourceBytes+=lease->bytes;
    auto found=entries_.find(entry.record.objectId);
    if(found!=entries_.end()){
        if(found->second.geometry->source_==entry.command){found->second.used=++tick_;++work_.hits;return found->second.geometry;}
        entries_.erase(found);
    }
    while(entries_.size()>=limits.maxEntries)if(!evictUnused())return denied();
    const auto count=compact?compact->content.get().stroke.knots.size():vector->content.get().stroke.vector_output.outline.size();
    const std::size_t unit=compact?16:64;
    const auto entryCap=std::min(limits.maxBytes,limits.maxEntryBytes);
    if(sourceBytes>=entryCap)return denied();
    const auto cap=entryCap-sourceBytes;
    const auto base=sizeof(PreparedNavigationGeometry)+65536;
    if(base>cap||count>(cap-base)/unit)return denied();
    auto estimate=base+count*unit;
    ink::CompactContour staged;
    resources::ResourceLease stagingLease;
    const bool contour=compact&&compact->content.get().stroke.config.algorithmVersion==4;
    if(contour){
        // The pinned compiler emits at most one anchor/cap per knot, plus
        // sixteen global taper cuts and two finish patches. Admit its exact
        // temporary capacity before compiling, then admit paths from actual
        // anchors rather than assuming 2KiB of paths for every input knot.
        constexpr auto extra=18U;
        const auto stagingUnit=sizeof(ink::ContourPatch)+sizeof(ink::ContourCap);
        if(count>std::numeric_limits<std::size_t>::max()-extra)return denied();
        if(count+extra>(cap-base)/stagingUnit){
            // Source-bound evaluator snapshots replace expanded paths only;
            // no new Brush geometry or larger entry/global budget is admitted.
            constexpr auto block=PreparedNavigationGeometry::replayBlockKnots;
            const auto blocks=(count+block-1)/block;
            const auto scratch=(block+PreparedNavigationGeometry::replayBlockExtras)*stagingUnit;
            const auto unit=sizeof(PreparedNavigationGeometry::ReplayBlock)+512;
            if(scratch>cap-base||blocks>(cap-base-scratch)/unit)return denied();
            const auto replayEstimate=base+scratch+blocks*unit;
            auto token=budget->reserve(resources::ResourceClass::kRecorded,replayEstimate,criticalReplay);
            while(!token&&evictUnused())token=budget->reserve(resources::ResourceClass::kRecorded,replayEstimate,criticalReplay);
            if(!token)return denied();resources::ResourceLease admission(budget,*token);
            try{
                auto candidate=std::make_shared<PreparedNavigationGeometry>();candidate->source_=entry.command;
                candidate->sourceLeases_=std::move(*retained);candidate->buildReplay();
                const auto bytes=candidate->footprint();if(bytes>replayEstimate)return denied();
                admission.reset();token=budget->reserve(resources::ResourceClass::kRecorded,bytes,criticalReplay);if(!token)return denied();
                candidate->lease_=resources::ResourceLease(budget,*token);candidate->bytes_=bytes;
                candidate->usage_=usage_;usage_->bytes+=bytes;
                entries_.insert_or_assign(entry.record.objectId,Entry{candidate,++tick_});++work_.builds;return candidate;
            }catch(const std::bad_alloc&){return denied();}
        }
        const auto stagingBytes=base+(count+extra)*stagingUnit;
        auto token=budget->reserve(resources::ResourceClass::kScratch,stagingBytes,false);
        while(!token&&evictUnused())token=budget->reserve(resources::ResourceClass::kScratch,stagingBytes,false);
        if(!token)return denied();stagingLease=resources::ResourceLease(budget,*token);
        try{
            staged.patches.reserve(count+extra);staged.caps.reserve(count+extra);
            const auto& stroke=compact->content.get().stroke;
            ink::CompactContourCompiler compiler(stroke.config);
            const auto total=ink::compactLength(stroke.knots);
            compiler.append(stroke.knots,total,staged);compiler.finish(total,staged);
        }catch(const std::bad_alloc&){return denied();}
        const auto ribbons=staged.patches.empty()?0:staged.patches.size()-1;
        const auto perRibbon=sizeof(SkPath)+512+sizeof(PreparedNavigationGeometry::Group);
        const auto perCap=sizeof(ink::ContourCap)+sizeof(PreparedNavigationGeometry::Group);
        estimate=base+(count+1)*sizeof(double);
        if(estimate>cap||staged.caps.capacity()>(cap-estimate)/perCap)return denied();
        estimate+=staged.caps.capacity()*perCap;
        if(ribbons>(cap-estimate)/perRibbon)return denied();estimate+=ribbons*perRibbon;
    }
    auto token=budget->reserve(resources::ResourceClass::kRecorded,estimate,false);
    while(!token&&evictUnused())token=budget->reserve(resources::ResourceClass::kRecorded,estimate,false);
    if(!token)return denied();
    resources::ResourceLease admission(budget,*token);
    try{
        auto candidate=std::make_shared<PreparedNavigationGeometry>();candidate->source_=entry.command;candidate->sourceLeases_=std::move(*retained);candidate->build(contour?&staged:nullptr);
        const auto bytes=candidate->footprint();if(bytes>estimate)return denied();
        // Owner-thread transaction: shrink the admitted peak to live storage.
        admission.reset();token=budget->reserve(resources::ResourceClass::kRecorded,bytes,false);if(!token)return denied();
        candidate->lease_=resources::ResourceLease(budget,*token);candidate->bytes_=bytes;
        candidate->usage_=usage_;usage_->bytes+=bytes;
        entries_.insert_or_assign(entry.record.objectId,Entry{candidate,++tick_});++work_.builds;return candidate;
    }catch(const std::bad_alloc&){return denied();}
}
}
