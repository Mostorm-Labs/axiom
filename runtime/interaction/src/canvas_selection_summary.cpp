#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/semantic/normalizer.hpp"
#include "canvas/semantic/validator.hpp"
#include "canvas/text/text_layout.hpp"

#include <algorithm>
#include <limits>

namespace canvas::interaction {
namespace {
using R = runtime::SelectionReason;
using A = runtime::SelectionAction;
using C = runtime::SelectionCapabilityState;
bool flag(const semantic::PropertyBag& bag, std::uint32_t field, bool fallback) {
    for (const auto& p : bag.entries)
        if (p.field_id == field)
            if (const auto* b = std::get_if<bool>(&p.value))
                return *b;
    return fallback;
}
void aggregate(runtime::SelectionPropertyAggregate& out,
               const RuntimeScene& scene,
               std::span<const foundation::ObjectId> roots) {
    using P = runtime::SelectionPropertyState;
    std::optional<semantic::PropertyValue> first;
    for (const auto id : roots) {
        const auto& record = *scene.find(id);
        if (!semantic::propertyAppliesToObjectKind(record.kind, out.fieldId))
            continue;
        ++out.applicableRoots;
        std::optional<semantic::PropertyValue> value;
        for (const auto& p : record.properties.entries)
            if (p.field_id == out.fieldId)
                value = p.value;
        if (!value)
            value = semantic::propertyDefaultValue(out.fieldId);
        if (!value)
            continue;
        ++out.knownRoots;
        if (!first)
            first = *value;
        else if (*first != *value)
            out.applicableValuesMixed = true;
    }
    out.reason = R::kNone;
    if (out.applicableRoots == 0) {
        out.state = P::kUnavailable;
        out.reason = R::kNotApplicable;
    } else if (out.knownRoots != out.applicableRoots) {
        out.state = P::kUnavailable;
        out.reason = R::kMissingProperty;
    } else if (out.applicableRoots != roots.size())
        out.state = P::kPartial;
    else if (out.applicableValuesMixed)
        out.state = P::kMixed;
    else {
        out.state = P::kUniform;
        out.value = std::move(first);
    }
}
bool measured(const RuntimeSceneRecord& r) {
    if (!r.worldBounds.isFiniteAndOrdered())
        return false;
    if (r.kind == semantic::ObjectKind::kGroup &&
        (r.worldBounds.right <= r.worldBounds.left || r.worldBounds.bottom <= r.worldBounds.top))
        return false;
    if (r.kind == semantic::ObjectKind::kRichText) {
        const auto& text = std::get<semantic::RichTextContent>(r.content);
        const bool nonempty = std::any_of(
            text.document.paragraphs.begin(), text.document.paragraphs.end(), [](const auto& p) {
                return std::any_of(p.runs.begin(), p.runs.end(), [](const auto& run) {
                    return !run.text.empty();
                });
            });
        if (nonempty &&
            (!r.textLayout || r.textLayout->lines.empty() || !r.textLayout->diagnostics.empty()))
            return false;
    }
    if (r.kind == semantic::ObjectKind::kConnector) {
        const auto* route = std::get_if<scene::ConnectorGeometry>(&r.derivedGeometry);
        if (!route || route->status != runtime::ConnectorRouteStatus::kReady)
            return false;
    }
    return true;
}
} // namespace
runtime::SelectionSummary CanvasEditingRuntime::selectionSummary() const noexcept {
    using S = runtime::SelectionSummaryStatus;
    using R = runtime::SelectionReason;
    runtime::SelectionSummary out;
    const auto unavailable = [&](R reason) {
        out.reason = reason;
        out.bounds.reason = reason;
        for (auto& p : out.properties)
            p.reason = reason;
        for (auto& action : out.actions)
            action = {C::kUnavailable, reason};
    };
    if (std::this_thread::get_id() != ownerThread_) {
        unavailable(R::kWrongThread);
        return out;
    }
    if (closed_) {
        unavailable(R::kClosed);
        return out;
    }
    try {
        out = selection_.summary(semanticObjects_);
        out.semanticGeneration = semanticGeneration_.current().value();
        out.sceneRevision = sceneRevision();
        if (sceneCoordinator_)
            out.sceneGeneration = sceneCoordinator_->runtimeScene().generation().value();
        const auto set = [&](A action, R reason = R::kNone) {
            out.actions[static_cast<std::size_t>(action)] = {
                reason == R::kNone ? C::kAvailable : C::kUnavailable, reason};
        };
        for (const auto action :
             {A::kGroup, A::kUngroup, A::kBulkProperties, A::kSnap, A::kAlignment})
            out.actions[static_cast<std::size_t>(action)] = {C::kNotImplemented,
                                                             R::kNotImplemented};
        set(A::kWrapInSection,
            R::kOutsideBatch); // Existing FC08 panel keeps its own command; no new shortcut.
        const auto denyLive = [&](R reason, C state = C::kUnavailable) {
            for (std::size_t i = 0; i < static_cast<std::size_t>(A::kGroup); ++i)
                out.actions[i] = {state, reason};
        };
        const auto stale = [&] {
            out.status = S::kStale;
            out.reason = R::kStaleScene;
            out.bounds = {S::kStale, R::kStaleScene, {}};
            for (auto& p : out.properties) {
                p.state = runtime::SelectionPropertyState::kUnavailable;
                p.reason = R::kStaleScene;
                p.value.reset();
            }
            denyLive(R::kStaleScene, C::kStaleSelection);
        };
        if (out.status != S::kAvailable) {
            out.bounds = {S::kInvalid, out.reason, {}};
            for (auto& p : out.properties)
                p.reason = out.reason;
            denyLive(out.reason, C::kInvalidSelection);
            return out;
        }
        if (!historySceneReady()) {
            stale();
            return out;
        }
        if (out.count == 0) {
            out.bounds.reason = R::kNoSelection;
            denyLive(R::kNoSelection);
            return out;
        }
        const auto& projection = sceneCoordinator_->runtimeScene();
        std::optional<foundation::WorldRect> bounds;
        bool ready = true;
        for (const auto id : out.roots) {
            const auto* r = projection.find(id);
            if (!r) {
                stale();
                return out;
            }
            ready = ready && measured(*r);
            // Group's published union depends on all of its descendants. A
            // Section's authored boundary does not depend on child extents.
            if (r->kind == semantic::ObjectKind::kGroup)
                for (const auto& candidate : projection.records()) {
                    const auto* member = semanticObjects_.find(candidate.objectId);
                    while (member && member->placement.parent_id) {
                        if (*member->placement.parent_id == id) {
                            ready = ready && measured(candidate);
                            break;
                        }
                        member = semanticObjects_.find(*member->placement.parent_id);
                    }
                }
            bounds = bounds ? foundation::unionRects(*bounds, r->worldBounds) : r->worldBounds;
        }
        out.bounds =
            ready ? runtime::SelectionBoundsSummary{S::kAvailable, R::kNone, bounds}
                  : runtime::SelectionBoundsSummary{S::kUnavailable, R::kBoundsUnavailable, {}};
        for (auto& property : out.properties)
            aggregate(property, projection, out.roots);
        // General commands still consume the formal selection. If an external
        // hierarchy edit changed its roots, never advertise a different target
        // set just because the read-only summary successfully normalized it.
        if (!std::equal(out.roots.begin(),
                        out.roots.end(),
                        selection_.selected().begin(),
                        selection_.selected().end()))
            denyLive(R::kNonNormalizedSelection);
        else if (!surface_.available || !canonicalWriteAdmission())
            denyLive(R::kSceneUnavailable);
        else if (regionSelection_.active() || selectionTransformActive() || creationActive() ||
                 imageCropPresentation_ || !brushSessions_.empty() || !eraserTraces_.empty() ||
                 !keyedStrokeIds_.empty() || !panPointers_.empty() ||
                 (interaction_->textSession() &&
                  interaction_->textSession()->read().phase != runtime::TextPhase::kClosed))
            denyLive(R::kBusy);
        else {
            denyLive(R::kMultipleRoots);
            set(A::kFit, ready ? R::kNone : R::kBoundsUnavailable);
            const bool locked = std::any_of(out.roots.begin(), out.roots.end(), [&](auto id) {
                return flag(projection.find(id)->properties, 2U, false);
            });
            if (locked)
                set(A::kDelete, R::kLocked);
            else {
                const auto sections =
                    std::count_if(out.roots.begin(), out.roots.end(), [&](auto id) {
                        return semanticObjects_.find(id)->kind == semantic::ObjectKind::kSection;
                    });
                if (sections != 0) {
                    bool release = false;
                    if (out.count == 1) {
                        const auto snapshot = sectionSnapshot(out.primary);
                        release =
                            snapshot
                                .actions[static_cast<std::size_t>(runtime::SectionAction::kRelease)]
                                .reason == runtime::SectionFailure::kNone;
                    }
                    set(A::kDelete, release ? R::kNone : R::kUnsupportedKind);
                } else {
                    auto nonce = std::numeric_limits<std::uint64_t>::max();
                    while (nonce && appliedOperations_.find(semantic::OperationId{
                                        foundation::ObjectId::fromUint64(nonce)}))
                        --nonce;
                    const semantic::Operation probe{
                        semantic::OperationId{foundation::ObjectId::fromUint64(nonce)},
                        documentId_,
                        1,
                        1,
                        semantic::DeleteObjectsOp{out.roots}};
                    const auto plan = operationEngine_.prepare(
                        probe, {semanticObjects_, appliedOperations_, resourceManifestPort_});
                    set(A::kDelete,
                        plan.disposition == semantic::PrepareDisposition::kPrepared
                            ? R::kNone
                            : R::kCommandRejected);
                }
            }
            if (out.count == 1) {
                set(A::kFitPrimary, ready ? R::kNone : R::kBoundsUnavailable);
                const auto& record = *semanticObjects_.find(out.primary);
                const bool lockable = semantic::propertyAppliesToObjectKind(record.kind, 2U);
                set(A::kLock, !lockable ? R::kUnsupportedKind : locked ? R::kLocked : R::kNone);
                bool ancestorLocked = false;
                auto parent = record.placement.parent_id;
                while (parent) {
                    const auto* p = semanticObjects_.find(*parent);
                    if (!p)
                        break;
                    ancestorLocked = ancestorLocked || flag(p->properties, 2U, false);
                    parent = p->placement.parent_id;
                }
                set(A::kUnlock,
                    !lockable                            ? R::kUnsupportedKind
                    : ancestorLocked                     ? R::kLocked
                    : flag(record.properties, 2U, false) ? R::kNone
                                                         : R::kCommandRejected);
                set(A::kTransform, locked ? R::kLocked : R::kNone);
                set(A::kOrder,
                    locked                                          ? R::kLocked
                    : record.kind == semantic::ObjectKind::kSection ? R::kUnsupportedKind
                                                                    : R::kNone);
                set(A::kSize,
                    locked ? R::kLocked
                    : (record.kind == semantic::ObjectKind::kShape ||
                       record.kind == semantic::ObjectKind::kSticky)
                        ? R::kNone
                        : R::kUnsupportedKind);
                set(A::kAppearance,
                    locked ? R::kLocked
                    : (record.kind == semantic::ObjectKind::kShape ||
                       record.kind == semantic::ObjectKind::kConnector)
                        ? R::kNone
                        : R::kUnsupportedKind);
            }
        }
        if (selection_.revision() != out.selectionRevision ||
            semanticGeneration_.current().value() != out.semanticGeneration ||
            sceneRevision() != out.sceneRevision ||
            projection.generation().value() != out.sceneGeneration)
            stale();
        return out;
    } catch (const std::bad_alloc&) {
        out = {};
        out.status = S::kUnavailable;
        out.reason = R::kResourceUnavailable;
        return out;
    }
}
} // namespace canvas::interaction
