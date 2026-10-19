#pragma once

#include "canvas/debug_ui/snapshot.hpp"

#include <cstdint>

namespace canvas::debug_ui {

struct DebugSnapshotSources final {
  const canvas::runtime::RuntimeFacade* runtime = nullptr;
  const canvas::runtime::IAxiomDiagnostics* axiom = nullptr;
  const canvas::runtime::IArcDiagnostics* arc = nullptr;
  const canvas::runtime::IPlatformDiagnostics* platform = nullptr;
  const canvas::runtime::ITelemetry* telemetry = nullptr;
  const DebugActivitySource* activity = nullptr;
};

class DebugSnapshotAssembler final {
 public:
  [[nodiscard]] DebugSnapshot capture(const DebugSnapshotSources& sources) const noexcept;
  [[nodiscard]] std::uint64_t snapshotSequence() const noexcept { return snapshotSequence_; }

 private:
  mutable std::uint64_t snapshotSequence_ = 0;
};

}  // namespace canvas::debug_ui
