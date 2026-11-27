#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/scene/hierarchy_geometry.hpp"
#include "cpu_pending_erase_state.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace canvas::interaction {
std::uint32_t CanvasEditingRuntime::siblingAttachmentPort(semantic::ObjectId sourceId,
                                                         std::uint32_t sourcePort) const noexcept {
    const auto* source = sceneCoordinator_ ? sceneCoordinator_->runtimeScene().find(sourceId) : nullptr;
    const auto pose = source ? source->transform : semantic::Transform2D{};
    const bool vertical = sourcePort == 1 || sourcePort == 3;
    const double sign = sourcePort == 1 || sourcePort == 4 ? -1.0 : 1.0;
    const double dx = sign * (vertical ? pose.c : pose.a);
    const double dy = sign * (vertical ? pose.d : pose.b);
    // New siblings are upright. Use their side facing the source's outward world direction.
    if (std::abs(dx) >= std::abs(dy)) return dx >= 0 ? 4U : 2U;
    return dy >= 0 ? 1U : 3U;
}
semantic::ObjectRecord CanvasEditingRuntime::makeConnectorSibling(semantic::ObjectId sourceId,
    std::uint32_t targetPort,foundation::WorldPoint end,semantic::ObjectRecord target) {
    const auto* source=semanticObjects_.find(sourceId);if(!source)return {};
    auto preset=CreationPreset::kRectangle;
    if(const auto* shape=std::get_if<semantic::ShapeContent>(&source->content))
        preset=shape->shape_kind==2?CreationPreset::kEllipse:shape->shape_kind==3?CreationPreset::kTriangle:CreationPreset::kRectangle;
    else if(source->kind==semantic::ObjectKind::kSticky)preset=CreationPreset::kSticky;
    if(target.id.isZero())target=makeCreationPrototype(preset,true);
    if(target.id.isZero())return {};
    double width=120,height=80;
    if(const auto* shape=std::get_if<semantic::ShapeContent>(&source->content)) {
        target.kind=semantic::ObjectKind::kShape;target.kind_version=shape->shape_kind==3?2:1;
        width=shape->width;height=shape->height;
        target.content=semantic::ShapeContent{shape->shape_kind,width,height};
    } else if(const auto* sticky=std::get_if<semantic::StickyContent>(&source->content)) {
        target.kind=semantic::ObjectKind::kSticky;target.kind_version=1;
        width=sticky->width;height=sticky->height;target.content=*sticky;
    }
    target.transform={};
    const bool triangle=target.kind==semantic::ObjectKind::kShape&&std::get<semantic::ShapeContent>(target.content).shape_kind==3;
    switch(targetPort) {
      case 1:target.transform.tx=end.x-width*.5;target.transform.ty=end.y;break;
      case 2:target.transform.tx=end.x-width*(triangle?.75:1);target.transform.ty=end.y-height*.5;break;
      case 3:target.transform.tx=end.x-width*.5;target.transform.ty=end.y-height;break;
      default:target.transform.tx=end.x-width*(triangle?.25:0);target.transform.ty=end.y-height*.5;break;
    }
    return target;
}
std::optional<scene::ConnectorGeometry> CanvasEditingRuntime::layoutConnectorSibling(
    const semantic::ObjectRecord& wire, semantic::ObjectRecord& sibling, std::uint32_t targetPort) const {
    // Top/bottom extensions share a row; left/right extensions share a column.
    const bool row = targetPort == 1 || targetPort == 3;
    const auto initial = scene::computeBounds(sibling);
    if (!initial.finite) return {};
    const auto& body = initial.geometry;
    const double span = row ? body.right-body.left : body.bottom-body.top;
    const double step = span + std::clamp(span/2, 24.0, 64.0);
    auto objects = semanticObjects_.allObjects();
    if (cpuPending_) {
        for (const auto& intent : cpuPending_->slots)
            if (intent && intent->serial)
                for (const auto& created : intent->creates)
                    if (!semanticObjects_.contains(created.id)) objects.push_back(created);
    }
    auto world = scene::projectWorldObjects(objects);
    const auto& base = initial.world;
    constexpr float clearance = 8;
    std::vector<std::pair<double,double>> occupied;
    for (const auto& item : world) {
        if (item.kind == semantic::ObjectKind::kGroup || item.kind == semantic::ObjectKind::kSection ||
            item.kind == semantic::ObjectKind::kConnector) continue;
        const bool hidden = std::any_of(item.properties.entries.begin(), item.properties.entries.end(),
            [](const auto& p) {return p.field_id==1 && std::get_if<bool>(&p.value) && !std::get<bool>(p.value);});
        if (hidden) continue;
        const auto* projected = sceneCoordinator_->runtimeScene().find(item.id);
        const auto b = projected ? projected->worldBounds : scene::computeBounds(item).world;
        if (row ? (b.bottom+clearance<=base.top || b.top-clearance>=base.bottom)
                : (b.right+clearance<=base.left || b.left-clearance>=base.right)) continue;
        const double lo = row ? b.left-base.right : b.top-base.bottom;
        const double hi = row ? b.right-base.left : b.bottom-base.top;
        const double first=std::ceil((lo-clearance)/step), last=std::floor((hi+clearance)/step);
        if (first<=last) occupied.emplace_back(first,last);
    }
    // Jump over whole occupied intervals, even when one wide object covers many slots.
    std::sort(occupied.begin(),occupied.end());
    const auto forward=[&](double slot) {
        for (const auto& [first,last] : occupied) {
            if (slot<first) break;
            if (slot<=last) slot=last+1;
        }
        return slot;
    };
    const auto backward=[&](double slot) {
        for (auto it=occupied.rbegin(); it!=occupied.rend(); ++it)
            if (slot>=it->first && slot<=it->second) slot=it->first-1;
        return slot;
    };
    world.push_back(sibling);
    double positive=forward(0), negative=backward(-1);
    // A free footprint can still have a blocked port. Try nearby alternatives
    // within the routing work limit, preserving the atomic insert preflight.
    unsigned predicates=0;
    for (unsigned attempt=0; attempt<8 && predicates<65536; ++attempt) {
        const bool usePositive = positive<=-negative;
        const double slot = usePositive ? positive : negative;
        const double offset = slot*step;
        world.back().transform = sibling.transform;
        (row ? world.back().transform.tx : world.back().transform.ty) += offset;
        auto route = scene::connectorGeometry(wire,world);
        if (!route) return {};
        if (route->status==runtime::ConnectorRouteStatus::kReady) {
            sibling.transform = world.back().transform;
            return scene::connectorDisplayRoute(wire,std::move(*route));
        }
        if (route->status!=runtime::ConnectorRouteStatus::kNoRouteInDomain) return {};
        predicates+=route->work.predicates;
        if (usePositive) positive=forward(slot+1); else negative=backward(slot-1);
    }
    return {};
}
const semantic::ObjectRecord* CanvasEditingRuntime::quickConnectorSibling() const noexcept {
    const auto s=creations_.find(latestCreationPointer_);
    const auto target=creationSiblingPrototypes_.find(latestCreationPointer_);
    if(!quickConnectorStarts_.contains(latestCreationPointer_) || s==creations_.end() ||
        !s->second.preview().valid || s->second.endAttachment() || target==creationSiblingPrototypes_.end())return nullptr;
    return &target->second;
}
bool CanvasEditingRuntime::validCreationHover(bool allowContact) const {
    if(!creationHover_||!surface_.available||closed_||!canonicalWriteAdmission()||!historySceneReady()||
       panTool_||textTool_||(creationActive()&&(!allowContact||creations_.size()>1||placementCreation_||textCreation_))||tableResize_||appearanceSession_||imageCropPresentation_||
       ((!allowContact&&dispatchRuntime_.activeCount()!=0)||(allowContact&&dispatchRuntime_.activeCount()>1))||selectionTransformActive())return false;
    auto* text=interaction_->textSession();
    if(text&&text->read().phase!=runtime::TextPhase::kClosed)return false;
    const auto& h=*creationHover_;
    return h.context==regionContext()&&h.generation==semanticGeneration_.current().value()&&
        (h.source.isZero() || h.pendingRevision==(cpuPending_?cpuPending_->revision:0))&&
        h.defaults==appearanceDefaults()&&h.toolMode==creationMode_&&h.preset==creationPreset_&&h.routing==activeConnectorDefaults().routing;
}
bool CanvasEditingRuntime::clearCreationHover() noexcept {
    const bool changed=creationHover_.has_value();creationHover_.reset();
    if(selectionView_)selectionView_->editingOverlay().clearCreationHover();
    if(changed){canonicalPresentationDirty_=true;transientOverlayHandoffPending_=true;}
    return changed;
}
void CanvasEditingRuntime::installCreationHover(render::EditingOverlay& overlay) {
    if(validCreationHover())overlay.setCreationHover(creationHover_->preview);
    else overlay.clearCreationHover();
}
bool CanvasEditingRuntime::updateCreationHover(float x,float y) {
    auto* editor=interaction_->textSession();
    if(!std::isfinite(x)||!std::isfinite(y)||x<0||y<0||x>=surface_.logicalWidth||y>=surface_.logicalHeight||
       !canonicalWriteAdmission()||!historySceneReady()||panTool_||textTool_||creationActive()||
       tableResize_||appearanceSession_||imageCropPresentation_||dispatchRuntime_.activeCount()!=0||(!creationMode_&&selectedObjectCount()>1)||
       (editor&&editor->read().phase!=runtime::TextPhase::kClosed))return clearCreationHover();
    CreationHover next;next.context=regionContext();next.generation=semanticGeneration_.current().value();
    next.defaults=appearanceDefaults();next.preset=creationPreset_;next.routing=activeConnectorDefaults().routing;next.toolMode=creationMode_;next.x=x;next.y=y;
    const bool defaultTool=creationMode_&&(creationPreset_<=CreationPreset::kTriangle||
        creationPreset_==CreationPreset::kSticky||creationPreset_==CreationPreset::kTable);
    if(defaultTool) {
        if(creationHover_&&creationHover_->source.isZero()&&creationHover_->preset==creationPreset_&&
           creationHover_->defaults==next.defaults)next.preview.primary=creationHover_->preview.primary;
        else next.preview.primary=makeCreationPrototype(creationPreset_);
        if(next.preview.primary.id.isZero())return clearCreationHover();
        const auto world=viewportController_->viewToContent(x,y);
        next.preview.primary.transform.tx=world.first;next.preview.primary.transform.ty=world.second;
    } else {
        const bool ports=selectionMode_&&selectionAcquisitionMode_==runtime::CanvasSelectionMode::kPoint;
        if(!ports||selection_.primary().isZero()||!textWritable(selection_.primary()))return clearCreationHover();
        (void)renderSelectionOverlay();if(!selectionView_)return clearCreationHover();
        const auto& points=selectionView_->editingOverlay().attachmentPoints();
        for(std::size_t i=0;i<points.size();++i)
            if(std::hypot(points[i].center.x-x,points[i].center.y-y)<=12){next.port=static_cast<std::uint32_t>(i+1);break;}
        if(!next.port)return clearCreationHover();
        next.source=selection_.primary();
        next.pendingRevision=cpuPending_?cpuPending_->revision:0;
        if(validCreationHover() && creationHover_->source==next.source && creationHover_->port==next.port) {
            creationHover_->x=x; creationHover_->y=y; return false;
        }
        auto preset=CreationPreset::kArrow;
        if(creationHover_&&creationHover_->source==next.source&&creationHover_->defaults==next.defaults) {
            next.preview.primary=creationHover_->preview.primary;next.preview.sibling=creationHover_->preview.sibling;
        } else next.preview.primary=makeCreationPrototype(preset);
        const semantic::AttachedEndpoint start{next.source,semantic::StablePortAnchor{next.port}};
        const auto end=defaultAttachmentEnd(start);
        const auto targetPort=siblingAttachmentPort(next.source,next.port);
        next.preview.sibling=makeConnectorSibling(next.source,targetPort,end,next.preview.sibling.value_or(semantic::ObjectRecord{}));
        if(next.preview.primary.id.isZero()||next.preview.sibling->id.isZero())return clearCreationHover();
        next.preview.primary.content=semantic::ConnectorContent{{start},
            {semantic::AttachedEndpoint{next.preview.sibling->id,semantic::StablePortAnchor{targetPort}}},activeConnectorDefaults().routing};
        next.preview.route=layoutConnectorSibling(next.preview.primary,*next.preview.sibling,targetPort);
        if(!next.preview.route)return clearCreationHover();
    }
    if(creationHover_&&validCreationHover()&&creationHover_->x==x&&creationHover_->y==y)return false;
    creationHover_=std::move(next);canonicalPresentationDirty_=true;return true;
}
} // namespace canvas::interaction
