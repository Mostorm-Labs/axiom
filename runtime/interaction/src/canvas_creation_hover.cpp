#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/scene/hierarchy_geometry.hpp"
#include "cpu_pending_erase_state.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace canvas::interaction {
namespace {
bool hiddenForLayout(const semantic::ObjectRecord& object) {
    return std::any_of(object.properties.entries.begin(),object.properties.entries.end(),[](const auto& p) {
        const auto* value=std::get_if<bool>(&p.value);
        return p.field_id==1 && value && !*value;
    });
}

float connectedSiblingAxis(std::span<const semantic::ObjectRecord> world,
                           const semantic::ConnectorEndpoint& start, std::uint32_t targetPort,
                           float sourceAxis, float defaultAxis) {
    const semantic::StablePortAnchor facing{targetPort};
    std::unordered_set<semantic::ObjectId,foundation::ObjectIdHash> targets;
    for(const auto& object:world) {
        const auto* wire=std::get_if<semantic::ConnectorContent>(&object.content);
        if(!wire || wire->start!=start || hiddenForLayout(object)) continue;
        const auto* end=std::get_if<semantic::AttachedEndpoint>(&wire->end.value);
        if(end && end->anchor==semantic::AnchorRef{facing})targets.insert(end->target_object_id);
    }
    if(targets.empty())return defaultAxis;

    const bool row=targetPort==1 || targetPort==3;
    std::vector<float> axes;
    for(const auto& object:world) {
        if(!targets.contains(object.id) || hiddenForLayout(object)) continue;
        const auto point=scene::connectorAnchorPoint(object,facing,{});
        if(!point)continue;
        const float axis=row ? point->y : point->x;
        if(defaultAxis>sourceAxis ? axis>sourceAxis : axis<sourceAxis) axes.push_back(axis);
    }
    // Preserve the existing board layout, including offscreen siblings. Count distinct
    // nodes, and group sub-grid roundoff from composed transforms. On a tie, prefer
    // the row/column closest to the ordinary placement, then the lower coordinate.
    std::sort(axes.begin(),axes.end());
    float chosen=defaultAxis;
    std::ptrdiff_t most=0;
    for(auto it=axes.begin();it!=axes.end();) {
        const auto end=std::upper_bound(it,axes.end(),double(*it)+1.0/64);
        const auto count=end-it;
        const auto axis=it[count/2];
        if(count>most || (count==most && std::abs(axis-defaultAxis)<std::abs(chosen-defaultAxis))) {
            most=count;
            chosen=axis;
        }
        it=end;
    }
    return chosen;
}

// Search integer slots on the chosen row or column. A single wide object
// can block many slots, so jump over whole intervals in either direction.
std::vector<std::pair<double,double>> occupiedSiblingSlots(
    const foundation::WorldRect& base, bool row, double step, std::span<const foundation::WorldRect> blockers) {
    std::vector<std::pair<double,double>> occupied;
    for (const auto& b:blockers) {
        if (row ? (b.bottom<=base.top || b.top>=base.bottom)
                : (b.right<=base.left || b.left>=base.right)) continue;
        const double lo=row ? b.left-base.right : b.top-base.bottom;
        const double hi=row ? b.right-base.left : b.bottom-base.top;
        const double first=std::floor(lo/step)+1, last=std::ceil(hi/step)-1;
        if (first<=last) occupied.emplace_back(first,last);
    }
    std::sort(occupied.begin(),occupied.end());
    return occupied;
}
}

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
    if(const auto* shape=std::get_if<semantic::ShapeContent>(&source->content)) {
        target.kind=semantic::ObjectKind::kShape;target.kind_version=shape->shape_kind==3?2:1;
        target.content=semantic::ShapeContent{shape->shape_kind,shape->width,shape->height};
    } else if(const auto* sticky=std::get_if<semantic::StickyContent>(&source->content)) {
        target.kind=semantic::ObjectKind::kSticky;target.kind_version=1;target.content=*sticky;
    }
    target.transform={};
    const auto local=scene::connectorAnchorPoint(target,semantic::StablePortAnchor{targetPort},{});
    if(!local)return {};
    target.transform.tx=double(end.x)-local->x;
    target.transform.ty=double(end.y)-local->y;
    return target;
}
std::optional<scene::ConnectorGeometry> CanvasEditingRuntime::layoutConnectorSibling(
    const semantic::ObjectRecord& wire, semantic::ObjectRecord& sibling, std::uint32_t targetPort,
    foundation::WorldPoint sourcePoint) const {
    const bool row = targetPort == 1 || targetPort == 3;
    auto objects = semanticObjects_.allObjects();
    if (cpuPending_) {
        for (const auto& intent : cpuPending_->slots)
            if (intent && intent->serial)
                for (const auto& created : intent->creates)
                    if (!semanticObjects_.contains(created.id)) objects.push_back(created);
    }
    auto world = scene::projectWorldObjects(objects);
    const auto initial=scene::computeBounds(sibling);
    if (!initial.finite) return {};
    constexpr float clearance=8;
    std::vector<foundation::WorldRect> blockers;
    for (const auto& item:world) {
        if (item.kind==semantic::ObjectKind::kGroup || item.kind==semantic::ObjectKind::kSection ||
            item.kind==semantic::ObjectKind::kConnector || hiddenForLayout(item)) continue;
        const auto* projected=sceneCoordinator_->runtimeScene().find(item.id);
        const auto b=projected ? projected->worldBounds : scene::computeBounds(item).world;
        blockers.push_back({b.left-clearance,b.top-clearance,b.right+clearance,b.bottom+clearance});
    }
    const bool occupied=std::any_of(blockers.begin(),blockers.end(),[&](const auto& b){
        return b.intersects(initial.world);
    });
    const auto originalCount=world.size();
    world.push_back(sibling);
    std::optional<scene::ConnectorGeometry> route;
    unsigned attempts=0, predicates=0;
    const auto trySite=[&](const semantic::Transform2D& pose) {
        world.back().transform=pose;
        route=scene::connectorGeometry(wire,world);
        ++attempts;
        if (route) predicates+=route->work.predicates;
    };
    const auto blockedPort=[&] {
        return route && route->status==runtime::ConnectorRouteStatus::kNoRouteInDomain &&
               attempts<8 && predicates<65536;
    };
    if (!occupied) trySite(sibling.transform);
    if (occupied || blockedPort()) {
        const auto defaultEnd=defaultAttachmentEnd(sourcePoint,targetPort);
        const float defaultAxis=row ? defaultEnd.y : defaultEnd.x;
        const auto& start=std::get<semantic::ConnectorContent>(wire.content).start;
        const auto axis=connectedSiblingAxis(std::span(world).first(originalCount),start,targetPort,
                                              row ? sourcePoint.y : sourcePoint.x,defaultAxis);
        auto axisPose=sibling.transform;
        (row ? axisPose.ty : axisPose.tx)+=double(axis)-defaultAxis;
        world.back().transform=axisPose;
        const auto base=scene::computeBounds(world.back());
        if (!base.finite) return {};
        const auto& body=initial.geometry;
        const double span=row ? body.right-body.left : body.bottom-body.top;
        const double step=span+std::clamp(span/2,24.0,64.0);
        const auto slots=occupiedSiblingSlots(base.world,row,step,blockers);
        double positive=!occupied && axis==defaultAxis ? 1 : 0, negative=-1;
        do {
            for(const auto& [first,last]:slots)
                if(first<=positive && positive<=last)positive=last+1;
            for(auto it=slots.rbegin();it!=slots.rend();++it)
                if(it->first<=negative && negative<=it->second)negative=it->first-1;
            const auto slot=positive<=-negative ? positive++ : negative--;
            auto pose=axisPose;
            (row ? pose.tx : pose.ty)+=slot*step;
            trySite(pose);
        } while(blockedPort());
    }
    if (!route || route->status!=runtime::ConnectorRouteStatus::kReady) return {};
    sibling.transform=world.back().transform;
    return scene::connectorDisplayRoute(wire,std::move(*route));
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
        const auto sourcePoint=fixedAttachmentPosition(start);
        if(!sourcePoint)return clearCreationHover();
        const auto targetPort=siblingAttachmentPort(next.source,next.port);
        const auto end=defaultAttachmentEnd(*sourcePoint,targetPort);
        next.preview.sibling=makeConnectorSibling(next.source,targetPort,end,next.preview.sibling.value_or(semantic::ObjectRecord{}));
        if(next.preview.primary.id.isZero()||next.preview.sibling->id.isZero())return clearCreationHover();
        next.preview.primary.content=semantic::ConnectorContent{{start},
            {semantic::AttachedEndpoint{next.preview.sibling->id,semantic::StablePortAnchor{targetPort}}},activeConnectorDefaults().routing};
        next.preview.route=layoutConnectorSibling(next.preview.primary,*next.preview.sibling,targetPort,*sourcePoint);
        if(!next.preview.route)return clearCreationHover();
    }
    if(creationHover_&&validCreationHover()&&creationHover_->x==x&&creationHover_->y==y)return false;
    creationHover_=std::move(next);canonicalPresentationDirty_=true;return true;
}
} // namespace canvas::interaction
