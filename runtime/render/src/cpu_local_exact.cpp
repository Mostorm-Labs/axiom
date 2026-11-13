#include "cpu_local_exact.hpp"
#include "include/pathops/SkPathOps.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <bit>

namespace canvas::render::internal {
namespace {
constexpr std::size_t pieceSize=32;
std::uint32_t word32(std::span<const std::uint8_t> b,std::size_t p){std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(b[p+i])<<(8*i);return v;}
std::uint64_t word64(std::span<const std::uint8_t> b,std::size_t p){std::uint64_t v=0;for(unsigned i=0;i<8;++i)v|=std::uint64_t(b[p+i])<<(8*i);return v;}
// This is a range witness in addition to complete retained-value ownership and
// exact job token matching, never the sole proof of geometry/cache equality.
void witness(std::array<std::uint32_t,3>& h,double v){const auto n=std::bit_cast<std::uint64_t>(v);for(unsigned i=0;i<8;++i){h[0]=(h[0]^std::uint8_t(n>>(8*i)))*16777619U;h[1]=(h[1]+std::uint8_t(n>>(8*i)))*2246822519U;h[2]=std::rotl(h[2],7)^std::uint8_t(n>>(8*i));}}
SkRect conservative(const semantic::EraseCubicSegment& s){
    const float width=float(2*std::max(s.p0.radius,s.p1.radius));
    auto bounds=SkRect::MakeLTRB(std::min({float(s.p0.position.x),float(s.p1.position.x),float(s.control1.x),float(s.control2.x)}),
        std::min({float(s.p0.position.y),float(s.p1.position.y),float(s.control1.y),float(s.control2.y)}),
        std::max({float(s.p0.position.x),float(s.p1.position.x),float(s.control1.x),float(s.control2.x)}),
        std::max({float(s.p0.position.y),float(s.p1.position.y),float(s.control1.y),float(s.control2.y)}));
    bounds.outset(width*.5F,width*.5F);
    const auto inf=std::numeric_limits<float>::infinity();
    return SkRect::MakeLTRB(std::nextafter(bounds.left(),-inf),std::nextafter(bounds.top(),-inf),std::nextafter(bounds.right(),inf),std::nextafter(bounds.bottom(),inf));
}
bool disjoint(const SkRect& a,const SkRect& b){return a.isFinite()&&b.isFinite()&&!a.isEmpty()&&!b.isEmpty()&&!SkRect::Intersects(a,b);}
}
struct LocalMaskIndex final {
    struct Piece {SkRect bounds{};SkPath path;bool ready=false;resources::ResourceLease lease;};
    semantic::EraseMaskGeometry geometry;
    std::vector<Piece> pieces;
    resources::ResourceLease lease;
    std::size_t cursor=0;
    std::array<std::uint32_t,3> digest{2166136261U,0x9e3779b9U,0x85ebca6bU};
    std::array<std::uint8_t,12> stamp{};
    SkPathBuilder filled;
    bool ready=false,invalid=false;
    bool advance(){
        if(ready||invalid)return ready;
        if(const auto* sweep=std::get_if<semantic::SweptCircleMask>(&geometry)){
            const auto end=std::min(cursor+pieceSize,sweep->segments.size());
            for(;cursor<end;++cursor){const auto& s=sweep->segments[cursor];
                for(double v:{s.p0.position.x,s.p0.position.y,s.p0.radius,s.p1.position.x,s.p1.position.y,s.p1.radius,s.control1.x,s.control1.y,s.control2.x,s.control2.y}){
                    if(!std::isfinite(v)||!std::isfinite(float(v))){invalid=true;return false;}witness(digest,v);
                }
                if(s.p0.radius<0||s.p1.radius<0||!std::isfinite(float(2*std::max(s.p0.radius,s.p1.radius)))){invalid=true;return false;}
                auto b=conservative(s);auto& p=pieces[cursor/pieceSize];
                if(cursor%pieceSize==0)p.bounds=b;else p.bounds.join(b);
            }
            ready=cursor==sweep->segments.size();
        }else{
            const auto& path=std::get<semantic::FilledPathMask>(geometry).path;
            if(!cursor)filled.setFillType(path.fill_rule==semantic::FillRule::kEvenOdd?SkPathFillType::kEvenOdd:SkPathFillType::kWinding);
            const auto end=std::min(cursor+pieceSize,path.commands.size());
            for(;cursor<end;++cursor)std::visit([&](const auto& command){using T=std::decay_t<decltype(command)>;
                const auto point=[&](semantic::Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(float(p.x))&&std::isfinite(float(p.y));};
                if constexpr(std::is_same_v<T,semantic::MoveTo>){if(!point(command.point)){invalid=true;return;}filled.moveTo(float(command.point.x),float(command.point.y));}
                else if constexpr(std::is_same_v<T,semantic::LineTo>){if(!point(command.end)){invalid=true;return;}filled.lineTo(float(command.end.x),float(command.end.y));}
                else if constexpr(std::is_same_v<T,semantic::QuadTo>){if(!point(command.control)||!point(command.end)){invalid=true;return;}filled.quadTo(float(command.control.x),float(command.control.y),float(command.end.x),float(command.end.y));}
                else if constexpr(std::is_same_v<T,semantic::CubicTo>){if(!point(command.control1)||!point(command.control2)||!point(command.end)){invalid=true;return;}filled.cubicTo(float(command.control1.x),float(command.control1.y),float(command.control2.x),float(command.control2.y),float(command.end.x),float(command.end.y));}
                else filled.close();
            },path.commands[cursor]);
            if(invalid)return false;
            ready=cursor==path.commands.size();
            if(ready){auto built=filled.detach();if(!built.isFinite()){invalid=true;return false;}
                pieces[0].bounds=built.getBounds();pieces[0].path=built;pieces[0].ready=true;}
        }
        if(ready)for(unsigned i=0;i<12;++i)stamp[i]=static_cast<std::uint8_t>(digest[i/4]>>((i%4)*8));
        return ready;
    }
};

std::shared_ptr<LocalMaskIndex> LocalExactCache::request(const semantic::EraseMaskGeometry& geometry,
    const std::shared_ptr<resources::ResourceBudgetManager>& budget,std::size_t capacity){
    for(const auto& p:entries_)if(p->geometry==geometry)return p;
    const auto* sweep=std::get_if<semantic::SweptCircleMask>(&geometry);
    const auto count=sweep?sweep->segments.size():std::get<semantic::FilledPathMask>(geometry).path.commands.size();
    const auto pieces=sweep?std::max<std::size_t>(1,(count+pieceSize-1)/pieceSize):1;
    const auto bytes=sizeof(LocalMaskIndex)+256+count*(sweep?sizeof(semantic::EraseCubicSegment):sizeof(semantic::PathCommand)+256)+pieces*sizeof(LocalMaskIndex::Piece);
    if(bytes>capacity)return {};
    auto grant=budget->reserve(resources::ResourceClass::kRecorded,bytes,true);
    if(!grant){std::erase_if(entries_,[](const auto& p){return p.use_count()==1;});grant=budget->reserve(resources::ResourceClass::kRecorded,bytes,true);}
    if(!grant)return {};
    resources::ResourceLease admission(budget,*grant);
    if(entries_.size()>=512){auto old=std::find_if(entries_.begin(),entries_.end(),[](const auto& p){return p.use_count()==1;});if(old==entries_.end())return {};entries_.erase(old);}
    auto entry=std::make_shared<LocalMaskIndex>();entry->lease=std::move(admission);entry->geometry=geometry;entry->pieces.resize(pieces);
    if(!count){entry->ready=true;entry->pieces[0].ready=true;}
    entries_.push_back(entry);return entry;
}
void LocalExactCache::clear()noexcept{entries_.clear();}
void LocalExactCache::reclaimUnused()noexcept{std::erase_if(entries_,[](const auto& p){return p.use_count()==1;});}

LocalExactJob::LocalExactJob(const RuntimeSceneRecord& basis,std::shared_ptr<LocalMaskIndex> query,
    LocalExactCache& cache,PreparedGeometryCache& bodies,const RenderCacheOptions& limits,
    std::shared_ptr<resources::ResourceBudgetManager> budget):basis_(basis),query_(std::move(query)),bodies_(bodies),limits_(limits),budget_(std::move(budget)),cache_(cache){
    bodyEntry_.record.content=basis.content;
    bodyEntry_.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(basis.content)};
    const auto bytes=sizeof(LocalExactJob)+4096+basis.eraseMasks.size()*(sizeof(std::shared_ptr<LocalMaskIndex>)+sizeof(SkPath)+sizeof(resources::ResourceLease));
    auto token=budget_->reserve(resources::ResourceClass::kRecorded,bytes,true);if(!token)return;
    metadata_=resources::ResourceLease(budget_,*token);
    admitted_=true;
    history_.reserve(basis.eraseMasks.size());fallbackPaths_.reserve(basis.eraseMasks.size());fallbackPathLeases_.reserve(basis.eraseMasks.size());
    for(const auto& m:basis.eraseMasks){auto p=cache.request(m.geometry,budget_,limits.maxEntryBytes);if(!p)return;history_.push_back(std::move(p));}
    bodyEntry_.record.content=basis.content;
    bodyEntry_.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(basis.content)};
}
bool LocalExactJob::matches(const RuntimeSceneRecord& r,const semantic::EraseMaskGeometry& geometry)const noexcept{
    return query_&&basis_.content.shared()==r.content.shared()&&basis_.eraseMasks.storage()==r.eraseMasks.storage()&&
        basis_.objectId==r.objectId&&basis_.transform==r.transform&&basis_.kindVersion==r.kindVersion&&query_->geometry==geometry;
}
const semantic::EraseMaskGeometry& LocalExactJob::fullGeometry()const noexcept{return query_->geometry;}

