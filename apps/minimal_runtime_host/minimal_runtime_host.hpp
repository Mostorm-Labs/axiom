#pragma once

#include "canvas/interaction/canvas_editing_runtime.hpp"

namespace canvas::minimal_runtime_host {
// No tool callbacks or private editor state: only input registration and an
// execution opportunity delegated to the same Runtime used by Playground.
class MinimalRuntimeHost final {
 public:
  [[nodiscard]] bool registerSource(input::InputSourceId source) noexcept;
  [[nodiscard]] bool unregisterSource(input::InputSourceId source,
                                      std::uint64_t sequence,
                                      std::uint64_t timestampNs) noexcept;
  [[nodiscard]] bool submit(const input::PlatformPointerBatch& batch) noexcept;
  [[nodiscard]] interaction::CanvasEditingRuntime& runtime() noexcept { return runtime_; }
 private:
  interaction::CanvasEditingRuntime runtime_;
};
}  // namespace canvas::minimal_runtime_host
