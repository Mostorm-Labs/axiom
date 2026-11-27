#include "canvas/interaction/sibling_order.hpp"
#include "canvas/runtime/canvas_runtime_facade.hpp"
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/scene/derived_geometry.hpp"
#include "canvas/scene/hierarchy_geometry.hpp"
#include "canvas/semantic/validator.hpp"

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
bool assignSiblingOrderKey(const std::vector<semantic::ObjectRecord>& objects,
                        semantic::Placement& placement) {
  if (!placement.order_key.bytes().empty()) return placement.order_key.isValid();
  std::vector<std::uint8_t> order;
  for (const auto& existing : objects) {
    if (existing.placement.parent_id!=placement.parent_id) continue;
    if (order.empty() || semantic::OrderKey(order) < existing.placement.order_key)
      order.assign(existing.placement.order_key.bytes().begin(), existing.placement.order_key.bytes().end());
  }
  if (!canvas::interaction::nextSiblingOrderKey(order)) return false;
  placement.order_key = semantic::OrderKey(std::move(order));
  return true;
}
}

ConnectorCommandReceipt CanvasRuntimeFacade::createConnector(const ConnectorCreateRequest& request) {
  if (!ownerThread()) return rejectConnector(ConnectorFailureReason::kWrongThread);
  if (!composition_ || !composition_->canMutate()) return rejectConnector(ConnectorFailureReason::kNotReady);
  semantic::ObjectRecord object{}; object.id=request.objectId.isZero()?runtime_.allocateTextIdentity():request.objectId; if(object.id.isZero())return rejectConnector(ConnectorFailureReason::kObjectRejected); object.kind=semantic::ObjectKind::kConnector; object.kind_version=1; object.placement=request.placement; object.transform=request.transform; object.content=request.content;
  runtime_.materializeAppearance(object,false);
  const auto& style=runtime_.connectorDefaults_;
  if(request.startArrow.value_or(style.startArrow))object.properties.entries.push_back({0x200U,semantic::ConnectorDecorationValue::kArrow});
  if(request.endArrow.value_or(style.endArrow))object.properties.entries.push_back({0x201U,semantic::ConnectorDecorationValue::kArrow});
  if(!object.placement.parent_id)runtime_.placeNewConnector(object);
  if (!assignSiblingOrderKey(runtime_.textObjects(), object.placement)) return rejectConnector(ConnectorFailureReason::kInvalidRequest);
  const auto f=runtime_.allocateOperationId(), i=runtime_.allocateOperationId();
  if(f.isZero()||i.isZero()) return rejectConnector(ConnectorFailureReason::kObjectRejected);
  semantic::Operation forward{f,runtime_.textDocument(),1,1,semantic::InsertObjectsOp{{object}}};
  if(runtime_.prepareTextOperation(forward).disposition!=semantic::PrepareDisposition::kPrepared || !runtime_.historySceneReady()) return rejectConnector(ConnectorFailureReason::kObjectRejected);
  {
    const auto route=scene::stagedConnectorRoute(object,runtime_.textObjects());
    const auto failure=route?routeFailure(route->status):ConnectorFailureReason::kInvalidEndpoint;
    if(failure!=ConnectorFailureReason::kNone)return rejectConnector(failure);
  }
  if(!runtime_.applyLocalHistoryOperation(forward,{i,runtime_.textDocument(),1,1,semantic::DeleteObjectsOp{{object.id}}})) return rejectConnector(ConnectorFailureReason::kObjectRejected);
  ConnectorCommandReceipt r{}; r.state=ConnectorCommandState::kApplied; r.reason=ConnectorFailureReason::kNone; r.objectId=object.id; r.semanticGeneration=runtime_.semanticGeneration().value(); r.historyChanged=true; return r;
}

