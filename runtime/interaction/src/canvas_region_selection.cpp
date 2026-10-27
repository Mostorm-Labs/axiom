#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/interaction/region_selection_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace canvas::interaction {
RegionSelectionContext CanvasEditingRuntime::regionContext() const noexcept {
    return {selection_.revision(),cameraGeneration(),surface_.generation,surface_.metricsGeneration,
            regionBinding_,selection_.groupScope()};
}
foundation::ObjectId CanvasEditingRuntime::resolveSelectionRoot(foundation::ObjectId id) const {
    if (const auto* child=semanticObjects_.find(id);child&&child->kind==semantic::ObjectKind::kRichText&&child->placement.parent_id) {
        if (const auto* parent=semanticObjects_.find(*child->placement.parent_id);parent&&parent->kind==semantic::ObjectKind::kSticky)
            id=parent->id;
    }
    return selection_.resolveHit(id,semanticObjects_);
}
bool CanvasEditingRuntime::setSelectionAcquisitionMode(runtime::CanvasSelectionMode mode) noexcept {
    if (mode>runtime::CanvasSelectionMode::kFreeform||closed_||imageCropPointer_||
        (interaction_->textSession()&&interaction_->textSession()->read().phase!=runtime::TextPhase::kClosed)) return false;
    if (!setSelectionMode(true)) return false;
    if (selectionAcquisitionMode_!=mode) {dispatchRuntime_.cancelAll();cancelRegionSelection();}
    selectionAcquisitionMode_=mode;panTool_=false;textTool_=false;
    canonicalPresentationDirty_=true;return true;
}
void CanvasEditingRuntime::cancelRegionSelection() noexcept {
    regionSelection_.cancel();regionCandidates_.clear();regionUnsupportedKinds_.clear();
    regionQueryStatus_=runtime::SelectionQueryStatus::kCancelled;
    ++regionBinding_;canonicalPresentationDirty_=true;
}
CanvasEditingRuntime::RegionQueryResult CanvasEditingRuntime::querySelectionRegion(
    std::span<const RegionPoint> points) const {
    using Status=runtime::SelectionQueryStatus;
    RegionQueryResult result;
    if (!runtimeSceneHost_||!sceneCoordinator_||!historySceneReady()) {result.status=Status::kStale;return result;}
    if (!validSelectionPolygon(points)) {result.status=Status::kInvalid;return result;}
    std::vector<RegionPoint> world;world.reserve(points.size());
    foundation::WorldRect bounds{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),
        std::numeric_limits<float>::lowest(),std::numeric_limits<float>::lowest()};
    for (auto p:points) {
        const auto w=viewportController_->viewToContent(static_cast<float>(p.x),static_cast<float>(p.y));
        if (!std::isfinite(w.first)||!std::isfinite(w.second)) {result.status=Status::kInvalid;return result;}
        world.push_back({w.first,w.second});bounds.left=std::min(bounds.left,w.first);bounds.top=std::min(bounds.top,w.second);
        bounds.right=std::max(bounds.right,w.first);bounds.bottom=std::max(bounds.bottom,w.second);
    }
    const auto queried=runtimeSceneHost_->query({bounds});
    if (!queried) return result;
    const auto& projected=sceneCoordinator_->runtimeScene();
    const auto generation=semanticGeneration_.current();
    if (runtimeSceneHost_->semanticGeneration()!=generation||projected.generation()!=generation) {result.status=Status::kStale;return result;}
    const auto read=runtimeSceneHost_->read();
    result.status=Status::kComplete;
    for (auto it=queried.value().backToFront.rbegin();it!=queried.value().backToFront.rend();++it) {
        const auto* record=read.find(*it);
        if (!record) {result.status=Status::kStale;return result;}
        const auto flags=static_cast<std::uint32_t>(record->flags);
        if ((flags&static_cast<std::uint32_t>(SceneRecordFlags::kVisible))==0||
            (flags&static_cast<std::uint32_t>(SceneRecordFlags::kLocked))!=0) continue;
        auto id=*it;
        const auto* canonical=semanticObjects_.find(id);
        if (!canonical) {result.status=Status::kStale;return result;}
        if (canonical->kind==semantic::ObjectKind::kRichText&&canonical->placement.parent_id) {
            if (const auto* parent=semanticObjects_.find(*canonical->placement.parent_id);parent&&parent->kind==semantic::ObjectKind::kSticky) {
                canonical=parent;id=parent->id;
            }
        }
        const auto root=resolveSelectionRoot(id);
        if (root.isZero()) continue;
        const auto* projection=projected.find(id);
        if (!projection) {result.status=Status::kStale;return result;}
        auto object=*canonical;object.transform=projection->transform;
        const auto intersection=intersectSelectionRegion(object,world);
        if (intersection==RegionIntersection::kInvalid) {result.status=Status::kFailed;return result;}
        if (intersection==RegionIntersection::kUnsupported) {
            result.status=Status::kPartial;
            const auto kind=static_cast<std::uint32_t>(object.kind);
            if (std::find(result.unsupportedKinds.begin(),result.unsupportedKinds.end(),kind)==result.unsupportedKinds.end())
                result.unsupportedKinds.push_back(kind);
        } else if (intersection==RegionIntersection::kHit&&std::find(result.roots.begin(),result.roots.end(),root)==result.roots.end())
            result.roots.push_back(root);
    }
    if (read.revision()!=queried.value().revision||runtimeSceneHost_->revision()!=read.revision()||semanticGeneration_.current()!=generation)
        result.status=Status::kStale;
    return result;
}
bool CanvasEditingRuntime::regionSelectionPointer(const input::PointerSample& sample) noexcept {
    using Status=runtime::SelectionQueryStatus;
    if (sample.phase==input::PointerPhase::kDown) {
        if (regionSelection_.active()) return true;
        const auto kind=selectionAcquisitionMode_==runtime::CanvasSelectionMode::kFreeform?RegionSelectionKind::kFreeform:
            selectionAcquisitionMode_==runtime::CanvasSelectionMode::kRectangle?RegionSelectionKind::kRectangle:RegionSelectionKind::kPendingRectangle;
        regionCandidates_.clear();regionUnsupportedKinds_.clear();regionQueryStatus_=Status::kIdle;
        return regionSelection_.begin(sample,kind,regionContext(),selection_.selected());
    }
    if (!regionSelection_.active()||sample.key!=regionSelection_.key()) return true;
    if (sample.phase==input::PointerPhase::kCancel) {cancelRegionSelection();(void)clearTransientOverlay();return true;}
    if (regionSelection_.context()!=regionContext()) {cancelRegionSelection();regionQueryStatus_=Status::kStale;return true;}
    if (sample.phase==input::PointerPhase::kUp) {
        const auto result=regionSelection_.finish(sample);
        if (result.status==RegionFinishStatus::kIgnored) return true;
        regionCandidates_.clear();regionUnsupportedKinds_.clear();
        if (result.status==RegionFinishStatus::kReady) {
            const auto query=querySelectionRegion(result.points);regionQueryStatus_=query.status;regionUnsupportedKinds_=query.unsupportedKinds;
            if (query.status==Status::kComplete&&selection_.applyAcquisition(query.roots,result.combine,result.context.selectionRevision)) {
                snapGuides_.clear();
            } else if (query.status==Status::kComplete) regionQueryStatus_=Status::kStale;
        } else if (result.status==RegionFinishStatus::kClick) {
            regionQueryStatus_=selection_.applyAcquisition({},result.combine,result.context.selectionRevision)?Status::kComplete:Status::kStale;
        } else regionQueryStatus_=Status::kInvalid;
        canonicalPresentationDirty_=true;(void)clearTransientOverlay();(void)renderSelectionOverlay();return true;
    }
    if (!regionSelection_.update(sample)) {cancelRegionSelection();regionQueryStatus_=Status::kInvalid;return true;}
    if (regionSelection_.dragged()) {
        const auto query=querySelectionRegion(regionSelection_.outline());regionCandidates_=query.roots;
        regionUnsupportedKinds_=query.unsupportedKinds;regionQueryStatus_=query.status;
    }
    canonicalPresentationDirty_=true;(void)renderSelectionOverlay();(void)presentTransientOverlayFrame(nextCanonicalFrameId());return true;
}
bool CanvasEditingRuntime::acquisitionPointer(const input::PointerSample& sample) noexcept {
    if (sample.predicted) return true;
    if (acquisitionClickPointers_.contains(sample.key)) {
        if (sample.phase==input::PointerPhase::kUp||sample.phase==input::PointerPhase::kCancel) acquisitionClickPointers_.erase(sample.key);
        return true;
    }
    if (regionSelection_.active()) return regionSelectionPointer(sample);
    if (sample.phase!=input::PointerPhase::kDown) return selectionPointer(sample.key,sample.phase,sample.x,sample.y);
    if (selectionAcquisitionMode_!=runtime::CanvasSelectionMode::kPoint) return regionSelectionPointer(sample);
    // Point mode keeps the existing handle/port owner priority.
    (void)renderSelectionOverlay();
    if (const auto* overlay=selectionOverlay()) {
        if (overlay->hitTest({sample.x,sample.y})!=render::HandleKind::kNone)
            return selectionPointer(sample.key,sample.phase,sample.x,sample.y);
        for (const auto& p:overlay->attachmentPoints())
            if (std::hypot(p.center.x-sample.x,p.center.y-sample.y)<=16)
                return selectionPointer(sample.key,sample.phase,sample.x,sample.y);
    }
    const auto w=viewportController_->viewToContent(sample.x,sample.y);
    const auto hit=runtimeSceneHost_->hitTest({{w.first,w.second},4,{HitTestKindMask::kAll,true},1});
    if (!hit) {regionQueryStatus_=runtime::SelectionQueryStatus::kFailed;return true;}
    if (hit.value().frontToBack.empty()) return regionSelectionPointer(sample);
    if (input::hasShift(sample.modifiers)||selection_.selected().size()>1) {
        const auto root=resolveSelectionRoot(hit.value().frontToBack.front());
        if (!root.isZero()) {
            const std::array roots{root};
            (void)selection_.applyAcquisition(roots,input::hasShift(sample.modifiers)?SelectionCombineMode::kAdd:SelectionCombineMode::kReplace,selection_.revision());
        }
        acquisitionClickPointers_.insert(sample.key);canonicalPresentationDirty_=true;(void)renderSelectionOverlay();return true;
    }
    return selectionPointer(sample.key,sample.phase,sample.x,sample.y);
}
} // namespace canvas::interaction
