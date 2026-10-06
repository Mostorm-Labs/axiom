#pragma once

#include <cstdint>

namespace canvas::runtime {

// Common owner marker.  Debug UI consumes these interfaces; it never owns the
// state behind them and never receives mutable Runtime/renderer pointers.
class DiagnosticsProvider {
 public:
  virtual ~DiagnosticsProvider() = default;
};

struct ArcDiagnosticsSnapshot final {
  std::uint64_t previewRevision = 0;
  std::uint64_t activePointerCount = 0;
  std::uint64_t inputBatchCount = 0;
  std::uint64_t handoffCount = 0;
  bool previewActive = false;
};

class ArcDiagnostics : public DiagnosticsProvider {
 public:
  ~ArcDiagnostics() override = default;
  [[nodiscard]] virtual ArcDiagnosticsSnapshot readArcDiagnostics() const noexcept = 0;
};

// P15 names the owner seams with an IAxiom/Arc/Platform prefix.  Keep the
// concrete C++ names short while exposing those stable contract names to
// common consumers without coupling them to a platform implementation.
using IArcDiagnostics = ArcDiagnostics;

struct PlatformDiagnosticsSnapshot final {
  std::uint64_t canonicalSurfaceGeneration = 0;
  std::uint64_t previewSurfaceGeneration = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  float devicePixelRatio = 1.0F;
  std::uint64_t presentCount = 0;
  std::uint64_t lostCount = 0;
  bool surfaceAvailable = false;
};

class PlatformDiagnostics : public DiagnosticsProvider {
 public:
  ~PlatformDiagnostics() override = default;
  [[nodiscard]] virtual PlatformDiagnosticsSnapshot readPlatformDiagnostics() const noexcept = 0;
};

using IPlatformDiagnostics = PlatformDiagnostics;

}  // namespace canvas::runtime
