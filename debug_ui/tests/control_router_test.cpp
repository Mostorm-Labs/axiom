#include "canvas/debug_ui/activity_log.hpp"
#include "canvas/debug_ui/control_router.hpp"
#include "canvas/runtime/debug_control.hpp"
#include "canvas/runtime/surface_debug_control.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace {
using namespace canvas;

struct RuntimeProbe final : runtime::RuntimeFacade {
  int productCalls = 0;
  int canvasCalls = 0;
  runtime::ProductControlState productState = runtime::ProductControlState::kApplied;
  runtime::CanvasControlReceiptState canvasState = runtime::CanvasControlReceiptState::kApplied;
  runtime::RuntimeStateSnapshot state{};
  runtime::ProductControlRequest lastProduct{};
  runtime::CanvasControlRequest lastCanvas{};

  runtime::RuntimeStateSnapshot readRuntimeState() const noexcept override { return state; }
  runtime::ProductControlReceipt submitProductControl(
      const runtime::ProductControlRequest& request) noexcept override {
    ++productCalls;
    lastProduct = request;
    return {request.requestId, productState, request.runtimeGeneration, std::nullopt};
  }
  runtime::ProductControlReceipt submitCanvasControl(
      const runtime::CanvasControlRequest& request) noexcept override {
    ++canvasCalls;
    lastCanvas = request;
    runtime::CanvasControlReceipt control{};
    control.key = {request.clientId, request.requestId};
    control.state = canvasState;
    return {request.requestId,
            canvasState == runtime::CanvasControlReceiptState::kApplied
                ? runtime::ProductControlState::kApplied
                : runtime::ProductControlState::kQueued,
            request.target.runtimeEpoch, control};
  }
  std::vector<runtime::CanvasTargetKey> mountedCanvasTargets() const override {
    return {{7, 3, 9, 12}};
  }
};

struct AxiomProbe final : runtime::AxiomDebugControl {
  int enqueueCalls = 0;
  runtime::AxiomDebugCommandState terminalState = runtime::AxiomDebugCommandState::kApplied;
  std::unordered_map<std::uint64_t, runtime::AxiomDebugCommandReceipt> receipts;
  runtime::AxiomDebugCommand lastCommand{};

  runtime::AxiomDebugCommandReceipt enqueue(
      const runtime::AxiomDebugCommand& command) noexcept override {
    ++enqueueCalls;
    lastCommand = command;
    const runtime::AxiomDebugCommandReceipt receipt{
        command.requestId, runtime::AxiomDebugCommandState::kQueued, 0,
        command.expectedRuntimeGeneration, command.expectedDocumentGeneration};
    receipts[command.requestId] = receipt;
    return receipt;
  }
  runtime::AxiomDebugCommandReceipt receipt(std::uint64_t id) const noexcept override {
    auto found = receipts.find(id);
    if (found == receipts.end()) return {id, runtime::AxiomDebugCommandState::kUnsupported};
    auto result = found->second;
    if (result.state == runtime::AxiomDebugCommandState::kQueued) result.state = terminalState;
    return result;
  }
};

struct PlatformProbe final : runtime::PlatformDebugControl {
  int enqueueCalls = 0;
  runtime::SurfaceControlState terminalState = runtime::SurfaceControlState::kApplied;
  std::unordered_map<std::uint64_t, runtime::SurfaceModeReceipt> receipts;
  runtime::SurfaceModeRequest lastRequest{};

  runtime::SurfaceModeReceipt enqueueSurfaceMode(
      const runtime::SurfaceModeRequest& request) noexcept override {
    ++enqueueCalls;
    lastRequest = request;
    const runtime::SurfaceModeReceipt receipt{
        request.requestId, runtime::SurfaceControlState::kQueued,
        request.target, request.mode, request.expectedGeneration};
    receipts[request.requestId] = receipt;
    return receipt;
  }
  runtime::SurfaceModeReceipt requestSurfaceMode(
      const runtime::SurfaceModeRequest& request) noexcept override {
    return {request.requestId, runtime::SurfaceControlState::kApplied,
            request.target, request.mode, request.expectedGeneration};
  }
  runtime::SurfaceModeReceipt receipt(std::uint64_t id) const noexcept override {
    auto found = receipts.find(id);
    if (found == receipts.end()) return {id, runtime::SurfaceControlState::kUnsupported};
    auto result = found->second;
    if (result.state == runtime::SurfaceControlState::kQueued) result.state = terminalState;
    return result;
  }
};

