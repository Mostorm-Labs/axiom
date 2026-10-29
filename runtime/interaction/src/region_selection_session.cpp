#include "canvas/interaction/region_selection_session.hpp"

#include <algorithm>
#include <cmath>

namespace canvas::interaction {
bool validSelectionPolygon(std::span<const RegionPoint> points) noexcept {
    if (points.size()<3) return false;
    for (auto p:points) if (!std::isfinite(p.x)||!std::isfinite(p.y)) return false;
    const auto a=points.front();
    auto b=a;
    for (auto p:points) if (p!=a) {b=p;break;}
    for (auto p:points)
        if (std::abs((b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x))>1e-9) return true;
    return false;
}

bool RegionSelectionSession::begin(const input::PointerSample& sample,RegionSelectionKind kind,
    RegionSelectionContext context,std::span<const foundation::ObjectId> initial) {
    if (active_||!sample.key.valid()||sample.predicted||sample.phase!=input::PointerPhase::kDown||
        !std::isfinite(sample.x)||!std::isfinite(sample.y)) return false;
    active_=true;dragged_=false;key_=sample.key;sequence_=sample.sequence;kind_=kind;context_=context;
    combine_=input::hasShift(sample.modifiers)?SelectionCombineMode::kAdd:SelectionCombineMode::kReplace;
    down_={sample.x,sample.y};outline_={down_};initial_.assign(initial.begin(),initial.end());return true;
}
bool RegionSelectionSession::update(const input::PointerSample& sample) {
    if (!active_||sample.key!=key_) return false;
    if (sample.predicted) return true;
    if (sample.phase==input::PointerPhase::kCancel) {cancel();return true;}
    if (sample.sequence<=sequence_) return false;
    if (!std::isfinite(sample.x)||!std::isfinite(sample.y)) {cancel();return false;}
    sequence_=sample.sequence;
    const RegionPoint p{sample.x,sample.y};
    dragged_=dragged_||std::hypot(p.x-down_.x,p.y-down_.y)>=kDragThreshold;
    if (kind_==RegionSelectionKind::kFreeform) {
        if (outline_.back()!=p) {
            if (outline_.size()>=kMaximumPoints) {cancel();return false;}
            outline_.push_back(p);
        }
    } else {
        const double l=std::min(down_.x,p.x),r=std::max(down_.x,p.x);
        const double t=std::min(down_.y,p.y),b=std::max(down_.y,p.y);
        outline_={{l,t},{r,t},{r,b},{l,b}};
    }
    return true;
}
RegionSelectionResult RegionSelectionSession::finish(const input::PointerSample& sample) {
    if (!active_||sample.key!=key_||sample.predicted||sample.sequence<=sequence_||
        sample.phase!=input::PointerPhase::kUp) return {};
    if (!update(sample)) return {RegionFinishStatus::kCancelled,{},combine_,context_};
    auto status=RegionFinishStatus::kInvalid;
    if (!dragged_&&kind_==RegionSelectionKind::kPendingRectangle) status=RegionFinishStatus::kClick;
    else if (dragged_&&validSelectionPolygon(outline_)) status=RegionFinishStatus::kReady;
    RegionSelectionResult result{status,std::move(outline_),combine_,context_};
    cancel();return result;
}
void RegionSelectionSession::cancel() noexcept {
    active_=false;dragged_=false;key_={};outline_.clear();initial_.clear();
}
} // namespace canvas::interaction
