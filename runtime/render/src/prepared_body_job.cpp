#include "prepared_body_job.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "compact_contour_skia_path.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace canvas::render::internal {
std::size_t PreparedBodyJob::minimumBytes(std::size_t count)noexcept{
    const auto base=sizeof(PreparedNavigationGeometry)+sizeof(PreparedBodyJob)+65536+
        64*(sizeof(ink::ContourPatch)+sizeof(ink::ContourCap));
    const auto blocks=count/32+(count%32!=0),unit=sizeof(PreparedNavigationGeometry::ReplayBlock)+512;
    return blocks>(std::numeric_limits<std::size_t>::max()-base)/unit?std::numeric_limits<std::size_t>::max():base+blocks*unit;
}
std::shared_ptr<PreparedNavigationGeometry> PreparedBodyJob::create(const ReferenceCommand& source,SourceRetentions sources,
    std::size_t cap,const std::shared_ptr<resources::ResourceBudgetManager>& budget,const std::shared_ptr<PreparedGeometryUsage>& usage){
    const auto& stroke=std::get<CompactStrokeReferenceCommand>(source).content.get().stroke;
    const auto count=stroke.knots.size();
    if(count>std::numeric_limits<std::size_t>::max()-50)return {};
    constexpr auto extra=std::size_t(64);
    const auto base=sizeof(PreparedNavigationGeometry)+sizeof(PreparedBodyJob)+65536+
        extra*(sizeof(ink::ContourPatch)+sizeof(ink::ContourCap));
    if(base>cap)return {};
    const auto fullUnit=sizeof(SkPath)+512+sizeof(ink::ContourCap)+sizeof(PreparedNavigationGeometry::Group);
    const bool full=count+extra<=(cap-base)/fullUnit;
    const auto blocks=count/32+(count%32!=0);
    const auto replayUnit=sizeof(PreparedNavigationGeometry::ReplayBlock)+512;
    if(!full&&blocks>(cap-base)/replayUnit)return {};
    auto estimate=base+(full?(count+extra)*fullUnit:blocks*replayUnit);
    auto token=budget->reserve(resources::ResourceClass::kRecorded,estimate,!full);
    // The existing compact checkpoint realization is also the quota fallback;
    // it has the same entry cap and all replay compilation stays observable.
    bool replay=!full;
    if(!token&&full){estimate=base+blocks*replayUnit;if(estimate>cap)return {};
        token=budget->reserve(resources::ResourceClass::kRecorded,estimate,true);replay=true;}
    if(!token)return {};
    resources::ResourceLease reservation(budget,*token);
    auto result=std::make_shared<PreparedNavigationGeometry>();
    auto job=std::make_unique<PreparedBodyJob>(stroke.config);job->replay_=replay;job->budget_=budget;
    job->emitted_.patches.reserve(extra);job->emitted_.caps.reserve(extra);
    if(replay){result->replay_.reserve(blocks);
        result->replayOutput_.patches.reserve(extra);result->replayOutput_.caps.reserve(extra);
    }
    else {result->ribbons_.reserve(count+extra);result->caps_.reserve(count+extra);
        result->ribbonGroups_.reserve((count+extra+31)/32);result->capGroups_.reserve((count+extra+31)/32);}
    result->source_=source;result->sourceLeases_=std::move(sources);result->contour_=true;result->ready_=false;
    result->lease_=std::move(reservation);result->bytes_=estimate;result->usage_=usage;usage->bytes+=estimate;
    result->job_=std::move(job);return result;
}
bool PreparedBodyJob::advance(PreparedNavigationGeometry& body,PreparedGeometryStatistics& work){
    const auto& stroke=std::get<CompactStrokeReferenceCommand>(body.source_).content.get().stroke;
    const auto& knots=stroke.knots;
    ++work.advances;
    if(phase_==Phase::kMetrics){
        const auto end=std::min(cursor_+32,knots.size());
        for(auto i=cursor_;i<end;++i){const auto& k=knots[i];const auto radius=ink::compactRadius(stroke.config,k.pressure);
            body.support_.join(SkRect::MakeLTRB(float(k.x-radius),float(k.y-radius),float(k.x+radius),float(k.y+radius)));++work.supportKnots;
            if(i){body.replayDistance_+=std::hypot(k.x-knots[i-1].x,k.y-knots[i-1].y);++work.distanceInputs;}}
        work.maxAdvanceKnots=std::max<std::uint64_t>(work.maxAdvanceKnots,end-cursor_);cursor_=end;
        if(cursor_==knots.size()){cursor_=0;phase_=Phase::kCompile;}return false;
    }
    if(phase_==Phase::kCompile){
        emitted_.patches.clear();emitted_.caps.clear();blockKnot_=cursor_;blockCount_=std::min(std::size_t(32),knots.size()-cursor_);
        checkpoint_=compiler_;compiler_.append(std::span(knots).subspan(cursor_,blockCount_),body.replayDistance_,emitted_);
        cursor_+=blockCount_;if(cursor_==knots.size())compiler_.finish(body.replayDistance_,emitted_);
        work.compileInputs+=blockCount_;work.maxAdvanceKnots=std::max<std::uint64_t>(work.maxAdvanceKnots,blockCount_);
        ribbon_=cap_=0;blockBounds_=SkRect::MakeEmpty();phase_=Phase::kPaths;return false;
    }
    if(phase_==Phase::kPaths){
        const auto join=[&](SkRect& target,SkRect b){if(b.isEmpty()||!b.isFinite()){
            const auto m=std::numeric_limits<float>::max();target=SkRect::MakeLTRB(-m,-m,m,m);
        }else target.join(b);};
        const auto group=[&](auto& groups,std::size_t index,SkRect b){
            if(index%32==0)groups.push_back({SkRect::MakeEmpty(),index,index});
            auto& current=groups.back();join(current.bounds,b);current.end=index+1;};
        std::size_t units=0;
        for(;ribbon_+1<emitted_.patches.size()&&units<32;++ribbon_,++units){
            auto path=compactContourRibbon(std::span(emitted_.patches).subspan(ribbon_,2));join(blockBounds_,path.getBounds());
            if(!replay_){group(body.ribbonGroups_,body.ribbons_.size(),path.getBounds());pathBytes_+=path.approximateBytesUsed();body.ribbons_.push_back(std::move(path));}++work.pathPrimitives;
        }
        if(ribbon_+1<emitted_.patches.size())return false;
        for(;cap_<emitted_.caps.size()&&units<32;++cap_,++units){const auto& cap=emitted_.caps[cap_];const auto bounds=compactContourCapBounds(cap);join(blockBounds_,bounds);
            if(!replay_){group(body.capGroups_,body.caps_.size(),bounds);body.caps_.push_back(cap);}++work.pathPrimitives;}
        if(cap_<emitted_.caps.size())return false;
        const auto emitted=(emitted_.patches.empty()?0:emitted_.patches.size()-1)+emitted_.caps.size();
        if(replay_)body.replay_.push_back({*checkpoint_,blockKnot_,blockCount_,primitive_,primitive_+emitted,blockBounds_});primitive_+=emitted;
        if(cursor_!=knots.size()){phase_=Phase::kCompile;return false;}
        // All source and path/bounds work was charged above. No final footprint
        // traversal or whole-history constructor is hidden in publication.
        const auto bytes=sizeof(body)+512+body.ribbons_.capacity()*sizeof(SkPath)+pathBytes_+
            body.caps_.capacity()*sizeof(ink::ContourCap)+(body.ribbonGroups_.capacity()+body.capGroups_.capacity())*sizeof(PreparedNavigationGeometry::Group)+
            body.replay_.capacity()*(sizeof(PreparedNavigationGeometry::ReplayBlock)+512)+
            body.replayOutput_.patches.capacity()*sizeof(ink::ContourPatch)+body.replayOutput_.caps.capacity()*sizeof(ink::ContourCap);
        if(bytes>body.bytes_){body.failed_=true;return true;}
        // Shrink from the already admitted peak using the accumulated footprint;
        // no end-of-job scan. Recorded CPU geometry has no pending GPU use.
        // Shrink the already admitted CPU-only lease in place. Releasing then
        // reserving at the optional soft cap could fail after unrelated
        // critical consumers arrived, despite reducing this job's footprint.
        const auto previous=body.bytes_;
        if(!body.lease_.shrink(bytes)){body.failed_=true;return true;}
        body.bytes_=bytes;body.usage_->bytes-=previous-bytes;
        body.ready_=true;phase_=Phase::kDone;++work.builds;return true;
    }
    return true;
}
}
