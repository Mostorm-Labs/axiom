#pragma once

#include "canvas/debug_ui/activity_log.hpp"
#include "canvas/debug_ui/snapshot.hpp"
#include "canvas/runtime/debug_control.hpp"
#include "canvas/runtime/runtime_facade.hpp"
#include "canvas/runtime/surface_debug_control.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace canvas::debug_ui {

class DebugControlRouter final {
 public:
  DebugControlRouter(canvas::runtime::RuntimeFacade* runtime,
                     canvas::runtime::AxiomDebugControl* axiomDebug,
                     canvas::runtime::PlatformDebugControl* platformDebug,
                     DebugActivityLog* activity) noexcept;

  void beginFrame(const DebugSnapshot& snapshot) noexcept;
  void refreshReceipts() noexcept;

  canvas::runtime::ProductControlReceipt setTool(
      canvas::runtime::CanvasToolKind tool) noexcept;
  canvas::runtime::ProductControlReceipt setBrush(std::uint32_t brushId,
                                                   std::uint32_t revision) noexcept;
  canvas::runtime::ProductControlReceipt setEraser(std::uint32_t eraserId) noexcept;
  canvas::runtime::ProductControlReceipt setSelectionMode(bool enabled) noexcept;
  canvas::runtime::ProductControlReceipt undo() noexcept;
  canvas::runtime::ProductControlReceipt redo() noexcept;
  canvas::runtime::ProductControlReceipt panBy(float dx, float dy) noexcept;
  canvas::runtime::ProductControlReceipt zoomAt(float anchorX, float anchorY,
                                                 float scaleDelta) noexcept;
  canvas::runtime::ProductControlReceipt fitToContent() noexcept;
  canvas::runtime::ProductControlReceipt fitToSelection() noexcept;
  canvas::runtime::ProductControlReceipt fitPrimaryObject() noexcept;
  canvas::runtime::AxiomDebugCommandReceipt submitAxiom(
      canvas::runtime::AxiomDebugCommandKind kind, std::uint64_t value = 0) noexcept;
  canvas::runtime::SurfaceModeReceipt setCanonicalSurfaceMode(
      canvas::runtime::SurfaceMode mode) noexcept;

 private:
  struct FrameContext final {
    DebugSnapshotStamp stamp{};
    std::uint32_t selectedObjectCount = 0;
    std::uint64_t primaryObject = 0;
  };
  struct PendingAxiom final { std::string action; };
  struct PendingSurface final { std::string action; };

  [[nodiscard]] std::uint64_t nextRequestId() noexcept;
  [[nodiscard]] std::uint64_t deadline() const noexcept { return frame_.stamp.sequence + 120U; }
  void recordProduct(const std::string& action,
                     const canvas::runtime::ProductControlReceipt& receipt) noexcept;
  void recordAxiom(const std::string& action,
                   const canvas::runtime::AxiomDebugCommandReceipt& receipt) noexcept;
  void recordSurface(const std::string& action,
                     const canvas::runtime::SurfaceModeReceipt& receipt) noexcept;
  [[nodiscard]] static DebugActivityState mapProduct(canvas::runtime::ProductControlState state) noexcept;
  [[nodiscard]] static DebugActivityState mapCanvas(canvas::runtime::CanvasControlReceiptState state) noexcept;
  [[nodiscard]] static DebugActivityState mapAxiom(canvas::runtime::AxiomDebugCommandState state) noexcept;
  [[nodiscard]] static DebugActivityState mapSurface(canvas::runtime::SurfaceControlState state) noexcept;
  [[nodiscard]] canvas::runtime::ProductControlReceipt localRejected(
      std::uint64_t requestId) const noexcept;

  canvas::runtime::RuntimeFacade* runtime_ = nullptr;
  canvas::runtime::AxiomDebugControl* axiomDebug_ = nullptr;
  canvas::runtime::PlatformDebugControl* platformDebug_ = nullptr;
  DebugActivityLog* activity_ = nullptr;
  FrameContext frame_{};
  std::uint64_t nextRequest_ = 1;
  std::unordered_map<std::uint64_t, PendingAxiom> pendingAxiom_;
  std::unordered_map<std::uint64_t, PendingSurface> pendingSurface_;
};

}  // namespace canvas::debug_ui
