#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "cpu_pending_erase_state.hpp"
#include "partial_erase_geometry.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/image_resource.hpp"
#include "canvas/semantic/normalizer.hpp"
#include "canvas/scene/hierarchy_geometry.hpp"
#include <algorithm>
namespace canvas::interaction {
namespace {
struct PendingAdmission final {
 CanvasEditingRuntime& runtime;input::PointerKey key;
 ~PendingAdmission(){runtime.cancelPendingLocal(key);}
};
void updatePendingBounds(RuntimeSceneRecord& record){const auto& t=record.transform;const auto& b=record.visualBounds;bool first=true;foundation::WorldRect out;for(float x:{b.left,b.right})for(float y:{b.top,b.bottom}){const float wx=static_cast<float>(t.a*x+t.c*y+t.tx),wy=static_cast<float>(t.b*x+t.d*y+t.ty);if(first){out={wx,wy,wx,wy};first=false;}else{out.left=std::min(out.left,wx);out.top=std::min(out.top,wy);out.right=std::max(out.right,wx);out.bottom=std::max(out.bottom,wy);}}record.worldBounds=out;}
}
std::uint64_t CanvasEditingRuntime::lastPendingLocalSerial()const noexcept{return cpuPending_?cpuPending_->lastAdmitted:0;}
bool CanvasEditingRuntime::pendingLocalEnabled()const noexcept{return cpuPending_&&cpuPending_->enabled&&!cpuPending_->headApplying;}
bool CanvasEditingRuntime::pendingHistoryBarrier()const noexcept{
 if(!cpuPending_)return false;for(const auto& p:cpuPending_->slots)if(p&&p->serial&&(p->kind==CpuPendingEraseState::Kind::Undo||p->kind==CpuPendingEraseState::Kind::Redo))return true;return false;
}
bool CanvasEditingRuntime::reservePendingLocal(input::PointerKey key,std::string_view kind)noexcept{
 if(!pendingLocalEnabled())return true;
 if(!canonicalWriteAdmission()||pendingHistoryBarrier()||cpuPending_->unavailable||cpuPending_->nextActive==UINT64_MAX||cpuPending_->nextSerial>=(std::uint64_t{1}<<63)-1)return false;
 for(const auto& p:cpuPending_->slots)if(p&&!p->serial&&p->key==key)return true;
 auto slot=std::find_if(cpuPending_->slots.begin(),cpuPending_->slots.end(),[](const auto& p){return !p;});if(slot==cpuPending_->slots.end())return false;
 const auto grant=reserveCpuWorkerGrant(cpuPending_->executor,65536);if(!grant)return false;
 try{auto p=std::make_unique<CpuPendingEraseState::Intent>();p->activeSerial=++cpuPending_->nextActive;p->kind=kind=="transform"?CpuPendingEraseState::Kind::Transform:kind=="brush"?CpuPendingEraseState::Kind::Brush:CpuPendingEraseState::Kind::Create;p->key=key;p->traceGrant=grant;p->grantExecutor=cpuPending_->executor;*slot=std::move(p);++cpuPending_->revision;return true;}catch(...){(void)releaseCpuWorkerGrant(cpuPending_->executor,grant);return false;}
}
void CanvasEditingRuntime::cancelPendingLocal(input::PointerKey key)noexcept{
 if(!cpuPending_)return;for(auto& p:cpuPending_->slots)if(p&&!p->serial&&p->kind!=CpuPendingEraseState::Kind::Erase&&p->key==key){const auto token=p->traceGrant,executor=p->grantExecutor;p.reset();(void)releaseCpuWorkerGrant(executor,token);++cpuPending_->revision;return;}
}
const semantic::ObjectRecord* CanvasEditingRuntime::pendingObject(foundation::ObjectId id)const noexcept{
 if(cpuPending_)for(const auto& p:cpuPending_->slots)if(p&&p->serial)for(const auto& object:p->creates)if(object.id==id)return &object;
 return semanticObjects_.find(id);
}
std::optional<semantic::Transform2D> CanvasEditingRuntime::pendingPose(foundation::ObjectId id)const{
 const auto resolve=[&](auto&& self,foundation::ObjectId objectId,std::uint64_t through,unsigned depth)->std::optional<semantic::Transform2D>{
  if(depth>128)return {};
  const auto* object=semanticObjects_.find(objectId);
  const semantic::Transform2D* finalWorld=nullptr;std::uint64_t poseSerial=0;
  if(cpuPending_)for(const auto& p:cpuPending_->slots)if(p&&p->serial&&p->serial<=through){
    for(const auto& created:p->creates)if(created.id==objectId)object=&created;
    for(const auto& item:p->transforms.items)if(item.object_id==objectId&&p->serial>=poseSerial){finalWorld=&item.transform;poseSerial=p->serial;}
  }
  if(!object)return {};
  auto local=object->transform;
  if(finalWorld){local=*finalWorld;if(object->placement.parent_id){const auto parentAt=self(self,*object->placement.parent_id,poseSerial,depth+1);semantic::Transform2D inverse;if(!parentAt||!scene::inverseTransform(*parentAt,inverse))return {};local=scene::composeTransform(inverse,local);}}
  if(object->placement.parent_id){const auto parent=self(self,*object->placement.parent_id,through,depth+1);if(!parent)return {};return scene::composeTransform(*parent,local);}return local;
 };
 return resolve(resolve,id,UINT64_MAX,0);
}

std::optional<RuntimeSceneRecord> CanvasEditingRuntime::pendingSceneRecord(foundation::ObjectId id)const{
 const auto pose=pendingPose(id);if(!pose)return {};
 if(sceneCoordinator_)if(const auto* current=sceneCoordinator_->runtimeScene().find(id)){auto record=*current;record.transform=*pose;updatePendingBounds(record);return record;}
 const auto* object=pendingObject(id);if(!object)return {};
 std::vector<semantic::ObjectRecord> values{*object};if(object->placement.parent_id)if(const auto* parent=pendingObject(*object->placement.parent_id))values.push_back(*parent);auto projected=projectRuntimeScene(values,semanticGeneration_.current(),textLayoutService_,true);const auto* found=projected.find(id);if(!found)return {};auto record=*found;record.transform=*pose;updatePendingBounds(record);return record;
}
bool CanvasEditingRuntime::pendingSelectionCreated()const noexcept{return !selection_.primary().isZero()&&!semanticObjects_.contains(selection_.primary())&&pendingObject(selection_.primary());}
void CanvasEditingRuntime::selectPendingCreated(foundation::ObjectId id)noexcept{
 (void)setSelectionAcquisitionMode(runtime::CanvasSelectionMode::kPoint);(void)selection_.click(id);++cpuPending_->revision;canonicalPresentationDirty_=true;transientOverlayDirty_=true;(void)renderSelectionOverlay();
}
bool CanvasEditingRuntime::enqueuePendingCreate(std::vector<semantic::ObjectRecord> objects,bool editText)noexcept{
 if(!pendingLocalEnabled()||objects.empty()||pendingHistoryBarrier())return false;
 try{
  if(!reservePendingLocal(displayInputKey_,"create"))return false;PendingAdmission admission{*this,displayInputKey_};
  CpuPendingEraseState::Intent* target=nullptr;for(auto& p:cpuPending_->slots)if(p&&!p->serial&&p->kind==CpuPendingEraseState::Kind::Create&&p->key==displayInputKey_){target=p.get();break;}if(!target||cpuPending_->nextSerial==UINT64_MAX)return false;
  // The prototype remains immutable; head resolves order after prior creates.
  std::size_t bytes=65536;for(const auto& object:objects){if(const auto* table=std::get_if<semantic::TableContent>(&object.content))bytes+=table->cells.size()*sizeof(semantic::TableCell);}
  auto quota=skiaRenderer_.reserveCpuPendingMetadata(bytes);if(!quota)return false;
  for(auto& object:objects){std::vector<std::uint8_t> order;for(const auto& existing:semanticObjects_.children(object.placement.parent_id))if(order.empty()||semantic::OrderKey(order)<existing.placement.order_key)order.assign(existing.placement.order_key.bytes().begin(),existing.placement.order_key.bytes().end());for(const auto& prior:cpuPending_->slots)if(prior&&prior->serial)for(const auto& existing:prior->creates)if(existing.placement.parent_id==object.placement.parent_id&&(order.empty()||semantic::OrderKey(order)<existing.placement.order_key))order.assign(existing.placement.order_key.bytes().begin(),existing.placement.order_key.bytes().end());if(order.size()==semantic::OrderKey::kMaxSize)return false;order.push_back(128);object.placement.order_key=semantic::OrderKey(std::move(order));}
  semantic::Operation prototype{semantic::OperationId{foundation::ObjectId::fromUint64(1)},documentId_,1,1,semantic::InsertObjectsOp{std::move(objects)}};
  auto normalized=semantic::normalizeOperation(prototype);if(!normalized.ok())return false;objects=std::get<semantic::InsertObjectsOp>(std::move(normalized.value.payload)).objects;
  for(const auto& object:objects)if(object.placement.parent_id&&!semanticObjects_.contains(*object.placement.parent_id))for(const auto& prior:cpuPending_->slots)if(prior&&prior->serial)for(const auto& created:prior->creates)if(created.id==*object.placement.parent_id)target->dependencies.push_back(prior->serial);
  const auto id=objects.front().id;
  const auto displaySession=(std::uint64_t{1}<<63)|(cpuPending_->nextSerial+1);if(displayOwner_&&!reserveDisplay(displaySession,DisplayKind::kLocal))return false;target->session=displaySession;
  target->prefixMetadata=std::move(*quota);target->creates=std::move(objects);target->editText=editText;target->serial=++cpuPending_->nextSerial;target->phase=CpuPendingEraseState::Phase::Ready;cpuPending_->lastAdmitted=target->serial;++cpuPending_->admitted;++cpuPending_->revision;
  selectPendingCreated(id);target->focusRevision=selection_.revision();return true;
 }catch(...){return false;}
}
bool CanvasEditingRuntime::enqueuePendingTransform(const semantic::SetTransformsOp& values)noexcept{
 if(!pendingLocalEnabled()||values.items.empty()||pendingHistoryBarrier())return false;
 try{
  if(!reservePendingLocal(editingPointer_,"transform"))return false;PendingAdmission admission{*this,editingPointer_};
  CpuPendingEraseState::Intent* target=nullptr;for(auto& p:cpuPending_->slots)if(p&&!p->serial&&p->kind==CpuPendingEraseState::Kind::Transform&&p->key==editingPointer_){target=p.get();break;}if(!target||cpuPending_->nextSerial==UINT64_MAX)return false;
  for(const auto& value:values.items){const auto* record=pendingObject(value.object_id);const auto pose=pendingPose(value.object_id);if(!record||!pose)return false;target->initialPoses.push_back({value.object_id,*pose});if(auto projected=pendingSceneRecord(value.object_id))target->transformBasis.push_back(std::move(*projected));if(!semanticObjects_.contains(value.object_id))for(const auto& prior:cpuPending_->slots)if(prior&&prior->serial)for(const auto& created:prior->creates)if(created.id==value.object_id)target->dependencies.push_back(prior->serial);}
  target->transforms=values;
  const auto displaySession=(std::uint64_t{1}<<63)| (cpuPending_->nextSerial+1);
  if(displayOwner_&&!reserveDisplay(displaySession,DisplayKind::kLocal))return false;target->session=displaySession;target->serial=++cpuPending_->nextSerial;target->phase=CpuPendingEraseState::Phase::Ready;target->focusRevision=selection_.revision();cpuPending_->lastAdmitted=target->serial;++cpuPending_->admitted;++cpuPending_->revision;transientOverlayDirty_=true;canonicalPresentationDirty_=true;return true;
 }catch(...){return false;}
}
bool CanvasEditingRuntime::queuePendingHistory(bool redo)noexcept{
 if(!pendingLocalEnabled()||pendingHistoryBarrier()||!reservePendingLocal({},"history"))return false;
 PendingAdmission admission{*this,{}};for(auto& p:cpuPending_->slots)if(p&&!p->serial&&!p->key.valid()&&p->kind==CpuPendingEraseState::Kind::Create){p->kind=redo?CpuPendingEraseState::Kind::Redo:CpuPendingEraseState::Kind::Undo;p->serial=++cpuPending_->nextSerial;p->phase=CpuPendingEraseState::Phase::Ready;cpuPending_->lastAdmitted=p->serial;++cpuPending_->admitted;++cpuPending_->revision;return true;}return false;
}
bool CanvasEditingRuntime::pendingTransformBasisCompatible()const noexcept{
 if(!pendingLocalEnabled()||!editingPendingBasis_)return false;
 try{const auto now=pendingSceneRecord(editingPendingBasis_->objectId);const auto* object=pendingObject(editingPendingBasis_->objectId);return now&&object&&now->transform==editingPendingBasis_->transform&&now->placement==editingPendingBasis_->placement&&now->content==editingPendingBasis_->content&&textWritable(object->id);}catch(...){return false;}
}
PendingLocalValue CanvasEditingRuntime::pendingLocalValue()const{
 PendingLocalValue out;if(!cpuPending_)return out;out.owner={cpuPending_->basis[0],cpuPending_->basis[1]};out.revision=cpuPending_->revision;
 std::vector<const CpuPendingEraseState::Intent*> ordered;for(const auto& p:cpuPending_->slots)if(p)ordered.push_back(p.get());std::stable_sort(ordered.begin(),ordered.end(),[](auto a,auto b){return a->serial<b->serial;});
 for(const auto* p:ordered){PendingLocalIntentValue v;v.token={cpuPending_->basis[0],cpuPending_->basis[1],p->serial?p->serial:p->activeSerial};v.kind=p->kind==CpuPendingEraseState::Kind::Erase?"erase":p->kind==CpuPendingEraseState::Kind::Transform?"transform":p->kind==CpuPendingEraseState::Kind::Undo?"undo":p->kind==CpuPendingEraseState::Kind::Redo?"redo":p->kind==CpuPendingEraseState::Kind::Image?"imageImport":p->kind==CpuPendingEraseState::Kind::Brush?"create":"create";v.phase=!p->serial?"active":p->phase==CpuPendingEraseState::Phase::Waiting?"waiting":p->phase==CpuPendingEraseState::Phase::Ready?"ready":p->phase==CpuPendingEraseState::Phase::Frozen?"queued":"preparing";for(const auto& object:p->creates)v.objects.push_back(object.id);for(const auto& t:p->transforms.items)v.objects.push_back(t.object_id);if(!p->serial&&p->kind==CpuPendingEraseState::Kind::Transform&&editingPendingBasis_)v.objects.push_back(editingPendingBasis_->objectId);v.dependencies=p->dependencies;out.intents.push_back(std::move(v));}
 if(!out.intents.empty()){out.selected.assign(selection_.selected().begin(),selection_.selected().end());if(auto record=pendingSceneRecord(selection_.primary())){const auto& b=record->visualBounds;const auto* active=transformOverrides_.find(record->objectId);const auto& t=active?*active:record->transform;float l=0,r=0,top=0,bottom=0;bool first=true;for(float x:{b.left,b.right})for(float y:{b.top,b.bottom}){const float wx=static_cast<float>(t.a*x+t.c*y+t.tx),wy=static_cast<float>(t.b*x+t.d*y+t.ty);if(first){l=r=wx;top=bottom=wy;first=false;}else{l=std::min(l,wx);r=std::max(r,wx);top=std::min(top,wy);bottom=std::max(bottom,wy);}}out.bounds=std::array{l,top,r,bottom};}}
 return out;
}
bool CanvasEditingRuntime::servicePendingLocalHead()noexcept{
 if(!cpuPending_||!cpuPending_->head())return false;auto& state=*cpuPending_;auto* p=state.head();if(p->kind==CpuPendingEraseState::Kind::Erase)return false;
 if(!canonicalWriteAdmission()||!historySceneReady())return true;
 if(p->kind==CpuPendingEraseState::Kind::Image)return true;
 bool ok=false;const auto generation=semanticGeneration_.current();
 try{
  state.headApplying=true;
  if(p->kind==CpuPendingEraseState::Kind::Create){
   auto objects=p->creates;
   // Final order keys were reserved against the ordered predecessor projection.

   std::vector<semantic::ObjectId> ids;for(const auto& object:objects)ids.push_back(object.id);
   const semantic::Operation forward{allocateOperationId(),documentId_,1,1,semantic::InsertObjectsOp{std::move(objects)}};
   ok=applyLocalHistoryOperation(forward,{allocateOperationId(),documentId_,1,1,semantic::DeleteObjectsOp{std::move(ids)}});
  }else if(p->kind==CpuPendingEraseState::Kind::Brush){
   if(p->brush)p->brush->id=localOperationIdentity(localOperationOrdinal());
   decltype(pendingCanonicalIdentities_) staged;staged.emplace(p->session,ink::CanonicalHandoffIdentity{});auto handoff=staged.extract(p->session);pendingCanonicalIdentities_.reserve(pendingCanonicalIdentities_.size()+1);
   ok=p->brush&&p->brushInverse&&applyLocalHistoryOperation(*p->brush,*p->brushInverse);
   if(ok&&lastAppliedCommit_){
    if(auto* display=findDisplay(p->session,DisplayKind::kBrush))sealDisplay(*display,*lastAppliedCommit_,p->creates.front().id);
    const ink::CanonicalHandoffIdentity identity{1U,p->session,1U,lastAppliedCommit_->operation_id,lastAppliedCommit_->commit_stamp,render::SurfaceGeneration{previewProvider_?previewProvider_->generation():surface_.generation}};
    if(canonicalVisibilitySink_)(void)canonicalVisibilitySink_->canonicalCommitted(identity,*lastAppliedCommit_);
    lastCanonicalIdentity_=identity;handoff.mapped()=identity;pendingCanonicalIdentities_.insert(std::move(handoff));
   }
  }else if(p->kind==CpuPendingEraseState::Kind::Transform){
   auto values=p->transforms;semantic::SetTransformsOp inverse;
   ok=true;
   for(auto& item:values.items){const auto* current=semanticObjects_.find(item.object_id);if(!current||!textWritable(item.object_id)){ok=false;break;}
    const auto initial=std::find_if(p->initialPoses.begin(),p->initialPoses.end(),[&](const auto& v){return v.first==item.object_id;});const auto* projected=sceneCoordinator_->runtimeScene().find(item.object_id);const auto body=std::find_if(p->transformBasis.begin(),p->transformBasis.end(),[&](const auto& v){return v.objectId==item.object_id;});if(initial==p->initialPoses.end()||!projected||projected->transform!=initial->second||body==p->transformBasis.end()||body->content!=projected->content||body->placement!=projected->placement||body->properties!=projected->properties){ok=false;break;}inverse.items.push_back({item.object_id,current->transform});if(current->placement.parent_id){const auto* parent=sceneCoordinator_->runtimeScene().find(*current->placement.parent_id);semantic::Transform2D inv;if(!parent||!scene::inverseTransform(parent->transform,inv)){ok=false;break;}item.transform=scene::composeTransform(inv,item.transform);}}
   if(ok)ok=applyLocalHistoryOperation({allocateOperationId(),documentId_,1,1,std::move(values)},{allocateOperationId(),documentId_,1,1,std::move(inverse)});
  }else if(p->kind==CpuPendingEraseState::Kind::Undo)ok=history_.undo();
  else if(p->kind==CpuPendingEraseState::Kind::Redo)ok=history_.redo();
  state.headApplying=false;
 }catch(...){state.headApplying=false;ok=false;}
 const auto focusId=p->creates.empty()?foundation::ObjectId{}:p->creates.front().id;
 const bool enterText=ok&&p->kind==CpuPendingEraseState::Kind::Create&&p->editText&&selection_.revision()==p->focusRevision&&selection_.primary()==focusId&&selectionMode_&&!creationActive()&&!selectionTransformActive();
 const auto failedPrimary=!ok&&std::any_of(p->creates.begin(),p->creates.end(),[&](const auto& object){return object.id==selection_.primary();});
 const auto serial=p->serial;const auto session=p->session;const auto key=p->key;const auto grant=p->traceGrant,executor=p->grantExecutor;
 if(ok){++state.committed;if(lastAppliedCommit_)if(auto* display=findDisplay(session,DisplayKind::kLocal))sealDisplay(*display,*lastAppliedCommit_);}else{cancelDisplay(session,p->kind==CpuPendingEraseState::Kind::Brush?DisplayKind::kBrush:DisplayKind::kLocal);++state.failed;state.error="LOCAL_INTENT_FAILED";}
 if(state.terminals.size()==64)state.terminals.erase(state.terminals.begin());state.terminals.push_back({serial,session,0,ok&&semanticGeneration_.current()!=generation?canonicalCommitFence().ordinal.value():0,{},ok?"Committed":"Failed",semanticGeneration_.current().value()});
 for(auto& slot:state.slots)if(slot.get()==p){slot.reset();break;}
 (void)key;(void)releaseCpuWorkerGrant(executor,grant);++state.revision;transientOverlayDirty_=true;canonicalPresentationDirty_=true;
 if(failedPrimary)selection_.cancel();
 if(!ok)failPendingDependents(serial);
 if(enterText)selectCreatedObject(focusId,true);
 return true;
}
void CanvasEditingRuntime::failPendingDependents(std::uint64_t serial)noexcept{
 if(!cpuPending_)return;std::array<std::uint64_t,9> failed{};failed[0]=serial;std::size_t count=1;
 for(std::size_t next=0;next<count;++next)for(auto& p:cpuPending_->slots)if(p&&std::find(p->dependencies.begin(),p->dependencies.end(),failed[next])!=p->dependencies.end()){
  if(count<failed.size())failed[count++]=p->serial;
  cancelDisplay(p->session,p->kind==CpuPendingEraseState::Kind::Brush?DisplayKind::kBrush:p->kind==CpuPendingEraseState::Kind::Erase?DisplayKind::kErase:DisplayKind::kLocal);
  for(const auto& object:p->creates){if(object.id==selection_.primary())selection_.cancel();if(imageResolver_)if(const auto* image=std::get_if<semantic::ImageContent>(&object.content))imageResolver_->releasePending(image->resource_id);}
  if(cpuPending_->terminals.size()==64)cpuPending_->terminals.erase(cpuPending_->terminals.begin());cpuPending_->terminals.push_back({p->serial,p->session,0,0,{},"FailedDependency",semanticGeneration_.current().value()});
  const auto trace=p->traceGrant,payload=p->payloadGrant,transport=p->transportGrant,e=p->grantExecutor,pe=p->payloadExecutor,te=p->transportExecutor;
  p.reset();(void)releaseCpuWorkerGrant(e,trace);if(payload)(void)releaseCpuWorkerGrant(pe,payload);if(transport)(void)releaseCpuWorkerGrant(te,transport);++cpuPending_->failed;++cpuPending_->revision;
 }
}
bool CanvasEditingRuntime::applyPendingPresentation(render::ReferenceDrawList& list)const{
 if(!cpuPending_||!cpuPendingBusy())return true;
 // The suffix is a read-only presentation over canonical sources. Body/pose
 // data is not applied to ObjectStore and never enters capture or sync.
 std::vector<const CpuPendingEraseState::Intent*> ordered;for(const auto& p:cpuPending_->slots)if(p&&p->serial)ordered.push_back(p.get());std::sort(ordered.begin(),ordered.end(),[](auto a,auto b){return a->serial<b->serial;});
 for(const auto* p:ordered){
  for(const auto& object:p->creates){auto record=pendingSceneRecord(object.id);if(!record)continue;auto command=render::DirectReferenceSource::presentationCommand(*record);if(!command)return false;render::ReferenceTraversalEntry entry;entry.record=*record;entry.contributesPixels=true;entry.command=std::move(command.value());list.entries.push_back(std::move(entry));}
  for(const auto& item:p->transforms.items)for(auto& entry:list.entries)if(entry.record.objectId==item.object_id)entry.record.transform=item.transform;
 }
 for(auto& entry:list.entries)if(const auto pose=pendingPose(entry.record.objectId)){entry.record.transform=*pose;updatePendingBounds(entry.record);}
 // Compare ancestor order paths, preserving the existing root ink ordering.
 const auto path=[&](foundation::ObjectId id){std::vector<const semantic::ObjectRecord*> result;for(unsigned depth=0;depth<128;++depth){const auto* object=pendingObject(id);if(!object)break;result.push_back(object);if(!object->placement.parent_id)break;id=*object->placement.parent_id;}std::reverse(result.begin(),result.end());return result;};
 std::stable_sort(list.entries.begin(),list.entries.end(),[&](const auto& a,const auto& b){const auto left=path(a.record.objectId),right=path(b.record.objectId);std::size_t i=0;while(i<std::min(left.size(),right.size())&&left[i]->id==right[i]->id)++i;if(i==std::min(left.size(),right.size()))return left.size()<right.size();if(i==0){const auto ink=[](auto kind){return kind==semantic::ObjectKind::kVectorStroke||kind==semantic::ObjectKind::kDabStroke;};if(ink(left[i]->kind)!=ink(right[i]->kind))return !ink(left[i]->kind);}if(left[i]->placement.order_key!=right[i]->placement.order_key)return left[i]->placement.order_key<right[i]->placement.order_key;return left[i]->id<right[i]->id;});list.visibleRecords=list.entries.size();list.canonicalBytes.clear();list.digest.clear();return true;
}
}

