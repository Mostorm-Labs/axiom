#include "undoable_command.hpp"
#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/normalizer.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>

namespace canvas::interaction {
using namespace semantic;
namespace {
bool container(const ObjectRecord* r) noexcept {
    return r && (r->kind == ObjectKind::kGroup || r->kind == ObjectKind::kSection);
}
ContainerStructuralExpectation node(const ObjectRecord& r) {
    return {r.id, r.kind, r.kind_version, r.placement, r.transform};
}
bool path(std::optional<ObjectId> id, const ObjectStore& view,
          const ObjectStore& basis, const std::set<ObjectId>& introduced,
          std::map<ObjectId, ContainerStructuralExpectation>& nodes) {
    std::set<ObjectId> seen;
    while (id) {
        if (!seen.insert(*id).second) return false;
        const auto* r = view.find(*id);
        if (!r) return false;
        if (!introduced.contains(*id)) {
            const auto* prior = basis.find(*id);
            if (!prior) return false;
            nodes.emplace(*id, node(*prior));
        }
        id = r->placement.parent_id;
    }
    return true;
}
bool guards(EditContainerTopologyOp& op, const ObjectStore& before) {
    StagedObjectView desired(before);
    std::set<ObjectId> introduced;
    std::set<std::optional<ObjectId>> parents;
    std::map<ObjectId, ContainerStructuralExpectation> nodes;
    if (const auto* effect = std::get_if<IntroduceWrappersEffect>(&op.effect))
        for (const auto& r : effect->containers) {
            introduced.insert(r.id); parents.insert(r.placement.parent_id);
            if (!desired.stageCreate(r)) return false;
        }
    if (const auto* effect = std::get_if<ReleaseWrappersEffect>(&op.effect))
        for (const auto& r : effect->containers_before) {
            parents.insert(r.placement.parent_id); parents.insert(r.id);
            if (!path(r.id, before, before, {}, nodes)) return false;
        }
    for (const auto& item : op.relocations) {
        const auto* r = before.find(item.object_id);
        if (!r || !path(r->id, before, before, {}, nodes)) return false;
        parents.insert(r->placement.parent_id); parents.insert(item.placement_after.parent_id);
        auto replacement = *r; replacement.placement = item.placement_after;
        replacement.transform = item.transform_after;
        if (!desired.stageReplace(std::move(replacement))) return false;
    }
    for (const auto& item : op.order_only_edits) {
        const auto* r = before.find(item.object_id);
        if (!r || !path(r->id, before, before, {}, nodes)) return false;
        auto replacement = *r; replacement.placement.order_key = item.order_key_after;
        if (!desired.stageReplace(std::move(replacement))) return false;
    }
    if (const auto* effect = std::get_if<ReleaseWrappersEffect>(&op.effect))
        for (const auto& r : effect->containers_before) if (!desired.stageDelete(r.id)) return false;
    for (const auto& item : op.relocations)
        if (!path(item.placement_after.parent_id, desired, before, introduced, nodes)) return false;
    if (const auto* effect = std::get_if<IntroduceWrappersEffect>(&op.effect))
        for (const auto& r : effect->containers)
            if (!path(r.id, desired, before, introduced, nodes)) return false;
    for (const auto& [id, value] : nodes) { (void)id; op.guard.nodes.push_back(value); }
    for (const auto& parent : parents) {
        if (parent && introduced.contains(*parent)) continue;
        ContainerChildrenExpectation value{parent, {}};
        for (const auto& r : before.children(parent)) value.complete_members.push_back({r.id, r.placement.order_key});
        op.guard.child_sets.push_back(std::move(value));
    }
    return true;
}
bool stage(StagedObjectView& view, const PreparedApplyPlan& plan) {
    for (const auto& r : plan.creates) if (!view.stageCreate(r)) return false;
    for (const auto& r : plan.replacements) if (!view.stageReplace(r)) return false;
    for (const auto& id : plan.deletes) if (!view.stageDelete(id)) return false;
    return true;
}
std::set<ObjectId> footprint(const PreparedApplyPlan& plan) {
    std::set<ObjectId> ids(plan.deletes.begin(), plan.deletes.end());
    for (const auto& r : plan.creates) ids.insert(r.id);
    for (const auto& r : plan.replacements) ids.insert(r.id);
    return ids;
}
}
bool buildContainerGuards(EditContainerTopologyOp& op,const ObjectStore& before) { return guards(op,before); }
bool supportsUndoableCommand(OperationKind kind) noexcept {
    return kind == OperationKind::kSetSectionContent || kind == OperationKind::kEditContainerTopology ||
        kind == OperationKind::kInsertObjects || kind == OperationKind::kDeleteObjects ||
        kind == OperationKind::kRestoreObjects || kind == OperationKind::kSetTransforms ||
        kind == OperationKind::kPatchProperties;
}
bool isContainerCommand(const Operation& op, const ObjectStore& store) noexcept {
    if (op.kind() == OperationKind::kSetSectionContent || op.kind() == OperationKind::kEditContainerTopology) return true;
    if (const auto* v = std::get_if<InsertObjectsOp>(&op.payload))
        for (const auto& r : v->objects) if (container(&r)) return true;
    if (const auto* v = std::get_if<RestoreObjectsOp>(&op.payload))
        for (const auto& r : v->objects) if (container(&r)) return true;
    if (const auto* v = std::get_if<DeleteObjectsOp>(&op.payload))
        for (const auto& id : v->object_ids) if (container(store.find(id))) return true;
    if (const auto* v = std::get_if<SetTransformsOp>(&op.payload))
        for (const auto& item : v->items) if (container(store.find(item.object_id))) return true;
    if (const auto* v = std::get_if<PatchPropertiesOp>(&op.payload))
        for (const auto& item : v->patches) if (container(store.find(item.object_id))) return true;
    return false;
}
bool historyFootprintMatches(const PreparedApplyPlan& plan, std::span<const HistoryExpectation> expected) {
    const auto actual = footprint(plan);
    std::set<ObjectId> ids;
    for (const auto& item : expected) ids.insert(item.objectId);
    return actual == ids;
}
std::optional<PreparedUndoableCommand> prepareUndoableCommand(
    const PreparedApplyPlan& plan, const StatefulValidationContext& context, const StagedObjectView& after) {
    if (!supportsUndoableCommand(plan.operation.kind()) || plan.manifest) return std::nullopt;
    const auto& before = context.objects;
    const auto& f = plan.operation;
    Operation u = f;
    // Placeholder identity is retained only in immutable intention data. Undo
    // replaces it with the existing owner's fresh ID before any real submit.
    auto nonce = std::numeric_limits<std::uint64_t>::max();
    do { u.id = OperationId{ObjectId::fromUint64(nonce--)}; }
    while (nonce && (u.id == f.id || context.applied_operations.find(u.id)));
    if (!nonce) return std::nullopt;
    HistoryAspect aspect = HistoryAspect::kWholeRecord;
    if (const auto* section = std::get_if<SetSectionContentOp>(&f.payload)) {
        SetSectionContentOp inverse;
        std::map<ObjectId, ContainerStructuralExpectation> basis;
        for (const auto& item : section->items) {
            const auto* r = before.find(item.object_id);
            const auto* content = r ? std::get_if<SectionContent>(&r->content) : nullptr;
            if (!content) return std::nullopt;
            inverse.items.push_back({item.object_id, r->kind_version, item.replacement_content, *content});
            if (content->boundary != item.replacement_content.boundary &&
                !path(r->id, after, after, {}, basis)) return std::nullopt;
        }
        for (const auto& [id, value] : basis) { (void)id; inverse.geometry_basis.push_back(value); }
        u.payload = std::move(inverse); aspect = HistoryAspect::kRichTextContent;
    } else if (const auto* topology = std::get_if<EditContainerTopologyOp>(&f.payload)) {
        EditContainerTopologyOp inverse;
        if (std::holds_alternative<IntroduceWrappersEffect>(topology->effect)) inverse.effect = ReleaseWrappersEffect{plan.creates};
        else if (std::holds_alternative<ReleaseWrappersEffect>(topology->effect)) {
            IntroduceWrappersEffect effect;
            for (const auto& id : plan.deletes) effect.containers.push_back(*before.find(id));
            inverse.effect = std::move(effect);
        } else inverse.effect = RelocateObjectsEffect{};
        for (const auto& item : topology->relocations) {
            const auto* r = before.find(item.object_id);
            if (!r) return std::nullopt;
            inverse.relocations.push_back({r->id, r->placement, r->transform});
        }
        for (const auto& item : topology->order_only_edits) {
            const auto* r = before.find(item.object_id);
            if (!r) return std::nullopt;
            inverse.order_only_edits.push_back({r->id, r->placement.order_key});
        }
        if (!guards(inverse, after)) return std::nullopt;
        u.payload = std::move(inverse); aspect = HistoryAspect::kTopology;
    } else if (std::holds_alternative<InsertObjectsOp>(f.payload) || std::holds_alternative<RestoreObjectsOp>(f.payload)) {
        DeleteObjectsOp inverse;
        for (const auto& r : plan.creates) inverse.object_ids.push_back(r.id);
        u.payload = std::move(inverse);
    } else if (std::holds_alternative<DeleteObjectsOp>(f.payload)) {
        RestoreObjectsOp inverse;
        for (const auto& id : plan.deletes) inverse.objects.push_back(*before.find(id));
        u.payload = std::move(inverse);
    } else if (const auto* transforms = std::get_if<SetTransformsOp>(&f.payload)) {
        SetTransformsOp inverse;
        for (const auto& item : transforms->items) inverse.items.push_back({item.object_id, before.find(item.object_id)->transform});
        u.payload = std::move(inverse); aspect = HistoryAspect::kTransform;
    } else if (const auto* properties = std::get_if<PatchPropertiesOp>(&f.payload)) {
        PatchPropertiesOp inverse;
        for (const auto& item : properties->patches) {
            PropertyPatch patch{item.object_id, item.field_id, PropertyPatchAction::kClear, {}};
            for (const auto& entry : before.find(item.object_id)->properties.entries)
                if (entry.field_id == item.field_id) { patch.action = PropertyPatchAction::kSet; patch.value = entry.value; }
            inverse.patches.push_back(std::move(patch));
        }
        u.payload = std::move(inverse); aspect = HistoryAspect::kProperties;
    } else return std::nullopt;
    const auto normalized = normalizeOperation(u);
    if (!normalized.ok()) return std::nullopt;
    u = normalized.value;
    const auto forwardSize = SemanticCodec::measureProtobufOperation(f);
    const auto inverseSize = SemanticCodec::measureProtobufOperation(u);
    if (!forwardSize.ok() || !inverseSize.ok()) return std::nullopt;
    OperationEngine engine;
    const auto reverse = engine.prepare(u, {after, context.applied_operations, context.manifest});
    if (reverse.disposition != PrepareDisposition::kPrepared || !reverse.plan) return std::nullopt;
    StagedObjectView restored(static_cast<const ObjectStore&>(after));
    if (!stage(restored, *reverse.plan) || restored.size() != before.size()) return std::nullopt;
    auto ids = footprint(plan);
    const auto reverseIds = footprint(*reverse.plan);
    ids.insert(reverseIds.begin(), reverseIds.end());
    bool changed = false;
    PreparedUndoableCommand result{f, std::move(u), {}, {}, forwardSize.bytes, inverseSize.bytes};
    for (const auto& id : ids) {
        const auto* prior = before.find(id); const auto* post = after.find(id); const auto* undo = restored.find(id);
        if ((prior == nullptr) != (undo == nullptr) || (prior && *prior != *undo)) return std::nullopt;
        changed = changed || (prior == nullptr) != (post == nullptr) || (prior && post && *prior != *post);
        const auto fieldAspect = !prior || !post ? HistoryAspect::kWholeRecord : aspect;
        std::vector<std::uint32_t> fields;
        if (const auto* patches = std::get_if<PatchPropertiesOp>(&f.payload))
            for (const auto& item : patches->patches) if (item.object_id == id) fields.push_back(item.field_id);
        result.before.push_back({id, prior ? std::optional<ObjectRecord>(*prior) : std::nullopt, fieldAspect, fields});
        result.after.push_back({id, post ? std::optional<ObjectRecord>(*post) : std::nullopt, fieldAspect, fields});
    }
    if (!changed) return std::nullopt;
    return result;
}
} // namespace canvas::interaction
