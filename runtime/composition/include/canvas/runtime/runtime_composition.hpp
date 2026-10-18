#pragma once

#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/runtime/resource_binding.hpp"
#include "canvas/runtime/scene_adapter.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>
#include <thread>

namespace canvas::render { class ImageResourceResolver; }
namespace canvas::text { class RichTextLayoutService; }

namespace canvas::runtime {
class CanvasRuntimeFacade;

enum class RuntimeCompositionState : std::uint8_t {
  kCreated,
  kBound,
  kReady,
  kLost,
  kRecovering,
  kStopping,
  kQuiescent,
  kClosed,
};

struct RuntimeCompositionReceipt final {
  RuntimeCompositionState state = RuntimeCompositionState::kCreated;
  std::uint64_t lifetimeGeneration = 1;
  std::uint64_t ownerPumpCount = 0;
  std::size_t lastOwnerControlCount = 0;
  std::uint64_t resourceRevision = 0;
  std::uint64_t sceneRevision = 0;
  std::uint64_t semanticGeneration = 0;
  std::size_t pendingNotifications = 0;
  bool canonicalSubmitted = false;
  bool canonicalDemand = false;
};

struct RuntimeFrameOpportunity final {
  bool needsPaint = false;
  bool canonicalRequested = false;
  bool previewActive = false;
  bool previewDirty = false;
  std::size_t pendingCanonicalHandoff = 0;
  std::uint64_t previewSurfaceGeneration = 0;
  std::uint64_t previewRevision = 0;
  std::optional<render::RuntimeFrameIdentity> canonicalFrame;
};

// Composition-owned, read-only surface identity.  This deliberately mirrors
// only the public surface facts needed by platform hosts and diagnostics; it
// does not expose CanvasEditingRuntime's interaction types.
struct RuntimeSurfaceSnapshot final {
  render::ViewId viewId{};
  std::uint64_t surfaceGeneration = 0;
  std::uint64_t metricsGeneration = 0;
  float logicalWidth = 0.0F;
  float logicalHeight = 0.0F;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  float devicePixelRatio = 1.0F;
  float displayScale = 1.0F;
  bool available = false;
  render::RenderTargetBackend backend = render::RenderTargetBackend::kRaster;
};

class RuntimeComposition final {
 public:
  explicit RuntimeComposition(render::ViewId viewId = render::ViewId{1});
  ~RuntimeComposition();
  RuntimeComposition(const RuntimeComposition&) = delete;
  RuntimeComposition& operator=(const RuntimeComposition&) = delete;

