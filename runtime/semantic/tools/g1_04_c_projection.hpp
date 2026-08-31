#pragma once

#include "canvas/semantic/object_store.hpp"
#include "canvas/semantic/apply_plan.hpp"

#include <string>
#include <vector>

namespace canvas::verification::g1_04_c {

[[nodiscard]] std::string projectObjects(
    const std::vector<canvas::semantic::ObjectRecord>& objects);
[[nodiscard]] std::string projectStore(const canvas::semantic::ObjectStore& store);
[[nodiscard]] std::string projectPlan(const canvas::semantic::PreparedApplyPlan& plan);

} // namespace canvas::verification::g1_04_c
