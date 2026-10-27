#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/scene/hierarchy_geometry.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "undoable_command.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace canvas::interaction {
namespace {
using namespace semantic;
bool container(const ObjectRecord* r) { return r&&(r->kind==ObjectKind::kGroup||r->kind==ObjectKind::kSection); }
ContainerStructuralExpectation node(const ObjectRecord& r) { return {r.id,r.kind,r.kind_version,r.placement,r.transform}; }
OrderKey ordinalKey(std::size_t n) {
    const auto v=static_cast<std::uint32_t>(n);
    return OrderKey({static_cast<std::uint8_t>(v>>24),static_cast<std::uint8_t>(v>>16),
        static_cast<std::uint8_t>(v>>8),static_cast<std::uint8_t>(v),1});
}
bool under(ObjectId id,ObjectId ancestor,const ObjectStore& store) {
    for(std::size_t n=0;n<=store.size();++n) {
        if(id==ancestor)return true;
        const auto* r=store.find(id);if(!r||!r->placement.parent_id)return false;
        id=*r->placement.parent_id;
    }
    return false;
}
std::vector<ObjectId> roots(std::span<const ObjectId> ids,const ObjectStore& store) {
    std::vector<ObjectId> result;
    for(auto id:ids) {
        if(!store.find(id))return {};
        if(std::any_of(ids.begin(),ids.end(),[&](auto a){return a!=id&&under(id,a,store);}))continue;
        if(std::find(result.begin(),result.end(),id)==result.end())result.push_back(id);
    }
    return result;
}
bool finite(const Transform2D& t) {
    return std::isfinite(t.a)&&std::isfinite(t.b)&&std::isfinite(t.c)&&std::isfinite(t.d)&&std::isfinite(t.tx)&&std::isfinite(t.ty);
}
bool renderableBoundary(const SectionContent& content,const Transform2D& world) {
    const auto& b=content.boundary;
    if(!finite(world)||b.width<=0||b.height<=0)return false;
    for(double x:{b.x,b.x+b.width})for(double y:{b.y,b.y+b.height}) {
        const double wx=(world.a*x+world.c*y)+world.tx,wy=(world.b*x+world.d*y)+world.ty;
        if(!std::isfinite(wx)||!std::isfinite(wy)||!std::isfinite(static_cast<float>(wx))||!std::isfinite(static_cast<float>(wy)))return false;
    }
    return true;
}
bool quality(const Transform2D& oldWorld,const Transform2D& parentWorld,
             const Transform2D& localAfter,const foundation::WorldRect& localBounds) {
    const auto actual=scene::composeTransform(parentWorld,localAfter);
    if(!finite(actual))return false;
    for(double x:{static_cast<double>(localBounds.left),static_cast<double>(localBounds.right)})
      for(double y:{static_cast<double>(localBounds.top),static_cast<double>(localBounds.bottom)}) {
        const double before[]{(oldWorld.a*x+oldWorld.c*y)+oldWorld.tx,(oldWorld.b*x+oldWorld.d*y)+oldWorld.ty};
        const double after[]{(actual.a*x+actual.c*y)+actual.tx,(actual.b*x+actual.d*y)+actual.ty};
        for(unsigned i=0;i<2;++i)if(!std::isfinite(before[i])||!std::isfinite(after[i])||
          std::abs(before[i]-after[i])>std::ldexp(1.0,-32)+std::ldexp(1.0,-47)*std::max({1.0,std::abs(before[i]),std::abs(after[i])}))return false;
      }
    return true;
}
}

