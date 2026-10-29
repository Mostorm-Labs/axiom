#include "canvas/runtime/canvas_runtime_facade.hpp"
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/scene/derived_geometry.hpp"
#include "canvas/scene/hierarchy_geometry.hpp"

namespace canvas::runtime {
namespace {
ConnectorCommandReceipt rejectConnector(ConnectorFailureReason reason) { ConnectorCommandReceipt r{}; r.reason=reason; return r; }
ConnectorFailureReason routeFailure(ConnectorRouteStatus s) {
  switch(s) {
    case ConnectorRouteStatus::kReady: return ConnectorFailureReason::kNone;
    case ConnectorRouteStatus::kBudgetExceeded: return ConnectorFailureReason::kBudgetExceeded;
    case ConnectorRouteStatus::kNoRouteInDomain: return ConnectorFailureReason::kNoRouteInDomain;
    case ConnectorRouteStatus::kCancelled: return ConnectorFailureReason::kCancelled;
    default: return ConnectorFailureReason::kInvalidEndpoint;
  }
}
StickyCommandReceipt rejectSticky(ConnectorFailureReason reason) { StickyCommandReceipt r{}; r.reason=reason; return r; }

// Public object creation must allocate the same canonical root ordering key
// used by the existing text/image/shape creation paths.  Leaving the key
// empty makes the semantic insert fail validation even though the request is
// otherwise valid.
bool assignRootOrderKey(const std::vector<semantic::ObjectRecord>& objects,
                        semantic::Placement& placement) {
  if (placement.parent_id || !placement.order_key.bytes().empty()) return placement.order_key.isValid();
  std::vector<std::uint8_t> order;
  for (const auto& existing : objects) {
    if (existing.placement.parent_id) continue;
    if (order.empty() || semantic::OrderKey(order) < existing.placement.order_key)
      order.assign(existing.placement.order_key.bytes().begin(), existing.placement.order_key.bytes().end());
  }
  if (order.size() >= semantic::OrderKey::kMaxSize) return false;
  order.push_back(128U);
  placement.order_key = semantic::OrderKey(std::move(order));
  return true;
}
}

ConnectorCommandReceipt CanvasRuntimeFacade::createConnector(const ConnectorCreateRequest& request) {
  if (!ownerThread()) return rejectConnector(ConnectorFailureReason::kWrongThread);
  if (!composition_ || !composition_->canMutate() || request.objectId.isZero()) return rejectConnector(ConnectorFailureReason::kNotReady);
  semantic::ObjectRecord object{}; object.id=request.objectId; object.kind=semantic::ObjectKind::kConnector; object.kind_version=1; object.placement=request.placement; object.transform=request.transform; object.content=request.content;
  if (!assignRootOrderKey(runtime_.textObjects(), object.placement)) return rejectConnector(ConnectorFailureReason::kInvalidRequest);
  const auto f=runtime_.allocateOperationId(), i=runtime_.allocateOperationId();
  if(f.isZero()||i.isZero()) return rejectConnector(ConnectorFailureReason::kObjectRejected);
  semantic::Operation forward{f,runtime_.textDocument(),1,1,semantic::InsertObjectsOp{{object}}};
  if(runtime_.prepareTextOperation(forward).disposition!=semantic::PrepareDisposition::kPrepared || !runtime_.historySceneReady()) return rejectConnector(ConnectorFailureReason::kObjectRejected);
  if(request.content.routing==semantic::ConnectorRouting::kOrthogonal) {
    auto objects=runtime_.textObjects();objects.push_back(object);
    const auto route=scene::connectorGeometry(object,objects);
    const auto failure=route?routeFailure(route->status):ConnectorFailureReason::kInvalidEndpoint;
    if(failure!=ConnectorFailureReason::kNone)return rejectConnector(failure);
  }
  if(!runtime_.applyLocalHistoryOperation(forward,{i,runtime_.textDocument(),1,1,semantic::DeleteObjectsOp{{request.objectId}}})) return rejectConnector(ConnectorFailureReason::kObjectRejected);
  ConnectorCommandReceipt r{}; r.state=ConnectorCommandState::kApplied; r.reason=ConnectorFailureReason::kNone; r.objectId=request.objectId; r.semanticGeneration=runtime_.semanticGeneration().value(); r.historyChanged=true; return r;
}

StickyCommandReceipt CanvasRuntimeFacade::createSticky(const StickyCreateRequest& request) {
  if (!ownerThread()) return rejectSticky(ConnectorFailureReason::kWrongThread);
  if (!composition_ || !composition_->canMutate() || request.objectId.isZero()) return rejectSticky(ConnectorFailureReason::kNotReady);
  semantic::ObjectRecord object{}; object.id=request.objectId; object.kind=semantic::ObjectKind::kSticky; object.kind_version=1; object.placement=request.placement; object.transform=request.transform; object.content=request.content;
  if (!assignRootOrderKey(runtime_.textObjects(), object.placement)) return rejectSticky(ConnectorFailureReason::kInvalidRequest);
  const auto f=runtime_.allocateOperationId(), i=runtime_.allocateOperationId();
  if(f.isZero()||i.isZero()) return rejectSticky(ConnectorFailureReason::kObjectRejected);
  semantic::Operation forward{f,runtime_.textDocument(),1,1,semantic::InsertObjectsOp{{object}}};
  if(runtime_.prepareTextOperation(forward).disposition!=semantic::PrepareDisposition::kPrepared || !runtime_.historySceneReady()) return rejectSticky(ConnectorFailureReason::kObjectRejected);
  if(!runtime_.applyLocalHistoryOperation(forward,{i,runtime_.textDocument(),1,1,semantic::DeleteObjectsOp{{request.objectId}}})) return rejectSticky(ConnectorFailureReason::kObjectRejected);
  StickyCommandReceipt r{}; r.state=ConnectorCommandState::kApplied; r.reason=ConnectorFailureReason::kNone; r.objectId=request.objectId; r.semanticGeneration=runtime_.semanticGeneration().value(); r.historyChanged=true; return r;
}

ConnectorCommandReceipt CanvasRuntimeFacade::setConnectorRouting(const ConnectorRoutingRequest& request) {
  if(!ownerThread())return rejectConnector(ConnectorFailureReason::kWrongThread);
  if(!composition_||!composition_->canMutate())return rejectConnector(ConnectorFailureReason::kNotReady);
  if(request.routing!=semantic::ConnectorRouting::kStraight&&request.routing!=semantic::ConnectorRouting::kOrthogonal)
    return rejectConnector(ConnectorFailureReason::kInvalidRequest);
  if(request.expectedSemanticGeneration!=0&&request.expectedSemanticGeneration!=runtime_.semanticGeneration().value())
    return rejectConnector(ConnectorFailureReason::kInvalidRequest);
  ConnectorCommandReceipt receipt{};receipt.objectId=request.objectId;
  if(request.objectId.isZero()) {
    runtime_.connectorRoutingPreset_=request.routing;
  } else {
    const auto* current=runtime_.textRecord(request.objectId);
    if(!current||current->kind!=semantic::ObjectKind::kConnector)return rejectConnector(ConnectorFailureReason::kInvalidTarget);
    for(const auto& p:current->properties.entries)if(p.field_id==2U)
      if(const auto* locked=std::get_if<bool>(&p.value);locked&&*locked)return rejectConnector(ConnectorFailureReason::kInvalidTarget);
    const auto before=std::get<semantic::ConnectorContent>(current->content);
    auto content=before;content.routing=request.routing;
    if(content!=before) {
      auto candidate=*current;candidate.content=content;
      if(request.routing==semantic::ConnectorRouting::kOrthogonal) {
        const auto route=scene::connectorGeometry(candidate,scene::projectWorldObjects(runtime_.textObjects()));
        const auto failure=route?routeFailure(route->status):ConnectorFailureReason::kInvalidEndpoint;
        if(failure!=ConnectorFailureReason::kNone)return rejectConnector(failure);
      }
      const auto f=runtime_.allocateOperationId(),i=runtime_.allocateOperationId();
      if(f.isZero()||i.isZero()||!runtime_.applyLocalHistoryOperation(
          {f,runtime_.textDocument(),1,1,semantic::SetConnectorContentOp{request.objectId,content}},
          {i,runtime_.textDocument(),1,1,semantic::SetConnectorContentOp{request.objectId,before}}))
        return rejectConnector(ConnectorFailureReason::kObjectRejected);
      receipt.historyChanged=true;
    }
  }
  receipt.state=ConnectorCommandState::kApplied;receipt.reason=ConnectorFailureReason::kNone;
  receipt.semanticGeneration=runtime_.semanticGeneration().value();return receipt;
}
semantic::ConnectorRouting CanvasRuntimeFacade::readConnectorRoutingPreset() const {
  return runtime_.connectorRoutingPreset_;
}
ConnectorSnapshot CanvasRuntimeFacade::readSelectedConnector() const {
  if(runtime_.selectedObjectCount()!=1U)return {};
  return readConnectorState(runtime_.selection_.primary());
}
ConnectorSnapshot CanvasRuntimeFacade::readConnectorState(semantic::ObjectId id) const {
  ConnectorSnapshot snapshot{};
  const auto* object=runtime_.textRecord(id);
  if(!object||object->kind!=semantic::ObjectKind::kConnector)return snapshot;
  snapshot.available=true;snapshot.objectId=object->id;snapshot.content=std::get<semantic::ConnectorContent>(object->content);
  snapshot.semanticGeneration=runtime_.semanticGeneration().value();snapshot.sceneRevision=runtime_.sceneRevision();
  if(const auto* record=runtime_.sceneCoordinator_->runtimeScene().find(object->id)) {
    if(const auto* geometry=std::get_if<scene::ConnectorGeometry>(&record->derivedGeometry)) {
      snapshot.routeStatus=geometry->status;snapshot.work=geometry->work;
      snapshot.algorithmRevision=geometry->algorithmRevision;
      for(const auto p:geometry->route)snapshot.route.push_back({p.x,p.y});
    }
  }
  return snapshot;
}
StickySnapshot CanvasRuntimeFacade::readSelectedSticky() const { if(runtime_.selectedObjectCount()!=1U)return {}; StickySnapshot r{}; const auto* o=runtime_.textRecord(runtime_.selection_.primary()); if(o&&o->kind==semantic::ObjectKind::kSticky){r.available=true;r.objectId=o->id;r.content=std::get<semantic::StickyContent>(o->content);} return r; }
}
