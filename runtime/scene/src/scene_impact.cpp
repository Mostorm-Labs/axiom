#include "canvas/scene/scene_impact.hpp"

#include <type_traits>
#include <variant>

namespace canvas::scene {
ImpactClassification classifyImpact(const semantic::ObjectSemanticChange& change) noexcept {
    return classifyImpact(change, nullptr);
}

ImpactClassification classifyImpact(
    const semantic::ObjectSemanticChange& change,
    const semantic::ObjectRecord* current_record) noexcept {
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
        if (current_record != nullptr) {
            out.relation = current_record->kind == semantic::ObjectKind::kConnector
                               ? DirtyState::kDirty : DirtyState::kReuse;
            out.resource = std::visit([](const auto& content) {
                using T = std::decay_t<decltype(content)>;
                if constexpr (std::is_same_v<T, semantic::ImageContent>) {
                    return !content.resource_id.value.isZero();
                } else if constexpr (std::is_same_v<T, semantic::RichTextContent>) {
                    for (const auto& paragraph : content.document.paragraphs) {
                        for (const auto& run : paragraph.runs) {
                            if (run.style.font_resource_id.has_value() &&
                                !run.style.font_resource_id->value.isZero()) return true;
                        }
                    }
                    return false;
                } else if constexpr (std::is_same_v<T, semantic::VectorStrokeContent>) {
                    return content.stroke.brush.texture_resource_id.has_value() &&
                           !content.stroke.brush.texture_resource_id->value.isZero();
                } else if constexpr (std::is_same_v<T, semantic::DabStrokeContent>) {
                    return content.stroke.brush.texture_resource_id.has_value() &&
                           !content.stroke.brush.texture_resource_id->value.isZero();
                } else {
                    return false;
                }
            }, current_record->content) ? DirtyState::kDirty : DirtyState::kReuse;
        } else {
            // Without a semantic record, retain the conservative historical
            // classification; callers with context use the overload above.
            out.relation = DirtyState::kDirty;
        }
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
        } else {
            out.ordinary_property = DirtyState::kDirty;
        }
    }
    return out;
}
} // namespace canvas::scene
