#pragma once

#include "canvas/runtime/feature_diagnostics.hpp"
#include "canvas/runtime/surface_debug_control.hpp"

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

struct AxiomDiagnosticsIdentityState final {
  std::uint64_t runtimeGeneration = 0;
  std::uint64_t documentGeneration = 0;
  std::uint64_t documentRevision = 0;
  std::uint64_t viewGeneration = 0;
  std::uint64_t surfaceGeneration = 0;
};

struct AxiomDiagnosticsDocumentState final {
  std::uint64_t canonicalOperationCount = 0;
};

struct AxiomDiagnosticsCameraState final {
  float scale = 1.0F;
  float translationX = 0.0F;
  float translationY = 0.0F;
  std::uint64_t generation = 0;
};

struct AxiomDiagnosticsHistoryState final {
  bool canUndo = false;
  bool canRedo = false;
};

struct AxiomDiagnosticsInteractionState final {
  std::uint32_t toolId = 0;
  bool selectionMode = false;
  std::uint32_t selectedObjectCount = 0;
  std::uint64_t selectedPrimaryObject = 0;
  std::uint64_t snapCandidateCount = 0;
  std::uint64_t overlayUpdateCount = 0;
  std::uint64_t transientTransformCount = 0;
};

struct AxiomDiagnosticsSnapshot final {
  AxiomDiagnosticsIdentityState identity{};
  AxiomDiagnosticsDocumentState document{};
  AxiomDiagnosticsCameraState camera{};
  AxiomDiagnosticsHistoryState history{};
  AxiomDiagnosticsInteractionState interaction{};
};

using RuntimeDiagnosticsSnapshot = AxiomDiagnosticsSnapshot;

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
  SurfaceMode canonicalSurfaceMode = SurfaceMode::kPlatformDefault;
};

class PlatformDiagnostics : public DiagnosticsProvider {
 public:
  ~PlatformDiagnostics() override = default;
  [[nodiscard]] virtual PlatformDiagnosticsSnapshot readPlatformDiagnostics() const noexcept = 0;
};

using IPlatformDiagnostics = PlatformDiagnostics;

class RuntimeDiagnostics : public DiagnosticsProvider {
 public:
  ~RuntimeDiagnostics() override = default;
  [[nodiscard]] virtual AxiomDiagnosticsSnapshot readDiagnostics() const noexcept = 0;
  [[nodiscard]] virtual FeatureDiagnosticsSnapshot readFeatureDiagnostics() const noexcept {
    return {};
  }
};

using AxiomDiagnostics = RuntimeDiagnostics;
using IAxiomDiagnostics = RuntimeDiagnostics;

}  // namespace canvas::runtime
