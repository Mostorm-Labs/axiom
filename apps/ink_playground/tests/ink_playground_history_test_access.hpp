#pragma once

#include "ink_playground_host.hpp"
#include "../../../runtime/scene/tests/incremental_runtime_test_access.hpp"

namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess final {
 public:
  static std::vector<semantic::ObjectRecord> objects(const InkPlaygroundHost& host) {
    return host.semanticObjects_.allObjects();
  }
  static void failPublication(InkPlaygroundHost& host) noexcept {
    IncrementalRuntimeTestAccess::failAt(*host.sceneCoordinator_, RuntimeCheckpoint::kBeforePublication);
  }
  static void clearFailure(InkPlaygroundHost& host) noexcept {
    IncrementalRuntimeTestAccess::clear(*host.sceneCoordinator_);
  }
  static void exhaustOperationIds(InkPlaygroundHost& host) noexcept {
    host.nextHistoryOperationOrdinal_ = 0U;
  }
};
}  // namespace canvas::ink_playground