bool CanvasEditingRuntime::sectionBoundaryPointer(const input::PointerKey& key,input::PointerPhase phase,
                                                  float x,float y) noexcept {
    if(!sectionBoundaryDrag_||key!=editingPointer_)return false;
    const auto captured=*sectionBoundaryDrag_;
    if(captured.generation!=semanticGeneration().value()||captured.camera!=cameraGeneration()||
       captured.metrics!=surface_.metricsGeneration||phase==input::PointerPhase::kCancel) {
        cancelSelectionTransform();(void)clearTransientOverlay();return false;
    }
    semantic::Transform2D inverse{};
    if(!scene::inverseTransform(captured.world,inverse)){cancelSelectionTransform();return false;}
    const auto world=viewportController_->viewToContent(x,y);
    const double dx=world.first-editingDownWorld_.x,dy=world.second-editingDownWorld_.y;
    const double lx=inverse.a*dx+inverse.c*dy,ly=inverse.b*dx+inverse.d*dy;
    auto b=captured.initial.boundary;
    using H=render::HandleKind;const auto h=editingHandle_;
    if(h==H::kLeft||h==H::kTopLeft||h==H::kBottomLeft){b.x+=lx;b.width-=lx;}
    if(h==H::kRight||h==H::kTopRight||h==H::kBottomRight)b.width+=lx;
    if(h==H::kTop||h==H::kTopLeft||h==H::kTopRight){b.y+=ly;b.height-=ly;}
    if(h==H::kBottom||h==H::kBottomLeft||h==H::kBottomRight)b.height+=ly;
    const bool valid=std::isfinite(b.x)&&std::isfinite(b.y)&&std::isfinite(b.width)&&std::isfinite(b.height)&&b.width>0&&b.height>0&&std::isfinite(b.x+b.width)&&std::isfinite(b.y+b.height);
    sectionBoundaryDrag_->valid=valid;
    if(valid)sectionBoundaryDrag_->preview.boundary=b;
    (void)renderSelectionOverlay();canonicalPresentationDirty_=true;
    (void)presentTransientOverlayFrame(nextCanonicalFrameId());
    if(phase==input::PointerPhase::kUp) {
        auto intent=runtime::SectionIntent{};intent.action=runtime::SectionAction::kResize;
        intent.objectId=captured.object;intent.content=sectionBoundaryDrag_->preview;
        intent.expectedSemanticGeneration=captured.generation;
        sectionBoundaryDrag_.reset();editingPointer_={};
        if(!valid){(void)clearTransientOverlay();return false;}
        const auto result=executeSectionIntent(intent);
        const bool accepted=result.failure==runtime::SectionFailure::kNone;
        transientOverlayHandoffPending_=accepted&&result.canonical==runtime::SectionCanonicalOutcome::kCommitted;
        if(!transientOverlayHandoffPending_)(void)clearTransientOverlay();
        return accepted;
    }
    return true;
}

void CanvasEditingRuntime::applyContainerPreview(render::ReferenceDrawList& list) {
    const auto* selected=semanticObjects_.find(selection_.primary());
    if(!sectionBoundaryDrag_&&(!container(selected)||!transformDrag_.active()))return;
    auto objects=semanticObjects_.allObjects();
    for(auto& object:objects) {
        if(const auto* pose=transformOverrides_.find(object.id)) {
            Transform2D parentInverse{};
            if(object.placement.parent_id) {
                const auto* parent=sceneCoordinator_->runtimeScene().find(*object.placement.parent_id);
                if(!parent||!scene::inverseTransform(parent->transform,parentInverse))return;
            }
            object.transform=scene::composeTransform(parentInverse,*pose);
        }
        if(sectionBoundaryDrag_&&object.id==sectionBoundaryDrag_->object)object.content=sectionBoundaryDrag_->preview;
    }
    const auto world=scene::projectWorldObjects(objects);
    for(auto& entry:list.entries) {
        const auto found=std::find_if(world.begin(),world.end(),[&](const auto& r){return r.id==entry.record.objectId;});
        if(found==world.end())continue;
        entry.record.transform=found->transform;
        if(found->kind==ObjectKind::kConnector) {
            entry.record.derivedGeometry=previewConnectorGeometry(*found);
        }
        const auto bounds=scene::computeBounds(*found,entry.record.textLayout.get(),
            std::get_if<scene::ConnectorGeometry>(&entry.record.derivedGeometry));
        entry.record.geometryBounds=bounds.geometry;entry.record.visualBounds=bounds.visual;entry.record.worldBounds=bounds.world;
        if(found->kind==ObjectKind::kSection) {
            entry.record.content=found->content;
            auto layout=entry.record.textLayout;
            if(textLayoutService_)layout=textLayoutService_->resolveSectionTitle(found->id,std::get<SectionContent>(found->content));
            entry.command=render::SectionReferenceCommand{std::get<SectionContent>(found->content),layout};
        }
    }
    list.canonicalBytes.clear();list.digest.clear();
}

