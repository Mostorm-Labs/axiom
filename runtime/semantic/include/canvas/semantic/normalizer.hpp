#pragma once

#include "canvas/semantic/operation.hpp"
#include "canvas/semantic/semantic_error.hpp"

#include <array>
#include <string_view>

namespace canvas::semantic {

struct OperationCollectionProfile final {
    std::string_view path;
    std::string_view key;
};

inline constexpr std::array<OperationCollectionProfile, 13> kOperationCollectionProfiles{{
    {"payload.insert_objects.objects", "id"},
    {"payload.delete_objects.object_ids", "object_id"},
    {"payload.restore_objects.objects", "id"},
    {"payload.set_placements.items", "object_id"},
    {"payload.set_transforms.items", "object_id"},
    {"payload.patch_properties.patches", "(object_id, field_id)"},
    {"payload.set_object_size.items", "object_id"},
    {"payload.split_strokes.splits", "source_stroke_id"},
    {"payload.split_strokes.splits.replacements", "id"},
    {"payload.add_erase_masks.items", "object_id"},
    {"payload.add_erase_masks.items.masks", "mask_id"},
    {"payload.remove_erase_masks.items", "object_id"},
    {"payload.remove_erase_masks.items.mask_ids", "mask_id"},
}};

inline constexpr std::array<std::string_view, 2> kOperationWideUniquePaths{{
    "payload.split_strokes.splits.replacements",
    "payload.add_erase_masks.items.masks",
}};

struct NormalizeResult final {
    Operation value{};
    SemanticError error = SemanticError::kNone;
    [[nodiscard]] bool ok() const noexcept { return error == SemanticError::kNone; }
};

[[nodiscard]] NormalizeResult normalizeOperation(const Operation& input);

} // namespace canvas::semantic
