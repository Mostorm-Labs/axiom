#pragma once
#include "canvas/semantic/object_record.hpp"
#include <variant>
#include <vector>
namespace canvas::semantic {
struct ContainerStructuralExpectation final {
    ObjectId object_id{};
    ObjectKind kind = ObjectKind::kShape;
    std::uint32_t kind_version = 0;
    Placement placement_before{};
    Transform2D transform_before{};
    bool operator==(const ContainerStructuralExpectation&) const = default;
};
struct ContainerChildExpectation final {
    ObjectId object_id{}; OrderKey order_key{};
    bool operator==(const ContainerChildExpectation&) const = default;
};
struct ContainerChildrenExpectation final {
    std::optional<ObjectId> parent_id;
    std::vector<ContainerChildExpectation> complete_members;
    bool operator==(const ContainerChildrenExpectation&) const = default;
};
struct ContainerTopologyGuard final {
    std::vector<ContainerStructuralExpectation> nodes;
    std::vector<ContainerChildrenExpectation> child_sets;
    bool operator==(const ContainerTopologyGuard&) const = default;
};
struct ContainerRelocation final {
    ObjectId object_id{}; Placement placement_after{}; Transform2D transform_after{};
    bool operator==(const ContainerRelocation&) const = default;
};
struct ContainerOrderOnlyEdit final {
    ObjectId object_id{}; OrderKey order_key_after{};
    bool operator==(const ContainerOrderOnlyEdit&) const = default;
};
struct IntroduceWrappersEffect final {
    std::vector<ObjectRecord> containers;
    bool operator==(const IntroduceWrappersEffect&) const = default;
};
struct ReleaseWrappersEffect final {
    std::vector<ObjectRecord> containers_before;
    bool operator==(const ReleaseWrappersEffect&) const = default;
};
struct RelocateObjectsEffect final { bool operator==(const RelocateObjectsEffect&) const = default; };
struct EditContainerTopologyOp final {
    ContainerTopologyGuard guard{};
    std::vector<ContainerRelocation> relocations;
    std::vector<ContainerOrderOnlyEdit> order_only_edits;
    std::variant<std::monostate, IntroduceWrappersEffect, ReleaseWrappersEffect, RelocateObjectsEffect> effect;
    bool operator==(const EditContainerTopologyOp&) const = default;
};
struct SectionContentEdit final {
    ObjectId object_id{}; std::uint32_t expected_kind_version = 0;
    SectionContent expected_content{}; SectionContent replacement_content{};
    bool operator==(const SectionContentEdit&) const = default;
};
struct SetSectionContentOp final {
    std::vector<SectionContentEdit> items;
    std::vector<ContainerStructuralExpectation> geometry_basis;
    bool operator==(const SetSectionContentOp&) const = default;
};
} // namespace canvas::semantic
