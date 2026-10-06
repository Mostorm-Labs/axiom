#include "canvas/debug_ui/controller.hpp"
#include "canvas/debug_ui/telemetry.hpp"
#include "canvas/debug_ui/surface.hpp"
#include "canvas/debug_ui/panels.hpp"
#include "canvas/debug_ui/surface_debug_queue.hpp"
#include "canvas/runtime/diagnostics.hpp"
#include "canvas/runtime/debug_control.hpp"
#include "canvas/debug_ui/debug_command_queue.hpp"
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
  static_assert(kDefaultDebugCommandQueueCapacity == 256);
  static_assert(kDefaultDebugCommandReceiptCapacity == 1024);
  MutexCopySnapshotChannel channel;
  DebugSnapshot snapshot; snapshot.stamp = {4, 9}; snapshot.canonicalRevision = 12; channel.publish(snapshot);
  const auto stamp = channel.read().stamp; assert(stamp.generation == 4 && stamp.sequence == 9);
  BoundedCommandQueue queue(1, 4);
  auto accepted = queue.admit(DebugCommand{1, 2, 10, DebugCommandKind::kInvalidatePreview, {}});
  assert(accepted && accepted->state == ReceiptState::kAccepted);
  assert(!queue.admit(DebugCommand{2,2,10,DebugCommandKind::kInvalidatePreview,{}}));
  BoundedCommandQueue staleQueue(2, 4);
  assert(staleQueue.admit(DebugCommand{3, 1, 100, DebugCommandKind::kSetCadence, {}}));
  assert(!staleQueue.take(1, 2));
  assert(staleQueue.receipt(3)->state == ReceiptState::kStaleGeneration);
  assert(staleQueue.admit(DebugCommand{4, 2, 2, DebugCommandKind::kSetCadence, {}}));
  assert(!staleQueue.take(3, 2));
  assert(staleQueue.receipt(4)->state == ReceiptState::kExpired);
  BoundedCommandQueue noDeadlineQueue(2, 4);
  assert(noDeadlineQueue.admit(DebugCommand{5, 1, 0, DebugCommandKind::kSetCadence, {}}));
  const auto noDeadline = noDeadlineQueue.take(1000, 1);
  assert(noDeadline && noDeadline->id == 5);
  auto cmd = queue.take(1,2); assert(cmd && cmd->id == 1);
  auto completed = queue.complete(1, ReceiptState::kCompleted); assert(completed && completed->state == ReceiptState::kCompleted);
  BoundedCommandQueue evictionQueue(4, 1);
  assert(evictionQueue.admit(DebugCommand{8, 1, 10, DebugCommandKind::kSetCadence, {}}));
  assert(evictionQueue.take(1, 1));
  assert(evictionQueue.complete(8, ReceiptState::kCompleted));
  assert(evictionQueue.admit(DebugCommand{9, 1, 10, DebugCommandKind::kSetCadence, {}}));
  InputCaptureGate gate; DebugInputSequence seq{7, 1};
  assert(gate.begin(seq, DebugInputOwner::kDebug) == DebugInputOwner::kDebug);
  assert(gate.begin(seq, DebugInputOwner::kCanvas) == DebugInputOwner::kDebug);
  assert(gate.route(seq) == DebugInputOwner::kDebug); assert(gate.terminal(seq)); assert(!gate.route(seq));
  RollingTelemetry telemetry(2); telemetry.push({1,1,1,0,1.0}); telemetry.push({2,1,1,1,2.0}); telemetry.push({3,1,2,1,2.5}); assert(telemetry.size()==2);
  DebugSnapshot panelSnapshot; panelSnapshot.capabilities.fill(CapabilityState::kAvailable); assert(panelAvailable(panelSnapshot, PanelCapability::kArcPreview));
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
    canvas::runtime::RuntimeDiagnosticsSnapshot readDiagnostics() const noexcept override { return {}; }
    canvas::runtime::ProductControlReceipt submitProductControl(
        const canvas::runtime::ProductControlRequest& request) noexcept override {
      return {request.requestId, canvas::runtime::ProductControlState::kUnsupported, 1};
    }
  } runtime;
  static_assert(std::is_base_of_v<canvas::runtime::RuntimeDiagnostics,
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
  const auto unsupported = runtime.submitProductControl(
      {canvas::runtime::ProductControlAction::kSetCamera, 20, 1, 10, 0});
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
