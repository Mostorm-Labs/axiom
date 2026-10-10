#include "canvas/debug_ui/snapshot_assembler.hpp"
#include "canvas/debug_ui/snapshot.hpp"
#include "canvas/debug_ui/activity.hpp"
#include "canvas/runtime/diagnostics.hpp"
#include "canvas/runtime/runtime_facade.hpp"
#include "canvas/runtime/telemetry.hpp"
#include <cassert>
#include <cstdint>
#include <optional>

namespace {
template <typename T>
concept HasCapabilities = requires(T value) { value.capabilities; };
struct RuntimeProbe final : canvas::runtime::RuntimeFacade {
  mutable int reads = 0;
  canvas::runtime::RuntimeStateSnapshot first{};
  canvas::runtime::RuntimeStateSnapshot second{};
  canvas::runtime::RuntimeStateSnapshot readRuntimeState() const noexcept override {
    ++reads;
    return reads == 1 ? first : second;
  }
  canvas::runtime::ProductControlReceipt submitProductControl(
      const canvas::runtime::ProductControlRequest& request) noexcept override {
    return {request.requestId, canvas::runtime::ProductControlState::kApplied,
            request.runtimeGeneration};
  }
};
struct AlwaysChangingRuntime final : canvas::runtime::RuntimeFacade {
  mutable int reads = 0;
  canvas::runtime::RuntimeStateSnapshot readRuntimeState() const noexcept override {
    ++reads;
    canvas::runtime::RuntimeStateSnapshot value{};
    value.identity.runtimeGeneration = static_cast<std::uint64_t>(reads);
    return value;
  }
  canvas::runtime::ProductControlReceipt submitProductControl(
      const canvas::runtime::ProductControlRequest& request) noexcept override {
    return {request.requestId, canvas::runtime::ProductControlState::kApplied,
            request.runtimeGeneration};
  }
};
struct AxiomProbe final : canvas::runtime::IAxiomDiagnostics {
  canvas::runtime::AxiomDiagnosticsSnapshot value{};
  canvas::runtime::AxiomDiagnosticsSnapshot readDiagnostics() const noexcept override { return value; }
};
struct TelemetryProbe final : canvas::runtime::ITelemetry {
  canvas::runtime::TelemetrySnapshot value{};
  canvas::runtime::TelemetrySnapshot readTelemetry() const noexcept override { return value; }
};
struct ArcProbe final : canvas::runtime::IArcDiagnostics {
  canvas::runtime::ArcDiagnosticsSnapshot value{};
  canvas::runtime::ArcDiagnosticsSnapshot readArcDiagnostics() const noexcept override { return value; }
};
struct PlatformProbe final : canvas::runtime::IPlatformDiagnostics {
  canvas::runtime::PlatformDiagnosticsSnapshot value{};
  canvas::runtime::PlatformDiagnosticsSnapshot readPlatformDiagnostics() const noexcept override { return value; }
};
struct ActivityProbe final : canvas::debug_ui::DebugActivitySource {
  canvas::debug_ui::DebugActivitySnapshot value{};
  canvas::debug_ui::DebugActivitySnapshot readActivity() const noexcept override { return value; }
};
}

