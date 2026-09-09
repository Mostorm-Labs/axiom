#pragma once

#include "g1_07_replay_inspector.hpp"

namespace canvas::verification::g1_07 {

[[nodiscard]] ReplayTrace makeMinimumTrace();
[[nodiscard]] ReplayTrace makeAllOperationFamiliesTrace();

} // namespace canvas::verification::g1_07