  [[nodiscard]] bool open() noexcept;
  [[nodiscard]] bool bindSurface(std::uint32_t width, std::uint32_t height) noexcept;
  [[nodiscard]] bool resizeSurface(std::uint32_t width, std::uint32_t height) noexcept;
  [[nodiscard]] bool registerSurfaceProvider(
      std::string profileId,
      std::unique_ptr<render::SkiaSurfaceProvider> provider) noexcept;
  [[nodiscard]] bool registerPreviewSurfaceProvider(
      std::string profileId,
      std::unique_ptr<render::SkiaSurfaceProvider> provider) noexcept;
  [[nodiscard]] bool bindManifest(const semantic::ResourceManifest& manifest,
                                  std::uint64_t documentBinding) noexcept;
  [[nodiscard]] bool registerSource(input::InputSourceId source) noexcept;
  [[nodiscard]] bool unregisterSource(input::InputSourceId source,
                                      std::uint64_t sequence,
                                      std::uint64_t timestampNs) noexcept;
  [[nodiscard]] bool cancelAllPointers() noexcept;
  [[nodiscard]] bool submit(const input::PlatformPointerBatch& batch) noexcept;
  [[nodiscard]] semantic::ApplyResult applyExternalOperation(
      const semantic::Operation& operation, semantic::ApplySource source) noexcept;
  [[nodiscard]] bool notifyResource(foundation::ObjectId resourceId) noexcept;
  [[nodiscard]] bool publishResource(foundation::ObjectId resourceId,
                                     std::vector<std::uint8_t> bytes) noexcept;
  [[nodiscard]] bool publishResource(const ResourceBindingToken& token,
                                     std::vector<std::uint8_t> bytes) noexcept;
  [[nodiscard]] bool ownerPump() noexcept;
  // The single public frame opportunity. Runtime owns work selection and
  // frame identity; platform hosts only execute/observe the returned work.
  [[nodiscard]] bool serviceFrameOpportunity(bool forceCanonical,
                                              RuntimeFrameOpportunity& out) noexcept;
  // The composition owns the single control facade and its queue.  Consumers
  // may borrow the stable interface, but must not construct a second facade or
  // attach an alternate owner.
  [[nodiscard]] CanvasRuntimeFacade& controlFacade() noexcept { return *controlFacade_; }
  [[nodiscard]] const CanvasRuntimeFacade& controlFacade() const noexcept {
    return *controlFacade_;
  }
  [[nodiscard]] bool requestCanonicalFrame(std::uint64_t frameId) noexcept;
  [[nodiscard]] bool loseSurface() noexcept;
  [[nodiscard]] bool rebindSurface() noexcept;
  [[nodiscard]] bool rebindSurface(const render::SurfaceMetrics& metrics) noexcept;
  // Adopt providers that the platform prepared before entering the composition
  // boundary. This validates the already-live identity without advancing it a
  // second time.
  [[nodiscard]] bool adoptPreparedSurface(std::uint64_t expectedGeneration,
                                          const render::SurfaceMetrics& metrics) noexcept;
  [[nodiscard]] bool receivePlatformPresented(
      const render::PresentedFeedback& feedback) noexcept;
  [[nodiscard]] bool stop() noexcept;
  [[nodiscard]] bool detach() noexcept;
  [[nodiscard]] bool close() noexcept;
  [[nodiscard]] RuntimeCompositionReceipt readback() const noexcept;
  [[nodiscard]] RuntimeSurfaceSnapshot surfaceSnapshot() const noexcept;
  [[nodiscard]] std::uint64_t previewSurfaceGeneration() const noexcept;
  [[nodiscard]] render::RenderTargetBackend canonicalBackend() const noexcept;
  [[nodiscard]] std::uint64_t nextCanonicalFrameId() const noexcept;
  [[nodiscard]] std::uint64_t canonicalContentSerial() const noexcept;
  [[nodiscard]] std::optional<render::RuntimeFrameIdentity> lastSubmittedCanonicalFrame() const noexcept;
  [[nodiscard]] bool canonicalPresentationDirty() const noexcept;
  [[nodiscard]] bool previewActive() const noexcept;
  [[nodiscard]] const render::SurfaceRenderState& previewRenderState() const noexcept;
  [[nodiscard]] bool capturePreviewPresentation(
      interaction::PreviewPresentationCapture& capture) const noexcept;
  [[nodiscard]] bool renderPreviewPresentation(
      const interaction::PreviewPresentationCapture& capture,
      std::uint64_t* presentCount) noexcept;
  [[nodiscard]] bool acknowledgePreviewPresentation(
      const interaction::PreviewPresentationCapture& capture,
      std::uint64_t presentCount) noexcept;
  [[nodiscard]] bool readCanonicalRgba(std::span<std::uint8_t> destination) noexcept;
  [[nodiscard]] bool readPreviewRgba(std::span<std::uint8_t> destination) noexcept;
  [[nodiscard]] bool canonicalProviderAvailable() const noexcept;
  [[nodiscard]] bool previewProviderAvailable() const noexcept;
  [[nodiscard]] bool setPlatformPresentationDeferred(bool deferred) noexcept;
  [[nodiscard]] RuntimeCompositionState state() const noexcept { return state_; }
  [[nodiscard]] interaction::CanvasEditingRuntime& runtime() noexcept { return *runtime_; }
  [[nodiscard]] const ResourceBindingSnapshot& resources() const noexcept {
    return resources_;
  }
  [[nodiscard]] std::optional<ResourcePortReceipt> resourceReceipt(
      foundation::ObjectId id) const noexcept;
  [[nodiscard]] ResourcePortError lastResourceError() const noexcept {
    return lastResourceError_;
  }

 private:
  struct Notification final {
    foundation::ObjectId resourceId{};
    ResourceBindingToken token{};
  };
  bool canMutate() const noexcept;
  bool transition(RuntimeCompositionState next) noexcept;
  bool refreshManifestDerivedState() noexcept;

  std::unique_ptr<interaction::CanvasEditingRuntime> runtime_;
  RuntimeCompositionState state_ = RuntimeCompositionState::kCreated;
  std::uint64_t ownerPumpCount_ = 0;
  std::size_t lastOwnerControlCount_ = 0;
  std::uint64_t providerEpoch_ = 0;
  std::uint64_t nextRequestId_ = 1;
  std::vector<Notification> notifications_;
  std::unordered_map<foundation::ObjectId, ResourcePortReceipt,
                     foundation::ObjectIdHash> resourceReceipts_;
  ResourceBindingSnapshot resources_;
  std::unique_ptr<MemoryResourceProvider> provider_;
#if defined(CANVAS_RENDER_HAS_SKIA)
  std::unique_ptr<canvas::render::ImageResourceResolver> imageResolver_;
#endif
  std::unique_ptr<canvas::text::RichTextLayoutService> textLayoutService_;
  ResourcePortError lastResourceError_ = ResourcePortError::kNone;
  std::thread::id ownerThread_;
  render::ViewId viewId_{render::ViewId{1}};
  std::unique_ptr<CanvasRuntimeFacade> controlFacade_;
  void refreshControlTarget() noexcept;
};

}  // namespace canvas::runtime