struct MinimalPlatform final : runtime::PlatformDebugControl {
  runtime::SurfaceModeReceipt requestSurfaceMode(
      const runtime::SurfaceModeRequest& request) noexcept override {
    return {request.requestId, runtime::SurfaceControlState::kApplied,
            request.target, request.mode, request.expectedGeneration};
  }
};

canvas::debug_ui::DebugActivityState lastState(
    const canvas::debug_ui::DebugActivityLog& log) {
  const auto entries = log.snapshot().entries;
  assert(!entries.empty());
  return entries.back().state;
}

void checkProduct(canvas::debug_ui::DebugControlRouter& router,
                  RuntimeProbe& runtime,
                  const canvas::debug_ui::DebugActivityLog& log,
                  runtime::ProductControlState state,
                  canvas::debug_ui::DebugActivityState expected) {
  runtime.productState = state;
  (void)router.setBrush(2, 3);
  assert(lastState(log) == expected);
}

void checkCanvas(canvas::debug_ui::DebugControlRouter& router,
                 RuntimeProbe& runtime,
                 const canvas::debug_ui::DebugActivityLog& log,
                 runtime::CanvasControlReceiptState state,
                 canvas::debug_ui::DebugActivityState expected) {
  runtime.canvasState = state;
  (void)router.setTool(runtime::CanvasToolKind::kPan);
  assert(lastState(log) == expected);
}

void checkAxiom(canvas::debug_ui::DebugControlRouter& router,
                AxiomProbe& axiom,
                const canvas::debug_ui::DebugActivityLog& log,
                runtime::AxiomDebugCommandState state,
                canvas::debug_ui::DebugActivityState expected) {
  axiom.terminalState = state;
  const auto receipt = router.submitAxiom(runtime::AxiomDebugCommandKind::kForceFullRedraw);
  assert(receipt.state == runtime::AxiomDebugCommandState::kQueued);
  router.refreshReceipts();
  assert(lastState(log) == expected);
}

void checkSurface(canvas::debug_ui::DebugControlRouter& router,
                  PlatformProbe& platform,
                  const canvas::debug_ui::DebugActivityLog& log,
                  runtime::SurfaceControlState state,
                  canvas::debug_ui::DebugActivityState expected) {
  platform.terminalState = state;
  const auto receipt = router.setCanonicalSurfaceMode(runtime::SurfaceMode::kGpuDefault);
  assert(receipt.state == runtime::SurfaceControlState::kQueued);
  router.refreshReceipts();
  assert(lastState(log) == expected);
}
}  // namespace

