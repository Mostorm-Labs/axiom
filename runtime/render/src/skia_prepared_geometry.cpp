#include "canvas/foundation/internal/g5_observation.hpp"
namespace g5 = canvas::foundation::internal::g5;
#include "skia_prepared_geometry.hpp"
#include "prepared_body_job.hpp"
#include "render_execution.hpp"
#include "compact_contour_skia_path.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "canvas/render/brush_outline_skia_path.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <array>
#include <bit>
#include <cstdio>

namespace canvas::render::internal {
namespace {
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
    G5_SCOPE(g5Work,g5::Tag::geometry_build,g5::Purpose::none);

    if(const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&std::as_const(source_))){
        const auto& s=compact->content.get().stroke;
        distances_.resize(s.knots.size()+1,0);
        for(std::size_t i=0;i<s.knots.size();++i){
            const auto& k=s.knots[i];const auto radius=ink::compactRadius(s.config,k.pressure);
            support_.join(SkRect::MakeLTRB(float(k.x-radius),float(k.y-radius),float(k.x+radius),float(k.y+radius)));
            distances_[i+1]=distances_[i]+(i?std::hypot(k.x-s.knots[i-1].x,k.y-s.knots[i-1].y):0);
        }
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
        capGroups_=groups(caps_.size(),[&](auto i){return compactContourCapBounds(caps_[i]);});
    }else if(const auto* vector=std::get_if<BrushStrokeReferenceCommand>(&std::as_const(source_))){
        const auto& output=vector->content.get().stroke.vector_output;
        vector_=buildVectorBrushOutlineSkPath(std::span(output.outline),output.fill_rule,output.closed);
    }
}
void PreparedNavigationGeometry::buildReplay(){
    G5_SCOPE(g5Work,g5::Tag::geometry_build,g5::Purpose::none);

    const auto& stroke=std::get<CompactStrokeReferenceCommand>(source_).content.get().stroke;
    contour_=true;replayDistance_=ink::compactLength(stroke.knots);
    for(const auto& k:stroke.knots){const auto radius=ink::compactRadius(stroke.config,k.pressure);
        support_.join(SkRect::MakeLTRB(float(k.x-radius),float(k.y-radius),float(k.x+radius),float(k.y+radius)));}
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
        for(const auto& cap:replayOutput_.caps)join(compactContourCapBounds(cap));
        const auto emitted=(replayOutput_.patches.empty()?0:replayOutput_.patches.size()-1)+replayOutput_.caps.size();
        replay_.push_back({std::move(checkpoint),first,count,primitive,primitive+emitted,bounds});primitive+=emitted;
    }
    replayOutput_.patches.clear();replayOutput_.caps.clear();
}
std::size_t PreparedNavigationGeometry::nextVisiblePrimitive(SkCanvas& canvas,std::size_t first,const SkRect* deviceDemand)const{
    first=std::min(first,primitiveCount());const auto clip=compactContourCullBounds(canvas,deviceDemand);
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
    G5_SCOPE(g5Work,g5::Tag::geometry_replay,g5::Purpose::none);

    PreparedGeometryDraw work;if(!contour_)return work;
    first=std::min(first,primitiveCount());const auto last=first+std::min(count,primitiveCount()-first);
    const auto& config=std::get<CompactStrokeReferenceCommand>(source_).content.get().stroke.config;
    const auto clip=compactContourCullBounds(canvas,deviceDemand);
    const auto visible=[&](const SkRect& b){return visibleContour(clip,b);};
    SkPaint paint;paint.setAntiAlias(true);paint.setBlendMode((canvas.recordingContext()||canvas.imageInfo().colorType()==kAlpha_8_SkColorType)?SkBlendMode::kSrcOver:SkBlendMode::kLighten);paint.setColor(SK_ColorWHITE);
    if(!replay_.empty()){
        const auto& stroke=std::get<CompactStrokeReferenceCommand>(source_).content.get().stroke;
        for(const auto& block:replay_){
            if(block.last<=first)continue;if(block.first>=last)break;
            ++work.chunksVisited;if(!visible(block.bounds))continue;
            replayOutput_.patches.clear();replayOutput_.caps.clear();auto compiler=block.compiler;
            compiler.append({stroke.knots.data()+block.knot,block.count},replayDistance_,replayOutput_);
            work.compiledKnots+=block.count;
            if(auto* execution=RenderExecutionScope::current())execution->sourceWork(0,0,0,block.count);
            if(block.knot+block.count==stroke.knots.size())compiler.finish(replayDistance_,replayOutput_);
            const auto ribbons=replayOutput_.patches.empty()?0:replayOutput_.patches.size()-1;
            for(auto i=std::max(first,block.first);i<std::min(last,block.last);++i){
                ++work.primitivesTested;const auto local=i-block.first;
                if(local<ribbons){
                    auto path=compactContourRibbon(std::span(replayOutput_.patches).subspan(local,2));
                    if(visible(path.getBounds())){canvas.drawPath(path,paint);++work.primitivesDrawn;}
                }else{const auto& cap=replayOutput_.caps[local-ribbons];
                    if(visible(compactContourCapBounds(cap))){drawCap(canvas,config,cap,paint);++work.primitivesDrawn;}}
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
            if(!visible(compactContourCapBounds(cap)))continue;
            drawCap(canvas,config,cap,paint);++work.primitivesDrawn;
        }
    }
    return work;
}
ImmutableSourceIdentity preparedBodyIdentity(const ReferenceCommand& command){
    if(const auto* c=std::get_if<CompactStrokeReferenceCommand>(&command))return {c->content.shared(),sourceTypeTag<semantic::CompactStrokeContent>(),command.index()};
    if(const auto* c=std::get_if<BrushStrokeReferenceCommand>(&command))return {c->content.shared(),sourceTypeTag<semantic::BrushStrokeContent>(),command.index()};
    return {};
}
namespace {
bool equalBody(const ReferenceCommand& a,const ReferenceCommand& b){
    // This is complete value equality, including config/seed/snapshot. Unknown
    // ownership must not use ImmutableValue's raw-pointee shortcut as proof.
    if(a.index()!=b.index())return false;
    if(const auto* c=std::get_if<CompactStrokeReferenceCommand>(&a))return c->content.get()==std::get<CompactStrokeReferenceCommand>(b).content.get();
    if(const auto* c=std::get_if<BrushStrokeReferenceCommand>(&a))return c->content.get()==std::get<BrushStrokeReferenceCommand>(b).content.get();
    return false;
}
std::uint64_t fingerprint(const ReferenceCommand& command,PreparedGeometryStatistics& work){
    // Deliberately an index, not content authority. Config/seed/snapshot
    // differences can collide and always go through complete equality below.
    // Bounded samples keep cold admission from adding a new whole-history hash.
    std::uint64_t hash=1469598103934665603ULL;
    const auto add=[&](auto value){
        if constexpr(std::is_floating_point_v<decltype(value)>)if(value==0)value=0; // equal +/-zero hash alike
        const auto bytes=std::bit_cast<std::array<unsigned char,sizeof(value)>>(value);
        work.fingerprintBytes+=bytes.size();for(auto byte:bytes){hash^=byte;hash*=1099511628211ULL;}
    };
    add(command.index());
    if(const auto* c=std::get_if<CompactStrokeReferenceCommand>(&command)){
        const auto& knots=c->content.get().stroke.knots;add(knots.size());
        if(!knots.empty()){std::size_t previous=~std::size_t{0};for(auto i:{std::size_t{0},knots.size()/2,knots.size()-1})if(i!=previous){
            const auto& k=knots[i];add(k.x);add(k.y);add(k.pressure);previous=i;}}
    }else if(const auto* c=std::get_if<BrushStrokeReferenceCommand>(&command)){
        const auto& outline=c->content.get().stroke.vector_output.outline;add(outline.size());
        if(!outline.empty()){std::size_t previous=~std::size_t{0};for(auto i:{std::size_t{0},outline.size()/2,outline.size()-1})if(i!=previous){
            const auto& point=outline[i];add(point.x);add(point.y);previous=i;}}
    }
    return hash;
}
}
void PreparedGeometryCache::clear()noexcept{aliases_.clear();fingerprints_.clear();entries_.clear();}
void PreparedGeometryCache::eraseBody(std::uint64_t id)noexcept{
    const auto found=entries_.find(id);if(found==entries_.end())return;
    for(auto it=aliases_.begin();it!=aliases_.end();)if(it->second.body==id)it=aliases_.erase(it);else ++it;
    const auto [first,last]=fingerprints_.equal_range(found->second.fingerprint);
    for(auto it=first;it!=last;)if(it->second==id)it=fingerprints_.erase(it);else ++it;
    entries_.erase(found);
}
bool PreparedGeometryCache::evictAlias()noexcept{
    auto victim=std::min_element(aliases_.begin(),aliases_.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});
    if(victim==aliases_.end())return false;aliases_.erase(victim);++work_.aliasEvictions;return true;
}
void PreparedGeometryCache::bindOwner(const std::shared_ptr<resources::ResourceBudgetManager>& budget){
    if(owner_.lock()!=budget){clear();owner_=budget;usage_=std::make_shared<PreparedGeometryUsage>();retention_->bind(budget);}
}
std::optional<SourceRetentions> PreparedGeometryCache::retainSource(const ReferenceTraversalEntry& entry,
    const std::shared_ptr<resources::ResourceBudgetManager>& budget,bool critical){
    if(!budget)return {};bindOwner(budget);
    auto denials=retention_->statistics().quotaDenials;auto result=retention_->get(entry,budget,critical);
    while(!result&&retention_->statistics().quotaDenials>denials){
        // Alias discovery is disposable even when a consumer pins the one
        // geometry. Reclaim it independently; invalid footprint/type failures
        // are not treated as quota misses and never trigger this retry.
        if(!evictAlias()&&!evictUnused())break;
        denials=retention_->statistics().quotaDenials;result=retention_->get(entry,budget,critical);
    }
    return result;
}
bool PreparedGeometryCache::evictUnused(){
    auto victim=entries_.end();
    for(auto i=entries_.begin();i!=entries_.end();++i)
        if(i->second.geometry.use_count()==1&&(victim==entries_.end()||i->second.used<victim->second.used))victim=i;
    if(victim==entries_.end())return false;
    const auto id=victim->first;eraseBody(id);++work_.evictions;return true;
}
std::shared_ptr<PreparedGeometryCache::IndexLease> PreparedGeometryCache::reserveIndex(std::size_t bytes,
    const std::shared_ptr<resources::ResourceBudgetManager>& budget,bool critical){
    auto token=budget->reserve(resources::ResourceClass::kRecorded,bytes,critical);
    while(!token&&(evictAlias()||evictUnused()))token=budget->reserve(resources::ResourceClass::kRecorded,bytes,critical);
    if(!token)return {};
    resources::ResourceLease reservation(budget,*token);auto result=std::make_shared<IndexLease>();
    result->lease=std::move(reservation);result->usage=usage_;result->bytes=bytes;usage_->indexBytes+=bytes;return result;
}
bool PreparedGeometryCache::remember(const ImmutableSourceIdentity& key,std::shared_ptr<const PreparedNavigationGeometry> geometry,
    std::uint64_t body,SourceRetentions sources,const RenderCacheOptions& limits,
    const std::shared_ptr<resources::ResourceBudgetManager>& budget,bool critical){
    while(aliases_.size()>=limits.maxEntries){
        if(!evictAlias())return false;
    }
    // Conservative container/control-block bookkeeping reservation, not a
    // measurement of allocator RSS or copied content bytes.
    const auto bytes=sizeof(Alias)+sizeof(ImmutableSourceIdentity)+sizeof(IndexLease)+128+sources.capacity()*sizeof(SourceRetentions::value_type);
    auto metadata=reserveIndex(bytes,budget,critical);if(!metadata)return false;
    aliases_.insert_or_assign(key,Alias{geometry,body,++tick_,std::move(sources),std::move(metadata)});++work_.aliasAdmissions;return true;
}
std::shared_ptr<const PreparedNavigationGeometry> PreparedGeometryCache::publish(std::shared_ptr<PreparedNavigationGeometry> geometry,
    std::uint64_t hash,const ImmutableSourceIdentity& key,SourceRetentions sources,const RenderCacheOptions& limits,
    const std::shared_ptr<resources::ResourceBudgetManager>& budget,bool critical){
    const auto bytes=sizeof(Entry)+sizeof(std::pair<const std::uint64_t,std::uint64_t>)+sizeof(IndexLease)+256;
    auto metadata=reserveIndex(bytes,budget,critical);if(!metadata){++work_.denied;return {};}
    geometry->initialIndexLeases_[0]=metadata;
    const auto id=++serial_;geometry->cacheId_=id;entries_.emplace(id,Entry{geometry,++tick_,hash,std::move(metadata)});
    struct Rollback {PreparedGeometryCache& cache;std::uint64_t id;bool committed=false;~Rollback(){if(!committed)cache.eraseBody(id);}} rollback{*this,id};
    fingerprints_.emplace(hash,id);
    if(!remember(key,geometry,id,std::move(sources),limits,budget,critical)){++work_.denied;return {};}
    ++work_.indexLookups;geometry->initialIndexLeases_[1]=aliases_.find(key)->second.metadata;
    rollback.committed=true;if(geometry->ready())++work_.builds;else ++work_.jobAdmissions;return geometry;
}
std::shared_ptr<const PreparedNavigationGeometry> PreparedGeometryCache::get(
    const ReferenceTraversalEntry& entry,const RenderCacheOptions& limits,
    const std::shared_ptr<resources::ResourceBudgetManager>& budget,bool criticalReplay,PreparedConsumer consumer,bool deferred){
    G5_SCOPE(g5Work,g5::Tag::lookup,g5::Purpose::none);
    ++work_.lookups;
    if(consumer==PreparedConsumer::kNavigation)++work_.navigationQueries;
    if(consumer==PreparedConsumer::kRetainedInk)++work_.retainedInkQueries;
    const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command);
    const auto* vector=std::get_if<BrushStrokeReferenceCommand>(&entry.command);
    if(!compact&&(!vector||vector->content.get().stroke.snapshot.material_mode!=1))return {};
    const auto denied=[&]()->std::shared_ptr<const PreparedNavigationGeometry>{++work_.denied;return {};};
    if(!budget||!limits.maxEntries||!limits.maxBytes||!limits.maxEntryBytes){if(deferred)lastRequestRejected_=true;return denied();}
    bindOwner(budget);
    const auto key=preparedBodyIdentity(entry.command);if(!key.storage)return denied();
    ++work_.indexLookups;
    if(auto alias=aliases_.find(key);alias!=aliases_.end()){
        if(auto geometry=alias->second.geometry.lock()){
            alias->second.used=++tick_;
            ++work_.indexLookups;
            if(auto body=entries_.find(alias->second.body);body!=entries_.end())body->second.used=tick_;
            ++work_.identityHits;++work_.hits;return geometry->ready()||deferred?geometry:nullptr;
        }
        aliases_.erase(alias);
    }
    const bool criticalContour=criticalReplay&&compact&&compact->content.get().stroke.config.algorithmVersion==4;
    auto retained=retainSource(entry,budget,criticalContour);if(!retained)return denied();
    std::size_t sourceBytes=0;for(const auto& lease:*retained)sourceBytes+=lease->bytes;
    const auto hash=fingerprint(entry.command,work_);++work_.indexLookups;
    const auto [first,last]=fingerprints_.equal_range(hash);
    for(auto it=first;it!=last;++it){
        ++work_.candidateChecks;++work_.indexLookups;auto body=entries_.find(it->second);if(body==entries_.end())continue;
        ++work_.contentComparisons;
        if(equalBody(body->second.geometry->source_,entry.command)){
            auto geometry=body->second.geometry;const auto id=body->first;
            try{if(!remember(key,geometry,id,std::move(*retained),limits,budget,criticalContour))return denied();}
            catch(const std::bad_alloc&){return denied();}
            ++work_.indexLookups;body=entries_.find(id);if(body!=entries_.end())body->second.used=++tick_;
            ++work_.hits;return geometry->ready()||deferred?geometry:nullptr;
        }
    }
    while(entries_.size()>=limits.maxEntries)if(!evictUnused()){
        // maxEntries bounds cache discoverability, not in-flight consumers.
        // A replaced pinned body keeps its own geometry/source/index leases.
        auto victim=entries_.end();
        for(auto it=entries_.begin();it!=entries_.end();++it)if(it->second.geometry->ready()&&
            (victim==entries_.end()||it->second.used<victim->second.used))victim=it;
        if(victim==entries_.end())return denied();const auto id=victim->first;eraseBody(id);++work_.evictions;
    }
    const auto count=compact?compact->content.get().stroke.knots.size():vector->content.get().stroke.vector_output.outline.size();
    if(deferred&&compact&&compact->content.get().stroke.config.algorithmVersion==4){
        const auto entryCap=std::min(limits.maxBytes,limits.maxEntryBytes);
        if(sourceBytes>=entryCap||PreparedBodyJob::minimumBytes(count)>entryCap-sourceBytes){lastRequestRejected_=true;return denied();}
        try{
            auto candidate=PreparedBodyJob::create(entry.command,*retained,entryCap-sourceBytes,budget,usage_);
            // Reclaim only cache-owned work on an actual quota miss. Current
            // frame dependencies and suspended jobs retain their own leases.
            while(!candidate&&evictUnused())candidate=PreparedBodyJob::create(entry.command,*retained,entryCap-sourceBytes,budget,usage_);
            if(!candidate)return denied();
            return publish(std::move(candidate),hash,key,std::move(*retained),limits,budget,true);
        }catch(const std::bad_alloc&){return denied();}
    }
    G5_OBSERVE(g5::Kind::Counter,g5::Tag::geometry_build,false,0,count);
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
    const auto stagingUnit=sizeof(ink::ContourPatch)+sizeof(ink::ContourCap);
    const auto replay=[&]()->std::shared_ptr<const PreparedNavigationGeometry>{
        // Drop optional full-path staging before admitting its compact replacement.
        staged={};stagingLease.reset();
        // Source-bound evaluator snapshots replace expanded paths only;
        // no new Brush geometry or larger entry/global budget is admitted.
        constexpr auto block=PreparedNavigationGeometry::replayBlockKnots;
        const auto blocks=(count+block-1)/block;
        const auto scratch=(block+PreparedNavigationGeometry::replayBlockExtras)*stagingUnit;
        const auto replayUnit=sizeof(PreparedNavigationGeometry::ReplayBlock)+512;
        if(scratch>cap-base||blocks>(cap-base-scratch)/replayUnit)return denied();
        const auto replayEstimate=base+scratch+blocks*replayUnit;
        auto token=budget->reserve(resources::ResourceClass::kRecorded,replayEstimate,criticalReplay);
        while(!token&&evictUnused())token=budget->reserve(resources::ResourceClass::kRecorded,replayEstimate,criticalReplay);
        if(!token)return denied();resources::ResourceLease admission(budget,*token);
        try{
            auto candidate=std::make_shared<PreparedNavigationGeometry>();candidate->source_=entry.command;
            candidate->sourceLeases_=*retained;candidate->buildReplay();
            const auto bytes=candidate->footprint();if(bytes>replayEstimate)return denied();
            admission.reset();token=budget->reserve(resources::ResourceClass::kRecorded,bytes,criticalReplay);if(!token)return denied();
            candidate->lease_=resources::ResourceLease(budget,*token);candidate->bytes_=bytes;
            candidate->usage_=usage_;usage_->bytes+=bytes;
            return publish(candidate,hash,key,std::move(*retained),limits,budget,criticalContour);
        }catch(const std::bad_alloc&){return denied();}
    };
    if(contour){
        // The pinned compiler emits at most one anchor/cap per knot, plus
        // sixteen global taper cuts and two finish patches. Admit its exact
        // temporary capacity before compiling, then admit paths from actual
        // anchors rather than assuming 2KiB of paths for every input knot.
        constexpr auto extra=18U;
        if(count>std::numeric_limits<std::size_t>::max()-extra)return denied();
        if(count+extra>(cap-base)/stagingUnit){
            return replay();
        }
        const auto stagingBytes=base+(count+extra)*stagingUnit;
        auto token=budget->reserve(resources::ResourceClass::kScratch,stagingBytes,false);
        while(!token&&evictUnused())token=budget->reserve(resources::ResourceClass::kScratch,stagingBytes,false);
        if(!token)return criticalContour?replay():denied();stagingLease=resources::ResourceLease(budget,*token);
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
    if(!token)return criticalContour?replay():denied();
    resources::ResourceLease admission(budget,*token);
    try{
        auto candidate=std::make_shared<PreparedNavigationGeometry>();candidate->source_=entry.command;candidate->sourceLeases_=*retained;candidate->build(contour?&staged:nullptr);
        const auto bytes=candidate->footprint();if(bytes>estimate)return denied();
        // Owner-thread transaction: shrink the admitted peak to live storage.
        admission.reset();token=budget->reserve(resources::ResourceClass::kRecorded,bytes,false);if(!token)return denied();
        candidate->lease_=resources::ResourceLease(budget,*token);candidate->bytes_=bytes;
        candidate->usage_=usage_;usage_->bytes+=bytes;
        return publish(candidate,hash,key,std::move(*retained),limits,budget,criticalContour);
    }catch(const std::bad_alloc&){return denied();}
}
std::shared_ptr<const PreparedNavigationGeometry> PreparedGeometryCache::lookup(const ReferenceTraversalEntry& entry,PreparedConsumer consumer){
    ++work_.lookups;++work_.indexLookups;
    if(consumer==PreparedConsumer::kCanonical)++work_.canonicalQueries;
    if(consumer==PreparedConsumer::kDirect)++work_.directQueries;
    if(consumer==PreparedConsumer::kRetainedInk)++work_.retainedInkQueries;
    if(consumer==PreparedConsumer::kNavigation)++work_.navigationQueries;
    const auto key=preparedBodyIdentity(entry.command);const auto found=aliases_.find(key);
    if(found==aliases_.end())return {};
    auto geometry=found->second.geometry.lock();if(!geometry||!geometry->ready())return {};
    found->second.used=++tick_;if(auto body=entries_.find(found->second.body);body!=entries_.end())body->second.used=tick_;
    ++work_.identityHits;++work_.hits;return geometry;
}
bool PreparedGeometryCache::advance(const std::shared_ptr<const PreparedNavigationGeometry>& value,PreparationAllowance& allowance){
    if(!value)return false;
    const auto found=entries_.find(value->cacheId_);
    if(found==entries_.end()||found->second.geometry!=value||value->usage_!=usage_)return false;
    if(value->ready())return true;if(allowance.exhausted())return false;
    auto geometry=std::const_pointer_cast<PreparedNavigationGeometry>(value);
    ++allowance.steps;
    if(geometry->job_&&geometry->job_->advance(*geometry,work_))geometry->job_.reset();
    return geometry->ready();
}
#if defined(AXIOM_ENABLE_FRAME_TIMING)
PreparedBodyTrace::~PreparedBodyTrace(){
    if(!cache_)return;const auto s=cache_->statistics();
    std::fprintf(stderr,"AXIOM_PREPARED_BODY_WORK {\"purpose\":\"%s\",\"service\":\"%p\",\"canonical_queries\":%llu,\"direct_queries\":%llu,\"job_admissions\":%llu,\"advances\":%llu,\"support_knots\":%llu,\"distance_inputs\":%llu,\"compile_inputs\":%llu,\"path_primitives\":%llu,\"max_advance_knots\":%llu}\n",
        purpose_,static_cast<const void*>(cache_.get()),
        static_cast<unsigned long long>(s.canonicalQueries-before_.canonicalQueries),static_cast<unsigned long long>(s.directQueries-before_.directQueries),
        static_cast<unsigned long long>(s.jobAdmissions-before_.jobAdmissions),static_cast<unsigned long long>(s.advances-before_.advances),
        static_cast<unsigned long long>(s.supportKnots-before_.supportKnots),static_cast<unsigned long long>(s.distanceInputs-before_.distanceInputs),
        static_cast<unsigned long long>(s.compileInputs-before_.compileInputs),static_cast<unsigned long long>(s.pathPrimitives-before_.pathPrimitives),static_cast<unsigned long long>(s.maxAdvanceKnots));

    std::fprintf(stderr,"AXIOM_PREPARED_BODY {\"version\":1,\"purpose\":\"%s\",\"service\":\"%p\",\"lookups\":%llu,\"index_queries\":%llu,\"builds\":%llu,\"identity_hits\":%llu,\"fingerprint_bytes\":%llu,\"content_comparisons\":%llu,\"candidate_checks\":%llu,\"navigation_queries\":%llu,\"ink_queries\":%llu,\"source_footprint_evaluations\":%llu,\"source_known_footprint_hits\":%llu,\"aliases\":%zu,\"bodies\":%zu,\"index_reservation_bytes\":%zu,\"resident_accounted_bytes\":%zu}\n",
        purpose_,static_cast<const void*>(cache_.get()),static_cast<unsigned long long>(s.lookups-before_.lookups),
        static_cast<unsigned long long>(s.indexLookups-before_.indexLookups),static_cast<unsigned long long>(s.builds-before_.builds),static_cast<unsigned long long>(s.identityHits-before_.identityHits),
        static_cast<unsigned long long>(s.fingerprintBytes-before_.fingerprintBytes),static_cast<unsigned long long>(s.contentComparisons-before_.contentComparisons),
        static_cast<unsigned long long>(s.candidateChecks-before_.candidateChecks),static_cast<unsigned long long>(s.navigationQueries-before_.navigationQueries),
        static_cast<unsigned long long>(s.retainedInkQueries-before_.retainedInkQueries),static_cast<unsigned long long>(s.sourceFootprintEvaluations-before_.sourceFootprintEvaluations),
        static_cast<unsigned long long>(s.sourceKnownFootprintHits-before_.sourceKnownFootprintHits),s.aliases,s.bodies,s.indexReservationBytes,s.residentBytes);
}
#endif

}
