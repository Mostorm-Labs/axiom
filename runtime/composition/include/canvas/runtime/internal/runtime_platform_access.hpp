#pragma once

namespace canvas::runtime {
class RuntimeComposition;
class RuntimeCompositionCore;

// Transitional internal bridge for platform migration. It never owns a core.
class RuntimePlatformAccess final {
 public:
  [[nodiscard]] static RuntimeCompositionCore& core(RuntimeComposition&) noexcept;
};
}  // namespace canvas::runtime
