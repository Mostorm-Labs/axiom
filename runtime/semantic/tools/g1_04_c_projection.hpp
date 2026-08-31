#pragma once

#include "canvas/semantic/object_store.hpp"
#include "canvas/semantic/apply_plan.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace canvas::verification::g1_04_c {

struct ObjectProjectionRecord final {
    std::string id;
    std::uint32_t kind = 0;
    std::uint32_t kindVersion = 0;
    std::optional<std::string> parentId;
    std::string orderKeyHex;

    bool operator==(const ObjectProjectionRecord&) const = default;
};

struct ObjectProjection final {
    std::vector<ObjectProjectionRecord> objects;

    bool operator==(const ObjectProjection&) const = default;
};

struct PlanProjection final {
    ObjectProjection creates;
    ObjectProjection replacements;
    std::vector<std::string> deletes;
    std::optional<std::vector<std::string>> deleteClosure;

    bool operator==(const PlanProjection&) const = default;
};

[[nodiscard]] ObjectProjection projectObjects(
    const std::vector<canvas::semantic::ObjectRecord>& objects);
[[nodiscard]] ObjectProjection projectStore(const canvas::semantic::ObjectStore& store);
[[nodiscard]] PlanProjection projectPlan(const canvas::semantic::PreparedApplyPlan& plan);

} // namespace canvas::verification::g1_04_c