int main() {
  RuntimeProbe runtime;
  runtime.first.identity = {7, 8, 9, 10, 11};
  runtime.second.identity = runtime.first.identity;
  runtime.second.tool.toolId = 4101;
  AxiomProbe axiom;
  ArcProbe arc;
  PlatformProbe platform;
  platform.value.surfaceAvailable = true;
  platform.value.presentCount = 42;
  TelemetryProbe telemetry;
  telemetry.value.sequence = 99;
  telemetry.value.canonicalFrames = 43;
  ActivityProbe activity;
  canvas::debug_ui::DebugSnapshotSources sources{&runtime, &axiom, &arc, &platform, &telemetry, &activity};
  canvas::debug_ui::DebugSnapshotAssembler assembler;
  const auto snapshot = assembler.capture(sources);
  assert(snapshot.coherence == canvas::debug_ui::SnapshotCoherence::kCoherent);
  assert(snapshot.product.availability == canvas::debug_ui::DebugAvailability::kAvailable);
  assert(snapshot.product.value.identity.documentRevision == 9);
  assert(snapshot.telemetry.value.sequence == 99);
  assert(snapshot.stamp.sequence == 99);
  assert(snapshot.stamp.frameId == 43);
  assert(snapshot.stamp.snapshotSequence == 1);
  assert(snapshot.stamp.monotonicTimeNs != 0);
  assert(runtime.reads == 2);
  static_assert(!HasCapabilities<canvas::debug_ui::DebugSnapshot>);
  canvas::debug_ui::MutexCopySnapshotChannel channel;
  channel.publish(snapshot);
  assert(channel.read().product.value.tool.toolId == 4101);

  RuntimeProbe retrying;
  retrying.first.identity = {20, 21, 22, 23, 24};
  retrying.second.identity = retrying.first.identity;
  retrying.second.tool.toolId = 4202;
  canvas::debug_ui::DebugSnapshotSources retryingSources{&retrying, &axiom, nullptr, &platform, &telemetry, &activity};
  const auto retryingSnapshot = assembler.capture(retryingSources);
  assert(retryingSnapshot.coherence == canvas::debug_ui::SnapshotCoherence::kCoherent);
  assert(retryingSnapshot.product.value.tool.toolId == 4202);
  assert(retrying.reads == 2);

  platform.value.surfaceAvailable = false;
  const auto degradedPlatform = assembler.capture(
      canvas::debug_ui::DebugSnapshotSources{&runtime, &axiom, nullptr, &platform, &telemetry, &activity});
  assert(degradedPlatform.platform.availability == canvas::debug_ui::DebugAvailability::kDegraded);
  assert(degradedPlatform.product.availability == canvas::debug_ui::DebugAvailability::kAvailable);
  assert(degradedPlatform.axiom.availability == canvas::debug_ui::DebugAvailability::kAvailable);
  assert(degradedPlatform.telemetry.availability == canvas::debug_ui::DebugAvailability::kAvailable);

  platform.value.surfaceAvailable = true;
  activity.value.productControl = canvas::runtime::ProductControlReceipt{77,
      canvas::runtime::ProductControlState::kApplied, 7};
  activity.value.surfaceControl = canvas::runtime::SurfaceModeReceipt{88,
      canvas::runtime::SurfaceControlState::kApplied,
      canvas::runtime::SurfaceRole::kCanonicalCanvas,
      canvas::runtime::SurfaceMode::kGpuDefault, 12};
  const auto activitySnapshot = assembler.capture(
      canvas::debug_ui::DebugSnapshotSources{&runtime, &axiom, nullptr, &platform, &telemetry, &activity});
  assert(activitySnapshot.activity.productControl.has_value());
  assert(activitySnapshot.activity.productControl->requestId == 77);
  assert(activitySnapshot.activity.surfaceControl.has_value());
  assert(activitySnapshot.activity.surfaceControl->requestId == 88);
  assert(activitySnapshot.stamp.snapshotSequence == 4);

  const auto fallbackFrame = assembler.capture(
      canvas::debug_ui::DebugSnapshotSources{&runtime, &axiom, nullptr, &platform, nullptr, nullptr});
  assert(fallbackFrame.telemetry.availability == canvas::debug_ui::DebugAvailability::kUnsupported);
  assert(fallbackFrame.stamp.frameId == platform.value.presentCount);
  assert(fallbackFrame.stamp.snapshotSequence == 5);

  AlwaysChangingRuntime changing;
  canvas::debug_ui::DebugSnapshotSources changingSources{&changing, nullptr, nullptr, nullptr, nullptr, nullptr};
  const auto stale = assembler.capture(changingSources);
  assert(stale.coherence == canvas::debug_ui::SnapshotCoherence::kMixedGeneration);
  assert(changing.reads == 4);

  canvas::debug_ui::DebugSnapshotSources absent{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
  const auto absentSnapshot = assembler.capture(absent);
  assert(absentSnapshot.coherence == canvas::debug_ui::SnapshotCoherence::kStale);
  assert(absentSnapshot.product.availability == canvas::debug_ui::DebugAvailability::kUnsupported);
  return 0;
}
