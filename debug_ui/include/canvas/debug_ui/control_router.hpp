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
#include <functional>

namespace canvas::debug_ui {

struct ImageFileResult final {
  std::vector<std::uint8_t> bytes;
  std::string mimeType;
  std::string error;
  bool cancelled = false;
};

class DebugControlRouter final {
 public:
  DebugControlRouter(canvas::runtime::RuntimeFacade* runtime,
                     canvas::runtime::AxiomDebugControl* axiomDebug,
                     canvas::runtime::PlatformDebugControl* platformDebug,
                     DebugActivityLog* activity) noexcept;

  void beginFrame(const DebugSnapshot& snapshot) noexcept;
  void refreshReceipts() noexcept;
  // UI consumer policy only: one attempt per bound Runtime, before the UI opens.
  void prepareConnectorDefaults(std::uint64_t runtimeEpoch, bool retry = false);
  [[nodiscard]] bool connectorDefaultsReady() const noexcept { return connectorDefaultsReady_; }
  [[nodiscard]] canvas::runtime::CanvasToolKind canvasTool() const;

  canvas::runtime::ProductControlReceipt setTool(
      canvas::runtime::CanvasToolKind tool) noexcept;
  canvas::runtime::ProductControlReceipt setBrush(std::uint32_t brushId,
                                                   std::uint32_t revision) noexcept;
  canvas::runtime::ProductControlReceipt setEraser(std::uint32_t eraserId) noexcept;
  canvas::runtime::TextEditorSnapshot textEditor() const;
  canvas::runtime::TextReceipt setTextWidth(double);
  canvas::runtime::TextReceipt setTextFormat(const canvas::runtime::TextFormatPatch&);
  void requestImageFile(bool replace);
  void processImageFileRequest(const std::function<ImageFileResult()>& chooseFile);
  // Asynchronous host adapter. Taking a request never commits; completion is
  // accepted once and revalidates the captured target/selection.
  [[nodiscard]] std::uint64_t beginImageFileRequest();
  bool completeImageFileRequest(std::uint64_t token, const ImageFileResult& file);
  [[nodiscard]] canvas::runtime::SelectedImageSnapshot selectedImage() const;
  [[nodiscard]] canvas::runtime::ImageCropSnapshot imageCrop() const;
  void beginImageCrop();
  void updateImageCrop(const canvas::runtime::ImageCropUpdateRequest&);
  void finishImageCrop(bool commit);
  [[nodiscard]] const std::string& imageStatus() const noexcept { return imageStatus_; }
  canvas::runtime::ProductControlReceipt setSelectionMode(bool enabled) noexcept;
  canvas::runtime::ProductControlReceipt undo() noexcept;
  canvas::runtime::ProductControlReceipt redo() noexcept;
  canvas::runtime::ProductControlReceipt setCreationPreset(
      canvas::runtime::CanvasCreationPreset preset) noexcept;
  canvas::runtime::StickyCommandReceipt createSticky() noexcept;
  canvas::runtime::ConnectorCommandReceipt createConnector() noexcept;
  canvas::runtime::ConnectorCommandReceipt setConnectorRouting(canvas::semantic::ConnectorRouting, bool selected) noexcept;
  [[nodiscard]] canvas::semantic::ConnectorRouting connectorRoutingPreset() const;
  [[nodiscard]] canvas::runtime::StickySnapshot selectedSticky() const;
  [[nodiscard]] canvas::runtime::SectionSnapshot sectionSnapshot() const;
  canvas::runtime::ProductControlReceipt sectionIntent(const canvas::runtime::SectionIntent&) noexcept;
  [[nodiscard]] canvas::runtime::ConnectorSnapshot selectedConnector() const;
  [[nodiscard]] const std::string& connectorStickyStatus() const noexcept {
    return connectorStickyStatus_;
  }
  canvas::runtime::ProductControlReceipt panBy(float dx, float dy) noexcept;
  canvas::runtime::ProductControlReceipt zoomAt(float anchorX, float anchorY,
                                                 float scaleDelta) noexcept;
  canvas::runtime::ProductControlReceipt fitToContent() noexcept;
  canvas::runtime::ProductControlReceipt fitToSelection() noexcept;
  canvas::runtime::ProductControlReceipt fitPrimaryObject() noexcept;
  canvas::runtime::ProductControlReceipt selectedAction(
      canvas::runtime::CanvasControlPayloadKind kind, bool flag = false) noexcept;
  canvas::runtime::AxiomDebugCommandReceipt submitAxiom(
      canvas::runtime::AxiomDebugCommandKind kind, std::uint64_t value = 0) noexcept;
  canvas::runtime::SurfaceModeReceipt setCanonicalSurfaceMode(
      canvas::runtime::SurfaceMode mode) noexcept;

  [[nodiscard]] bool hasRuntimeOwner() const noexcept { return runtime_ != nullptr; }
  [[nodiscard]] bool hasAxiomDebugOwner() const noexcept { return axiomDebug_ != nullptr; }
  [[nodiscard]] bool hasPlatformDebugOwner() const noexcept { return platformDebug_ != nullptr; }

 private:
  struct FrameContext final {
    DebugSnapshotStamp stamp{};
    std::uint32_t selectedObjectCount = 0;
    std::uint64_t primaryObject = 0;
  };
  struct PendingAxiom final { std::string action; };
  struct PendingSurface final { std::string action; };

  [[nodiscard]] std::uint64_t nextRequestId() noexcept;
  [[nodiscard]] std::uint64_t productDeadline() const noexcept;
  [[nodiscard]] std::uint64_t uiDeadline() const noexcept {
    return frame_.stamp.sequence + 120U;
  }
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
  void recordImage(const char* action, const canvas::runtime::ImageCommandReceipt&);
  void recordText(const char* action, const canvas::runtime::TextReceipt&);
  struct PendingText final { std::uint64_t client; std::string action; };
  std::unordered_map<std::uint64_t, PendingText> pendingText_;

  canvas::runtime::RuntimeFacade* runtime_ = nullptr;
  canvas::runtime::AxiomDebugControl* axiomDebug_ = nullptr;
  canvas::runtime::PlatformDebugControl* platformDebug_ = nullptr;
  DebugActivityLog* activity_ = nullptr;
  FrameContext frame_{};
  std::uint64_t nextRequest_ = 1;
  struct ImageFileRequest final {
    bool replace = false;
    canvas::runtime::SelectedImageSnapshot selected;
    canvas::runtime::CanvasTargetKey target;
    std::uint64_t token = 0;
    bool dispatched = false;
  };
  std::optional<ImageFileRequest> imageFileRequest_;
  std::string imageStatus_;
  std::string connectorStickyStatus_;
  std::uint64_t connectorDefaultsEpoch_ = 0;
  bool connectorDefaultsReady_ = false;
  std::unordered_map<std::uint64_t, PendingAxiom> pendingAxiom_;
  std::unordered_map<std::uint64_t, PendingSurface> pendingSurface_;
  std::unordered_map<canvas::runtime::CanvasControlReceiptKey, std::string,
                     canvas::runtime::CanvasControlReceiptKeyHash> pendingProduct_;
};

}  // namespace canvas::debug_ui