int main() {
  canvas::debug_ui::DebugActivityLog bounded(2);
  bounded.record({0, 1, canvas::debug_ui::DebugControlOwner::kProduct, "one",
                  canvas::debug_ui::DebugActivityState::kApplied, 0, 0, 0, 0,
                  std::nullopt, std::nullopt});
  bounded.record({0, 2, canvas::debug_ui::DebugControlOwner::kProduct, "two",
                  canvas::debug_ui::DebugActivityState::kRejected, 0, 0, 0, 0,
                  std::nullopt, std::nullopt});
  bounded.record({0, 3, canvas::debug_ui::DebugControlOwner::kProduct, "three",
                  canvas::debug_ui::DebugActivityState::kPending, 0, 0, 0, 0,
                  std::nullopt, std::nullopt});
  assert(bounded.size() == 2);
  const auto boundedSnapshot = bounded.snapshot();
  const auto boundedRead = bounded.readActivity();
  assert(boundedSnapshot.entries.size() == 2 && boundedRead.entries.size() == 2);
  assert(boundedSnapshot.entries.front().requestId == 2 &&
         boundedSnapshot.entries.back().requestId == 3);
  assert(boundedRead.entries.front().requestId == 2 &&
         boundedRead.entries.back().requestId == 3);

  RuntimeProbe runtime;
  AxiomProbe axiom;
  PlatformProbe platform;
  canvas::debug_ui::DebugActivityLog log;
  canvas::debug_ui::DebugControlRouter router(&runtime, &axiom, &platform, &log);
  canvas::debug_ui::DebugSnapshot snapshot{};
  snapshot.stamp.sequence = 50;
  snapshot.stamp.frameId = 8;
  snapshot.stamp.runtimeGeneration = 7;
  snapshot.stamp.documentGeneration = 11;
  snapshot.stamp.surfaceGeneration = 13;
  snapshot.platform.availability = canvas::debug_ui::DebugAvailability::kAvailable;
  snapshot.platform.value.canonicalSurfaceGeneration = 17;
  router.beginFrame(snapshot);

  const auto brush = router.setBrush(2, 3);
  assert(brush.state == runtime::ProductControlState::kApplied);
  assert(runtime.productCalls == 1 && runtime.canvasCalls == 0);
  (void)router.setEraser(2);
  (void)router.setSelectionMode(true);
  (void)router.undo();
  (void)router.redo();
  (void)router.panBy(2.0F, 3.0F);
  (void)router.zoomAt(4.0F, 5.0F, 1.2F);
  (void)router.fitToContent();
  assert(runtime.productCalls == 8 && runtime.canvasCalls == 0);

  const auto pan = router.setTool(runtime::CanvasToolKind::kPan);
  assert(pan.state == runtime::ProductControlState::kApplied);
  assert(runtime.canvasCalls == 1);
  assert(runtime.lastCanvas.payload.tool == runtime::CanvasToolKind::kPan);
  assert(runtime.lastCanvas.deadlineSequence == 170);

  const auto axiomQueued = router.submitAxiom(
      runtime::AxiomDebugCommandKind::kForceFullRedraw);
  const auto surfaceQueued = router.setCanonicalSurfaceMode(runtime::SurfaceMode::kGpuDefault);
  assert(axiomQueued.state == runtime::AxiomDebugCommandState::kQueued);
  assert(surfaceQueued.state == runtime::SurfaceControlState::kQueued);
  assert(axiomQueued.requestId != 0 && surfaceQueued.requestId > axiomQueued.requestId);
  assert(axiom.lastCommand.expectedRuntimeGeneration == 7);
  assert(axiom.lastCommand.expectedDocumentGeneration == 11);
  assert(axiom.lastCommand.deadlineSequence == 170);
  assert(platform.lastRequest.expectedGeneration == 17);
  assert(platform.lastRequest.deadlineSequence == 170);

  const auto beforeQueuedRefresh = log.size();
  axiom.terminalState = runtime::AxiomDebugCommandState::kQueued;
  platform.terminalState = runtime::SurfaceControlState::kQueued;
  router.refreshReceipts();
  assert(log.size() == beforeQueuedRefresh);
  axiom.terminalState = runtime::AxiomDebugCommandState::kApplied;
  platform.terminalState = runtime::SurfaceControlState::kApplied;
  router.refreshReceipts();
  const auto afterTerminalRefresh = log.size();
  assert(lastState(log) == canvas::debug_ui::DebugActivityState::kApplied);
  router.refreshReceipts();
  assert(log.size() == afterTerminalRefresh);

  checkProduct(router, runtime, log, runtime::ProductControlState::kApplied,
               canvas::debug_ui::DebugActivityState::kApplied);
  checkProduct(router, runtime, log, runtime::ProductControlState::kQueued,
               canvas::debug_ui::DebugActivityState::kPending);
  checkProduct(router, runtime, log, runtime::ProductControlState::kUnsupported,
               canvas::debug_ui::DebugActivityState::kUnsupported);
  checkProduct(router, runtime, log, runtime::ProductControlState::kRejected,
               canvas::debug_ui::DebugActivityState::kRejected);
  checkProduct(router, runtime, log, runtime::ProductControlState::kFailed,
               canvas::debug_ui::DebugActivityState::kFailed);

  const std::array<std::pair<runtime::CanvasControlReceiptState,
                             canvas::debug_ui::DebugActivityState>, 8> canvasStates{{
      {runtime::CanvasControlReceiptState::kExpired, canvas::debug_ui::DebugActivityState::kExpired},
      {runtime::CanvasControlReceiptState::kStaleTarget, canvas::debug_ui::DebugActivityState::kStaleGeneration},
      {runtime::CanvasControlReceiptState::kTargetDestroyed, canvas::debug_ui::DebugActivityState::kUnavailable},
      {runtime::CanvasControlReceiptState::kQueueFull, canvas::debug_ui::DebugActivityState::kQueueFull},
      {runtime::CanvasControlReceiptState::kBusy, canvas::debug_ui::DebugActivityState::kUnavailable},
      {runtime::CanvasControlReceiptState::kUnsupported, canvas::debug_ui::DebugActivityState::kUnsupported},
      {runtime::CanvasControlReceiptState::kRejected, canvas::debug_ui::DebugActivityState::kRejected},
      {runtime::CanvasControlReceiptState::kFailed, canvas::debug_ui::DebugActivityState::kFailed},
  }};
  for (const auto [state, expected] : canvasStates) checkCanvas(router, runtime, log, state, expected);

  const std::array<std::pair<runtime::AxiomDebugCommandState,
                             canvas::debug_ui::DebugActivityState>, 7> axiomStates{{
      {runtime::AxiomDebugCommandState::kRejected, canvas::debug_ui::DebugActivityState::kRejected},
      {runtime::AxiomDebugCommandState::kStaleGeneration, canvas::debug_ui::DebugActivityState::kStaleGeneration},
      {runtime::AxiomDebugCommandState::kExpired, canvas::debug_ui::DebugActivityState::kExpired},
      {runtime::AxiomDebugCommandState::kDestroyed, canvas::debug_ui::DebugActivityState::kUnavailable},
      {runtime::AxiomDebugCommandState::kQueueFull, canvas::debug_ui::DebugActivityState::kQueueFull},
      {runtime::AxiomDebugCommandState::kUnsupported, canvas::debug_ui::DebugActivityState::kUnsupported},
      {runtime::AxiomDebugCommandState::kFailed, canvas::debug_ui::DebugActivityState::kFailed},
  }};
  for (const auto [state, expected] : axiomStates) checkAxiom(router, axiom, log, state, expected);

  const std::array<std::pair<runtime::SurfaceControlState,
                             canvas::debug_ui::DebugActivityState>, 6> surfaceStates{{
      {runtime::SurfaceControlState::kStaleGeneration, canvas::debug_ui::DebugActivityState::kStaleGeneration},
      {runtime::SurfaceControlState::kExpired, canvas::debug_ui::DebugActivityState::kExpired},
      {runtime::SurfaceControlState::kQueueFull, canvas::debug_ui::DebugActivityState::kQueueFull},
      {runtime::SurfaceControlState::kUnavailable, canvas::debug_ui::DebugActivityState::kUnavailable},
      {runtime::SurfaceControlState::kUnsupported, canvas::debug_ui::DebugActivityState::kUnsupported},
      {runtime::SurfaceControlState::kFailed, canvas::debug_ui::DebugActivityState::kFailed},
  }};
  for (const auto [state, expected] : surfaceStates) checkSurface(router, platform, log, state, expected);

  const auto beforeFit = runtime.productCalls;
  snapshot.product.value.selection.selectedObjectCount = 0;
  snapshot.product.value.selection.primaryObject = 0;
  router.beginFrame(snapshot);
  assert(router.fitToSelection().state == runtime::ProductControlState::kRejected);
  assert(router.fitPrimaryObject().state == runtime::ProductControlState::kRejected);
  assert(runtime.productCalls == beforeFit);
  snapshot.product.value.selection.selectedObjectCount = 1;
  snapshot.product.value.selection.primaryObject = 42;
  runtime.productState = runtime::ProductControlState::kApplied;
  router.beginFrame(snapshot);
  assert(router.fitToSelection().state == runtime::ProductControlState::kApplied);
  assert(router.fitPrimaryObject().state == runtime::ProductControlState::kApplied);

  MinimalPlatform minimal;
  const auto defaultReceipt = minimal.receipt(99);
  assert(defaultReceipt.requestId == 99 &&
         defaultReceipt.state == runtime::SurfaceControlState::kUnsupported);
  return 0;
}
