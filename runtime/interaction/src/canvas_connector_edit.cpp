#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/scene/connector_router.hpp"
#include "canvas/scene/hierarchy_geometry.hpp"
#include "canvas/semantic/connector_validation.hpp"

#include <algorithm>
#include <cmath>

namespace canvas::interaction {
namespace {
using Point = foundation::WorldPoint;
Point map(const semantic::Transform2D& t, double x, double y) {
    return {float(t.a * x + t.c * y + t.tx), float(t.b * x + t.d * y + t.ty)};
}
runtime::ConnectorCommandReceipt rejected(runtime::ConnectorFailureReason reason) {
    runtime::ConnectorCommandReceipt out;
    out.reason = reason;
    return out;
}
} // namespace
runtime::ConnectorCommandReceipt CanvasEditingRuntime::commitConnectorContent(
    semantic::ObjectId id, const semantic::ConnectorContent& content, std::uint64_t generation) {
    using Reason = runtime::ConnectorFailureReason;
    if (!canonicalWriteAdmission() || !historySceneReady())
        return rejected(Reason::kNotReady);
    if (generation && semanticGeneration().value() != generation)
        return rejected(Reason::kStale);
    const auto* object = semanticObjects_.find(id);
    if (!object || object->kind != semantic::ObjectKind::kConnector)
        return rejected(Reason::kInvalidTarget);
    if (!textWritable(id))
        return rejected(Reason::kLocked);
    const auto before = std::get<semantic::ConnectorContent>(object->content);
    runtime::ConnectorCommandReceipt result;
    if (before != content) {
        auto candidate = *object;
        candidate.content = content;
        const auto route = scene::stagedConnectorRoute(candidate, semanticObjects_.allObjects());
        if (!route || route->status != runtime::ConnectorRouteStatus::kReady) {
            if (route && route->status == runtime::ConnectorRouteStatus::kNoRouteInDomain)
                return rejected(Reason::kNoRouteInDomain);
            if (route && route->status == runtime::ConnectorRouteStatus::kBudgetExceeded)
                return rejected(Reason::kBudgetExceeded);
            return rejected(Reason::kInvalidEndpoint);
        }
        const auto f = allocateOperationId(), i = allocateOperationId();
        if (f.isZero() || i.isZero() ||
            !applyLocalHistoryOperation(
                {f, documentId_, 1, 1, semantic::SetConnectorContentOp{id, content}},
                {i, documentId_, 1, 1, semantic::SetConnectorContentOp{id, before}}))
            return rejected(Reason::kObjectRejected);
        result.historyChanged = true;
    }
    result.state = runtime::ConnectorCommandState::kApplied;
    result.reason = Reason::kNone;
    result.objectId = id;
    result.semanticGeneration = semanticGeneration().value();
    return result;
}

runtime::ConnectorCommandReceipt
CanvasEditingRuntime::editConnectorEndpoint(const runtime::ConnectorEndpointRequest& r) {
    using Reason = runtime::ConnectorFailureReason;
    if (r.expectedSemanticGeneration != semanticGeneration().value())
        return rejected(Reason::kStale);
    const auto* o = semanticObjects_.find(r.objectId);
    if (!o || o->kind != semantic::ObjectKind::kConnector)
        return rejected(Reason::kInvalidTarget);
    auto content = std::get<semantic::ConnectorContent>(o->content);
    auto& original = r.atStart ? content.start : content.end;
    if (const auto* a = std::get_if<semantic::AttachedEndpoint>(&r.endpoint.value);
        original != r.endpoint && a) {
        if (!semanticObjects_.contains(a->target_object_id))
            return rejected(Reason::kInvalidTarget);
        else if (!textWritable(a->target_object_id))
            return rejected(Reason::kLocked);
    }
    original = r.endpoint;
    return commitConnectorContent(o->id, content, r.expectedSemanticGeneration);
}

void CanvasEditingRuntime::placeNewConnector(semantic::ObjectRecord& wire,
                                             std::span<const semantic::ObjectRecord> added) const {
    if (wire.placement.parent_id || !sceneCoordinator_)
        return;
    const auto& content = std::get<semantic::ConnectorContent>(wire.content);
    const auto parentOf =
        [&](const semantic::ConnectorEndpoint& endpoint) -> std::optional<semantic::ObjectId> {
        if (const auto* a = std::get_if<semantic::AttachedEndpoint>(&endpoint.value)) {
            for (const auto& o : added)
                if (o.id == a->target_object_id)
                    return o.placement.parent_id;
            const auto* target = semanticObjects_.find(a->target_object_id);
            return target ? target->placement.parent_id : std::nullopt;
        }
        const auto p = std::get<semantic::FreePointEndpoint>(endpoint.value).point;
        const auto world = map(wire.transform, p.x, p.y);
        const auto hits =
            runtimeSceneHost_->query({{world.x - 1, world.y - 1, world.x + 1, world.y + 1}});
        if (!hits)
            return {};
        for (auto it = hits.value().backToFront.rbegin(); it != hits.value().backToFront.rend();
             ++it) {
            const auto* r = sceneCoordinator_->runtimeScene().find(*it);
            if (!r || r->kind != semantic::ObjectKind::kSection || !textWritable(*it))
                continue;
            semantic::Transform2D inverse;
            if (!scene::inverseTransform(r->transform, inverse))
                continue;
            const auto q = map(inverse, world.x, world.y);
            const auto b = std::get<semantic::SectionContent>(r->content.get()).boundary;
            if (q.x >= b.x && q.x <= b.x + b.width && q.y >= b.y && q.y <= b.y + b.height)
                return *it;
        }
        return {};
    };
    const auto ancestors = [&](auto id) {
        std::vector<semantic::ObjectId> parents;
        while (id) {
            const auto* parent = semanticObjects_.find(*id);
            if (!parent)
                break;
            if ((parent->kind == semantic::ObjectKind::kGroup ||
                 parent->kind == semantic::ObjectKind::kSection) &&
                textWritable(*id))
                parents.push_back(*id);
            id = parent->placement.parent_id;
        }
        return parents;
    };
    const auto a = ancestors(parentOf(content.start)), b = ancestors(parentOf(content.end));
    for (const auto parent : a)
        if (std::find(b.begin(), b.end(), parent) != b.end()) {
            const auto* r = sceneCoordinator_->runtimeScene().find(parent);
            semantic::Transform2D inverse;
            if (r && scene::inverseTransform(r->transform, inverse)) {
                wire.transform = scene::composeTransform(inverse, wire.transform);
                wire.placement.parent_id = parent;
            }
            break;
        }
}

CanvasEditingRuntime::ConnectorAttachmentPick CanvasEditingRuntime::connectorAttachmentAt(
    float x, float y, const std::optional<semantic::AttachedEndpoint>& previous, bool touch) const {
    if (!sceneCoordinator_ || !runtimeSceneHost_) return {};
    const auto scale=viewportController_->state().scale;
    const auto [wx,wy]=viewportController_->viewToContent(x,y);
    const Point pointer{wx,wy};
    const float enter=touch ? 16.f : 10.f, release=touch ? 22.f : 14.f;
    const float discover=touch ? 36.f : 26.f;
    const float reach=discover/scale;
    const auto hits=runtimeSceneHost_->query({{wx-reach,wy-reach,wx+reach,wy+reach}});
    if (!hits) return {};
    const auto distance=[&](Point p){return std::hypot(p.x-wx,p.y-wy)*scale;};
    float bestPort=INFINITY,bestRim=INFINITY,bestNearby=INFINITY;
    std::optional<semantic::AttachedEndpoint> port,rim;
    semantic::ObjectId nearby{};
    for(auto it=hits.value().backToFront.rbegin();it!=hits.value().backToFront.rend();++it) {
        const auto* record=sceneCoordinator_->runtimeScene().find(*it);
        if(!record || !semantic::isConnectableObjectKind(record->kind,record->kindVersion) || !textWritable(*it))continue;
        const auto* source=semanticObjects_.find(*it);
        if(!source)continue;
        auto target=*source;
        target.transform=record->transform;
        for(unsigned n=1;n<=4;++n) {
            const semantic::AttachedEndpoint candidate{*it,semantic::StablePortAnchor{n}};
            const auto position=scene::connectorAnchorPoint(target,candidate.anchor,pointer);
            if(!position)continue;
            const float d=distance(*position);
            const bool held=previous==candidate;
            if(d<=(held ? release : enter) && (d<bestPort || (d==bestPort && held))) {
                bestPort=d;
                port=candidate;
            }
        }
        if(port)continue;
        const auto hit=scene::nearestConnectorPerimeter(target,pointer);
        if(!hit)continue;
        const float d=distance(hit->point);
        if((hit->inside || d<=discover) && d<bestNearby) {
            bestNearby=d;
            nearby=*it;
        }
        // A drag can slide along the same contour while its directional hint changes.
        const bool held=previous && previous->target_object_id==*it &&
                        std::holds_alternative<semantic::AutoPerimeterAnchor>(previous->anchor);
        if(d<=(held ? release : enter) && (d<bestRim || (d==bestRim && held))) {
            bestRim=d;
            rim=semantic::AttachedEndpoint{*it,hit->anchor};
        }
    }
    const auto& snapped=port ? port : rim;
    return {snapped,snapped ? snapped->target_object_id : nearby};
}
bool CanvasEditingRuntime::endpointEditCurrent() const noexcept {
    return endpointDrag_ && endpointDrag_->generation == semanticGeneration().value() &&
           endpointDrag_->context == regionContext() && selection_.primary() == endpointDrag_->id &&
           textWritable(endpointDrag_->id) && canonicalWriteAdmission();
}
void CanvasEditingRuntime::clearEndpointEdit(runtime::ConnectorFailureReason reason) noexcept {
    if (!endpointDrag_)
        return;
    lastEndpointEdit_ = rejected(reason);
    lastEndpointEdit_->objectId = endpointDrag_->id;
    lastEndpointEdit_->semanticGeneration = semanticGeneration().value();
    endpointDrag_.reset();
    connectorHover_.reset();
    transientOverlayHandoffPending_ = true;
    transientOverlayDirty_ = false;
    canonicalPresentationDirty_ = true;
}
void CanvasEditingRuntime::serviceEndpointRoute() {
    if (!endpointDrag_)
        return;
    if (!endpointEditCurrent()) {
        acquisitionClickPointers_.insert(endpointDrag_->key);
        clearEndpointEdit(runtime::ConnectorFailureReason::kStale);
        return;
    }
    auto& edit = *endpointDrag_;
    if (!edit.job)
        return;
    const auto status = edit.job->advance();
    edit.route = scene::connectorDisplayRoute(edit.candidate, edit.job->result());
    if (status != runtime::ConnectorRouteStatus::kPending)
        edit.job.reset();
    canonicalPresentationDirty_ = true;
    transientOverlayDirty_ = true;
}
bool CanvasEditingRuntime::connectorEndpointPointer(const input::PointerSample& sample) {
    using Phase = input::PointerPhase;
    if (!endpointDrag_) {
        if (sample.phase != Phase::kDown || !selectionMode_ ||
            selectionAcquisitionMode_ != runtime::CanvasSelectionMode::kPoint ||
            selectedObjectCount() != 1 || !historySceneReady() || !canonicalWriteAdmission() ||
            appearanceSession_ || creationActive() || transformDrag_.active() ||
            imageCropPresentation_)
            return false;
        const auto* object = semanticObjects_.find(selection_.primary());
        if (!object || object->kind != semantic::ObjectKind::kConnector ||
            !textWritable(object->id))
            return false;
        const auto* r = sceneCoordinator_->runtimeScene().find(object->id);
        if (!r)
            return false;
        const auto* g = std::get_if<scene::ConnectorGeometry>(&r->derivedGeometry);
        if (!g || g->route.size() < 2)
            return false;
        const auto& cam = viewportController_->state();
        const auto distance = [&](Point p) {
            const auto w = map(r->transform, p.x, p.y);
            return std::hypot(w.x * cam.scale + cam.translationX - sample.x,
                              w.y * cam.scale + cam.translationY - sample.y);
        };
        const auto first = distance(g->route.front()), last = distance(g->route.back());
        if (std::min(first, last) >
            (sample.capabilities.tool == input::PointerTool::kFinger ? 18 : 12))
            return false;
        auto& d = endpointDrag_.emplace();
        d.key = sample.key;
        d.id = object->id;
        d.atStart = first < last;
        d.downX = sample.x;
        d.downY = sample.y;
        d.context = regionContext();
        d.generation = semanticGeneration().value();
        d.candidate = *object;
        d.candidate.transform = r->transform;
        d.route = *g;
        d.world = scene::projectWorldObjects(semanticObjects_.allObjects());
        const auto& c = std::get<semantic::ConnectorContent>(object->content);
        if (const auto* a =
                std::get_if<semantic::AttachedEndpoint>(&(d.atStart ? c.start : c.end).value))
            d.target = *a;
        connectorHover_.reset();
        canonicalPresentationDirty_ = true;
        return true;
    }
    if (endpointDrag_->key != sample.key)
        return false;
    if (sample.phase == Phase::kCancel) {
        clearEndpointEdit();
        return true;
    }
    if (!endpointEditCurrent()) {
        clearEndpointEdit(runtime::ConnectorFailureReason::kStale);
        return true;
    }
    auto& edit = *endpointDrag_;
    if (std::hypot(sample.x - edit.downX, sample.y - edit.downY) > 2)
        edit.moved = true;
    if (edit.moved) {
        auto& c = std::get<semantic::ConnectorContent>(edit.candidate.content);
        auto& e = edit.atStart ? c.start : c.end;
        const auto previous = e;
        const auto pick=connectorAttachmentAt(sample.x,sample.y,edit.target,
                                               sample.capabilities.tool==input::PointerTool::kFinger);
        edit.target=pick.attachment;
        edit.nearbyTarget=pick.nearby;
        if (std::hypot(sample.x - edit.downX, sample.y - edit.downY) <= 2) {
            const auto& original =
                std::get<semantic::ConnectorContent>(semanticObjects_.find(edit.id)->content);
            e = edit.atStart ? original.start : original.end;
            if (const auto* a = std::get_if<semantic::AttachedEndpoint>(&e.value))
                edit.target = *a;
            else
                edit.target.reset();
        } else if (edit.target)
            e.value = *edit.target;
        else {
            const auto [wx, wy] = viewToContent(sample.x, sample.y);
            semantic::Transform2D inverse;
            if (!scene::inverseTransform(edit.candidate.transform, inverse)) {
                clearEndpointEdit();
                return true;
            }
            const auto q = map(inverse, wx, wy);
            e.value = semantic::FreePointEndpoint{{q.x, q.y}};
        }
        if (e != previous) {
            edit.job = std::make_unique<scene::ConnectorRouteJob>(edit.candidate, edit.world);
            serviceEndpointRoute();
        }
    }
    if (sample.phase == Phase::kUp) {
        const auto& c = std::get<semantic::ConnectorContent>(edit.candidate.content);
        const runtime::ConnectorEndpointRequest request{
            edit.id, edit.atStart, edit.atStart ? c.start : c.end, edit.generation};
        clearEndpointEdit();
        lastEndpointEdit_ = editConnectorEndpoint(request);
        lastEndpointEdit_->objectId = request.objectId;
    } else {
        canonicalPresentationDirty_ = true;
        transientOverlayDirty_ = true;
    }
    (void)renderSelectionOverlay();
    if (!platformPresentationDeferred_)
        (void)presentTransientOverlayFrame(nextCanonicalFrameId());
    return true;
}
void CanvasEditingRuntime::applyEndpointPreview(render::ReferenceDrawList& list) const {
    if (!endpointEditCurrent() || !endpointDrag_->moved)
        return;
    const auto& edit = *endpointDrag_;
    for (auto& entry : list.entries)
        if (entry.record.objectId == edit.id) {
            entry.record.content = edit.candidate.content;
            entry.record.derivedGeometry = edit.route;
            const auto bounds = scene::computeBounds(edit.candidate, nullptr, &edit.route);
            entry.record.geometryBounds = bounds.geometry;
            entry.record.visualBounds = bounds.visual;
            entry.record.worldBounds = bounds.world;
            auto command = render::DirectReferenceSource::presentationCommand(entry.record);
            if (command)
                entry.command = std::move(command.value());
            list.digest.clear();
            list.canonicalBytes.clear();
            break;
        }
}
void CanvasEditingRuntime::connectorFeedback(render::EditingOverlay& overlay) const {
    overlay.connectorGrips.reset();
    overlay.connectorTargetOutline.clear();
    overlay.connectorTargetPort.reset();
    overlay.connectorTargetPorts.reset();
    const auto& cam = viewportController_->state();
    const auto screen = [&](Point p) {
        return render::ScreenPoint{p.x * cam.scale + cam.translationX,
                                   p.y * cam.scale + cam.translationY};
    };
    const auto* edit = endpointEditCurrent() ? &*endpointDrag_ : nullptr;
    if (selectionMode_ && selectedObjectCount() == 1 && textWritable(selection_.primary())) {
        const auto* r = sceneCoordinator_->runtimeScene().find(selection_.primary());
        if (r && r->kind == semantic::ObjectKind::kConnector) {
            const auto* g =
                edit ? &edit->route : std::get_if<scene::ConnectorGeometry>(&r->derivedGeometry);
            const auto& content = std::get<semantic::ConnectorContent>(
                edit ? edit->candidate.content : r->content.get());
            if (g && g->route.size() >= 2) {
                const auto grip = [&](Point p, const semantic::ConnectorEndpoint& e) {
                    return render::EditingOverlay::ConnectorGrip{
                        screen(map(r->transform, p.x, p.y)),
                        std::holds_alternative<semantic::AttachedEndpoint>(e.value)};
                };
                overlay.connectorGrips = std::array{grip(g->route.front(), content.start),
                                                    grip(g->route.back(), content.end)};
            }
        }
    }
    auto target=edit ? edit->target : connectorHover_;
    auto nearby=edit ? edit->nearbyTarget : semantic::ObjectId{};
    if(const auto it=creations_.find(latestCreationPointer_);it!=creations_.end()) {
        target=it->second.endAttachment();
        nearby=it->second.nearbyEndTarget();
    }
    const auto id=target ? target->target_object_id : nearby;
    if(id.isZero() || !textWritable(id)) return;
    const auto* object=semanticObjects_.find(id);
    const auto* r=sceneCoordinator_->runtimeScene().find(id);
    if(!object || !r) return;
    auto world=*object;
    world.transform=r->transform;
    if(!nearby.isZero()) {
        auto& ports=overlay.connectorTargetPorts.emplace();
        for(unsigned n=0;n<4;++n) {
            const auto p=scene::connectorAnchorPoint(world,semantic::StablePortAnchor{n+1},{});
            if(!p){overlay.connectorTargetPorts.reset();break;}
            ports[n]=screen(*p);
        }
    }
    if(!target)return;
    const auto* automatic=std::get_if<semantic::AutoPerimeterAnchor>(&target->anchor);
    if(!automatic || automatic->hint) {
        if(const auto p=scene::connectorAnchorPoint(world,target->anchor,{})) {
            overlay.connectorTargetPort=screen(*p);
            if(edit && edit->moved && overlay.connectorGrips)
                (*overlay.connectorGrips)[edit->atStart ? 0 : 1].visible=false;
        }
    }
    if(!automatic)return;
    // Sample the same local perimeter as the route solver, including curved and rotated targets.
    const auto b = r->geometryBounds;
    const double cx = (b.left + b.right) * .5, cy = (b.top + b.bottom) * .5;
    for (unsigned n = 0; n < 64; ++n) {
        const double a = n * (6.283185307179586 / 64);
        const auto towards = map(world.transform,
                                 cx + std::cos(a) * (b.right - b.left),
                                 cy + std::sin(a) * (b.bottom - b.top));
        if (auto p = scene::connectorAnchorPoint(world, semantic::AutoPerimeterAnchor{}, towards))
            overlay.connectorTargetOutline.push_back(screen(*p));
    }
}
} // namespace canvas::interaction
