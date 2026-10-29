#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/interaction/region_selection_geometry.hpp"
#include "canvas/text/text_layout.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace canvas::interaction {
bool CanvasEditingRuntime::renderAcquisitionOverlay() noexcept {
    if (!surface_.available||!sceneCoordinator_||!runtimeSceneHost_||surface_.logicalWidth<=0||surface_.logicalHeight<=0) {
        selectionView_.reset();return false;
    }
    const auto viewport=viewportController_->state();
    const float zoom=viewport.scale;
    const render::FrameState frame{viewId_,
        render::CameraState{{(surface_.logicalWidth*.5F-viewport.translationX)/zoom,
                             (surface_.logicalHeight*.5F-viewport.translationY)/zoom},zoom,0,
                             render::CameraGeneration{cameraGeneration()}},
        {-1e9F,-1e9F,1e9F,1e9F},
        {surface_.logicalWidth,surface_.logicalHeight,surface_.width,surface_.height,surface_.devicePixelRatio,surface_.displayScale},
        semanticGeneration_.current(),runtimeSceneHost_->revision(),render::SurfaceGeneration{surface_.generation},
        render::MetricsGeneration{surface_.metricsGeneration},render::FrameId{nextCanonicalFrameId()},
        {0,0,surface_.logicalWidth,surface_.logicalHeight}};
    selectionView_=std::make_unique<render::RenderViewRuntime>(frame);
    auto& overlay=selectionView_->editingOverlay();
    const auto append=[&](auto ids,auto& outlines) {
        for (auto id:ids) {
            const auto* r=sceneCoordinator_->runtimeScene().find(id);if(!r)continue;
            const auto& b=r->visualBounds;
            const auto* transient=transformOverrides_.find(id);
            const auto& t=transient?*transient:r->transform;
            const auto map=[&](float x,float y) {return render::ScreenPoint{
                static_cast<float>((t.a*x+t.c*y+t.tx)*zoom+viewport.translationX),
                static_cast<float>((t.b*x+t.d*y+t.ty)*zoom+viewport.translationY)};};
            outlines.push_back(render::SelectionOutline{{map(b.left,b.top),map(b.right,b.top),map(b.right,b.bottom),map(b.left,b.bottom)},{},true});
        }
    };
    append(selection_.selected(),overlay.readOnlyOutlines);
    append(regionCandidates_,overlay.candidateOutlines);
    if (regionSelection_.active()&&regionSelection_.dragged())
        for (auto p:regionSelection_.outline()) overlay.acquisitionOutline.push_back({static_cast<float>(p.x),static_cast<float>(p.y)});
    ++selectionOverlayUpdates_;return true;
}
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
foundation::Result<HitTestResult> CanvasEditingRuntime::selectionPointHits(foundation::WorldPoint point) const {
    if(!runtimeSceneHost_||!historySceneReady())return foundation::Result<HitTestResult>::failure(
        {foundation::ErrorCode::kInvalidRevision,"Selection requires a coherent published Scene"});
    auto result=runtimeSceneHost_->hitTest({point,4,{HitTestKindMask::kAll,true},std::max<std::size_t>(1,semanticObjects_.size())});
    if(!result)return result;
    auto& ids=result.value().frontToBack;
    ids.erase(std::remove_if(ids.begin(),ids.end(),[&](auto id){return resolveSelectionRoot(id).isZero();}),ids.end());
    return result;
}
bool CanvasEditingRuntime::enterGroupAtViewPoint(float x,float y) {
    if (!selectionMode_ || selectionAcquisitionMode_!=runtime::CanvasSelectionMode::kPoint ||
        !std::isfinite(x) || !std::isfinite(y)) return false;
    const auto world=viewportController_->viewToContent(x,y);
    const auto hit=selectionPointHits({world.first,world.second});
    if (!hit || hit.value().frontToBack.empty()) return false;
    const auto id=hit.value().frontToBack.front();
    const auto root=resolveSelectionRoot(id);
    const auto* object=semanticObjects_.find(root);
    if (!object || object->kind!=semantic::ObjectKind::kGroup) return false;
    auto after=selection_;
    if (!after.enterGroupScope(root,semanticObjects_)) return false;
    const auto child=after.resolveHit(id,semanticObjects_);
    if (!child.isZero())(void)after.click(child);
    selection_=std::move(after);selectionView_.reset();canonicalPresentationDirty_=true;
    return true;
}
bool CanvasEditingRuntime::publishSelectionAcquisition(std::span<const foundation::ObjectId> acquired,
    SelectionCombineMode mode,std::uint64_t expectedRevision) {
    if (selection_.revision()!=expectedRevision) return false;
    std::vector<foundation::ObjectId> combined;
    if(mode==SelectionCombineMode::kAdd)combined.assign(selection_.selected().begin(),selection_.selected().end());
    combined.insert(combined.end(),acquired.begin(),acquired.end());
    const auto roots=selection_.resolveRoots(combined,semanticObjects_);
    return roots&&selection_.applyAcquisition(*roots,SelectionCombineMode::kReplace,expectedRevision);
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
    // The transient surface contains a full scene. Retire it only after a
    // replacement canonical submission, including invalidation during rendering.
    // Do not clear here: renderSelectionOverlay may run under preview locks.
    if (regionSelection_.active()) transientOverlayHandoffPending_=true;
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
    std::vector<foundation::ObjectId> sectionBackgrounds;
    // Missing text metrics imply unknown extent, so the spatial index cannot
    // prove NoHit. Keep this explicitly incomplete until the font/layout arrives.
    for(const auto& r:projected.records()) {
        if(r.kind!=semantic::ObjectKind::kRichText||(r.textLayout&&!r.textLayout->lines.empty()&&r.textLayout->diagnostics.empty())||
            !textWritable(r.objectId)||resolveSelectionRoot(r.objectId).isZero())continue;
        if(r.placement.parent_id)if(const auto* parent=semanticObjects_.find(*r.placement.parent_id);
            parent&&parent->kind==semantic::ObjectKind::kSticky)continue;
        const auto* text=std::get_if<semantic::RichTextContent>(&r.content);if(!text){result.status=Status::kFailed;return result;}
        const bool hasText=std::any_of(text->document.paragraphs.begin(),text->document.paragraphs.end(),[](const auto& paragraph){
            return std::any_of(paragraph.runs.begin(),paragraph.runs.end(),[](const auto& run){return !run.text.empty();});
        });
        if(hasText) {
            result.status=Status::kPartial;
            const auto kind=static_cast<std::uint32_t>(r.kind);
            if(std::find(result.unsupportedKinds.begin(),result.unsupportedKinds.end(),kind)==result.unsupportedKinds.end())result.unsupportedKinds.push_back(kind);
        }
    }
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
        const auto intersection=intersectSelectionRegion(object,world,projection);
        if (intersection==RegionIntersection::kInvalid) {result.status=Status::kFailed;return result;}
        if (intersection==RegionIntersection::kUnsupported) {
            result.status=Status::kPartial;
            const auto kind=static_cast<std::uint32_t>(object.kind);
            if (std::find(result.unsupportedKinds.begin(),result.unsupportedKinds.end(),kind)==result.unsupportedKinds.end())
                result.unsupportedKinds.push_back(kind);
        } else if(intersection==RegionIntersection::kHit) {
            auto& targets=object.kind==semantic::ObjectKind::kSection&&selectionRegionInsideSection(object,world)?sectionBackgrounds:result.roots;
            if(std::find(targets.begin(),targets.end(),root)==targets.end())targets.push_back(root);
        }
    }
    if(result.roots.empty())result.roots=std::move(sectionBackgrounds);
    const auto roots=selection_.resolveRoots(result.roots,semanticObjects_);
    if(!roots){result.status=Status::kStale;return result;}
    result.roots=*roots;
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
            if (query.status==Status::kComplete&&publishSelectionAcquisition(query.roots,result.combine,result.context.selectionRevision)) {
                snapGuides_.clear();
            } else if (query.status==Status::kComplete) regionQueryStatus_=Status::kStale;
        } else if (result.status==RegionFinishStatus::kClick) {
            regionQueryStatus_=publishSelectionAcquisition({},result.combine,result.context.selectionRevision)?Status::kComplete:Status::kStale;
        } else regionQueryStatus_=Status::kInvalid;
        canonicalPresentationDirty_=true;(void)clearTransientOverlay();(void)renderSelectionOverlay();return true;
    }
    if (!regionSelection_.update(sample)) {
        // Overflow/invalid coordinates may already have cancelled the leaf session.
        transientOverlayHandoffPending_=true;cancelRegionSelection();regionQueryStatus_=Status::kInvalid;return true;
    }
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
    if (sample.phase!=input::PointerPhase::kDown) {
        const bool clickOnly = sample.phase == input::PointerPhase::kUp && pendingMultiClick_ &&
            editingPointer_ == sample.key && !editingMultiInitial_.empty() &&
            std::abs(viewportController_->viewToContent(sample.x,sample.y).first-editingDownWorld_.x)<1e-6 &&
            std::abs(viewportController_->viewToContent(sample.x,sample.y).second-editingDownWorld_.y)<1e-6;
        const auto root = pendingMultiClick_;
        const bool accepted = selectionPointer(sample.key,sample.phase,sample.x,sample.y);
        if (sample.phase==input::PointerPhase::kUp||sample.phase==input::PointerPhase::kCancel) {
            pendingMultiClick_.reset();
            if (clickOnly && root) {
                const std::array roots{*root};
                (void)publishSelectionAcquisition(roots,SelectionCombineMode::kReplace,selection_.revision());
                canonicalPresentationDirty_=true;(void)renderSelectionOverlay();
            }
        }
        return accepted;
    }
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
    const auto hit=selectionPointHits({w.first,w.second});
    if (!hit) {regionQueryStatus_=runtime::SelectionQueryStatus::kFailed;return true;}
    if (hit.value().frontToBack.empty()) return regionSelectionPointer(sample);
    if (!input::hasShift(sample.modifiers) && selection_.selected().size()>1) {
        const auto root=resolveSelectionRoot(hit.value().frontToBack.front());
        if (!root.isZero() && std::find(selection_.selected().begin(),selection_.selected().end(),root)!=selection_.selected().end()) {
            pendingMultiClick_=root;
            if (!selectionPointer(sample.key,sample.phase,sample.x,sample.y)) {
                pendingMultiClick_.reset();acquisitionClickPointers_.insert(sample.key);
            }
            return true;
        }
    }
    if (input::hasShift(sample.modifiers)||selection_.selected().size()>1) {
        const auto root=resolveSelectionRoot(hit.value().frontToBack.front());
        if (!root.isZero()) {
            const std::array roots{root};
            (void)publishSelectionAcquisition(roots,input::hasShift(sample.modifiers)?SelectionCombineMode::kAdd:SelectionCombineMode::kReplace,selection_.revision());
        }
        acquisitionClickPointers_.insert(sample.key);canonicalPresentationDirty_=true;(void)renderSelectionOverlay();return true;
    }
    return selectionPointer(sample.key,sample.phase,sample.x,sample.y);
}
} // namespace canvas::interaction
