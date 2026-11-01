#pragma once

#include "canvas/runtime/canvas_runtime_facade.hpp"
#include "ink_playground_host.hpp"

#include <cstddef>
#include <optional>
#include <vector>
#include <thread>

namespace canvas::ink_playground {

// Common RuntimeFacade adapter for platform shells.  Platforms retain their
// diagnostics and legacy input hooks, while all typed canvas controls use one
// owner binding and one bounded service queue.
class CanvasRuntimeFacadeAdapter : public runtime::CanvasRuntimeFacade {
 public:
  CanvasRuntimeFacadeAdapter(InkPlaygroundHost& host,
                             runtime::CanvasTargetKey target) noexcept
      : runtime::CanvasRuntimeFacade(host, target) {}

 public:
  // Called only by the platform/runtime owner at its serialized safe point.
  // Submission never executes the queue inline or re-enters the owner.
  [[nodiscard]] std::size_t processCanvasControls(std::uint64_t ownerSequence) noexcept {
    return process(ownerSequence);
  }

 protected:
  [[nodiscard]] const runtime::CanvasTargetKey& canvasTarget() const noexcept {
    return target();
  }
};

}  // namespace canvas::ink_playground
