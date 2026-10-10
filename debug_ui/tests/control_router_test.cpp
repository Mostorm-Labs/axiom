#include "canvas/debug_ui/activity_log.hpp"
#include "canvas/debug_ui/control_router.hpp"
#include "canvas/debug_ui/controller.hpp"
#include "canvas/runtime/surface_debug_control.hpp"

#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace {
using namespace canvas;

struct RuntimeProbe final : runtime::RuntimeFacade {
  int productCalls = 0;
  int canvasCalls = 0;
  runtime::RuntimeStateSnapshot state{};
  runtime::RuntimeStateSnapshot readRuntimeState() const noexcept override { return state; }
  runtime::ProductControlReceipt submitProductControl(
      const runtime::ProductControlRequest& request) noexcept override {
    ++productCalls;
    return {request.requestId, runtime::ProductControlState::kApplied,
            request.runtimeGeneration, std::nullopt};
  }
  runtime::ProductControlReceipt submitCanvasControl(
      const runtime::CanvasControlRequest& request) noexcept override {
    ++canvasCalls;
    runtime::CanvasControlReceipt control{};
    control.key = {request.clientId, request.requestId};
    control.state = runtime::CanvasControlReceiptState::kApplied;
    return {request.requestId, runtime::ProductControlState::kApplied,
            request.target.runtimeEpoch, control};
  }
  std::vector<runtime::CanvasTargetKey> mountedCanvasTargets() const override {
    return {{7, 3, 9, 12}};
  }
};

struct AxiomProbe final : runtime::AxiomDebugControl {
  int enqueueCalls = 0;
  bool applied = false;
  std::unordered_map<std::uint64_t, runtime::AxiomDebugCommandReceipt> receipts;
  runtime::AxiomDebugCommandReceipt enqueue(
      const runtime::AxiomDebugCommand& command) noexcept override {
    ++enqueueCalls;
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
    if (applied) result.state = runtime::AxiomDebugCommandState::kApplied;
    return result;
  }
};

struct PlatformProbe final : runtime::PlatformDebugControl {
  int enqueueCalls = 0;
  bool applied = false;
  std::unordered_map<std::uint64_t, runtime::SurfaceModeReceipt> receipts;
  runtime::SurfaceModeReceipt enqueueSurfaceMode(
      const runtime::SurfaceModeRequest& request) noexcept override {
    ++enqueueCalls;
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
    if (applied) result.state = runtime::SurfaceControlState::kApplied;
    return result;
  }
};
}

int main() {
  canvas::debug_ui::DebugActivityLog log(2);
  log.record({0, 1, canvas::debug_ui::DebugControlOwner::kProduct, "one",
              canvas::debug_ui::DebugActivityState::kApplied, 0, 0, 0, 0,
              std::nullopt, std::nullopt});
  log.record({0, 2, canvas::debug_ui::DebugControlOwner::kProduct, "two",
              canvas::debug_ui::DebugActivityState::kRejected, 0, 0, 0, 0,
              std::nullopt, std::nullopt});
  log.record({0, 3, canvas::debug_ui::DebugControlOwner::kProduct, "three",
              canvas::debug_ui::DebugActivityState::kPending, 0, 0, 0, 0,
              std::nullopt, std::nullopt});
  assert(log.size() == 2);
  const auto bounded = log.snapshot();
  assert(bounded.entries.size() == 2 && bounded.entries.front().requestId == 2 &&
         bounded.entries.back().requestId == 3);

  RuntimeProbe runtime;
  AxiomProbe axiom;
  PlatformProbe platform;
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
  assert(brush.state == canvas::runtime::ProductControlState::kApplied);
  assert(runtime.productCalls == 1 && runtime.canvasCalls == 0);
  const auto pan = router.setTool(4107);
  assert(pan.state == canvas::runtime::ProductControlState::kApplied);
  assert(runtime.canvasCalls == 1);
  const auto axiomReceipt = router.submitAxiom(
      canvas::runtime::AxiomDebugCommandKind::kForceFullRedraw);
  assert(axiomReceipt.state == canvas::runtime::AxiomDebugCommandState::kQueued);
  const auto surfaceReceipt = router.setCanonicalSurfaceMode(
      canvas::runtime::SurfaceMode::kGpuDefault);
  assert(surfaceReceipt.state == canvas::runtime::SurfaceControlState::kQueued);
  assert(axiomReceipt.requestId != 0 && surfaceReceipt.requestId != 0);
  axiom.applied = true;
  platform.applied = true;
  router.refreshReceipts();
  const auto afterRefresh = log.snapshot();
  assert(afterRefresh.entries.back().state == canvas::debug_ui::DebugActivityState::kApplied);

  const auto beforeFit = runtime.productCalls;
  const auto rejectedSelection = router.fitToSelection();
  assert(rejectedSelection.state == canvas::runtime::ProductControlState::kRejected);
  assert(runtime.productCalls == beforeFit);
  snapshot.product.value.selection.selectedObjectCount = 1;
  snapshot.product.value.selection.primaryObject = 42;
  router.beginFrame(snapshot);
  assert(router.fitToSelection().state == canvas::runtime::ProductControlState::kApplied);
  assert(router.fitPrimaryObject().state == canvas::runtime::ProductControlState::kApplied);
  return 0;
}
