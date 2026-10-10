#include "canvas/debug_ui/controller.hpp"
#include "canvas/debug_ui/telemetry.hpp"
#include "canvas/debug_ui/surface.hpp"
#include "canvas/debug_ui/panels.hpp"
#include "canvas/debug_ui/surface_debug_queue.hpp"
#include "canvas/debug_ui/debug_command_queue.hpp"
#include "canvas/runtime/diagnostics.hpp"
#include "canvas/runtime/debug_control.hpp"
#include "canvas/runtime/telemetry.hpp"
#include <cassert>
#include <type_traits>
class TestPlatformDebugControl final : public canvas::runtime::PlatformDebugControl {
 public:
  canvas::runtime::SurfaceModeReceipt requestSurfaceMode(
      const canvas::runtime::SurfaceModeRequest& request) noexcept override {
    canvas::runtime::SurfaceModeReceipt receipt{request.requestId,
      canvas::runtime::SurfaceControlState::kFailed, request.target, request.mode, generation_};
    if (request.target != canvas::runtime::SurfaceRole::kCanonicalCanvas) {
      receipt.state = canvas::runtime::SurfaceControlState::kUnsupported;
      return receipt;
    }
    if (request.expectedGeneration != generation_) {
      receipt.state = canvas::runtime::SurfaceControlState::kStaleGeneration;
      return receipt;
    }
    ++generation_;
    receipt.generation = generation_;
    receipt.state = canvas::runtime::SurfaceControlState::kApplied;
    return receipt;
  }
 private:
  std::uint64_t generation_ = 1;
};
using namespace canvas::debug_ui;
int main() {
  static_assert(canvas::runtime::kDefaultSurfaceModeQueueCapacity == 256);
  static_assert(canvas::runtime::kDefaultSurfaceModeReceiptCapacity == 1024);
  MutexCopySnapshotChannel channel;
  DebugSnapshot snapshot; snapshot.stamp.generation = 4; snapshot.stamp.sequence = 9;
  snapshot.axiom.value.document.canonicalOperationCount = 12; channel.publish(snapshot);
  const auto stamp = channel.read().stamp; assert(stamp.generation == 4 && stamp.sequence == 9);
  InputCaptureGate gate; DebugInputSequence seq{7, 1};
  assert(gate.begin(seq, DebugInputOwner::kDebug) == DebugInputOwner::kDebug);
  assert(gate.begin(seq, DebugInputOwner::kCanvas) == DebugInputOwner::kDebug);
  assert(gate.route(seq) == DebugInputOwner::kDebug); assert(gate.terminal(seq)); assert(!gate.route(seq));
  RollingTelemetry telemetry(2); telemetry.push({1,1,1,0,1.0}); telemetry.push({2,1,1,1,2.0}); telemetry.push({3,1,2,1,2.5}); assert(telemetry.size()==2);
  DebugSnapshot panelSnapshot; panelSnapshot.arc.availability = DebugAvailability::kAvailable;
  assert(panelAvailable(panelSnapshot, PanelCapability::kArcPreview));
  BoundedTraceSession trace(4); assert(trace.start()); assert(trace.append(4)); assert(!trace.append(1)); assert(trace.state()==TraceState::kOverflow);
  TestPlatformDebugControl surfaces;
  const auto receipt = surfaces.requestSurfaceMode({1, SurfaceRole::kCanonicalCanvas,
                                                    SurfaceMode::kCpuReference, 1});
  assert(receipt.state == SurfaceControlState::kApplied && receipt.generation == 2);
  const auto stale = surfaces.requestSurfaceMode({2, SurfaceRole::kCanonicalCanvas,
                                                  SurfaceMode::kGpuDefault, 1});
  assert(stale.state == SurfaceControlState::kStaleGeneration && stale.generation == 2);

  canvas::runtime::BoundedSurfaceModeQueue modeQueue(1);
  const auto queued = modeQueue.enqueue(
      {10, SurfaceRole::kCanonicalCanvas, SurfaceMode::kGpuDefault, 2}, 2);
  assert(queued.state == SurfaceControlState::kQueued);
  const auto full = modeQueue.enqueue(
      {11, SurfaceRole::kCanonicalCanvas, SurfaceMode::kPlatformDefault, 2}, 2);
  assert(full.state == SurfaceControlState::kQueueFull);
  const auto pending = modeQueue.take(2);
  assert(pending && pending->requestId == 10);
  const auto applied = modeQueue.complete(10, SurfaceControlState::kApplied, 3);
  assert(applied.generation == 3);
  const auto staleRequest = modeQueue.enqueue(
      {12, SurfaceRole::kCanonicalCanvas, SurfaceMode::kGpuDefault, 2}, 3);
  assert(staleRequest.state == SurfaceControlState::kStaleGeneration);
  assert(modeQueue.destroyGeneration(2) == 0);
  const auto expiring = modeQueue.enqueue(
      {13, SurfaceRole::kCanonicalCanvas, SurfaceMode::kGpuDefault, 3, 5}, 3, 4);
  assert(expiring.state == SurfaceControlState::kQueued);
  assert(modeQueue.expire(6) == 1);
  assert(modeQueue.receipt(13)->state == SurfaceControlState::kExpired);
  canvas::runtime::BoundedSurfaceModeQueue receiptQueue(4, 1);
  assert(receiptQueue.enqueue({21, SurfaceRole::kCanonicalCanvas,
                               SurfaceMode::kGpuDefault, 3}, 3).state ==
         SurfaceControlState::kQueued);
  assert(receiptQueue.complete(21, SurfaceControlState::kApplied, 4).state ==
         SurfaceControlState::kApplied);
  assert(receiptQueue.enqueue({22, SurfaceRole::kCanonicalCanvas,
                               SurfaceMode::kGpuDefault, 4}, 4).state ==
         SurfaceControlState::kQueued);
  assert(!receiptQueue.receipt(21));

  struct RuntimeProbe final : canvas::runtime::RuntimeFacade {
    canvas::runtime::RuntimeStateSnapshot readRuntimeState() const noexcept override { return {}; }
    canvas::runtime::ProductControlReceipt submitProductControl(
        const canvas::runtime::ProductControlRequest& request) noexcept override {
      return {request.requestId, canvas::runtime::ProductControlState::kUnsupported, 1, std::nullopt};
    }
  } runtime;
  static_assert(!std::is_base_of_v<canvas::runtime::RuntimeDiagnostics,
                                   canvas::runtime::RuntimeFacade>);
  static_assert(std::is_base_of_v<canvas::runtime::DiagnosticsProvider,
                                  canvas::runtime::PlatformDiagnostics>);
  static_assert(std::is_same_v<canvas::runtime::IAxiomDiagnostics,
                               canvas::runtime::RuntimeDiagnostics>);
  static_assert(std::is_base_of_v<canvas::runtime::DiagnosticsProvider,
                                  canvas::runtime::ArcDiagnostics>);
  static_assert(std::is_base_of_v<canvas::runtime::TelemetryProvider,
                                  canvas::runtime::Telemetry>);
  struct DebugProbe final : canvas::runtime::AxiomDebugControl {
    canvas::runtime::AxiomDebugCommandReceipt enqueue(
        const canvas::runtime::AxiomDebugCommand& command) noexcept override {
      return {command.requestId, canvas::runtime::AxiomDebugCommandState::kUnsupported};
    }
    canvas::runtime::AxiomDebugCommandReceipt receipt(
        std::uint64_t requestId) const noexcept override {
      return {requestId, canvas::runtime::AxiomDebugCommandState::kUnsupported};
    }
  } debug;
  assert(debug.enqueue({1, canvas::runtime::AxiomDebugCommandKind::kForceFullRedraw})
             .state == canvas::runtime::AxiomDebugCommandState::kUnsupported);
  canvas::runtime::BoundedAxiomDebugCommandQueue debugQueue(1, 2);
  const auto debugQueued = debugQueue.enqueue(
      {30, canvas::runtime::AxiomDebugCommandKind::kForceFullRedraw, 1, 4, 10},
      1, 4, 2);
  assert(debugQueued.state == canvas::runtime::AxiomDebugCommandState::kQueued);
  const auto debugFull = debugQueue.enqueue(
      {31, canvas::runtime::AxiomDebugCommandKind::kForceFullRedraw, 1, 4, 10},
      1, 4, 2);
  assert(debugFull.state == canvas::runtime::AxiomDebugCommandState::kQueueFull);
  const auto debugStale = debugQueue.enqueue(
      {32, canvas::runtime::AxiomDebugCommandKind::kForceFullRedraw, 2, 4, 10},
      1, 4, 2);
  assert(debugStale.state == canvas::runtime::AxiomDebugCommandState::kStaleGeneration);
  const auto debugCommand = debugQueue.take(1, 4, 3);
  assert(debugCommand && debugCommand->requestId == 30);
  assert(debugQueue.complete(30, canvas::runtime::AxiomDebugCommandState::kApplied, 9,
                             1, 4).state == canvas::runtime::AxiomDebugCommandState::kApplied);
  assert(debugQueue.receipt(30)->state == canvas::runtime::AxiomDebugCommandState::kApplied);
  canvas::runtime::ProductControlRequest unsupportedRequest{};
  unsupportedRequest.action = canvas::runtime::ProductControlAction::kSetCamera;
  unsupportedRequest.requestId = 20;
  unsupportedRequest.runtimeGeneration = 1;
  unsupportedRequest.deadlineSequence = 10;
  const auto unsupported = runtime.submitProductControl(unsupportedRequest);
  assert(unsupported.state == canvas::runtime::ProductControlState::kUnsupported);
  canvas::runtime::ProductControlRequest brushRequest{};
  brushRequest.action = canvas::runtime::ProductControlAction::kSetBrush;
  brushRequest.requestId = 21;
  brushRequest.runtimeGeneration = 1;
  brushRequest.brushId = 3;
  brushRequest.brushRevision = 1;
  assert(runtime.submitProductControl(brushRequest).state ==
         canvas::runtime::ProductControlState::kUnsupported);
  return 0;
}
