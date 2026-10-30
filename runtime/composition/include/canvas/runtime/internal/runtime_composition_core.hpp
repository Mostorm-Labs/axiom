#pragma once

#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/runtime/resource_binding.hpp"
#include "canvas/runtime/scene_adapter.hpp"
#include "canvas/runtime/runtime_facade.hpp"
#include "canvas/runtime/runtime_composition.hpp"

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
class MultiSelectionPort;

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

// Platform echoes the original work value; per-view frame counters alone
// cannot identify an execution across Runtime instances or binding lifetimes.
struct CanonicalWork final {
  std::uint64_t runtimeInstanceId = 0;
  std::uint64_t lifetimeGeneration = 0;
  std::uint64_t attachmentEpoch = 0;
  std::uint64_t workId = 0;
  render::RuntimeFrameIdentity frame{};
  bool operator==(const CanonicalWork&) const = default;
};

struct CanonicalCompletion final {
  CanonicalWork work{};
  render::PresentOutcome outcome = render::PresentOutcome::kUnknown;
  render::PresentEvidenceKind evidence = render::PresentEvidenceKind::kNone;
  std::optional<std::uint64_t> presentedTimeNs;
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
  std::optional<CanonicalWork> canonicalWork;
  bool previewRendered = false;
  struct PreviewWork final {
    std::uint64_t runtimeInstanceId = 0;
    std::uint64_t viewId = 0;
    std::uint64_t lifetimeGeneration = 0;
    std::uint64_t workId = 0;
    std::uint64_t contentRevision = 0;
    std::uint64_t surfaceGeneration = 0;
    std::uint64_t metricsGeneration = 0;
    bool operator==(const PreviewWork&) const = default;
  };
  std::optional<PreviewWork> previewWork;
};

struct RuntimeFrameCompletion final {
  std::optional<RuntimeFrameOpportunity::PreviewWork> previewWork;
  bool platformPresented = false;
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

class RuntimeCompositionCore final {
  friend class CanvasRuntimeFacade;
  friend class MultiSelectionPort;
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
  friend class canvas::qualification::Access;
#endif
 public:
  explicit RuntimeCompositionCore(render::ViewId viewId = render::ViewId{1});
  ~RuntimeCompositionCore();
  RuntimeCompositionCore(const RuntimeCompositionCore&) = delete;
  RuntimeCompositionCore& operator=(const RuntimeCompositionCore&) = delete;

