#pragma once

#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/runtime/internal/runtime_platform_access.hpp"

namespace canvas::minimal_runtime_host {
// No tool callbacks or private editor state: only input registration and an
// execution opportunity delegated to the same Runtime used by Playground.
class MinimalRuntimeHost final {
 public:
  MinimalRuntimeHost() noexcept;
  [[nodiscard]] bool registerSource(input::InputSourceId source) noexcept;
  [[nodiscard]] bool unregisterSource(input::InputSourceId source,
                                      std::uint64_t sequence,
                                      std::uint64_t timestampNs) noexcept;
  [[nodiscard]] bool submit(const input::PlatformPointerBatch& batch) noexcept;
  [[nodiscard]] runtime::RuntimeCompositionCore& composition() noexcept {
    return runtime::RuntimePlatformAccess::core(composition_);
  }
  [[nodiscard]] runtime::RuntimeComposition& publicComposition() noexcept { return composition_; }
 private:
  runtime::RuntimeComposition composition_;
};
}  // namespace canvas::minimal_runtime_host