namespace canvas::interaction {
bool CanvasEditingRuntime::enqueuePendingImage(runtime::ImageImportRequest request,semantic::ObjectRecord object)noexcept{
 if(!pendingLocalEnabled()||pendingHistoryBarrier()||!reservePendingLocal({},"image"))return false;
 PendingAdmission admission{*this,{}};try{auto quota=skiaRenderer_.reserveCpuPendingMetadata(request.bytes.capacity()+65536);if(!quota)return false;
  semantic::Operation prototype{semantic::OperationId{foundation::ObjectId::fromUint64(1)},documentId_,1,1,semantic::InsertObjectsOp{{std::move(object)}}};auto normalized=semantic::normalizeOperation(prototype);if(!normalized.ok())return false;object=std::move(std::get<semantic::InsertObjectsOp>(normalized.value.payload).objects.front());
  for(auto& p:cpuPending_->slots)if(p&&!p->serial&&!p->key.valid()&&p->kind==CpuPendingEraseState::Kind::Create){p->kind=CpuPendingEraseState::Kind::Image;p->prefixMetadata=std::move(*quota);p->image=std::move(request);p->creates.push_back(std::move(object));p->serial=++cpuPending_->nextSerial;p->phase=CpuPendingEraseState::Phase::Ready;cpuPending_->lastAdmitted=p->serial;++cpuPending_->admitted;++cpuPending_->revision;selectPendingCreated(p->creates.front().id);p->focusRevision=selection_.revision();return true;}
 }catch(...){}return false;
}
std::optional<std::pair<std::uint64_t,runtime::ImageImportRequest>> CanvasEditingRuntime::pendingHeadImage()const{
 if(!cpuPending_||!cpuPending_->head()||!canonicalWriteAdmission())return {};const auto* p=cpuPending_->head();if(p->kind!=CpuPendingEraseState::Kind::Image||!p->image)return {};auto result=std::pair{p->serial,*p->image};cpuPending_->headApplying=true;return result;
}
void CanvasEditingRuntime::completePendingImage(std::uint64_t serial,bool ok)noexcept{
 if(!cpuPending_)return;cpuPending_->headApplying=false;auto* p=cpuPending_->head();if(!p||p->serial!=serial)return;
 if(cpuPending_->terminals.size()==64)cpuPending_->terminals.erase(cpuPending_->terminals.begin());cpuPending_->terminals.push_back({serial,0,0,ok?canonicalCommitFence().ordinal.value():0,{},ok?"Committed":"Failed",semanticGeneration_.current().value()});if(ok)++cpuPending_->committed;else ++cpuPending_->failed;
 if(!ok&&std::any_of(p->creates.begin(),p->creates.end(),[&](const auto& object){return object.id==selection_.primary();}))selection_.cancel();
 if(imageResolver_&&!p->creates.empty())if(const auto* image=std::get_if<semantic::ImageContent>(&p->creates.front().content))imageResolver_->releasePending(image->resource_id);

 if(!ok)failPendingDependents(serial);
 const auto grant=p->traceGrant,executor=p->grantExecutor;for(auto& slot:cpuPending_->slots)if(slot.get()==p){slot.reset();break;}(void)releaseCpuWorkerGrant(executor,grant);++cpuPending_->revision;transientOverlayDirty_=true;canonicalPresentationDirty_=true;
}
}
