#include "canvas/debug_ui/controller.hpp"
#include "canvas/debug_ui/telemetry.hpp"
#include "canvas/debug_ui/surface.hpp"
#include "canvas/debug_ui/panels.hpp"
#include <cassert>
using namespace canvas::debug_ui;
int main() {
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
  assert(staleQueue.receipt(3)->state == ReceiptState::kDestroyed);
  assert(staleQueue.admit(DebugCommand{4, 2, 2, DebugCommandKind::kSetCadence, {}}));
  assert(!staleQueue.take(3, 2));
  assert(staleQueue.receipt(4)->state == ReceiptState::kExpired);
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
  PlatformDebugControl surfaces; auto receipt=surfaces.requestSurfaceMode(1,1,SurfaceMode::kCpuReference); assert(receipt.accepted && receipt.generation==2); auto stale=surfaces.requestSurfaceMode(2,1,SurfaceMode::kGpuDefault); assert(!stale.accepted && stale.generation==2);
  return 0;
}