std::optional<runtime::SectionOutcome> CanvasEditingRuntime::removeSelectedSections() noexcept {
    try {
    const auto chosen=roots(selection_.selected(),semanticObjects_);
    const auto sections=std::count_if(chosen.begin(),chosen.end(),[&](auto id){return semanticObjects_.find(id)->kind==ObjectKind::kSection;});
    if(sections==0)return std::nullopt;
    if(sections!=static_cast<std::ptrdiff_t>(chosen.size())||chosen.size()!=1) {
        runtime::SectionOutcome outcome;
        outcome.failure=sections!=static_cast<std::ptrdiff_t>(chosen.size())?runtime::SectionFailure::kUnsupportedMixedRemoval:runtime::SectionFailure::kUnsupported;
        return outcome;
    }
    runtime::SectionIntent intent;intent.action=runtime::SectionAction::kRelease;intent.objectId=chosen.front();
    intent.expectedSemanticGeneration=semanticGeneration().value();return executeSectionIntent(intent);
    } catch(const std::bad_alloc&) { runtime::SectionOutcome outcome;outcome.failure=runtime::SectionFailure::kCommandRejected;return outcome; }
}

runtime::SectionSnapshot CanvasEditingRuntime::sectionSnapshot(foundation::ObjectId id) const {
    runtime::SectionSnapshot out;out.semanticGeneration=semanticGeneration().value();out.sceneRevision=sceneRevision();
    out.selectedRoots=roots(selection_.selected(),semanticObjects_);
    out.selectionRevision=selection_.revision();
    out.groupScope=selection_.groupScope();
    out.busy=creationActive()||selectionTransformActive()||
        (interaction_&&interaction_->textSession()&&interaction_->textSession()->read().phase!=runtime::TextPhase::kClosed)||imageCropPresentation_.has_value();
    if(id.isZero())id=selection_.primary();
    const auto* object=semanticObjects_.find(id);
    const bool isSection=object&&object->kind==ObjectKind::kSection;
    for(std::size_t i=0;i<out.actions.size();++i) {
        const auto action=static_cast<runtime::SectionAction>(i);auto& availability=out.actions[i];
        availability.sessionAdmitted=!closed_&&surface_.available;
        availability.writable=canonicalWriteAdmission()&&(!object||textWritable(object->id));
        availability.contextEligible=action==runtime::SectionAction::kCreate?true:
            (action==runtime::SectionAction::kWrap||action==runtime::SectionAction::kReparent)?!out.selectedRoots.empty():
            action==runtime::SectionAction::kEnterGroupScope?object&&object->kind==ObjectKind::kGroup:
            action==runtime::SectionAction::kLeaveGroupScope?selection_.groupScope().has_value():isSection;
        availability.temporarilyReady=historySceneReady()&&!out.busy;
        availability.reason=!availability.contextEligible?runtime::SectionFailure::kInvalidTarget:
            !availability.writable||!availability.sessionAdmitted||out.busy?runtime::SectionFailure::kBusy:
            !historySceneReady()?runtime::SectionFailure::kProjectionUnavailable:runtime::SectionFailure::kNone;
    }
    if(!object||object->kind!=ObjectKind::kSection) {out.unavailableReason=runtime::SectionFailure::kInvalidTarget;return out;}
    out.available=true;out.objectId=id;out.parentId=object->placement.parent_id;
    out.content=std::get<SectionContent>(object->content);out.localTransform=object->transform;
    const auto* derived=sceneCoordinator_?sceneCoordinator_->runtimeScene().find(id):nullptr;
    out.projectionReady=historySceneReady()&&derived;
    out.unavailableReason=out.projectionReady?runtime::SectionFailure::kNone:runtime::SectionFailure::kProjectionUnavailable;
    if(out.projectionReady) {
        out.worldTransform=derived->transform;
        if(derived->textLayout) {
            out.titleDiagnostics=derived->textLayout->diagnostics;
            for(const auto& run:derived->textLayout->runs)out.titleGlyphCount+=run.glyphs.size();
            out.titleLayoutReady=out.titleDiagnostics.empty()&&(out.content.title.empty()||out.titleGlyphCount!=0);
        }
        const auto& b=out.content.boundary;const auto& t=out.worldTransform;
        const double xs[]{b.x,b.x+b.width,b.x+b.width,b.x},ys[]{b.y,b.y,b.y+b.height,b.y+b.height};
        for(unsigned i=0;i<4;++i)out.worldQuad[i]={static_cast<float>((t.a*xs[i]+t.c*ys[i])+t.tx),static_cast<float>((t.b*xs[i]+t.d*ys[i])+t.ty)};
    }
    for(const auto& child:semanticObjects_.children(id)) {
        out.directChildren.push_back(child.id);
        const auto* pose=sceneCoordinator_?sceneCoordinator_->runtimeScene().find(child.id):nullptr;
        out.members.push_back({child.id,child.kind,child.transform,out.projectionReady&&pose?pose->transform:Transform2D{}});
    }
    return out;
}

