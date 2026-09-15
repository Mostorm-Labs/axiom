#pragma once

#include "canvas/scene/incremental_runtime_coordinator.hpp"

namespace canvas {

class IncrementalRuntimeTestAccess final {
  public:
    static void failAt(IncrementalRuntimeCoordinator& coordinator,
                       RuntimeCheckpoint checkpoint) noexcept {
        coordinator.setCheckpointFailure(checkpoint);
    }
    static void clear(IncrementalRuntimeCoordinator& coordinator) noexcept {
        coordinator.clearCheckpointFailure();
    }
    static void corrupt(IncrementalRuntimeCoordinator& coordinator) noexcept {
        coordinator.corruptRuntimeProjectionForTest();
    }
};

} // namespace canvas
