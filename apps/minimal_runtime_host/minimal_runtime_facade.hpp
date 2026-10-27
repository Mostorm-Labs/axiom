#pragma once

#include "minimal_runtime_host.hpp"
#include "canvas/runtime/diagnostics.hpp"
#include "canvas/runtime/runtime_facade.hpp"

namespace canvas::minimal_runtime_host {

class MinimalRuntimeFacade final : public runtime::RuntimeFacade,
                                   public runtime::PlatformDiagnostics {
 public:
  explicit MinimalRuntimeFacade(MinimalRuntimeHost& host) noexcept;
  [[nodiscard]] runtime::SelectionSummary readSelectionSummary() const override;
  [[nodiscard]] runtime::SectionSnapshot readSection(foundation::ObjectId = {}) const override;
  runtime::TextReceipt submitTextIntent(const runtime::TextIntent&) override;
  runtime::TextReceipt submitTextInput(const runtime::TextInputEvent&) override;
  runtime::TextEditorSnapshot readTextEditor() const override;
  [[nodiscard]] runtime::FontCatalogSnapshot readFontCatalog() const override;
  [[nodiscard]] std::optional<runtime::FontCatalogEntry> readFontResourceStatus(foundation::ObjectId) const override;
  [[nodiscard]] runtime::TextGeometrySnapshot readTextGeometry(const std::optional<runtime::TextGeometryToken>& expected = std::nullopt) const override;
  std::optional<runtime::TextReceipt> textInputReceipt(const runtime::TextSessionToken&,std::uint64_t) const override;
  std::optional<runtime::TextReceipt> textIntentReceipt(std::uint64_t,std::uint64_t) const override;
  bool acknowledgeTextInput(const runtime::TextSessionToken&,std::uint64_t) override;
  [[nodiscard]] runtime::ImageCommandReceipt importImage(
      const runtime::ImageImportRequest&) override;
  [[nodiscard]] runtime::ImageCommandReceipt replaceImage(
      const runtime::ImageReplaceRequest&) override;
  [[nodiscard]] runtime::ImageCommandReceipt resizeImage(
      const runtime::ImageResizeRequest&) override;
  [[nodiscard]] runtime::ImageCommandReceipt beginImageCrop(
      const runtime::ImageCropBeginRequest&) override;
  [[nodiscard]] runtime::ImageCommandReceipt updateImageCrop(
      const runtime::ImageCropUpdateRequest&) override;
  [[nodiscard]] runtime::ImageCommandReceipt commitImageCrop(std::uint64_t) override;
  [[nodiscard]] runtime::ImageCommandReceipt cancelImageCrop(std::uint64_t) override;
  [[nodiscard]] runtime::ImageCropSnapshot readImageCrop() const override;
  [[nodiscard]] runtime::SelectedImageSnapshot readSelectedImage() const override;
  [[nodiscard]] runtime::ConnectorCommandReceipt createConnector(const runtime::ConnectorCreateRequest&) override;
  [[nodiscard]] runtime::ConnectorCommandReceipt setConnectorRouting(const runtime::ConnectorRoutingRequest&) override;
  [[nodiscard]] semantic::ConnectorRouting readConnectorRoutingPreset() const override;
  [[nodiscard]] runtime::ConnectorSnapshot readConnectorState(semantic::ObjectId) const override;
  [[nodiscard]] runtime::StickyCommandReceipt createSticky(const runtime::StickyCreateRequest&) override;
  [[nodiscard]] runtime::ConnectorSnapshot readSelectedConnector() const override;
  [[nodiscard]] runtime::StickySnapshot readSelectedSticky() const override;
  [[nodiscard]] runtime::RuntimeStateSnapshot readRuntimeState() const noexcept override;
  [[nodiscard]] runtime::ProductControlReceipt submitProductControl(
      const runtime::ProductControlRequest& request) noexcept override;
  [[nodiscard]] runtime::ProductControlReceipt submitCanvasControl(
      const runtime::CanvasControlRequest& request) noexcept override;
  [[nodiscard]] runtime::CanvasControlSnapshot readCanvasControlSnapshot(
      const runtime::CanvasTargetKey& target) const noexcept override;
  [[nodiscard]] std::vector<runtime::CanvasTargetKey> mountedCanvasTargets() const override;
  [[nodiscard]] std::vector<runtime::BrushPresetRef> brushPresets() const override;
  [[nodiscard]] std::uint64_t controlSequence() const noexcept override;
  [[nodiscard]] std::optional<runtime::CanvasControlReceipt> canvasControlReceipt(
      runtime::CanvasControlReceiptKey key) const noexcept override;
  [[nodiscard]] const runtime::CanvasTargetKey& target() const noexcept;
  [[nodiscard]] runtime::PlatformDiagnosticsSnapshot readPlatformDiagnostics() const noexcept override;
  [[nodiscard]] std::size_t process() noexcept;

 private:
  MinimalRuntimeHost& host_;
};

}  // namespace canvas::minimal_runtime_host
