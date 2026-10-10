#pragma once

#include "canvas/runtime/product_control.hpp"
#include "canvas/runtime/surface_debug_control.hpp"

#include <optional>

namespace canvas::debug_ui {

struct DebugActivitySnapshot final {
  std::optional<canvas::runtime::ProductControlReceipt> productControl;
  std::optional<canvas::runtime::SurfaceModeReceipt> surfaceControl;
};

class DebugActivitySource {
 public:
  virtual ~DebugActivitySource() = default;
  [[nodiscard]] virtual DebugActivitySnapshot readActivity() const noexcept = 0;
};

}  // namespace canvas::debug_ui
