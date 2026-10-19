#pragma once

#include "minimal_runtime_host.hpp"
#include "canvas/runtime/canvas_runtime_facade.hpp"
#include "canvas/runtime/diagnostics.hpp"

namespace canvas::minimal_runtime_host {

class MinimalRuntimeFacade final : public runtime::RuntimeFacade,
                                   public runtime::PlatformDiagnostics {
 public:
  explicit MinimalRuntimeFacade(MinimalRuntimeHost& host) noexcept;
  runtime::TextReceipt submitTextIntent(const runtime::TextIntent&) override;
  runtime::TextReceipt submitTextInput(const runtime::TextInputEvent&) override;
  runtime::TextEditorSnapshot readTextEditor() const override;
  [[nodiscard]] runtime::TextGeometrySnapshot readTextGeometry(const std::optional<runtime::TextGeometryToken>& expected = std::nullopt) const override;
  std::optional<runtime::TextReceipt> textInputReceipt(const runtime::TextSessionToken&,std::uint64_t) const override;
  std::optional<runtime::TextReceipt> textIntentReceipt(std::uint64_t,std::uint64_t) const override;
  bool acknowledgeTextInput(const runtime::TextSessionToken&,std::uint64_t) override;
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
