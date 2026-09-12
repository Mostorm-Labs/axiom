#include "canvas/scene/scene_impact.hpp"

namespace canvas::scene {
ImpactClassification classifyImpact(const semantic::ObjectSemanticChange& change) noexcept {
    ImpactClassification out;
    using F = semantic::SemanticChangeFlags;
    const auto has = [&](F flag) { return (static_cast<unsigned char>(change.flags) & static_cast<unsigned char>(flag)) != 0; };
    if (has(F::kCreated) || has(F::kDeleted)) {
        out.record = out.local_geometry = out.visual_bounds = out.world_bounds = out.spatial =
            out.hierarchy = DirtyState::kDirty;
        return out;
    }
    if (has(F::kPlacement)) {
        out.hierarchy = out.world_bounds = out.spatial = DirtyState::kDirty;
    }
    if (has(F::kTransform)) {
        out.world_bounds = out.spatial = DirtyState::kDirty;
    }
    if (has(F::kContent)) {
        out.local_geometry = out.visual_bounds = out.world_bounds = out.spatial = DirtyState::kDirty;
        out.relation = DirtyState::kDirty;
    }
    if (has(F::kEraseMasks)) {
        out.visual_bounds = out.world_bounds = out.spatial = DirtyState::kDirty;
    }
    if (has(F::kProperties)) {
        bool opacity_only = !change.changed_fields.empty();
        bool visibility_only = !change.changed_fields.empty();
        for (const auto field : change.changed_fields) {
            opacity_only = opacity_only && field == 3U;
            visibility_only = visibility_only && field == 1U;
        }
        if (visibility_only) {
            out.visibility = DirtyState::kDirty;
        } else if (!opacity_only) {
            out.visual_bounds = out.world_bounds = out.spatial = DirtyState::kDirty;
            bool resource = false;
            for (const auto field : change.changed_fields) resource = resource || field >= 0x300U;
            if (resource) out.resource = DirtyState::kDirty;
        } else {
            out.ordinary_property = DirtyState::kDirty;
        }
    }
    return out;
}
} // namespace canvas::scene
