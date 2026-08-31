#pragma once

#include "canvas/semantic/object_store.hpp"

#include <string>
#include <vector>

namespace canvas::verification::g1_04_c {

[[nodiscard]] std::string projectObjects(
    const std::vector<canvas::semantic::ObjectRecord>& objects);
[[nodiscard]] std::string projectStore(const canvas::semantic::ObjectStore& store);

} // namespace canvas::verification::g1_04_c
