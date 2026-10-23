#include "canvas/runtime/canvas_runtime_facade.hpp"
#include "canvas/runtime/runtime_composition.hpp"

namespace canvas::runtime {
namespace {
ConnectorCommandReceipt rejectConnector(ConnectorFailureReason reason) { ConnectorCommandReceipt r{}; r.reason=reason; return r; }
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

ConnectorSnapshot CanvasRuntimeFacade::readSelectedConnector() const { ConnectorSnapshot r{}; const auto* o=runtime_.textRecord(runtime_.selection_.primary()); if(o&&o->kind==semantic::ObjectKind::kConnector){r.available=true;r.objectId=o->id;r.content=std::get<semantic::ConnectorContent>(o->content);} return r; }
StickySnapshot CanvasRuntimeFacade::readSelectedSticky() const { StickySnapshot r{}; const auto* o=runtime_.textRecord(runtime_.selection_.primary()); if(o&&o->kind==semantic::ObjectKind::kSticky){r.available=true;r.objectId=o->id;r.content=std::get<semantic::StickyContent>(o->content);} return r; }
}
