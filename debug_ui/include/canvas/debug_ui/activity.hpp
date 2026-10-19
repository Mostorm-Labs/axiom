#pragma once

#include "canvas/runtime/product_control.hpp"
#include "canvas/runtime/surface_debug_control.hpp"

#include <optional>
#include <cstdint>
#include <string>
#include <vector>

namespace canvas::debug_ui {

enum class DebugControlOwner : std::uint8_t {
  kProduct,
  kAxiomDebug,
  kPlatformDebug,
};

enum class DebugActivityState : std::uint8_t {
  kPending,
  kApplied,
  kRejected,
  kUnsupported,
  kUnavailable,
  kStaleGeneration,
  kExpired,
  kQueueFull,
  kFailed,
};

struct DebugActivityEntry final {
  std::uint64_t sequence = 0;
  std::uint64_t requestId = 0;
  DebugControlOwner owner = DebugControlOwner::kProduct;
  std::string action;
  DebugActivityState state = DebugActivityState::kPending;
  std::uint64_t runtimeGeneration = 0;
  std::uint64_t documentGeneration = 0;
  std::uint64_t surfaceGeneration = 0;
  std::uint64_t frameId = 0;
  std::optional<canvas::runtime::ProductControlReceipt> productReceipt;
  std::optional<canvas::runtime::SurfaceModeReceipt> surfaceReceipt;
};

struct DebugActivitySnapshot final {
  std::optional<canvas::runtime::ProductControlReceipt> productControl;
  std::optional<canvas::runtime::SurfaceModeReceipt> surfaceControl;
  std::vector<DebugActivityEntry> entries;
};

class DebugActivitySource {
 public:
  virtual ~DebugActivitySource() = default;
  [[nodiscard]] virtual DebugActivitySnapshot readActivity() const noexcept = 0;
};

}  // namespace canvas::debug_ui
