#include "minimal_runtime_facade.hpp"

#include "canvas/render/render_target.hpp"

namespace canvas::minimal_runtime_host {

MinimalRuntimeFacade::MinimalRuntimeFacade(MinimalRuntimeHost& host) noexcept
    : host_(host) {}

runtime::SelectionSummary MinimalRuntimeFacade::readSelectionSummary() const {
  return host_.publicComposition().controlFacade().readSelectionSummary();
}

bool MinimalRuntimeFacade::setMultiSelectionPermission(runtime::MultiSelectionPermissionMode mode) noexcept {
  return host_.publicComposition().controlFacade().setMultiSelectionPermission(mode);
}
runtime::MultiSelectionProjection MinimalRuntimeFacade::readMultiSelection() const noexcept {
  return host_.publicComposition().controlFacade().readMultiSelection();
}
std::optional<std::vector<runtime::MultiSelectionRootPropertyValue>> MinimalRuntimeFacade::readMultiSelectionPropertyValues(
    std::uint32_t field, const runtime::MultiSelectionStamp& stamp,
    const std::vector<foundation::ObjectId>& eligible) const noexcept {
  return host_.publicComposition().controlFacade().readMultiSelectionPropertyValues(field, stamp, eligible);
}
runtime::MultiSelectionSubmitReceipt MinimalRuntimeFacade::submitMultiSelectionProperty(
    std::uint32_t field, const semantic::PropertyValue& value, const runtime::MultiSelectionStamp& stamp,
    const std::vector<foundation::ObjectId>& eligible, std::uint64_t client, std::uint64_t request) noexcept {
  return host_.publicComposition().controlFacade().submitMultiSelectionProperty(field, value, stamp, eligible, client, request);
}
runtime::MultiSelectionSubmitReceipt MinimalRuntimeFacade::submitMultiSelectionStrokeWidth(
    double width, const runtime::MultiSelectionStamp& stamp, const std::vector<foundation::ObjectId>& eligible,
    std::uint64_t client, std::uint64_t request) noexcept {
  return host_.publicComposition().controlFacade().submitMultiSelectionStrokeWidth(width, stamp, eligible, client, request);
}
std::optional<runtime::MultiSelectionOutcome> MinimalRuntimeFacade::multiSelectionReceipt(
    std::uint64_t client, std::uint64_t request) const noexcept {
  return host_.publicComposition().controlFacade().multiSelectionReceipt(client, request);
}

runtime::SectionSnapshot MinimalRuntimeFacade::readSection(foundation::ObjectId id) const {
  return host_.publicComposition().controlFacade().readSection(id);
}

runtime::RuntimeStateSnapshot MinimalRuntimeFacade::readRuntimeState() const noexcept {
  return host_.publicComposition().controlFacade().readRuntimeState();
}
runtime::ProductControlReceipt MinimalRuntimeFacade::submitProductControl(
    const runtime::ProductControlRequest& request) noexcept {
  return host_.publicComposition().controlFacade().submitProductControl(request);
}
runtime::ProductControlReceipt MinimalRuntimeFacade::submitCanvasControl(
    const runtime::CanvasControlRequest& request) noexcept {
  return host_.publicComposition().controlFacade().submitCanvasControl(request);
}
runtime::CanvasControlSnapshot MinimalRuntimeFacade::readCanvasControlSnapshot(
    const runtime::CanvasTargetKey& target) const noexcept {
  return host_.publicComposition().controlFacade().readCanvasControlSnapshot(target);
}
std::vector<runtime::CanvasTargetKey> MinimalRuntimeFacade::mountedCanvasTargets() const {
  return host_.publicComposition().controlFacade().mountedCanvasTargets();
}
std::vector<runtime::BrushPresetRef> MinimalRuntimeFacade::brushPresets() const {
  return host_.publicComposition().controlFacade().brushPresets();
}
std::uint64_t MinimalRuntimeFacade::controlSequence() const noexcept {
  return host_.publicComposition().controlFacade().controlSequence();
}
std::optional<runtime::CanvasControlReceipt> MinimalRuntimeFacade::canvasControlReceipt(
    runtime::CanvasControlReceiptKey key) const noexcept {
  return host_.publicComposition().controlFacade().canvasControlReceipt(key);
}
const runtime::CanvasTargetKey& MinimalRuntimeFacade::target() const noexcept {
  static thread_local runtime::CanvasTargetKey value{};
  const auto targets = host_.publicComposition().controlFacade().mountedCanvasTargets();
  value = targets.empty() ? runtime::CanvasTargetKey{} : targets.front();
  return value;
}

std::size_t MinimalRuntimeFacade::process() noexcept {
  if (!host_.composition().ownerPump()) return 0U;
  return host_.composition().readback().lastOwnerControlCount;
}

runtime::PlatformDiagnosticsSnapshot
MinimalRuntimeFacade::readPlatformDiagnostics() const noexcept {
  runtime::PlatformDiagnosticsSnapshot out{};
  const auto surface = host_.composition().surfaceSnapshot();
  out.canonicalSurfaceGeneration = surface.surfaceGeneration;
  out.previewSurfaceGeneration = host_.composition().previewSurfaceGeneration();
  out.width = surface.width;
  out.height = surface.height;
  out.devicePixelRatio = surface.devicePixelRatio;
  out.presentCount = host_.composition().canonicalPresentCount();
  out.surfaceAvailable = surface.available;
  out.canonicalSurfaceMode = surface.available &&
      host_.composition().canonicalBackend() == render::RenderTargetBackend::kD3D12
      ? runtime::SurfaceMode::kGpuDefault : runtime::SurfaceMode::kCpuReference;
  return out;
}

runtime::TextReceipt MinimalRuntimeFacade::submitTextIntent(const runtime::TextIntent& r) { return host_.publicComposition().controlFacade().submitTextIntent(r); }
runtime::TextReceipt MinimalRuntimeFacade::submitTextInput(const runtime::TextInputEvent& r) { return host_.publicComposition().controlFacade().submitTextInput(r); }
runtime::TextEditorSnapshot MinimalRuntimeFacade::readTextEditor() const { return host_.publicComposition().controlFacade().readTextEditor(); }
runtime::FontCatalogSnapshot MinimalRuntimeFacade::readFontCatalog() const { return host_.publicComposition().controlFacade().readFontCatalog(); }
std::optional<runtime::FontCatalogEntry> MinimalRuntimeFacade::readFontResourceStatus(foundation::ObjectId id) const { return host_.publicComposition().controlFacade().readFontResourceStatus(id); }
std::optional<runtime::TextReceipt> MinimalRuntimeFacade::textInputReceipt(const runtime::TextSessionToken& t,std::uint64_t s) const { return host_.publicComposition().controlFacade().textInputReceipt(t,s); }
std::optional<runtime::TextReceipt> MinimalRuntimeFacade::textIntentReceipt(std::uint64_t c,std::uint64_t r) const { return host_.publicComposition().controlFacade().textIntentReceipt(c,r); }
bool MinimalRuntimeFacade::acknowledgeTextInput(const runtime::TextSessionToken& t,std::uint64_t s) { return host_.publicComposition().controlFacade().acknowledgeTextInput(t,s); }
runtime::ImageCommandReceipt MinimalRuntimeFacade::importImage(const runtime::ImageImportRequest& request) { return host_.publicComposition().controlFacade().importImage(request); }
runtime::ImageCommandReceipt MinimalRuntimeFacade::replaceImage(const runtime::ImageReplaceRequest& request) { return host_.publicComposition().controlFacade().replaceImage(request); }
runtime::ImageCommandReceipt MinimalRuntimeFacade::resizeImage(const runtime::ImageResizeRequest& request) { return host_.publicComposition().controlFacade().resizeImage(request); }
runtime::ImageCommandReceipt MinimalRuntimeFacade::beginImageCrop(const runtime::ImageCropBeginRequest& request) { return host_.publicComposition().controlFacade().beginImageCrop(request); }
runtime::ImageCommandReceipt MinimalRuntimeFacade::updateImageCrop(const runtime::ImageCropUpdateRequest& request) { return host_.publicComposition().controlFacade().updateImageCrop(request); }
runtime::ImageCommandReceipt MinimalRuntimeFacade::commitImageCrop(std::uint64_t token) { return host_.publicComposition().controlFacade().commitImageCrop(token); }
runtime::ImageCommandReceipt MinimalRuntimeFacade::cancelImageCrop(std::uint64_t token) { return host_.publicComposition().controlFacade().cancelImageCrop(token); }
runtime::ImageCropSnapshot MinimalRuntimeFacade::readImageCrop() const { return host_.publicComposition().controlFacade().readImageCrop(); }
runtime::SelectedImageSnapshot MinimalRuntimeFacade::readSelectedImage() const { return host_.publicComposition().controlFacade().readSelectedImage(); }
runtime::ConnectorCommandReceipt MinimalRuntimeFacade::createConnector(const runtime::ConnectorCreateRequest& r) { return host_.publicComposition().controlFacade().createConnector(r); }
runtime::ConnectorCommandReceipt MinimalRuntimeFacade::setConnectorRouting(const runtime::ConnectorRoutingRequest& r) { return host_.publicComposition().controlFacade().setConnectorRouting(r); }
semantic::ConnectorRouting MinimalRuntimeFacade::readConnectorRoutingPreset() const { return host_.publicComposition().controlFacade().readConnectorRoutingPreset(); }
runtime::ConnectorSnapshot MinimalRuntimeFacade::readConnectorState(semantic::ObjectId id) const { return host_.publicComposition().controlFacade().readConnectorState(id); }
runtime::StickyCommandReceipt MinimalRuntimeFacade::createSticky(const runtime::StickyCreateRequest& r) { return host_.publicComposition().controlFacade().createSticky(r); }
runtime::ConnectorSnapshot MinimalRuntimeFacade::readSelectedConnector() const { return host_.publicComposition().controlFacade().readSelectedConnector(); }
runtime::StickySnapshot MinimalRuntimeFacade::readSelectedSticky() const { return host_.publicComposition().controlFacade().readSelectedSticky(); }

}  // namespace canvas::minimal_runtime_host

namespace canvas::minimal_runtime_host { runtime::TextGeometrySnapshot MinimalRuntimeFacade::readTextGeometry(const std::optional<runtime::TextGeometryToken>& expected) const { return host_.publicComposition().controlFacade().readTextGeometry(expected); } }
