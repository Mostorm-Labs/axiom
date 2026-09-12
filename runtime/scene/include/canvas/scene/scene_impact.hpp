#pragma once

#include "canvas/semantic/change_set.hpp"

namespace canvas::scene {

enum class DirtyState : unsigned char { kReuse, kDirty };

struct ImpactClassification final {
    DirtyState record = DirtyState::kReuse;
    DirtyState hierarchy = DirtyState::kReuse;
    DirtyState local_geometry = DirtyState::kReuse;
    DirtyState visual_bounds = DirtyState::kReuse;
    DirtyState world_bounds = DirtyState::kReuse;
    DirtyState spatial = DirtyState::kReuse;
    DirtyState relation = DirtyState::kReuse;
    DirtyState resource = DirtyState::kReuse;
    DirtyState visibility = DirtyState::kReuse;
    DirtyState ordinary_property = DirtyState::kReuse;
};

[[nodiscard]] ImpactClassification classifyImpact(const semantic::ObjectSemanticChange& change) noexcept;
[[nodiscard]] ImpactClassification classifyImpact(
    const semantic::ObjectSemanticChange& change,
    const semantic::ObjectRecord* current_record) noexcept;

} // namespace canvas::scene

namespace canvas { using scene::DirtyState; using scene::ImpactClassification; using scene::classifyImpact; }