std::optional<interaction::SubmitResult> CanvasEditingRuntime::tryCommitSectionDrop(
    const semantic::SetTransformsOp& desiredWorld) noexcept {
    if(editingHandle_!=render::HandleKind::kNone||!transformDrag_.active()||desiredWorld.items.size()!=1)return std::nullopt;
    try {
        const auto id=desiredWorld.items.front().object_id;
        const auto* source=semanticObjects_.find(id);if(!source||!historySceneReady())return interaction::SubmitResult::rejected();
        std::optional<ObjectId> destination=selection_.groupScope();
        const auto records=sceneCoordinator_->runtimeScene().records();
        // Same coherent committed projection as the captured gesture. The
        // release point is tested in actual Section-local space, not its AABB.
        for(auto it=records.rbegin();it!=records.rend();++it) {
            if(it->kind!=ObjectKind::kSection||under(it->objectId,id,semanticObjects_)||
               !textWritable(it->objectId)||selection_.resolveHit(it->objectId,semanticObjects_)!=it->objectId)continue;
            Transform2D inverse{};
            if(!scene::inverseTransform(it->transform,inverse)) {
                const auto& b=it->worldBounds;const auto p=editingDropWorld_;
                if(p.x>=b.left&&p.x<=b.right&&p.y>=b.top&&p.y<=b.bottom)return interaction::SubmitResult::rejected();
                continue;
            }
            const double x=(inverse.a*editingDropWorld_.x+inverse.c*editingDropWorld_.y)+inverse.tx;
            const double y=(inverse.b*editingDropWorld_.x+inverse.d*editingDropWorld_.y)+inverse.ty;
            const auto& b=std::get<SectionContent>(it->content).boundary;
            if(x>=b.x&&x<=b.x+b.width&&y>=b.y&&y<=b.y+b.height){destination=it->objectId;break;}
        }
        if(destination==source->placement.parent_id)return std::nullopt;
        runtime::SectionIntent intent;intent.action=runtime::SectionAction::kReparent;intent.objects={id};intent.parentId=destination;
        intent.expectedSemanticGeneration=editingGeneration_.value();intent.selectionRevision=editingSelectionRevision_;
        const auto result=executeSectionCommand(intent,&desiredWorld);
        return result.failure==runtime::SectionFailure::kNone?interaction::SubmitResult::acceptedResult():interaction::SubmitResult::rejected();
    } catch(const std::bad_alloc&) {return interaction::SubmitResult::rejected();}
}

runtime::SectionOutcome CanvasEditingRuntime::executeSectionIntent(const runtime::SectionIntent& intent) noexcept {
    return executeSectionCommand(intent,nullptr);
}