LocalExactStatus LocalExactJob::advance(PreparationAllowance& allowance){
    if(status_==LocalExactStatus::kHit||status_==LocalExactStatus::kMiss||status_==LocalExactStatus::kInvalid||status_==LocalExactStatus::kNeedFull)return status_;
    if(!admitted_){
        const auto bytes=sizeof(LocalExactJob)+4096+basis_.eraseMasks.size()*(sizeof(std::shared_ptr<LocalMaskIndex>)+sizeof(SkPath)+sizeof(resources::ResourceLease));
        auto token=budget_->reserve(resources::ResourceClass::kRecorded,bytes,true);if(!token){cache_.reclaimUnused();token=budget_->reserve(resources::ResourceClass::kRecorded,bytes,true);}if(!token)return LocalExactStatus::kPending;
        metadata_=resources::ResourceLease(budget_,*token);
        admitted_=true;
        history_.reserve(basis_.eraseMasks.size());fallbackPaths_.reserve(basis_.eraseMasks.size());fallbackPathLeases_.reserve(basis_.eraseMasks.size());
    }
    if(!query_)return LocalExactStatus::kPending;
    while(history_.size()!=basis_.eraseMasks.size()){
        auto p=cache_.request(basis_.eraseMasks[history_.size()].geometry,budget_,limits_.maxEntryBytes);
        if(!p)return LocalExactStatus::kPending;
        history_.push_back(std::move(p));
    }
    while(validating_<=history_.size()){
        if(allowance.exhausted())return LocalExactStatus::kPending;
        auto& p=validating_==0?query_:history_[validating_-1];
        if(!p->ready){++allowance.steps;(void)p->advance();work_.validated+=pieceSize;if(p->invalid)return status_=LocalExactStatus::kInvalid;}
        if(p->ready)++validating_;
    }
    if(!body_)body_=bodies_.request(bodyEntry_,limits_,budget_);
    if(!body_)return LocalExactStatus::kPending;
    while(!body_->ready()){
        if(allowance.exhausted())return LocalExactStatus::kPending;
        bodies_.advance(body_,allowance);
    }
    if(!body_->exactValidated())return status_=LocalExactStatus::kInvalid;
    const auto& config=std::get<semantic::CompactStrokeContent>(basis_.content.get()).stroke.config;
    if(config.algorithmVersion!=4||(config.family!=foundation::InkFamily::kRound&&config.family!=foundation::InkFamily::kMarker))return status_=LocalExactStatus::kInvalid;
    if(config.opacity*config.color[3]<=0)return status_=LocalExactStatus::kMiss;
    // Equality is a complete-value no-op proof after full input validation.
    if(!selection_)for(const auto& old:history_)if(old->geometry==query_->geometry)return status_=LocalExactStatus::kMiss;
    const auto need=[&](const std::shared_ptr<LocalMaskIndex>& p,std::size_t i){waiting_=p;waitingPiece_=i;status_=LocalExactStatus::kNeedPiece;};
    while(queryPiece_<query_->pieces.size()){
        if(allowance.exhausted())return LocalExactStatus::kPending;
        auto& q=query_->pieces[queryPiece_];
        auto demand=q.bounds;if(selection_)demand.join(selectionContact_.getBounds());
        if(disjoint(body_->support(),demand)){++queryPiece_;component_=0;continue;}
        if(!q.ready){need(query_,queryPiece_);return status_;}
        if(!haveOverlap_){
            bool found=false;std::size_t at=0;SkPath part;ExactComponentWork local;
            body_->visitExact(demand,[&](const SkPath& path){if(at++<component_)return true;part=path;found=true;return false;},local);
            if(!found){++queryPiece_;component_=0;continue;}
            ++allowance.steps;
            if(selection_)overlap_=part;
            else{auto overlap=Op(part,q.path,kIntersect_SkPathOp);if(!overlap){++work_.fullFallbacks;return status_=LocalExactStatus::kNeedFull;}overlap_=*overlap;}
            haveOverlap_=true;historyMask_=0;historyPiece_=0;
        }
        while(historyMask_<history_.size()&&!overlap_.isEmpty()){
            auto& old=history_[historyMask_];
            while(historyPiece_<old->pieces.size()&&!overlap_.isEmpty()){
                if(allowance.exhausted())return LocalExactStatus::kPending;
                auto& p=old->pieces[historyPiece_];
                if(disjoint(overlap_.getBounds(),p.bounds)){++historyPiece_;continue;}
                if(!p.ready){need(old,historyPiece_);return status_;}
                ++allowance.steps;
                auto remaining=Op(overlap_,p.path,kDifference_SkPathOp);
                if(!remaining){++work_.fullFallbacks;return status_=LocalExactStatus::kNeedFull;}
                overlap_=*remaining;++historyPiece_;
            }
            if(historyPiece_==old->pieces.size()){++historyMask_;historyPiece_=0;}
        }
        if(!overlap_.isEmpty()){
            if(!selection_)return status_=LocalExactStatus::kHit;
            for(const auto* qpath:{&q.path,&selectionContact_}){
                const auto hit=Op(overlap_,*qpath,kIntersect_SkPathOp);
                if(!hit){++work_.fullFallbacks;return status_=LocalExactStatus::kNeedFull;}
                if(!hit->isEmpty())return status_=LocalExactStatus::kHit;
            }
        }
        haveOverlap_=false;++component_;
    }
    return status_=LocalExactStatus::kMiss;
}
std::vector<std::uint8_t> LocalExactJob::encode(const std::array<std::uint64_t,8>& token){
    if(status_!=LocalExactStatus::kNeedPiece||!waiting_||!waiting_->ready)return {};
    const auto* sweep=std::get_if<semantic::SweptCircleMask>(&waiting_->geometry);if(!sweep||sweep->segments.size()>std::numeric_limits<std::uint32_t>::max()||waitingPiece_>std::numeric_limits<std::uint32_t>::max()/pieceSize)return {};
    CpuRequest request;request.identity.words=token;request.piece=true;request.fullCount=static_cast<std::uint32_t>(sweep->segments.size());request.first=static_cast<std::uint32_t>(waitingPiece_*pieceSize);request.fullWitness=waiting_->stamp;
    semantic::SweptCircleMask subset;subset.segments.assign(sweep->segments.begin()+request.first,
        sweep->segments.begin()+std::min<std::size_t>(request.first+pieceSize,sweep->segments.size()));request.geometry=std::move(subset);
    token_=token;request_=cpuEncodeRequest(request);++work_.pieceRequests;work_.pieceUnions+=std::get<semantic::SweptCircleMask>(request.geometry).segments.size();return request_;
}
bool LocalExactJob::import(std::span<const std::uint8_t> bytes,const std::array<std::uint64_t,8>& token,bool* resourcePending){
    if(resourcePending)*resourcePending=false;
    if(token!=token_||request_.empty()||bytes.size()<128||bytes.size()>cpuOutputLimit||
       word32(bytes,0)!=0x31525043||word32(bytes,4)!=1||word32(bytes,100)!=3||word32(bytes,96)!=bytes.size()-128||
       word32(bytes,8)!=word32(request_,8)||word32(bytes,12)!=word32(request_,12)||
       !std::equal(bytes.begin()+16,bytes.begin()+96,request_.begin()+16)||!std::equal(bytes.begin()+104,bytes.begin()+128,request_.begin()+104))return false;
    for(unsigned i=0;i<8;++i)if(word64(bytes,32+i*8)!=token[i])return false;
    if(!waiting_||waitingPiece_>=waiting_->pieces.size())return false;
    // Bounded32-segment PathOps output is at most the existing1MiB envelope;
    // reserve before decoding and retain the exact actual path footprint.
    auto grant=budget_->reserve(resources::ResourceClass::kRecorded,2U*1024U*1024U,true);
    if(!grant){cache_.reclaimUnused();grant=budget_->reserve(resources::ResourceClass::kRecorded,2U*1024U*1024U,true);}
    if(!grant){if(resourcePending)*resourcePending=true;return false;}
    resources::ResourceLease lease(budget_,*grant);auto path=cpuDecodePath(bytes.subspan(128));if(!path)return false;
    const auto footprint=path->approximateBytesUsed()+256;if(footprint>2U*1024U*1024U||!lease.shrink(footprint))return false;
    auto& piece=waiting_->pieces[waitingPiece_];piece.path=*path;piece.ready=true;piece.lease=std::move(lease);
    waiting_.reset();request_.clear();status_=LocalExactStatus::kPending;return true;
}
void LocalExactJob::setSelectionContact(const SkPath& path){selection_=true;selectionContact_=path;}
void LocalExactJob::cancel()noexcept{status_=LocalExactStatus::kInvalid;request_.clear();waiting_.reset();localPiece_.reset();localPieceLease_.reset();fallbackJob_.reset();fallbackLease_.reset();}
bool LocalExactJob::advanceSelectionFallback(PreparationAllowance& allowance){
    if(!selection_||status_!=LocalExactStatus::kNeedFull)return false;
    while(fallbackPaths_.size()<history_.size()){
        if(allowance.exhausted())return false;
        auto& old=history_[fallbackPaths_.size()];
        if(!fallbackJob_){
            const auto count=std::visit([](const auto& g)->std::size_t{if constexpr(requires{g.segments;})return g.segments.size();else return g.path.commands.size();},old->geometry);
            auto estimate=cpuConstructionEstimate(count,128+count*80,std::holds_alternative<semantic::SweptCircleMask>(old->geometry));
            if(!estimate){status_=LocalExactStatus::kInvalid;return false;}
            auto token=budget_->reserve(resources::ResourceClass::kScratch,*estimate,true);if(!token)return false;
            fallbackLease_=resources::ResourceLease(budget_,*token);fallbackJob_=std::make_unique<CpuExactPathJob>(old->geometry);++work_.fullFallbacks;
        }
        ++allowance.steps;const auto before=fallbackJob_->cursor();
        if(!fallbackJob_->advance(8,2000)){status_=LocalExactStatus::kInvalid;return false;}
        work_.pieceUnions+=fallbackJob_->cursor()-before;
        if(!fallbackJob_->ready())continue;
        const auto bytes=fallbackJob_->path().approximateBytesUsed()+128;
        auto token=budget_->reserve(resources::ResourceClass::kRecorded,bytes,true);if(!token)return false;
        resources::ResourceLease lease(budget_,*token);
        fallbackPaths_.push_back(fallbackJob_->path());fallbackPathLeases_.push_back(std::move(lease));
        fallbackJob_.reset();fallbackLease_.reset();
    }
    auto demand=query_->pieces[0].bounds;demand.join(selectionContact_.getBounds());
    while(!allowance.exhausted()){
        if(!fallbackOverlap_){
            bool found=false;std::size_t at=0;ExactComponentWork local;
            body_->visitExact(demand,[&](const SkPath& part){if(at++<fallbackComponent_)return true;overlap_=part;found=true;return false;},local);
            if(!found){status_=LocalExactStatus::kMiss;return true;}
            fallbackHistory_=0;fallbackOverlap_=true;
        }
        while(fallbackHistory_<fallbackPaths_.size()){
            if(allowance.exhausted())return false;++allowance.steps;
            const auto remaining=Op(overlap_,fallbackPaths_[fallbackHistory_++],kDifference_SkPathOp);
            if(!remaining){status_=LocalExactStatus::kInvalid;return false;}overlap_=*remaining;
        }
        for(const auto* query:{&query_->pieces[0].path,&selectionContact_}){
            const auto hit=Op(overlap_,*query,kIntersect_SkPathOp);
            if(!hit){status_=LocalExactStatus::kInvalid;return false;}
            if(!hit->isEmpty()){status_=LocalExactStatus::kHit;return true;}
        }
        ++fallbackComponent_;fallbackOverlap_=false;++allowance.steps;
    }
    return false;
}
bool LocalExactJob::advanceLocalPiece(PreparationAllowance& allowance){
    if(status_!=LocalExactStatus::kNeedPiece||!waiting_)return false;
    if(!localPiece_){
        auto grant=budget_->reserve(resources::ResourceClass::kScratch,256U*1024U,true);if(!grant){cache_.reclaimUnused();grant=budget_->reserve(resources::ResourceClass::kScratch,256U*1024U,true);}if(!grant)return false;
        localPieceLease_=resources::ResourceLease(budget_,*grant);
        const auto* sweep=std::get_if<semantic::SweptCircleMask>(&waiting_->geometry);if(!sweep)return false;
        semantic::SweptCircleMask subset;const auto first=waitingPiece_*pieceSize;
        subset.segments.assign(sweep->segments.begin()+first,sweep->segments.begin()+std::min(first+pieceSize,sweep->segments.size()));
        localPiece_=std::make_unique<CpuExactPathJob>(std::move(subset));++work_.pieceRequests;
    }
    if(allowance.exhausted())return false;
    const auto before=localPiece_->cursor();++allowance.steps;
    if(!localPiece_->advance(8,2000)){status_=LocalExactStatus::kNeedFull;return false;}
    work_.pieceUnions+=localPiece_->cursor()-before;
    if(!localPiece_->ready())return false;
    const auto bytes=localPiece_->path().approximateBytesUsed()+256;
    auto token=budget_->reserve(resources::ResourceClass::kRecorded,bytes,true);if(!token)return false;
    auto& p=waiting_->pieces[waitingPiece_];p.path=localPiece_->path();p.ready=true;p.lease=resources::ResourceLease(budget_,*token);
    localPiece_.reset();localPieceLease_.reset();waiting_.reset();status_=LocalExactStatus::kPending;return true;
}
}
