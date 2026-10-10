// Contract consumer: product control compiles independently of diagnostics.
#include "canvas/runtime/runtime_facade.hpp"
#include "canvas/runtime/diagnostics.hpp"
#include "canvas/runtime/telemetry.hpp"

#include <cassert>
#include <type_traits>

namespace {
using namespace canvas::runtime;

struct RuntimeProbe : RuntimeFacade {
  ProductControlRequest submitted{};
  [[nodiscard]] RuntimeStateSnapshot readRuntimeState() const noexcept override {
    return {{11U, 12U, 13U, 14U, 15U}, {4102U, 2U, 3U, 1U},
            {2.0F, 10.0F, -20.0F}, {true, false}, {true, 2U, 99U, 7U}};
  }
  [[nodiscard]] ProductControlReceipt submitProductControl(
      const ProductControlRequest& request) noexcept override {
    submitted = request;
    return {request.requestId, ProductControlState::kApplied, request.runtimeGeneration};
  }
};

struct DiagnosticsProbe : IAxiomDiagnostics {
  [[nodiscard]] AxiomDiagnosticsSnapshot readDiagnostics() const noexcept override {
    return {{11U, 12U, 13U, 14U, 15U}, {21U}, {2.0F, 10.0F, -20.0F, 8U},
            {true, false}, {4102U, true, 2U, 99U, 7U, 4U, 5U}};
  }
};

struct CombinedOwner final : RuntimeProbe, DiagnosticsProbe {};

void productStateDoesNotRequireDiagnostics() {
  RuntimeProbe owner;
  RuntimeFacade& product = owner;
  const auto state = product.readRuntimeState();
  assert(state.identity.runtimeGeneration == 11U);
  assert(state.identity.documentGeneration == 12U);
  assert(state.identity.documentRevision == 13U);
  assert(state.identity.viewGeneration == 14U);
  assert(state.identity.surfaceGeneration == 15U);
  assert(state.tool.toolId == 4102U);
  assert(state.tool.brushId == 2U && state.tool.brushRevision == 3U);
  assert(state.tool.eraserId == 1U);
  assert(state.camera.scale == 2.0F);
  assert(state.camera.translationX == 10.0F && state.camera.translationY == -20.0F);
  assert(state.history.canUndo && !state.history.canRedo);
  assert(state.selection.enabled && state.selection.selectedObjectCount == 2U);
  assert(state.selection.primaryObject == 99U && state.selection.snapCandidateCount == 7U);

  const auto empty = RuntimeStateSnapshot{};
  assert(empty.identity.runtimeGeneration == 0U && empty.tool.toolId == 0U);
  assert(empty.camera.scale == 1.0F && empty.camera.translationX == 0.0F);
  assert(!empty.history.canUndo && !empty.history.canRedo && !empty.selection.enabled);
}

void brushAndEraserIdentityNeverOverloadToolId() {
  RuntimeProbe owner;
  const auto brush = owner.setBrush(3U, 4U, 42U, 11U);
  assert(brush.requestId == 42U && brush.runtimeGeneration == 11U);
  assert(brush.state == ProductControlState::kApplied);
  assert(owner.submitted.action == ProductControlAction::kSetBrush);
  assert(owner.submitted.brushId == 3U && owner.submitted.brushRevision == 4U);
  assert(owner.submitted.toolId == 0U && owner.submitted.eraserId == 0U);

  const auto eraser = owner.setEraser(2U, 43U, 11U);
  assert(eraser.requestId == 43U && eraser.runtimeGeneration == 11U);
  assert(owner.submitted.action == ProductControlAction::kSetEraser);
  assert(owner.submitted.eraserId == 2U && owner.submitted.toolId == 0U);
  assert(owner.submitted.brushId == 0U && owner.submitted.brushRevision == 0U);

  (void)owner.setTool(4108U, 44U, 11U);
  assert(owner.submitted.action == ProductControlAction::kSetTool);
  assert(owner.submitted.toolId == 4108U);
  assert(owner.submitted.brushId == 0U && owner.submitted.eraserId == 0U);

  const auto pointer = owner.submitSelectionPointer({7U, SelectionPointerPhase::kDown,
                                                     20.0F, 30.0F, 45U, 11U});
  assert(pointer.requestId == 45U && pointer.runtimeGeneration == 11U);
  assert(pointer.state == ProductControlState::kUnsupported);
}

void diagnosticsCanBeConsumedWithoutProductMutation() {
  const DiagnosticsProbe diagnostics;
  const IAxiomDiagnostics& observer = diagnostics;
  const auto core = observer.readDiagnostics();
  assert(core.identity.runtimeGeneration == 11U && core.identity.documentRevision == 13U);
  assert(core.document.canonicalOperationCount == 21U);
  assert(core.camera.scale == 2.0F && core.camera.generation == 8U);
  assert(core.camera.translationX == 10.0F && core.camera.translationY == -20.0F);
  assert(core.history.canUndo && !core.history.canRedo);
  assert(core.interaction.toolId == 4102U && core.interaction.selectionMode);
  assert(core.interaction.selectedObjectCount == 2U && core.interaction.selectedPrimaryObject == 99U);
  assert(core.interaction.snapCandidateCount == 7U);
  assert(core.interaction.overlayUpdateCount == 4U && core.interaction.transientTransformCount == 5U);
  const auto features = observer.readFeatureDiagnostics();
  assert(!features.ink.supported && !features.ink.available);
  assert(!features.shape.supported && !features.shape.available);
  assert(!features.richText.supported && !features.richText.available);
  assert(!features.connector.supported && !features.connector.available);
  assert(!features.snap.supported && !features.snap.available);
  assert(!features.image.supported && !features.image.available);

  CombinedOwner combined;
  RuntimeFacade& product = combined;
  const IAxiomDiagnostics& separateCapability = combined;
  assert(product.readRuntimeState().identity.runtimeGeneration == 11U);
  assert(separateCapability.readDiagnostics().identity.runtimeGeneration == 11U);
}

void platformAndTelemetryExposeOwnerObservations() {
  PlatformDiagnosticsSnapshot platform{};
  platform.canonicalSurfaceMode = SurfaceMode::kGpuDefault;
  platform.canonicalSurfaceGeneration = 5U;
  assert(platform.canonicalSurfaceMode == SurfaceMode::kGpuDefault);
  assert(platform.canonicalSurfaceGeneration == 5U);
  TelemetrySnapshot telemetry{};
  telemetry.sampleHz = 120.0;
  telemetry.frameMs = 8.0;
  telemetry.queueAgeMs = 2.0;
  assert(telemetry.sampleHz == 120.0 && telemetry.frameMs == 8.0 && telemetry.queueAgeMs == 2.0);
}
}  // namespace

int main() {
  static_assert(!std::is_base_of_v<canvas::runtime::RuntimeDiagnostics,
                                  canvas::runtime::RuntimeFacade>);
  static_assert(!std::is_base_of_v<canvas::runtime::RuntimeFacade,
                                  canvas::runtime::IAxiomDiagnostics>);
  static_assert(std::is_default_constructible_v<canvas::runtime::RuntimeStateSnapshot>);
  static_assert(std::is_trivially_copyable_v<canvas::runtime::RuntimeStateSnapshot>);
  static_assert(static_cast<unsigned>(canvas::runtime::ProductControlAction::kSetTool) == 0U);
  static_assert(static_cast<unsigned>(canvas::runtime::ProductControlAction::kCanvasControl) == 7U);
  static_assert(static_cast<unsigned>(canvas::runtime::ProductControlState::kFailed) == 4U);
  productStateDoesNotRequireDiagnostics();
  brushAndEraserIdentityNeverOverloadToolId();
  diagnosticsCanBeConsumedWithoutProductMutation();
  platformAndTelemetryExposeOwnerObservations();
  return 0;
}