runtime::SectionOutcome CanvasEditingRuntime::executeSectionCommand(const runtime::SectionIntent& intent,
    const semantic::SetTransformsOp* confirmedDrop) noexcept {
    using A=runtime::SectionAction;using Failure=runtime::SectionFailure;
    runtime::SectionOutcome result;result.objectId=intent.objectId;
    result.semanticGeneration=semanticGeneration().value();result.sceneRevision=sceneRevision();
    const auto fail=[&](Failure reason){
        if(intent.objectId.isZero()&&(intent.action==A::kCreate||intent.action==A::kWrap))result.objectId={};
        result.failure=reason;return result;
    };
    try {
        if(closed_||!canonicalWriteAdmission())return fail(Failure::kBusy);
        if(intent.expectedSemanticGeneration!=0&&intent.expectedSemanticGeneration!=semanticGeneration().value())return fail(Failure::kStale);
        if(intent.selectionRevision&&*intent.selectionRevision!=selection_.revision())return fail(Failure::kStale);
        if(!historySceneReady())return fail(Failure::kProjectionUnavailable);
        if(confirmedDrop&&(intent.action!=A::kReparent||!transformDrag_.active()||editingHandle_!=render::HandleKind::kNone||
           confirmedDrop->items.size()!=1||intent.objects.size()!=1||confirmedDrop->items.front().object_id!=intent.objects.front()))return fail(Failure::kInvalidTarget);
        if(creationActive()||(!confirmedDrop&&selectionTransformActive())||imageCropPresentation_||
           (interaction_&&interaction_->textSession()&&interaction_->textSession()->read().phase!=runtime::TextPhase::kClosed))return fail(Failure::kBusy);
        const auto* object=semanticObjects_.find(intent.objectId);
        auto chosen=roots(intent.objects,semanticObjects_);
        const bool selectionOnly=intent.action==A::kSelect||intent.action==A::kSelectChildren||intent.action==A::kEnterGroupScope||intent.action==A::kLeaveGroupScope;
        if(!selectionOnly) {
            auto checked=chosen;if(object)checked.push_back(object->id);if(intent.parentId)checked.push_back(*intent.parentId);
            if(const auto scope=selection_.groupScope())
                for(auto id:checked)if(!under(id,*scope,semanticObjects_))return fail(Failure::kInvalidHierarchy);
            for(auto id:checked)if(const auto* record=sceneCoordinator_->runtimeScene().find(id))
                for(const auto& property:record->properties.entries)
                    if(property.field_id==2U)if(const auto* locked=std::get_if<bool>(&property.value);locked&&*locked)return fail(Failure::kInvalidTarget);
        }
        SelectionSession afterSelection=selection_;
        if(intent.action==A::kEnterGroupScope||intent.action==A::kLeaveGroupScope) {
            if(intent.action==A::kEnterGroupScope&&!afterSelection.enterGroupScope(intent.objectId,semanticObjects_))return fail(Failure::kInvalidTarget);
            if(intent.action==A::kLeaveGroupScope)afterSelection.leaveGroupScope(semanticObjects_);
            selection_=std::move(afterSelection);selectionView_.reset();canonicalPresentationDirty_=true;
            result.failure=Failure::kNone;result.canonical=runtime::SectionCanonicalOutcome::kNoChange;return result;
        }
        if(intent.action==A::kSelect||intent.action==A::kSelectChildren) {
            if(!object||object->kind!=ObjectKind::kSection)return fail(Failure::kInvalidTarget);
            if(intent.action==A::kSelect) {
                const auto resolved=selection_.resolveHit(object->id,semanticObjects_);
                if(resolved.isZero())return fail(Failure::kInvalidTarget);
                (void)afterSelection.click(resolved);
            }
            else {
                std::vector<HitCandidate> candidates;
                for(const auto& child:semanticObjects_.children(object->id)) {
                    const auto resolved=selection_.resolveHit(child.id,semanticObjects_);
                    if(!resolved.isZero())candidates.push_back({resolved,true,false});
                }
                (void)afterSelection.marquee(candidates);
            }
            selection_=std::move(afterSelection);selectionView_.reset();canonicalPresentationDirty_=true;
            result.failure=Failure::kNone;result.canonical=runtime::SectionCanonicalOutcome::kNoChange;
            result.projection=runtime::SectionProjectionOutcome::kReady;return result;
        }
        Operation operation{allocateOperationId(),documentId_,1,1,{}};
        if(intent.action==A::kRename||intent.action==A::kResize) {
            if(!object||object->kind!=ObjectKind::kSection)return fail(Failure::kInvalidTarget);
            auto content=std::get<SectionContent>(object->content);
            if(intent.action==A::kRename)content.title=intent.content.title;
            else content.boundary=intent.content.boundary;
            const auto* world=sceneCoordinator_->runtimeScene().find(object->id);
            if(!world||!renderableBoundary(content,world->transform))return fail(Failure::kNumericRange);
            if(content==std::get<SectionContent>(object->content)) {
                result.failure=Failure::kNone;result.canonical=runtime::SectionCanonicalOutcome::kNoChange;return result;
            }
            SetSectionContentOp edit;edit.items.push_back({object->id,1,std::get<SectionContent>(object->content),content});
            if(intent.action==A::kResize) {
                const auto* basis=object;
                while(basis) {edit.geometry_basis.push_back(node(*basis));basis=basis->placement.parent_id?semanticObjects_.find(*basis->placement.parent_id):nullptr;}
            }
            operation.payload=std::move(edit);
        } else {
            EditContainerTopologyOp edit;
            std::optional<ObjectId> parent=intent.parentId;
            if(intent.action==A::kCreate&&!parent)parent=selection_.groupScope();
            const auto& projection=sceneCoordinator_->runtimeScene();
            if(parent&&!container(semanticObjects_.find(*parent)))return fail(Failure::kInvalidHierarchy);
            if(intent.action==A::kWrap) {
                if(chosen.empty())return fail(Failure::kInvalidTarget);
                // Nearest common parent, including root, from the existing forest.
                parent=semanticObjects_.find(chosen.front())->placement.parent_id;
                while(parent&&std::any_of(chosen.begin(),chosen.end(),[&](auto id){return !under(id,*parent,semanticObjects_);}))
                    parent=semanticObjects_.find(*parent)->placement.parent_id;
            }
            if(intent.action==A::kRelease) {
                if(!object||object->kind!=ObjectKind::kSection)return fail(Failure::kInvalidTarget);
                parent=object->placement.parent_id;chosen.clear();
                for(const auto& child:semanticObjects_.children(object->id))chosen.push_back(child.id);
                edit.effect=ReleaseWrappersEffect{{*object}};
            } else if(intent.action==A::kReparent) {
                if(chosen.empty())return fail(Failure::kInvalidTarget);
                if(parent&&std::any_of(chosen.begin(),chosen.end(),[&](auto id){return under(*parent,id,semanticObjects_);}))return fail(Failure::kInvalidHierarchy);
                edit.effect=RelocateObjectsEffect{};
            } else if(intent.action!=A::kCreate&&intent.action!=A::kWrap)return fail(Failure::kUnsupported);
            Transform2D parentWorld{};
            if(const auto scope=selection_.groupScope();scope&&(!parent||!under(*parent,*scope,semanticObjects_)))return fail(Failure::kInvalidHierarchy);
            if(parent) {const auto* p=projection.find(*parent);if(!p)return fail(Failure::kProjectionUnavailable);parentWorld=p->transform;}
            Transform2D parentInverse{};
            if(intent.action==A::kWrap&&!scene::inverseTransform(parentWorld,parentInverse))return fail(Failure::kNumericRange);
            const auto selected=[&](ObjectId id){return std::find(chosen.begin(),chosen.end(),id)!=chosen.end();};
            // Canonical path traversal, not caller ordering, defines member order.
            std::stable_sort(chosen.begin(),chosen.end(),[&](auto a,auto b){
                return std::find_if(projection.records().begin(),projection.records().end(),[&](const auto& r){return r.objectId==a;})<
                       std::find_if(projection.records().begin(),projection.records().end(),[&](const auto& r){return r.objectId==b;});
            });
            const bool introduce=intent.action==A::kCreate||intent.action==A::kWrap;
            ObjectRecord wrapper{};
            if(introduce) {
                wrapper.id=intent.objectId.isZero()?allocateTextIdentity():intent.objectId;
                if(wrapper.id.isZero()||semanticObjects_.contains(wrapper.id))return fail(Failure::kInvalidTarget);
                wrapper.kind=ObjectKind::kSection;wrapper.kind_version=1;wrapper.placement.parent_id=parent;
                wrapper.transform=intent.action==A::kWrap?Transform2D{}:intent.transform;wrapper.content=intent.content;
                result.objectId=wrapper.id;
                if(intent.action==A::kWrap) {
                    bool have=false;double l=0,t=0,r=0,b=0;
                    // Include all structural descendants, including those outside
                    // their Section boundary; never use the visible subset.
                    for(const auto& item:projection.records())if(std::any_of(chosen.begin(),chosen.end(),[&](auto id){return under(item.objectId,id,semanticObjects_);})) {
                        if(item.kind==ObjectKind::kGroup)continue;
                        const auto local=scene::composeTransform(parentInverse,item.transform);
                        for(double x:{static_cast<double>(item.visualBounds.left),static_cast<double>(item.visualBounds.right)})
                          for(double y:{static_cast<double>(item.visualBounds.top),static_cast<double>(item.visualBounds.bottom)}) {
                            const double px=(local.a*x+local.c*y)+local.tx,py=(local.b*x+local.d*y)+local.ty;
                            if(!std::isfinite(px)||!std::isfinite(py))return fail(Failure::kNumericRange);
                            if(!have){l=r=px;t=b=py;have=true;}else{l=std::min(l,px);r=std::max(r,px);t=std::min(t,py);b=std::max(b,py);}
                          }
                    }
                    if(!have){l=t=0;r=b=1;}
                    auto content=intent.content;
                    content.boundary={l-content.padding,t-content.padding-content.header_height,
                        std::max(1.0,r-l)+2*content.padding,std::max(1.0,b-t)+2*content.padding+content.header_height};
                    wrapper.content=std::move(content);
                }
            }
            auto destination=semanticObjects_.children(parent);
            std::vector<ObjectId> finalOrder;std::size_t insertion=destination.size();
            if(intent.action==A::kWrap) {
                insertion=0;
                for(std::size_t i=0;i<destination.size();++i)
                    if(std::any_of(chosen.begin(),chosen.end(),[&](auto id){return under(id,destination[i].id,semanticObjects_);}))insertion=i+1;
            }
            for(std::size_t i=0;i<=destination.size();++i) {
                if(introduce&&i==insertion)finalOrder.push_back(wrapper.id);
                if(i==destination.size())break;
                if(intent.action==A::kRelease&&destination[i].id==object->id)finalOrder.insert(finalOrder.end(),chosen.begin(),chosen.end());
                else if(!selected(destination[i].id))finalOrder.push_back(destination[i].id);
            }
            if(intent.action==A::kReparent)finalOrder.insert(finalOrder.end(),chosen.begin(),chosen.end());
            if(finalOrder.size()>65535||chosen.size()>65535)return fail(Failure::kCommandRejected);
            for(std::size_t i=0;i<finalOrder.size();++i) {
                const auto id=finalOrder[i];auto key=ordinalKey(i+1);
                if(introduce&&id==wrapper.id)wrapper.placement.order_key=key;
                else if(!selected(id)&&semanticObjects_.find(id)->placement.order_key!=key)edit.order_only_edits.push_back({id,key});
            }
            const auto destinationWorld=introduce?scene::composeTransform(parentWorld,wrapper.transform):parentWorld;
            if(introduce&&!renderableBoundary(std::get<SectionContent>(wrapper.content),destinationWorld))return fail(Failure::kNumericRange);
            Transform2D inverse{};
            if(!chosen.empty()&&!scene::inverseTransform(destinationWorld,inverse))return fail(Failure::kNumericRange);
            for(std::size_t i=0;i<chosen.size();++i) {
                const auto* item=projection.find(chosen[i]);if(!item)return fail(Failure::kProjectionUnavailable);
                const auto desired=confirmedDrop?confirmedDrop->items.front().transform:item->transform;
                const auto local=scene::composeTransform(inverse,desired);
                auto testBounds=item->kind==ObjectKind::kGroup?foundation::WorldRect{0,0,1,1}:item->visualBounds;
                for(const auto& descendant:projection.records()) {
                    if(descendant.objectId==item->objectId||!under(descendant.objectId,item->objectId,semanticObjects_))continue;
                    std::vector<Transform2D> path;
                    const auto* cursor=semanticObjects_.find(descendant.objectId);
                    while(cursor&&cursor->id!=item->objectId) {
                        path.push_back(cursor->transform);cursor=cursor->placement.parent_id?semanticObjects_.find(*cursor->placement.parent_id):nullptr;
                    }
                    Transform2D relative{};for(auto it=path.rbegin();it!=path.rend();++it)relative=scene::composeTransform(relative,*it);
                    for(double x:{static_cast<double>(descendant.visualBounds.left),static_cast<double>(descendant.visualBounds.right)})
                      for(double y:{static_cast<double>(descendant.visualBounds.top),static_cast<double>(descendant.visualBounds.bottom)}) {
                        const double px=(relative.a*x+relative.c*y)+relative.tx,py=(relative.b*x+relative.d*y)+relative.ty;
                        if(!std::isfinite(px)||!std::isfinite(py))return fail(Failure::kNumericRange);
                        testBounds=foundation::unionRects(testBounds,{static_cast<float>(px),static_cast<float>(py),static_cast<float>(px),static_cast<float>(py)});
                      }
                }
                if(!quality(desired,destinationWorld,local,testBounds))return fail(Failure::kNumericRange);
                const auto pos=std::find(finalOrder.begin(),finalOrder.end(),chosen[i]);
                const auto key=introduce?ordinalKey(i+1):ordinalKey(static_cast<std::size_t>(pos-finalOrder.begin())+1);
                edit.relocations.push_back({chosen[i],{introduce?std::optional<ObjectId>(wrapper.id):parent,key},local});
            }
            if(introduce)edit.effect=IntroduceWrappersEffect{{wrapper}};
            if(!buildContainerGuards(edit,semanticObjects_))return fail(Failure::kInvalidHierarchy);
            // Prepare the existing selection owner's root-first consequence
            // before commit; a new/reparented child cannot bypass Group scope.
            std::optional<ObjectId> outerGroup;
            auto ancestor=parent;
            while(ancestor&&ancestor!=selection_.groupScope()) {
                const auto* record=semanticObjects_.find(*ancestor);if(!record)break;
                if(record->kind==ObjectKind::kGroup)outerGroup=record->id;
                ancestor=record->placement.parent_id;
            }
            if(introduce)(void)afterSelection.click(outerGroup.value_or(wrapper.id));
            else if(intent.action==A::kRelease) {
                std::vector<HitCandidate> candidates;
                for(auto id:chosen) {
                    const auto resolved=selection_.resolveHit(id,semanticObjects_);
                    if(!resolved.isZero())candidates.push_back({resolved,true,false});
                }
                (void)afterSelection.marquee(candidates);
            } else if(outerGroup) {
                std::vector<HitCandidate> candidates;
                for(auto id:selection_.selected()) {
                    const bool moved=std::any_of(chosen.begin(),chosen.end(),[&](auto root){return under(id,root,semanticObjects_);});
                    candidates.push_back({moved?*outerGroup:id,true,false});
                }
                (void)afterSelection.marquee(candidates);
            }
            operation.payload=std::move(edit);
        }
        const auto operationId=operation.id;
        sectionDefersPublication_=true;
        const bool accepted=history_.executeUndoable(std::move(operation));
        sectionDefersPublication_=false;
        if(!accepted)return fail(Failure::kCommandRejected);
        // Prepared local consequence is installed before the same owner publishes
        // Scene; no delayed Scene callback can overwrite a newer selection.
        selection_=std::move(afterSelection);selectionView_.reset();
        publishAdmitted();canonicalPresentationDirty_=true;
        result.failure=Failure::kNone;result.canonical=runtime::SectionCanonicalOutcome::kCommitted;
        result.operationId=operationId.value();result.semanticGeneration=semanticGeneration().value();result.sceneRevision=sceneRevision();
        result.projection=historySceneReady()?runtime::SectionProjectionOutcome::kReady:runtime::SectionProjectionOutcome::kPending;
        if(lastAppliedCommit_) {result.commitEpoch=lastAppliedCommit_->commit_stamp.runtime_epoch.value();result.commitOrdinal=lastAppliedCommit_->commit_stamp.ordinal.value();}
        return result;
    } catch(const std::bad_alloc&) {sectionDefersPublication_=false;return fail(Failure::kCommandRejected);}
}
}