StickyCommandReceipt CanvasRuntimeFacade::createSticky(const StickyCreateRequest& request) {
  if (!ownerThread()) return rejectSticky(ConnectorFailureReason::kWrongThread);
  if (!composition_ || !composition_->canMutate()) return rejectSticky(ConnectorFailureReason::kNotReady);
  semantic::ObjectRecord object{}; object.id=request.objectId.isZero()?runtime_.allocateTextIdentity():request.objectId; if(object.id.isZero())return rejectSticky(ConnectorFailureReason::kObjectRejected); object.kind=semantic::ObjectKind::kSticky; object.kind_version=1; object.placement=request.placement; object.transform=request.transform; object.content=request.content;
  runtime_.materializeAppearance(object,true);
  if (!assignSiblingOrderKey(runtime_.textObjects(), object.placement)) return rejectSticky(ConnectorFailureReason::kInvalidRequest);
  if(runtime_.pendingLocalEnabled()){if(!runtime_.enqueuePendingCreate({object},true))return rejectSticky(ConnectorFailureReason::kObjectRejected);StickyCommandReceipt r;r.state=ConnectorCommandState::kPending;r.pendingSerial=runtime_.lastPendingLocalSerial();r.reason=ConnectorFailureReason::kNone;r.objectId=object.id;return r;}
  const auto f=runtime_.allocateOperationId(), i=runtime_.allocateOperationId();
  if(f.isZero()||i.isZero()) return rejectSticky(ConnectorFailureReason::kObjectRejected);
  semantic::Operation forward{f,runtime_.textDocument(),1,1,semantic::InsertObjectsOp{{object}}};
  if(runtime_.prepareTextOperation(forward).disposition!=semantic::PrepareDisposition::kPrepared || !runtime_.historySceneReady()) return rejectSticky(ConnectorFailureReason::kObjectRejected);
  if(!runtime_.applyLocalHistoryOperation(forward,{i,runtime_.textDocument(),1,1,semantic::DeleteObjectsOp{{object.id}}})) return rejectSticky(ConnectorFailureReason::kObjectRejected);
  StickyCommandReceipt r{}; r.state=ConnectorCommandState::kApplied; r.reason=ConnectorFailureReason::kNone; r.objectId=object.id; r.semanticGeneration=runtime_.semanticGeneration().value(); r.historyChanged=true; return r;
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
    runtime_.activeConnectorDefaults().routing=request.routing;
    (void)runtime_.clearCreationHover();
  } else {
    const auto* current=runtime_.textRecord(request.objectId);
    if(!current||current->kind!=semantic::ObjectKind::kConnector)return rejectConnector(ConnectorFailureReason::kInvalidTarget);
    if(runtime_.selectionTransformActive())return rejectConnector(ConnectorFailureReason::kNotReady);
    auto content=std::get<semantic::ConnectorContent>(current->content);
    content.routing=request.routing;
    return runtime_.commitConnectorContent(request.objectId,content,request.expectedSemanticGeneration);
  }
  receipt.state=ConnectorCommandState::kApplied;receipt.reason=ConnectorFailureReason::kNone;
  receipt.semanticGeneration=runtime_.semanticGeneration().value();return receipt;
}
semantic::ConnectorRouting CanvasRuntimeFacade::readConnectorRoutingPreset() const {
  return runtime_.activeConnectorDefaults().routing;
}
ConnectorToolDefaults CanvasRuntimeFacade::readConnectorDefaults(bool connectionTool) const {
    return connectionTool ? runtime_.connectorDefaults_ : runtime_.activeConnectorDefaults();
}
ConnectorCommandReceipt
CanvasRuntimeFacade::setConnectorEndpoint(const ConnectorEndpointRequest& r) {
    if (!ownerThread())
        return rejectConnector(ConnectorFailureReason::kWrongThread);
    if (!composition_ || !composition_->canMutate() || !runtime_.textReady() ||
        runtime_.appearanceSession_)
        return rejectConnector(ConnectorFailureReason::kNotReady);
    return runtime_.editConnectorEndpoint(r);
}
ConnectorCommandReceipt
CanvasRuntimeFacade::setConnectorDecoration(const ConnectorDecorationRequest& r) {
    if (!ownerThread())
        return rejectConnector(ConnectorFailureReason::kWrongThread);
    if (!composition_ || !composition_->canMutate() || !runtime_.textReady() ||
        runtime_.appearanceSession_)
        return rejectConnector(ConnectorFailureReason::kNotReady);
    ConnectorCommandReceipt result;
    result.objectId = r.objectId;
    if (r.expectedSemanticGeneration &&
        r.expectedSemanticGeneration != runtime_.semanticGeneration().value())
        return rejectConnector(ConnectorFailureReason::kInvalidRequest);
    if (r.objectId.isZero()) {
        auto& d = runtime_.activeConnectorDefaults();
        d.startArrow = r.startArrow;
        d.endArrow = r.endArrow;
        (void)runtime_.clearCreationHover();
    } else {
        const auto* o = runtime_.textRecord(r.objectId);
        if (!o || o->kind != semantic::ObjectKind::kConnector || !runtime_.textWritable(o->id))
            return rejectConnector(ConnectorFailureReason::kInvalidTarget);
        semantic::PatchPropertiesOp forward, inverse;
        for (const auto& [field, arrow] :
             {std::pair{0x200U, r.startArrow}, std::pair{0x201U, r.endArrow}}) {
            const auto old = std::find_if(o->properties.entries.begin(),
                                          o->properties.entries.end(),
                                          [&](const auto& e) { return e.field_id == field; });
            const bool was = old != o->properties.entries.end() &&
                             std::get<semantic::ConnectorDecorationValue>(old->value) ==
                                 semantic::ConnectorDecorationValue::kArrow;
            if (was == arrow)
                continue;
            const auto patch = [&](bool on) {
                semantic::PropertyPatch p{o->id, field, semantic::PropertyPatchAction::kClear, {}};
                if (on) {
                    p.action = semantic::PropertyPatchAction::kSet;
                    p.value = semantic::PropertyValue{semantic::ConnectorDecorationValue::kArrow};
                }
                return p;
            };
            forward.patches.push_back(patch(arrow));
            inverse.patches.push_back(patch(was));
        }
        if (!forward.patches.empty()) {
            const auto f = runtime_.allocateOperationId(), i = runtime_.allocateOperationId();
            if (f.isZero() || i.isZero() ||
                !runtime_.applyLocalHistoryOperation(
                    {f, runtime_.textDocument(), 1, 1, std::move(forward)},
                    {i, runtime_.textDocument(), 1, 1, std::move(inverse)}))
                return rejectConnector(ConnectorFailureReason::kObjectRejected);
            result.historyChanged = true;
        }
    }
    result.state = ConnectorCommandState::kApplied;
    result.reason = ConnectorFailureReason::kNone;
    result.semanticGeneration = runtime_.semanticGeneration().value();
    return result;
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
  snapshot.editingEndpoint=runtime_.endpointEditCurrent() && runtime_.endpointDrag_->id==id;
  if(runtime_.lastEndpointEdit_ && runtime_.lastEndpointEdit_->objectId==id)snapshot.lastEndpointEdit=runtime_.lastEndpointEdit_;
  snapshot.semanticGeneration=runtime_.semanticGeneration().value();snapshot.sceneRevision=runtime_.sceneRevision();
  if(const auto* record=runtime_.sceneCoordinator_->runtimeScene().find(object->id)) {
    snapshot.worldTransform=record->transform;
    if(const auto* geometry=std::get_if<scene::ConnectorGeometry>(&record->derivedGeometry)) {
      snapshot.routeStatus=geometry->status;snapshot.work=geometry->work;
      snapshot.algorithmRevision=geometry->algorithmRevision;
      snapshot.startArrow=geometry->arrow_start;snapshot.endArrow=geometry->arrow_end;
      for(const auto p:geometry->route)snapshot.displayRoute.push_back({p.x,p.y});
      snapshot.route=snapshot.displayRoute;
      if(snapshot.route.size()>=2){snapshot.route.front()={geometry->start.x,geometry->start.y};snapshot.route.back()={geometry->end.x,geometry->end.y};}
    }
  }
  return snapshot;
}
ClipboardResult CanvasRuntimeFacade::clipboardCommand(const ClipboardRequest& request) {
 if(!ownerThread()||!composition_||!composition_->canMutate())return {};
 return runtime_.clipboardCommand(request,readMultiSelection());
}
AppearanceSnapshot CanvasRuntimeFacade::readAppearance(bool defaults) const {
 if(!ownerThread())return {};return runtime_.appearanceSnapshot(readMultiSelection(),defaults);
}
AppearanceResult CanvasRuntimeFacade::appearanceCommand(const AppearanceRequest& request) {
 if(!ownerThread()||!composition_||!composition_->canMutate())return {};
 return runtime_.appearanceCommand(request,readMultiSelection());
}
TableResult CanvasRuntimeFacade::tableCommand(const TableRequest& request) {
  if(!ownerThread()||!composition_||!composition_->canMutate())return {};
  return runtime_.tableCommand(request);
}
TableSnapshot CanvasRuntimeFacade::readTable(foundation::ObjectId id) const {
  return ownerThread()?runtime_.tableSnapshot(id):TableSnapshot{};
}
StickySnapshot CanvasRuntimeFacade::readSelectedSticky() const { if(runtime_.selectedObjectCount()!=1U)return {}; StickySnapshot r{}; const auto* o=runtime_.textRecord(runtime_.selection_.primary()); if(o&&o->kind==semantic::ObjectKind::kSticky){r.available=true;r.objectId=o->id;r.content=std::get<semantic::StickyContent>(o->content);} return r; }
VectorPathResult CanvasRuntimeFacade::createVectorPath(const VectorPathCreateRequest& request) {
  VectorPathResult out;out.objectId=request.objectId;
  if(!ownerThread()){out.error=VectorPathError::kWrongThread;return out;}
  if(!composition_||!composition_->canMutate()){out.error=VectorPathError::kNotReady;return out;}
  if(!runtime_.canonicalWriteAdmission()||runtime_.appearanceSession_||runtime_.creationActive()||runtime_.selectionTransformActive()||runtime_.dispatchRuntime_.activeCount()!=0){out.error=VectorPathError::kBusy;return out;}
  if(!semantic::validateSketchVectorPath(request.geometry)){out.error=VectorPathError::kInvalid;return out;}
  semantic::ObjectRecord object;object.id=request.objectId.isZero()?runtime_.allocateTextIdentity():request.objectId;out.objectId=object.id;if(object.id.isZero()){out.error=VectorPathError::kRejected;return out;}object.kind=semantic::ObjectKind::kVectorPath;object.kind_version=1;object.content=semantic::VectorPathContent{request.geometry};object.transform=request.transform;
  object.placement.parent_id=runtime_.selection_.groupScope();
  if(object.placement.parent_id){const auto* parent=runtime_.sceneCoordinator_->runtimeScene().find(*object.placement.parent_id);semantic::Transform2D inverse;if(!parent||!runtime_.textWritable(parent->objectId)||!scene::inverseTransform(parent->transform,inverse)){out.error=VectorPathError::kRejected;return out;}object.transform=scene::composeTransform(inverse,object.transform);}
  const auto values=runtime_.appearanceDefaults();
  object.properties.entries={{3U,static_cast<float>(values.opacity)},{0x100U,values.fillKind==AppearanceFill::kNone?semantic::FillStyleValue{semantic::NoFill{}}:semantic::FillStyleValue{semantic::SolidFill{values.fill}}},{0x101U,values.strokeEnabled?semantic::StrokeStyleValue{semantic::SolidStroke{values.stroke,values.strokeWidth,semantic::StrokeCap::kButt,semantic::MiterJoin{4},semantic::SolidDash{}}}:semantic::StrokeStyleValue{semantic::NoStroke{}}}};
  runtime_.materializeAppearance(object,true);
  // This entry point uses versioned fill/stroke semantics even with roughness off.
  // Legacy unversioned paths retain their historical painter and reader contract.
  if(std::none_of(object.properties.entries.begin(),object.properties.entries.end(),[](const auto& p){return p.field_id==0x102U;}))
    object.properties.entries.push_back({0x102U,semantic::SketchStyleV1{1,runtime_.allocateSketchSeed(),values.roughness,values.enabled}});
  if(object.placement.parent_id){std::vector<std::uint8_t> order;for(const auto& r:runtime_.textObjects())if(r.placement.parent_id==object.placement.parent_id&&(order.empty()||semantic::OrderKey(order)<r.placement.order_key))order.assign(r.placement.order_key.bytes().begin(),r.placement.order_key.bytes().end());if(order.size()>=semantic::OrderKey::kMaxSize){out.error=VectorPathError::kRejected;return out;}order.push_back(128);object.placement.order_key=semantic::OrderKey(order);}
  if(!assignSiblingOrderKey(runtime_.textObjects(),object.placement)){out.error=VectorPathError::kRejected;return out;}
  const auto forward=runtime_.allocateOperationId(),inverse=runtime_.allocateOperationId();
  if(forward.isZero()||inverse.isZero()||!runtime_.applyLocalHistoryOperation({forward,runtime_.textDocument(),1,1,semantic::InsertObjectsOp{{object}}},{inverse,runtime_.textDocument(),1,1,semantic::DeleteObjectsOp{{object.id}}})){out.error=VectorPathError::kRejected;return out;}
  (void)runtime_.selection_.click(object.id);runtime_.canonicalPresentationDirty_=true;out.applied=true;out.error=VectorPathError::kNone;out.generation=runtime_.semanticGeneration().value();return out;
}
VectorPathSnapshot CanvasRuntimeFacade::readVectorPath(foundation::ObjectId id) const {
  VectorPathSnapshot out;if(!ownerThread()||!runtime_.historySceneReady())return out;if(id.isZero())id=runtime_.selection_.primary();const auto* object=runtime_.semanticObjects_.find(id);if(!object||object->kind!=semantic::ObjectKind::kVectorPath)return out;
  out.available=true;out.objectId=id;out.geometry=std::get<semantic::VectorPathContent>(object->content).geometry;out.transform=object->transform;out.appearance=runtime_.appearanceValuesFor(*object);out.generation=runtime_.semanticGeneration().value();return out;
}

}
