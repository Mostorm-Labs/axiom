#pragma once

#include "minimal_runtime_host.hpp"
#include "canvas/runtime/canvas_runtime_facade.hpp"
#include "canvas/runtime/diagnostics.hpp"

namespace canvas::minimal_runtime_host {

class MinimalRuntimeFacade final : public runtime::CanvasRuntimeFacade,
                                   public runtime::PlatformDiagnostics {
 public:
  explicit MinimalRuntimeFacade(MinimalRuntimeHost& host) noexcept;
  [[nodiscard]] runtime::PlatformDiagnosticsSnapshot readPlatformDiagnostics() const noexcept override;
  [[nodiscard]] std::size_t process() noexcept;

 private:
  MinimalRuntimeHost& host_;
  std::uint64_t ownerSequence_ = 0;
};

}  // namespace canvas::minimal_runtime_host