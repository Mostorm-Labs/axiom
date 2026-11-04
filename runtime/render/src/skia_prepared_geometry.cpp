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
std::size_t payloadBytes(const ReferenceCommand& source){
    if(const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&source))
        return compact->content.stroke.knots.size()*sizeof(foundation::InkKnot);
    if(const auto* vector=std::get_if<BrushStrokeReferenceCommand>(&source)){
        const auto& s=vector->content.stroke;
        std::size_t bytes=s.confirmed_samples.size()*sizeof(semantic::BrushConfirmedSample)+
            s.vector_output.outline.size()*sizeof(semantic::Vec2)+
            s.snapshot.stages.size()*sizeof(semantic::BrushStageBinding)+
            s.snapshot.resources.size()*sizeof(semantic::BrushResourceBinding);
        for(const auto& resource:s.snapshot.resources)bytes+=resource.content_sha256.size()+1;
        return bytes;
    }
    return 0;
}
SkRect capBounds(const ink::ContourCap& cap){
    // The draw uses a float center/radius. Bound that exact representation,
    // rounding outward: double bounds cast to float may collapse a tiny cap
    // at a large world origin even though a zoomed Skia circle is visible.
    const auto x=float(cap.x),y=float(cap.y),r=float(cap.radius);
    const auto inf=std::numeric_limits<float>::infinity();
    return SkRect::MakeLTRB(std::nextafter(x-r,-inf),std::nextafter(y-r,-inf),
                           std::nextafter(x+r,inf),std::nextafter(y+r,inf));
}
}
PreparedNavigationGeometry::~PreparedNavigationGeometry(){if(usage_)usage_->bytes-=bytes_;}
void PreparedNavigationGeometry::build(){
    if(const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&source_)){
        const auto& s=compact->content.stroke;
        distances_.resize(s.knots.size()+1,0);
        for(std::size_t i=2;i<distances_.size();++i)
            distances_[i]=distances_[i-1]+std::hypot(s.knots[i-1].x-s.knots[i-2].x,s.knots[i-1].y-s.knots[i-2].y);
        if(s.config.algorithmVersion!=4)return;
        contour_=true;
        auto contour=ink::compileCompactContour(s.config,s.knots);
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
    }else if(const auto* vector=std::get_if<BrushStrokeReferenceCommand>(&source_)){
        const auto& output=vector->content.stroke.vector_output;
        vector_=buildVectorBrushOutlineSkPath(std::span(output.outline),output.fill_rule,output.closed);
    }
}
std::size_t PreparedNavigationGeometry::footprint()const{
    std::size_t bytes=sizeof(*this)+payloadBytes(source_)+512+distances_.capacity()*sizeof(double)+
        ribbons_.capacity()*sizeof(SkPath)+caps_.capacity()*sizeof(ink::ContourCap)+
        (ribbonGroups_.capacity()+capGroups_.capacity())*sizeof(Group);
    for(const auto& path:ribbons_)bytes+=path.approximateBytesUsed();
    if(vector_)bytes+=vector_->approximateBytesUsed();return bytes;
}
PreparedGeometryDraw PreparedNavigationGeometry::drawContour(SkCanvas& canvas)const{
    PreparedGeometryDraw work;if(!contour_)return work;
    const auto& config=std::get<CompactStrokeReferenceCommand>(source_).content.stroke.config;
    auto clip=canvas.getLocalClipBounds();SkMatrix inverse;
    bool cull=!canvas.getTotalMatrix().hasPerspective()&&canvas.getTotalMatrix().invert(&inverse)&&clip.isFinite();
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
    const auto visible=[&](const SkRect& bounds){return !cull||bounds.isEmpty()||!bounds.isFinite()||SkRect::Intersects(clip,bounds);};
    SkPaint paint;paint.setAntiAlias(true);paint.setBlendMode(SkBlendMode::kLighten);paint.setColor(SK_ColorWHITE);
    for(const auto& group:ribbonGroups_){
        ++work.chunksVisited;if(!visible(group.bounds))continue;
        for(auto i=group.begin;i<group.end;++i){++work.primitivesTested;
            if(visible(ribbons_[i].getBounds())){canvas.drawPath(ribbons_[i],paint);++work.primitivesDrawn;}}
    }
    for(const auto& group:capGroups_){
        ++work.chunksVisited;if(!visible(group.bounds))continue;
        for(auto i=group.begin;i<group.end;++i){++work.primitivesTested;const auto& cap=caps_[i];
            if(!visible(capBounds(cap)))continue;
            canvas.save();canvas.translate(float(cap.x),float(cap.y));
            if(config.family==foundation::InkFamily::kMarker){canvas.rotate(float(config.markerAngle*57.29577951308232));canvas.scale(1,float(config.markerFlatness));}
            canvas.drawCircle(0,0,float(cap.radius),paint);canvas.restore();++work.primitivesDrawn;
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
    const std::shared_ptr<resources::ResourceBudgetManager>& budget){
    const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command);
    const auto* vector=std::get_if<BrushStrokeReferenceCommand>(&entry.command);
    if(!compact&&(!vector||vector->content.stroke.snapshot.material_mode!=1))return {};
    const auto denied=[&]()->std::shared_ptr<const PreparedNavigationGeometry>{++work_.denied;return {};};
    if(!budget||!limits.maxEntries||!limits.maxBytes||!limits.maxEntryBytes)return denied();
    if(owner_.lock()!=budget){clear();owner_=budget;}
    auto found=entries_.find(entry.record.objectId);
    if(found!=entries_.end()){
        if(found->second.geometry->source_==entry.command){found->second.used=++tick_;++work_.hits;return found->second.geometry;}
        entries_.erase(found);
    }
    while(entries_.size()>=limits.maxEntries)if(!evictUnused())return denied();
    const auto count=compact?compact->content.stroke.knots.size():vector->content.stroke.vector_output.outline.size();
    const std::size_t unit=compact?(compact->content.stroke.config.algorithmVersion==4?2048:16):64;
    const auto cap=std::min(limits.maxBytes,limits.maxEntryBytes);
    const auto base=sizeof(PreparedNavigationGeometry)+payloadBytes(entry.command)+65536;
    if(base>cap||count>(cap-base)/unit)return denied();
    const auto estimate=base+count*unit;
    auto token=budget->reserve(resources::ResourceClass::kRecorded,estimate,false);
    while(!token&&evictUnused())token=budget->reserve(resources::ResourceClass::kRecorded,estimate,false);
    if(!token)return denied();
    resources::ResourceLease admission(budget,*token);
    try{
        auto candidate=std::make_shared<PreparedNavigationGeometry>();candidate->source_=entry.command;candidate->build();
        const auto bytes=candidate->footprint();if(bytes>estimate)return denied();
        // Owner-thread transaction: shrink the admitted peak to live storage.
        admission.reset();token=budget->reserve(resources::ResourceClass::kRecorded,bytes,false);if(!token)return denied();
        candidate->lease_=resources::ResourceLease(budget,*token);candidate->bytes_=bytes;
        candidate->usage_=usage_;usage_->bytes+=bytes;
        entries_.insert_or_assign(entry.record.objectId,Entry{candidate,++tick_});++work_.builds;return candidate;
    }catch(const std::bad_alloc&){return denied();}
}
}