  [[nodiscard]] bool open() noexcept;
  [[nodiscard]] bool openDocument(semantic::DocumentId id) noexcept;
  [[nodiscard]] semantic::DocumentId documentId() const noexcept;
  [[nodiscard]] bool setLocalOperationSink(interaction::LocalOperationSink* sink) noexcept;
  [[nodiscard]] semantic::CanonicalCommitStamp canonicalCommitFence() const noexcept;
  [[nodiscard]] DocumentSnapshotResult captureDocumentSnapshot(const DocumentSnapshotRequest& request) noexcept;
  [[nodiscard]] DocumentRestoreStatus restoreSnapshotBeforeSurface(const semantic::SemanticSnapshot& snapshot,
      const semantic::SnapshotBootstrapper::CheckpointBinding& binding, std::uint64_t resourceBinding) noexcept;
  // Reference compatibility only: these overloads mean logical=physical,
  // DPR=1. Platform consumers pass the complete accepted measurements.
  [[nodiscard]] bool bindSurface(std::uint32_t width, std::uint32_t height) noexcept;
  [[nodiscard]] bool bindSurface(const render::SurfaceMetrics& metrics) noexcept;
  [[nodiscard]] bool resizeSurface(std::uint32_t width, std::uint32_t height) noexcept;
  [[nodiscard]] bool resizeSurface(const render::SurfaceMetrics& metrics) noexcept;
  [[nodiscard]] bool registerSurfaceProvider(
      std::string profileId,
      std::unique_ptr<render::SkiaSurfaceProvider> provider) noexcept;
  // Both registration methods stage candidates. bind/resize/rebind commits
  // the complete pair; registration never changes the accepted snapshot.
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
  // Internal platform receipts. Generation ownership stays in input ingress;
  // native capture loss must echo the accepted full key, never a bare ID.
  [[nodiscard]] std::optional<input::PointerKey> platformKey(
      input::InputSourceId source, input::PointerId pointer) const noexcept;
  [[nodiscard]] bool cancelPlatformPointer(const input::PointerKey& key,
      std::uint64_t sequence, std::uint64_t timestampNs) noexcept;
  [[nodiscard]] std::size_t semanticObjectCount() const noexcept;
  [[nodiscard]] bool submit(const input::PlatformPointerBatch& batch) noexcept;
  [[nodiscard]] bool hoverSelectionAtViewPoint(float x, float y) noexcept;
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
  // Completes the Runtime-owned preview handoff after the platform has
  // supplied its real presentation observation. Hosts do not capture or
  // acknowledge preview sessions themselves.
  [[nodiscard]] bool finishFrameOpportunity(
      const RuntimeFrameCompletion& completion) noexcept;
  // The composition owns the single control facade and its queue.  Consumers
  // may borrow the stable interface, but must not construct a second facade or
  // attach an alternate owner.
  [[nodiscard]] RuntimeFacade& controlFacade() noexcept;
  [[nodiscard]] const RuntimeFacade& controlFacade() const noexcept;
  [[nodiscard]] MultiSelectionPort& multiSelectionPort() noexcept;
#if defined(AXIOM_RUNTIME_QUALIFICATION)
  [[nodiscard]] bool requestCanonicalFrame(std::uint64_t frameId) noexcept;
#endif
  [[nodiscard]] bool loseSurface() noexcept;
  [[nodiscard]] bool rebindSurface() noexcept;
  [[nodiscard]] bool rebindSurface(const render::SurfaceMetrics& metrics) noexcept;
  // Adopt providers that the platform prepared before entering the composition
  // boundary. This validates the already-live identity without advancing it a
  // second time.
  [[nodiscard]] bool adoptPreparedSurface(std::uint64_t expectedGeneration,
                                          const render::SurfaceMetrics& metrics) noexcept;
  [[nodiscard]] bool receivePlatformPresented(
      const CanonicalCompletion& completion) noexcept;
  [[nodiscard]] bool stop() noexcept;
  [[nodiscard]] bool detach() noexcept;
  [[nodiscard]] bool close() noexcept;
  [[nodiscard]] std::uint64_t canonicalPresentCount() const noexcept;
  [[nodiscard]] RuntimeCompositionReceipt readback() const noexcept;
  [[nodiscard]] RuntimeSurfaceSnapshot surfaceSnapshot() const noexcept;
  [[nodiscard]] std::uint64_t previewSurfaceGeneration() const noexcept;
  [[nodiscard]] render::RenderTargetBackend canonicalBackend() const noexcept;
#if defined(AXIOM_RUNTIME_QUALIFICATION)
  [[nodiscard]] std::uint64_t nextCanonicalFrameId() const noexcept;
#endif
  [[nodiscard]] std::uint64_t canonicalContentSerial() const noexcept;
  [[nodiscard]] std::optional<render::RuntimeFrameIdentity> lastSubmittedCanonicalFrame() const noexcept;
  [[nodiscard]] std::optional<CanonicalWork> lastSubmittedCanonicalWork() const noexcept;
  [[nodiscard]] bool canonicalPresentationDirty() const noexcept;
  [[nodiscard]] bool previewActive() const noexcept;
  [[nodiscard]] const render::SurfaceRenderState& previewRenderState() const noexcept;
  [[nodiscard]] bool readCanonicalRgba(std::span<std::uint8_t> destination) noexcept;
  [[nodiscard]] bool readPreviewRgba(std::span<std::uint8_t> destination) noexcept;
  [[nodiscard]] bool canonicalProviderAvailable() const noexcept;
  [[nodiscard]] bool previewProviderAvailable() const noexcept;
  [[nodiscard]] bool setPlatformPresentationDeferred(bool deferred) noexcept;
  [[nodiscard]] RuntimeCompositionState state() const noexcept { return state_; }
#if defined(AXIOM_RUNTIME_QUALIFICATION)
  // White-box qualification only. Ordinary consumers must use RuntimeFacade,
  // Composition snapshots and platform execution ports.
  [[nodiscard]] interaction::CanvasEditingRuntime& qualificationRuntime() noexcept {
    return *runtime_;
  }
  [[nodiscard]] const interaction::CanvasEditingRuntime& qualificationRuntime() const noexcept {
    return *runtime_;
  }
  [[nodiscard]] interaction::CanvasEditingRuntime& runtime() noexcept {
    return *runtime_;
  }
#endif
  [[nodiscard]] const ResourceBindingSnapshot& resources() const noexcept {
    return resources_;
  }
  [[nodiscard]] std::optional<std::vector<std::uint8_t>>
  readResourceBytes(const ResourceBindingToken&) const noexcept;
  [[nodiscard]] std::optional<ResourcePortReceipt> resourceReceipt(
      foundation::ObjectId id) const noexcept;
  [[nodiscard]] bool imageResourceDecodeReady(
      foundation::ObjectId id) noexcept;
  [[nodiscard]] std::optional<ResourceBindingToken> currentResourceToken(
      foundation::ObjectId id) const noexcept;
  [[nodiscard]] ResourcePortError lastResourceError() const noexcept {
    return lastResourceError_;
  }

 private:
  [[nodiscard]] bool capturePreviewPresentation(
      interaction::PreviewPresentationCapture& capture) const noexcept;
  [[nodiscard]] bool renderPreviewPresentation(
      const interaction::PreviewPresentationCapture& capture,
      std::uint64_t* presentCount) noexcept;
  [[nodiscard]] bool acknowledgePreviewPresentation(
      const interaction::PreviewPresentationCapture& capture,
      std::uint64_t presentCount) noexcept;
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
  std::unique_ptr<MultiSelectionPort> multiSelectionPort_;
  std::optional<interaction::PreviewPresentationCapture> pendingPreviewCapture_;
  std::uint64_t pendingPreviewPresentCount_ = 0;
  std::uint64_t pendingPreviewWorkId_ = 0;
  std::optional<RuntimeFrameOpportunity::PreviewWork> pendingPreviewIdentity_;
  std::uint64_t nextPreviewWorkId_ = 1;
  std::uint64_t runtimeInstanceId_ = 0;
  std::optional<CanonicalWork> pendingCanonicalWork_;
  std::uint64_t nextCanonicalWorkId_ = 1;
  [[nodiscard]] bool captureCanonicalWork() noexcept;
  std::uint64_t attachmentEpoch_ = 1;
  bool surfaceTransitionActive_ = false;
  [[nodiscard]] bool canAdvanceSurfaceBinding() const noexcept;
  void surfaceBindingChanged() noexcept;
  void refreshControlTarget() noexcept;
};

}  // namespace canvas::runtime
