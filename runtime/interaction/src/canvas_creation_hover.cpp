#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/scene/hierarchy_geometry.hpp"
#include <cmath>
#include <utility>

namespace canvas::interaction {
std::uint32_t CanvasEditingRuntime::oppositeAttachmentPort(std::uint32_t port) noexcept {
    switch(port){case 1:return 3;case 2:return 4;case 3:return 1;case 4:return 2;default:return 4;}
}
semantic::ObjectRecord CanvasEditingRuntime::makeConnectorSibling(semantic::ObjectId sourceId,
    std::uint32_t sourcePort,foundation::WorldPoint end,semantic::ObjectRecord target) {
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
    target.transform={};const auto port=oppositeAttachmentPort(sourcePort);
    const bool triangle=target.kind==semantic::ObjectKind::kShape&&std::get<semantic::ShapeContent>(target.content).shape_kind==3;
    switch(port) {
      case 1:target.transform.tx=end.x-width*.5;target.transform.ty=end.y;break;
      case 2:target.transform.tx=end.x-width*(triangle?.75:1);target.transform.ty=end.y-height*.5;break;
      case 3:target.transform.tx=end.x-width*.5;target.transform.ty=end.y-height;break;
      default:target.transform.tx=end.x-width*(triangle?.25:0);target.transform.ty=end.y-height*.5;break;
    }
    return target;
}
foundation::WorldPoint CanvasEditingRuntime::defaultAttachmentEnd(const semantic::AttachedEndpoint& attached) const {
    const auto* anchor=std::get_if<semantic::StablePortAnchor>(&attached.anchor);
    if(!anchor||!selectionView_||anchor->port_id<1||anchor->port_id>4)return {};
    const auto& points=selectionView_->editingOverlay().attachmentPoints();
    if(anchor->port_id>points.size())return {};
    const auto screen=points[anchor->port_id-1].center;
    const auto start=viewportController_->viewToContent(screen.x,screen.y);
    foundation::WorldPoint end{start.first,start.second};
    switch(anchor->port_id){case 1:end.y-=120;break;case 3:end.y+=120;break;case 4:end.x-=120;break;default:end.x+=120;}
    semantic::ObjectRecord connector;connector.kind=semantic::ObjectKind::kConnector;connector.kind_version=1;
    connector.content=semantic::ConnectorContent{{attached},{semantic::FreePointEndpoint{{end.x,end.y}}},semantic::ConnectorRouting::kStraight};
    if(const auto* source=sceneCoordinator_->runtimeScene().find(attached.target_object_id)) {
        auto world=*semanticObjects_.find(attached.target_object_id);world.transform=source->transform;
        if(const auto route=scene::connectorGeometry(connector,{world});route&&route->status==runtime::ConnectorRouteStatus::kReady) {
            if(anchor->port_id==1||anchor->port_id==3)end.x=route->start.x;else end.y=route->start.y;
        }
    }
    return end;
}
bool CanvasEditingRuntime::validCreationHover(bool allowContact) const {
    if(!creationHover_||!surface_.available||closed_||!canonicalWriteAdmission()||!historySceneReady()||
       panTool_||textTool_||(creationActive()&&(!allowContact||creations_.size()>1||placementCreation_||textCreation_))||tableResize_||appearanceSession_||imageCropPresentation_||
       ((!allowContact&&dispatchRuntime_.activeCount()!=0)||(allowContact&&dispatchRuntime_.activeCount()>1))||selectionTransformActive())return false;
    auto* text=interaction_->textSession();
    if(text&&text->read().phase!=runtime::TextPhase::kClosed)return false;
    const auto& h=*creationHover_;
    return h.context==regionContext()&&h.generation==semanticGeneration_.current().value()&&
        h.defaults==appearanceDefaults()&&h.toolMode==creationMode_&&h.preset==creationPreset_&&h.routing==connectorRoutingPreset_;
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
    next.defaults=appearanceDefaults();next.preset=creationPreset_;next.routing=connectorRoutingPreset_;next.toolMode=creationMode_;next.x=x;next.y=y;
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
        const bool connector=creationMode_&&(creationPreset_==CreationPreset::kLine||creationPreset_==CreationPreset::kArrow);
        if((!ports&&!connector)||selection_.primary().isZero()||!textWritable(selection_.primary()))return clearCreationHover();
        (void)renderSelectionOverlay();if(!selectionView_)return clearCreationHover();
        const auto& points=selectionView_->editingOverlay().attachmentPoints();
        for(std::size_t i=0;i<points.size();++i)
            if(std::hypot(points[i].center.x-x,points[i].center.y-y)<=12){next.port=static_cast<std::uint32_t>(i+1);break;}
        if(!next.port)return clearCreationHover();
        next.source=selection_.primary();
        auto preset=creationPreset_==CreationPreset::kArrow?CreationPreset::kArrow:CreationPreset::kLine;
        if(creationHover_&&creationHover_->source==next.source&&creationHover_->defaults==next.defaults) {
            next.preview.primary=creationHover_->preview.primary;next.preview.sibling=creationHover_->preview.sibling;
        } else next.preview.primary=makeCreationPrototype(preset);
        const semantic::AttachedEndpoint start{next.source,semantic::StablePortAnchor{next.port}};
        const auto end=defaultAttachmentEnd(start);
        next.preview.sibling=makeConnectorSibling(next.source,next.port,end,next.preview.sibling.value_or(semantic::ObjectRecord{}));
        if(next.preview.primary.id.isZero()||next.preview.sibling->id.isZero())return clearCreationHover();
        next.preview.primary.content=semantic::ConnectorContent{{start},
            {semantic::AttachedEndpoint{next.preview.sibling->id,semantic::StablePortAnchor{oppositeAttachmentPort(next.port)}}},connectorRoutingPreset_};
        auto staged=semanticObjects_.allObjects();staged.push_back(*next.preview.sibling);staged.push_back(next.preview.primary);
        staged=scene::projectWorldObjects(staged);
        next.preview.route=scene::connectorGeometry(next.preview.primary,staged);
        if(!next.preview.route||next.preview.route->status!=runtime::ConnectorRouteStatus::kReady)return clearCreationHover();
    }
    if(creationHover_&&validCreationHover()&&creationHover_->x==x&&creationHover_->y==y)return false;
    creationHover_=std::move(next);canonicalPresentationDirty_=true;return true;
}
} // namespace canvas::interaction
