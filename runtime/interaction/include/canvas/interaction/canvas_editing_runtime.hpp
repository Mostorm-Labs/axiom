#pragma once
namespace canvas::runtime { class CanvasRuntimeFacade; class MultiSelectionPort; }
#include "canvas/interaction/text_edit_session.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"

#include "canvas/input/input_router.hpp"
#include "canvas/input/platform_input_contract.hpp"
#include "canvas/ink/ink_engine.hpp"
#include "canvas/ink/preview_model.hpp"
#include "canvas/ink/brush_session.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/render/brush_render_point.hpp"
#include "canvas/interaction/interaction_runtime.hpp"
#include "canvas/interaction/interaction_dispatch_runtime.hpp"
#include "canvas/interaction/editor_history.hpp"
#include "canvas/interaction/canvas_interaction_coordinator.hpp"
#include "canvas/interaction/viewport_interaction_controller.hpp"
#include "canvas/interaction/viewport_constraints.hpp"
#include "canvas/interaction/selection_session.hpp"
#include "canvas/interaction/region_selection_session.hpp"
#include "canvas/interaction/transform_handle_drag.hpp"
#include "canvas/render/presentation_tracker.hpp"
#include "canvas/render/surface_lifecycle.hpp"
#include "canvas/render/runtime_app_binding.hpp"
#include "canvas/render/runtime_frame_coordinator.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#include "canvas/render/skia_renderer.hpp"
#include "canvas/render/render_view_runtime.hpp"
#include "canvas/render/preview_surface.hpp"
#include "canvas/render/render_backend.hpp"
#include "canvas/interaction/brush_commit_adapter.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/semantic_generation.hpp"
#include "canvas/ink/arc_runtime_sinks.hpp"
#include "canvas/scene/scene.hpp"
#include "canvas/scene/scene_binding.hpp"
#include "canvas/scene/connector_router.hpp"
#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "canvas/text/text_layout.hpp"
#include "canvas/interaction/creation_session.hpp"
#include "canvas/interaction/local_operation_sink.hpp"
#include "canvas/runtime/canvas_control_types.hpp"

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <limits>
#include <string>
#include <string_view>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <functional>

namespace canvas::ink_playground { class InkPlaygroundHistoryTestAccess; }
namespace canvas::render { class ImageResourceResolver; }

namespace canvas::interaction {

enum class SnapshotRestoreOutcome : std::uint8_t { kRestored, kInvalid, kSceneFailed };

struct HudSnapshot final {
  double sampleHz = 0.0;
  std::size_t batch = 0;
  double queueAgeMs = 0.0;
  double inkMs = 0.0;
  std::uint64_t previewRevision = 0;
  std::size_t predictionDepth = 0;
  std::string presentEvidenceKind = "none";
  std::size_t pendingHandoffCount = 0;
  double frameMs = 0.0;
};

struct QualificationObservation final {
  std::uint64_t sceneGeneration = 0;
  std::uint64_t cameraGeneration = 0;
  std::uint64_t candidatesExamined = 0;
  std::uint64_t overlayUpdates = 0;
  std::uint64_t canonicalOperationCount = 0;
  std::uint64_t invalidationRectCount = 0;
  bool fullSceneInvalidation = false;
  double queryMs = 0.0;
  double renderMs = 0.0;
};

struct SurfaceBinding final {
  std::uint64_t generation = 0;
  std::uint64_t metricsGeneration = 0;
  float logicalWidth = 0.0F;
  float logicalHeight = 0.0F;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  bool available = false;
  float devicePixelRatio = 1.0F;
  float displayScale = 1.0F;
};

struct BaselineTraceSample final {
  std::uint64_t sequence = 0;
  std::uint64_t timestampNs = 0;
  float viewX = 0.0F;
  float viewY = 0.0F;
  float contentX = 0.0F;
  float contentY = 0.0F;
  float pressure = 0.0F;
  input::PointerKey key{};
  input::PointerPhase phase = input::PointerPhase::kMove;
  interaction::ContactDisposition disposition = interaction::ContactDisposition::kIgnored;
  bool viewportClaimed = false;
  std::uint64_t downTimestampNs = 0;
  std::uint64_t downTimeDeltaNs = 0;
};

struct BaselineViewportObservation final {
  std::uint64_t sequence = 0;
  bool claimed = false;
  float scale = 1.0F;
  float translationX = 0.0F;
  float translationY = 0.0F;
};

struct PreviewPresentationCapture final {
  render::PreviewGeometry geometry{};
  render::PreviewStyleOverride style{};
  std::uint64_t contentRevision = 0;
  std::uint64_t surfaceGeneration = 0;
};

// Shared editor composition. Existing Semantic/Session/Scene/History/Render
// owners and their lifecycle are composed here; platform Hosts supply resources
// and execution opportunities without implementing tool behavior.
class CanvasEditingRuntime : public interaction::SemanticReadPort,
                                private interaction::TextEditPort,
                                public interaction::SceneQueryPort,
                                public interaction::ViewStatePort,
                                public interaction::OperationSubmitPort,
                                public interaction::HistorySubmitPort,
                                public interaction::TransformSubmitPort,
                                public interaction::TransientPresentationPort,
                                public render::SessionCancelSink,
                                public interaction::InteractionDispatchPort {
  friend class canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  friend class canvas::runtime::CanvasRuntimeFacade;
  friend class canvas::runtime::MultiSelectionPort;
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
  friend class canvas::qualification::Access;
  std::shared_ptr<canvas::qualification::State> fc08Qualification_;
#endif
  public:
  enum class ToolMode : std::uint8_t { kBrush = 0, kObjectEraser = 1, kPartialEraser = 2 };
  explicit CanvasEditingRuntime(render::ViewId viewId = render::ViewId{1});
  virtual ~CanvasEditingRuntime() = default;
  void close() noexcept;
  [[nodiscard]] bool closed() const noexcept { return closed_; }
  [[nodiscard]] bool beginExplicitIntent() noexcept;
  [[nodiscard]] bool prepareExplicitIntent(std::span<const semantic::Operation> operations);
  [[nodiscard]] bool commitExplicitIntent() noexcept;
  void abortExplicitIntent() noexcept;
  [[nodiscard]] bool registerInputSource(input::InputSourceId source,
                                         std::string_view platform) noexcept;
  [[nodiscard]] bool unregisterInputSource(input::InputSourceId source,
                                           std::uint64_t sequence,
                                           std::uint64_t timestampNs) noexcept;
  [[nodiscard]] bool beginStroke(std::uint64_t strokeId) noexcept;
  [[nodiscard]] bool beginStroke(const input::PointerKey& key, std::uint64_t strokeId) noexcept;
  [[nodiscard]] bool accept(const input::PointerSampleBatch& batch,
                            std::uint64_t observationTimeNs);
  [[nodiscard]] bool acceptPlatformBatch(const input::PlatformPointerBatch& batch,
                                         std::uint64_t observationTimeNs);
  [[nodiscard]] bool cancelPlatformPointer(const input::PointerKey& key,
                                           std::uint64_t sequence,
                                           std::uint64_t timestampNs) noexcept;
  [[nodiscard]] bool selectCreationPreset(CreationPreset preset) noexcept;
  [[nodiscard]] bool creationPointer(const input::PointerKey& key, input::PointerPhase phase,
                                     float x, float y) noexcept;
  [[nodiscard]] bool creationActive() const noexcept { return !creations_.empty(); }
  [[nodiscard]] CreationPreset creationPreset() const noexcept { return creationPreset_; }
  [[nodiscard]] const CreationPreview& creationPreview() const noexcept { static const CreationPreview empty{}; const auto it = creations_.find(latestCreationPointer_); return it == creations_.end() ? empty : it->second.preview(); }
  [[nodiscard]] std::optional<input::PointerKey> platformKey(
      input::InputSourceId source, input::PointerId pointer) const noexcept {
      return appBinding_ == nullptr ? std::nullopt : appBinding_->keyFor(source, pointer);
  }
  [[nodiscard]] std::optional<std::uint64_t> platformStrokeId(
      const input::PointerKey& key) const noexcept {
    const auto it = keyedStrokeIds_.find(key);
    return it == keyedStrokeIds_.end() ? std::nullopt
                                       : std::optional<std::uint64_t>(it->second);
  }
  [[nodiscard]] bool commitStroke(std::uint64_t strokeId,
                                  std::uint64_t operationId) noexcept;
  [[nodiscard]] bool commitStroke(const input::PointerKey& key, std::uint64_t strokeId,
                                  std::uint64_t operationId) noexcept;
  // New Brush Engine composition boundary. Platform code may normalize input
  // and present output, but it cannot own BrushRuntime/BrushDefinition state.
  [[nodiscard]] bool beginBrushSession(std::uint64_t pointerId,
                                       std::uint32_t profile = 1U) noexcept;
  [[nodiscard]] bool selectTool(ToolMode mode) noexcept;
  [[nodiscard]] bool setPanTool(bool enabled) noexcept;
  [[nodiscard]] bool panTool() const noexcept { return panTool_; }
  [[nodiscard]] bool setSelectionMode(bool enabled) noexcept;
  [[nodiscard]] bool selectionMode() const noexcept { return selectionMode_; }
  [[nodiscard]] bool setSelectionAcquisitionMode(runtime::CanvasSelectionMode) noexcept;
  [[nodiscard]] runtime::CanvasSelectionMode selectionAcquisitionMode() const noexcept { return selectionAcquisitionMode_; }
  [[nodiscard]] bool regionSelectionActive() const noexcept { return regionSelection_.active(); }
  void cancelRegionSelection() noexcept;
  [[nodiscard]] bool selectAtViewPoint(float x, float y) noexcept;
  [[nodiscard]] runtime::SectionOutcome executeSectionIntent(const runtime::SectionIntent&) noexcept;
  [[nodiscard]] runtime::SectionSnapshot sectionSnapshot(foundation::ObjectId = {}) const;
  [[nodiscard]] std::optional<runtime::SectionOutcome> removeSelectedSections() noexcept;
  [[nodiscard]] bool setSelectedObjectSize(double width, double height) noexcept;
  [[nodiscard]] bool patchSelectedProperty(std::uint32_t fieldId,
                                            semantic::PropertyValue value) noexcept;
  [[nodiscard]] bool deleteSelectedObjects() noexcept;
  [[nodiscard]] bool selectedObjectLocked() const noexcept;
  [[nodiscard]] std::optional<semantic::ObjectRecord> selectedObjectRecord() const;
  [[nodiscard]] bool setSelectedTransform(const semantic::Transform2D&) noexcept;
  [[nodiscard]] bool setSelectedOrder(bool front) noexcept;
  [[nodiscard]] bool setFutureAppearance(const runtime::CanvasAppearance&) noexcept;
  [[nodiscard]] bool setSelectedAppearance(const runtime::CanvasAppearance&) noexcept;
  [[nodiscard]] bool setSelectedStrokeWidth(double width) noexcept;
  [[nodiscard]] runtime::CanvasAppearance futureAppearance() const noexcept { return futureAppearance_; }
  [[nodiscard]] bool creationMode() const noexcept { return creationMode_; }
  [[nodiscard]] bool unlockSelectedObjects() noexcept;
  // Qualification/composition seam: toggle an already hit object without
  // introducing a second selection model or changing production pointer
  // routing semantics.
  [[nodiscard]] bool toggleSelectionAtViewPoint(float x, float y) noexcept;
  [[nodiscard]] bool selectionPointer(const input::PointerKey& key, input::PointerPhase phase,
                                      float x, float y) noexcept;
  [[nodiscard]] bool hoverSelectionAtViewPoint(float x, float y) noexcept;
  [[nodiscard]] bool selectionPointer(std::uint64_t pointer, input::PointerPhase phase,
                                      float x, float y) noexcept;
  void cancelSelectionTransform() noexcept;
  [[nodiscard]] bool selectionTransformActive() const noexcept { return transformDrag_.active() || sectionBoundaryDrag_.has_value(); }
  [[nodiscard]] std::size_t transientTransformCount() const noexcept { return transformOverrides_.size(); }
  void clearSelection() noexcept {
    cancelRegionSelection();
    cancelSelectionTransform();
    selection_.cancel();
    selectionView_.reset();
    snapGuides_.clear();
    canonicalPresentationDirty_ = true;
  }
  [[nodiscard]] std::size_t selectedObjectCount() const noexcept {
    return selection_.selected().size();
  }
  [[nodiscard]] runtime::SelectionSummary selectionSummary() const noexcept;
  [[nodiscard]] foundation::ObjectId selectedPrimaryObject() const noexcept {
    return selection_.primary();
  }
  [[nodiscard]] std::uint64_t selectedPrimaryObjectValue() const noexcept {
    const auto id = selectedPrimaryObject();
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < sizeof(value); ++i) {
      value |= static_cast<std::uint64_t>(id.bytes[i]) << (i * 8U);
    }
    return value;
  }
  [[nodiscard]] bool renderSelectionOverlay() noexcept;
  // Qualification fixture setup uses the existing semantic operation lane;
  // it never mutates the object store directly.
  [[nodiscard]] std::size_t qualificationObjectCount() const noexcept {
    return semanticObjects_.size();
  }
#if defined(AXIOM_RUNTIME_QUALIFICATION)
  [[nodiscard]] const semantic::ObjectRecord* qualificationObjectRecord(
      semantic::ObjectId id) const noexcept { return semanticObjects_.find(id); }
#endif
  [[nodiscard]] std::uint64_t qualificationSceneGeneration() const noexcept {
    return runtimeSceneHost_ == nullptr ? 0U : runtimeSceneHost_->revision().value();
  }
  // Called only by Composition before the initial document is opened.
  [[nodiscard]] bool setInitialDocumentId(semantic::DocumentId id) noexcept {
    if (id.isZero() || closed_ || surface_.available || semanticObjects_.size() != 0U) return false;
    documentId_ = id; return true;
  }
  [[nodiscard]] semantic::DocumentId documentId() const noexcept { return documentId_; }
  // Composition supplies a fresh origin before open, including checkpoint recovery.
  // Standalone qualification owners retain their deterministic numeric fixture IDs.
  [[nodiscard]] bool setInitialOperationNamespace(foundation::ObjectId origin) noexcept {
    if (origin.isZero() || operationNamespaceBound_ || closed_ || surface_.available ||
        semanticObjects_.size() != 0U || submittedOperationCount_ != 0U) return false;
    operationNamespace_ = origin; operationNamespaceBound_ = true; return true;
  }
  [[nodiscard]] bool snapshotCaptureReady() const noexcept { return textReady(); }
  [[nodiscard]] std::vector<semantic::ObjectRecord> snapshotObjects() const { return semanticObjects_.allObjects(); }
  [[nodiscard]] SnapshotRestoreOutcome restoreSnapshotBeforeSurface(const semantic::SemanticSnapshot& snapshot,
      const semantic::SnapshotBootstrapper::CheckpointBinding& binding,
      semantic::ResourceManifest& manifest) noexcept;
  [[nodiscard]] std::uint64_t selectionOverlayUpdates() const noexcept {
    return selectionOverlayUpdates_;
  }
  [[nodiscard]] QualificationObservation qualificationObservation() const noexcept {
    return qualificationObservation_;
  }
  [[nodiscard]] const render::EditingOverlay* selectionOverlay() const noexcept {
    return selectionView_ == nullptr ? nullptr : &selectionView_->editingOverlay();
  }
  // View-only animation, advanced by the Composition frame opportunity.
  // Layout geometry and canonical text remain independent of the blink phase.
  void updateTextCaretPresentation(std::chrono::steady_clock::time_point now) noexcept;
  bool setTextTool(bool value) noexcept {
    if (textTool_ != value) resetTouchTextTap();
    textTool_=value;
    return true;
  }
  bool textTool() const noexcept {return textTool_;}
  [[nodiscard]] bool selectBrushProfile(std::string_view profileId,
                                        std::uint32_t revision = 1U) noexcept;
  [[nodiscard]] bool setInkOptions(float size, float red, float green, float blue,
                                   float alpha, float opacity) noexcept;
  void clearInkSizeOverride() noexcept;
  void clearInkColorOverride() noexcept;
  void clearInkOpacityOverride() noexcept;
  [[nodiscard]] bool inkSizeOverridden() const noexcept { return inkSizeOverride_; }
  [[nodiscard]] bool inkColorOverridden() const noexcept { return inkColorOverride_; }
  [[nodiscard]] bool inkOpacityOverridden() const noexcept { return inkOpacityOverride_; }
  void setInkOverrideState(bool size, bool color, bool opacity) noexcept {
    inkSizeOverride_ = size; inkColorOverride_ = color; inkOpacityOverride_ = opacity;
  }
  [[nodiscard]] bool setEraserDiameterLogicalPx(float diameter) noexcept;
  [[nodiscard]] bool setEraserMode(std::uint32_t mode) noexcept;
  [[nodiscard]] bool setEraserOptions(std::uint32_t mode, float diameter) noexcept;
  [[nodiscard]] std::uint32_t eraserMode() const noexcept { return eraserMode_; }
  struct InkDefaults final { float size, red, green, blue, alpha, opacity; };
  [[nodiscard]] std::optional<InkDefaults> brushCatalogDefaults(
      std::string_view profile, std::uint32_t revision) const noexcept;
  [[nodiscard]] float inkSize() const noexcept { return inkSize_; }
  [[nodiscard]] float inkOpacity() const noexcept { return inkOpacity_; }
  [[nodiscard]] std::array<float, 4> inkColor() const noexcept {
    return {inkRed_, inkGreen_, inkBlue_, inkAlpha_};
  }
  [[nodiscard]] float eraserDiameterLogicalPx() const noexcept {
    return eraserDiameterLogicalPx_;
  }
  [[nodiscard]] bool eraserBegin(std::uint64_t pointerId) noexcept;
  [[nodiscard]] bool eraserSample(std::uint64_t pointerId, double x, double y,
                                  bool renderPreview = true) noexcept;
  [[nodiscard]] bool eraserFinish(std::uint64_t pointerId) noexcept;
  [[nodiscard]] bool eraserCancel(std::uint64_t pointerId) noexcept;
  [[nodiscard]] ToolMode toolMode() const noexcept { return toolMode_; }
  [[nodiscard]] const std::string& selectedBrushProfile() const noexcept {
    return selectedBrushProfile_;
  }
  [[nodiscard]] std::uint32_t selectedBrushRevision() const noexcept {
    return selectedBrushRevision_;
  }
  [[nodiscard]] bool beginBrushSession(std::uint64_t pointerId,
                                       const ink::BrushPackage& package,
                                       std::uint64_t seed) noexcept;
  [[nodiscard]] bool appendBrushSample(std::uint64_t pointerId, double x, double y,
                                       double pressure, std::uint64_t sequence,
                                       bool predicted = false,
                                       bool renderPreview = true) noexcept;
  [[nodiscard]] bool presentBrushPreview() noexcept;
  [[nodiscard]] bool capturePreviewPresentation(
      PreviewPresentationCapture& capture) noexcept;
  [[nodiscard]] bool renderPreviewPresentation(
      const PreviewPresentationCapture& capture,
      std::uint64_t* presentCount = nullptr) noexcept;
  [[nodiscard]] bool acknowledgePreviewPresentation(
      const PreviewPresentationCapture& capture,
      std::uint64_t presentCount) noexcept;
  void setPreviewOverlayVisible(bool visible) noexcept;
  void setPlatformPresentationDeferred(bool deferred) noexcept;
  [[nodiscard]] bool platformPresentationDeferred() const noexcept {
    return platformPresentationDeferred_;
  }
  [[nodiscard]] bool finishBrushSession(std::uint64_t pointerId) noexcept;
  [[nodiscard]] bool cancelBrushSession(std::uint64_t pointerId) noexcept;
  [[nodiscard]] std::vector<render::BrushRenderPoint> brushRenderPoints() const;
  [[nodiscard]] std::size_t brushPrimitiveCount() const noexcept { return committedBrushPoints_.size(); }
  [[nodiscard]] std::uint64_t brushDigest() const noexcept { return brushDigest_; }
  [[nodiscard]] std::uint64_t brushResolvedStateDigest() const noexcept {
    return brushResolvedStateDigest_;
  }
  [[nodiscard]] const std::string& brushPackageDigest() const noexcept { return brushPackageDigest_; }
  [[nodiscard]] std::uint64_t brushPreviewDigest() const noexcept { return brushPreviewDigest_; }
  [[nodiscard]] std::uint64_t brushSealedOutlineDigest() const noexcept {
    return brushSealedOutlineDigest_;
  }
  [[nodiscard]] std::uint64_t brushReplayDigest() const noexcept { return brushReplayDigest_; }
  [[nodiscard]] semantic::SemanticGeneration semanticGeneration() const noexcept {
    return semanticGeneration_.current();
  }
  [[nodiscard]] std::uint64_t sceneRevision() const noexcept {
    return runtimeSceneHost_ == nullptr ? 0U : runtimeSceneHost_->revision().value();
  }
  [[nodiscard]] std::uint64_t canonicalCommitOrdinal() const noexcept {
    return canonicalCommitClock_.lastCommittedOrdinal().value();
  }
  [[nodiscard]] std::size_t semanticObjectCount() const noexcept {
    return semanticObjects_.size();
  }
  void setArcPreviewSink(ink::ArcPreviewSink* sink) noexcept { arcPreviewSink_ = sink; }
  void setCanonicalVisibilitySink(ink::CanonicalVisibilitySink* sink) noexcept {
    canonicalVisibilitySink_ = sink;
  }
  void setLocalOperationSink(LocalOperationSink* sink) noexcept { localOperationSink_ = sink; }
  [[nodiscard]] semantic::CanonicalCommitStamp canonicalCommitFence() const noexcept {
    return {canonicalCommitClock_.runtimeEpoch(), canonicalCommitClock_.lastCommittedOrdinal()};
  }
  [[nodiscard]] semantic::ApplyResult applyExternalOperation(
      const semantic::Operation&, semantic::ApplySource);
  [[nodiscard]] OperationOutcomeSnapshot operationOutcome() const noexcept {
    AxisOperationStatus axis{};
    if (localOperationSink_ != nullptr && lastAppliedCommit_.has_value() &&
        lastAppliedCommit_->source != semantic::ApplySource::kRemoteSync &&
        lastAppliedCommit_->source != semantic::ApplySource::kRestoreReplay) {
      axis = localOperationSink_->status(lastAppliedCommit_->operation_id,
                                         lastAppliedCommit_->commit_stamp);
    }
    if (!surface_.available || lifecycle_ == nullptr ||
        lifecycle_->current().surfaceGeneration != presentedSurfaceGeneration_ ||
        lifecycle_->current().metricsGeneration != presentedMetricsGeneration_) {
      return {lastAppliedCommit_, semantic::SemanticGeneration{}, axis};
    }
    return {lastAppliedCommit_, presentedSemanticGeneration_, axis};
  }
  [[nodiscard]] const std::vector<BaselineTraceSample>& baselineTrace() const noexcept {
    return baselineTrace_;
  }
  [[nodiscard]] const std::vector<BaselineViewportObservation>& baselineViewport() const noexcept {
    return baselineViewport_;
  }
  [[nodiscard]] bool cancelStroke(const input::PointerKey& key) noexcept;
  void cancelAllPointers() noexcept;
  void onSessionCancel(
      const input::PlatformInteractionIngressResult& result) noexcept override;
  [[nodiscard]] interaction::ContactDisposition pointerDisposition(
      const input::PointerKey& key) const noexcept;
  [[nodiscard]] bool viewportGestureClaimed() const noexcept {
    return coordinator_->viewportClaimed();
  }
  [[nodiscard]] const interaction::ViewportGesture& viewportGesture() const noexcept {
    return viewportController_->state();
  }
  [[nodiscard]] bool applyViewportNavigation(
      const interaction::ViewportNavigationSample& sample) noexcept;
  // Set an absolute logical zoom while preserving the content point under a
  // view-space anchor.  This is the owner seam for numeric/anchored UI zoom;
  // gesture navigation continues to use applyViewportNavigation().
  [[nodiscard]] bool setViewportZoomAt(float zoom, float anchorX,
                                       float anchorY) noexcept;
  [[nodiscard]] bool fitViewportToContent(
      const std::optional<foundation::WorldRect>& target = std::nullopt) noexcept;
  [[nodiscard]] bool fitViewportToSelection() noexcept;
  [[nodiscard]] bool fitViewportToObject(foundation::ObjectId objectId) noexcept;
  [[nodiscard]] std::uint64_t cameraGeneration() const noexcept {
    return viewportController_ == nullptr ? 0U : viewportController_->cameraGeneration();
  }
  [[nodiscard]] std::uint64_t snapCandidateCount() const noexcept {
    return snapCandidateCount_;
  }
  [[nodiscard]] std::pair<float, float> viewToContent(float x, float y) const noexcept {
    return viewportController_->viewToContent(x, y);
  }
  [[nodiscard]] interaction::MultiContactPolicy multiContactPolicy() const noexcept {
    return coordinator_->policy();
  }
  [[nodiscard]] bool setMultiContactPolicy(
      interaction::MultiContactPolicy policy) noexcept {
    if (!keyedStrokeIds_.empty()) return false;
    return coordinator_->setPolicy(policy);
  }

  [[nodiscard]] bool bindSurface(std::uint32_t width, std::uint32_t height) noexcept;
  [[nodiscard]] bool bindSurface(const render::SurfaceMetrics& metrics) noexcept;
  [[nodiscard]] bool resizeSurface(std::uint32_t width, std::uint32_t height) noexcept;
  [[nodiscard]] bool resizeSurface(const render::SurfaceMetrics& metrics) noexcept;
  [[nodiscard]] bool stageSurfaceProvider(std::string profileId,
      std::unique_ptr<render::SkiaSurfaceProvider> provider, bool preview) noexcept;
  [[nodiscard]] bool hasStagedSurfaceProviders() const noexcept {
    return pendingCanonicalProvider_.has_value() || pendingPreviewProvider_.has_value();
  }
  [[nodiscard]] bool loseSurface() noexcept;
  [[nodiscard]] bool rebindSurface() noexcept;
  [[nodiscard]] bool rebindSurface(const render::SurfaceMetrics& metrics) noexcept;
  [[nodiscard]] bool adoptPreparedSurface(const render::SurfaceMetrics& metrics) noexcept;
  [[nodiscard]] bool rebindPreviewSurface() noexcept;
  [[nodiscard]] bool registerSurfaceProvider(std::string profileId,
                                              std::unique_ptr<render::SkiaSurfaceProvider> provider) noexcept;
  [[nodiscard]] bool selectCanonicalSurfaceProfile(std::string_view profileId,
                                                    std::uint64_t expectedGeneration,
                                                    render::RenderTargetFormat format =
                                                        render::RenderTargetFormat::kBgra8888) noexcept;
  [[nodiscard]] bool rebindCanonicalSurface(std::uint64_t expectedGeneration) noexcept;
  [[nodiscard]] bool registerPreviewSurfaceProvider(std::string profileId,
                                                    std::unique_ptr<render::SkiaSurfaceProvider> provider) noexcept;
  [[nodiscard]] render::SkiaSurfaceProvider* activeSurfaceProvider() noexcept;
  [[nodiscard]] render::SkiaSurfaceProvider* previewSurfaceProvider() noexcept;
  [[nodiscard]] render::CanonicalViewportTransform previewViewport() const noexcept;
  [[nodiscard]] const render::SurfaceRenderState& previewRenderState() const noexcept;
  [[nodiscard]] std::uint64_t previewSurfaceGeneration() const noexcept;
  [[nodiscard]] std::uint64_t previewSubmissionCount() const noexcept;
  [[nodiscard]] std::uint64_t previewPresentCount() const noexcept;
  [[nodiscard]] std::uint64_t resizeEventCount() const noexcept { return resizeEvents_; }
  [[nodiscard]] std::uint64_t surfaceLostCount() const noexcept { return surfaceLostEvents_; }
  [[nodiscard]] std::uint64_t rebindEventCount() const noexcept { return rebindEvents_; }
  [[nodiscard]] bool previewActive() const noexcept;
  [[nodiscard]] std::size_t pendingCanonicalHandoffCount() const noexcept {
    return pendingCanonicalIdentities_.size();
  }
  [[nodiscard]] bool presentCanonicalFrame(std::uint64_t frameId,
                                           double frameMs,
                                           bool retirePreview = true) noexcept;
  // Platform execution feedback is an explicit boundary.  A renderer/provider
  // submission only proves that work was accepted by the local backend; it
  // cannot qualify a frame as physically presented or retire a preview.  The
  // platform must call this seam with the exact frame and live generations
  // after it has an independent PresentedFeedback observation.
  [[nodiscard]] bool receivePlatformPresented(
      const render::PresentedFeedback& feedback) noexcept;
  [[nodiscard]] std::optional<render::RuntimeFrameIdentity>
      lastSubmittedCanonicalFrame() const noexcept;
  [[nodiscard]] std::uint64_t nextCanonicalFrameId() const noexcept {
    return frameIdHighWater_ == std::numeric_limits<std::uint64_t>::max()
               ? 0U : frameIdHighWater_ + 1U;
  }
  [[nodiscard]] bool presentTransientOverlayFrame(std::uint64_t frameId) noexcept;
  [[nodiscard]] bool retryTransientOverlayFrame() noexcept;
  [[nodiscard]] bool clearTransientOverlay() noexcept;
  [[nodiscard]] bool transientOverlayDirty() const noexcept {
    return transientOverlayDirty_;
  }
  // A deferred platform presenter uses this bit to schedule a Canonical
  // redraw after a viewport-only change.  It is cleared only after the
  // current viewport has been successfully presented.
  [[nodiscard]] bool canonicalPresentationDirty() const noexcept {
    return canonicalPresentationDirty_ || connectorRoutingPending_ || connectorPreviewRoutingPending();
  }
  bool serviceConnectorRouting() noexcept;
  [[nodiscard]] bool connectorPreviewRoutingPending() const noexcept;
  scene::ConnectorGeometry previewConnectorGeometry(const semantic::ObjectRecord&);
  unsigned serviceConnectorPreviewRouting(unsigned quanta);
  [[nodiscard]] bool canonicalPresentationRetryable() const noexcept {
    return canonicalPresentationRetryable_;
  }
  [[nodiscard]] bool previewPresentationRetryable() const noexcept {
    return previewPresentationRetryable_;
  }
  // Owner-side derived resources can invalidate the current pixels without
  // mutating the semantic document or history.
  void markCanonicalPresentationDirty() noexcept { canonicalPresentationDirty_ = true; }
  void cancelConnectorRouting() noexcept;
  void bindResourceServices(render::ImageResourceResolver* images,
                            text::RichTextLayoutService* textService) noexcept;
  void bindResourceManifest(semantic::ResourceManifest* manifest) noexcept {
    resourceManifestPort_ = manifest == nullptr ? &resourceManifest_ : manifest;
  }
  void refreshResourceManifestServices() noexcept;
  [[nodiscard]] bool refreshResourceDerivedState() noexcept;
  [[nodiscard]] std::uint64_t canonicalFrameCount() const noexcept {
    return canonicalFrameCount_;
  }
  [[nodiscard]] const SurfaceBinding& surface() const noexcept { return surface_; }
  [[nodiscard]] std::vector<ink::StrokePoint> previewPoints() const;
  // Current render-ready outline produced by the active BrushSession. This is
  // presentation data only; it never mutates canonical state.
  [[nodiscard]] std::vector<ink::reference::StrokeOutlinePoint> brushPreviewOutline() const;
  [[nodiscard]] std::vector<std::vector<ink::reference::StrokeOutlinePoint>>
      brushPreviewOutlines() const;
  [[nodiscard]] std::vector<ink::StrokePoint> transientPreviewPoints() const;
  [[nodiscard]] std::vector<std::vector<ink::StrokePoint>> previewStrokes() const;
  void setRuntimePreviewVisible(bool visible) noexcept { runtimePreviewVisible_ = visible; }
  [[nodiscard]] bool runtimePreviewVisible() const noexcept { return runtimePreviewVisible_; }
  [[nodiscard]] const std::vector<std::vector<ink::StrokePoint>>& canonicalStrokes() const noexcept {
    return committedStrokes_;
  }

  [[nodiscard]] bool productionPathComplete() const noexcept {
    return input_ != nullptr && ink_ != nullptr && preview_ != nullptr &&
           interaction_ != nullptr && lifecycle_ != nullptr && tracker_ != nullptr;
  }
  [[nodiscard]] const HudSnapshot& hud() const noexcept { return hud_; }
  [[nodiscard]] std::size_t submittedOperationCount() const noexcept {
    return submittedOperationCount_;
  }
  [[nodiscard]] bool canUndo() const noexcept {
    return !closed_ && surface_.available && historySceneReady() && !transformDrag_.active() && !coordinator_->viewportClaimed() &&
           brushSessions_.empty() && eraserTraces_.empty() &&
           keyedStrokeIds_.empty() && history_.canUndo();
  }
  [[nodiscard]] bool canRedo() const noexcept {
    return !closed_ && surface_.available && historySceneReady() && !transformDrag_.active() && !coordinator_->viewportClaimed() &&
           brushSessions_.empty() && eraserTraces_.empty() &&
           keyedStrokeIds_.empty() && history_.canRedo();
  }
  [[nodiscard]] bool undo() noexcept {
    if (!canUndo() || !history_.undo()) return false;
    canonicalPresentationDirty_ = true;
    return true;
  }
  [[nodiscard]] bool redo() noexcept {
    if (!canRedo() || !history_.redo()) return false;
    canonicalPresentationDirty_ = true;
    return true;
  }
  void recordPresentation(std::string evidenceKind, std::size_t pendingHandoffs,
                          double frameMs);

 protected:
  [[nodiscard]] interaction::DispatchMode dispatchMode() const noexcept override;
  [[nodiscard]] bool selection(const input::PointerSample&, std::uint64_t) noexcept override;
  [[nodiscard]] bool creation(const input::PointerSample&, std::uint64_t) noexcept override;
  [[nodiscard]] bool pan(const input::PointerSample&, std::uint64_t) noexcept override;
  [[nodiscard]] bool brush(const input::PointerSample&, std::uint64_t) noexcept override;
  [[nodiscard]] bool brushBatch(
      std::span<const input::PointerSample>, std::uint64_t) noexcept override;
  [[nodiscard]] bool appendBrushSamples(
      std::uint64_t pointerId,
      std::span<const ink::BrushSample> confirmed,
      std::span<const ink::BrushSample> predicted,
      bool renderPreview = true) noexcept;
  [[nodiscard]] bool eraser(const input::PointerSample&, std::uint64_t) noexcept override;
  [[nodiscard]] bool cancel(interaction::DispatchMode, const input::PointerSample&,
                            std::uint64_t) noexcept override;
  [[nodiscard]] bool attached() const noexcept override { return true; }
  [[nodiscard]] bool available() const noexcept override { return true; }
  [[nodiscard]] std::uint64_t generation() const noexcept override { return 1; }
  [[nodiscard]] interaction::SubmitResult submit(
      const interaction::OperationRequest&) override;
  [[nodiscard]] interaction::SubmitResult submit(const semantic::SetTransformsOp&) override;
  [[nodiscard]] semantic::OperationId allocateOperationId() override;
  [[nodiscard]] semantic::Operation legacyStrokeOperation(
      const ink::StrokeRecord&, std::uint64_t operationId) const;
  [[nodiscard]] std::uint64_t localOperationOrdinal() const noexcept;
  [[nodiscard]] semantic::OperationId localOperationIdentity(std::uint64_t ordinal) const noexcept;
  // Public owners share one namespace/counter for every automatically allocated
  // object or RichText identity. Unbound qualification fixtures keep numeric IDs.
  [[nodiscard]] foundation::ObjectId allocateObjectIdentity(foundation::ObjectId legacyIdentity) noexcept;
  [[nodiscard]] bool historySceneReady() const noexcept;
  [[nodiscard]] bool canonicalWriteAdmission() const noexcept;
  [[nodiscard]] bool prepareExplicitIntentInternal(std::span<const semantic::Operation>, bool);
  [[nodiscard]] bool submitHistoryIntent(std::span<const semantic::Operation>,
      std::span<const HistoryExpectation>, EditorHistory::IntentCommit, void*);
  bool textReady() const noexcept override;
  std::vector<std::uint32_t> textGraphemes(const std::u16string&) const override;
  runtime::TextGeometrySnapshot textGeometry(const runtime::CanvasTargetKey&);
  std::optional<std::uint32_t> textHit(const semantic::ObjectRecord&,float,float);
  semantic::ObjectId hitTextAtView(float,float);
  bool enterGroupAtViewPoint(float,float);
  bool textPointer(const input::PointerSample&);
  bool observeTouchTextTap(const input::PointerSample&);
  bool beginShapeTextEditAt(float, float, semantic::ObjectId);
  void resetTouchTextTap() noexcept;
  bool shapeExpandPointer(const input::PointerSample&);
  std::optional<semantic::Transform2D> fittedShapeLabelTransform(const semantic::ObjectRecord&);
  std::optional<semantic::ObjectRecord> textPresentation();
  bool expandShapeLabel(semantic::ObjectId);
  bool textClosed() const noexcept override { return closed_; }
  bool textWritable(semantic::ObjectId) const noexcept override;
  semantic::ObjectId textSelectedObject() const noexcept override { return selectedObjectCount()==1?selection_.primary():semantic::ObjectId{}; }
  semantic::DocumentId textDocument() const noexcept override { return documentId_; }
  std::uint64_t textGeneration() const noexcept override { return semanticGeneration_.current().value(); }
  const semantic::ObjectRecord* textRecord(semantic::ObjectId id) const noexcept override { return semanticObjects_.find(id); }
  std::vector<semantic::ObjectRecord> textObjects() const override { return semanticObjects_.allObjects(); }
  std::string textFontName(semantic::ObjectId) const override;
  const semantic::ResourceManifest* textManifest() const noexcept override { return resourceManifestPort_; }
  semantic::ObjectId allocateTextIdentity() override;
  semantic::OperationId allocateTextOperation() override { return allocateOperationId(); }
  semantic::PrepareResult prepareTextOperation(const semantic::Operation&) const override;
  [[nodiscard]] bool applyResourceManifestOperation(const semantic::Operation&) noexcept;
  bool applyTextOperation(semantic::Operation f,semantic::Operation i) override;
  std::pair<float,float> textContentPoint(float x,float y) const noexcept override { return viewportController_->viewToContent(x,y); }
  void observeApplied(const semantic::Operation&, const semantic::ApplyResult&) noexcept;
  [[nodiscard]] bool applyLocalHistoryOperation(
      semantic::Operation forward, semantic::Operation inverse) noexcept;
  [[nodiscard]] bool recoverHistoryScene() noexcept;
  [[nodiscard]] interaction::SubmitResult submit(
      std::span<const semantic::Operation> operations,
      semantic::ApplySource source) override;
  [[nodiscard]] interaction::SubmitResult submitConditional(
      std::span<const semantic::Operation> operations, semantic::ApplySource source,
      std::span<const interaction::HistoryExpectation> expectations) override;
  [[nodiscard]] interaction::SubmitResult submitAdmitted(const semantic::Operation&,
      semantic::ApplySource, semantic::OperationAdmission&) override;
  void publishAdmitted() noexcept override;
  void cancel(std::uint64_t) noexcept override;

  bool historyPublicationPending_ = false;
  std::unique_ptr<input::InputRouter> input_;
  std::unique_ptr<ink::InkEngine> ink_;
  std::unique_ptr<ink::PreviewModel> preview_;
  std::unique_ptr<interaction::InteractionRuntime> interaction_;
  std::unique_ptr<render::SurfaceLifecycle> lifecycle_;
  std::unique_ptr<render::PresentationTracker> tracker_;
  render::RuntimeFrameCoordinator frameCoordinator_;
  HudSnapshot hud_;
  std::size_t submittedOperationCount_ = 0;
  std::uint64_t strokeStartNs_ = 0;
  SurfaceBinding surface_{};
  std::vector<std::vector<ink::StrokePoint>> committedStrokes_;
  bool runtimePreviewVisible_ = true;
  std::unordered_map<input::PointerKey, std::uint64_t, input::PointerKeyHash> keyedStrokeIds_;
  std::unique_ptr<interaction::CanvasInteractionCoordinator> coordinator_;
  interaction::InteractionDispatchRuntime dispatchRuntime_;
  std::unique_ptr<interaction::ViewportInteractionController> viewportController_;
  std::unique_ptr<render::RuntimeAppBinding> appBinding_;
  std::optional<render::SurfaceProfile> pendingCanonicalProvider_;
  std::optional<render::SurfaceProfile> pendingPreviewProvider_;
  bool surfaceTransitionActive_ = false;
  bool surfaceAbortRequested_ = false;
  [[nodiscard]] bool commitSurfaceBinding(const render::SurfaceMetrics& metrics,
                                           bool restoring) noexcept;
  void publishSurfaceBinding() noexcept;
  render::ViewId viewId_{render::ViewId{1}};
  render::SkiaSurfaceProvider* previewProvider_ = nullptr;
  std::unique_ptr<render::PreviewSurfaceController> previewController_;
  std::unordered_map<std::uint64_t, std::unique_ptr<ink::BrushSession>> brushSessions_;
  ink::BrushPackageCatalog brushCatalog_;
  std::unordered_map<std::uint64_t, ink::BrushPackage> brushSessionPackages_;
  std::unordered_map<std::uint64_t, std::uint64_t> brushSessionSeeds_;
  std::unordered_map<std::uint64_t, std::vector<ink::reference::StrokeOutlinePoint>> brushPreviews_;
  ToolMode toolMode_ = ToolMode::kBrush;
  bool panTool_ = false;
  std::uint32_t eraserMode_ = 2U;
  std::unordered_map<input::PointerKey, std::pair<float, float>, input::PointerKeyHash> panPointers_;
  std::string selectedBrushProfile_ = "vector-solid-v1";
  std::uint32_t selectedBrushRevision_ = 1U;
  float inkSize_ = 16.0F;
  float inkRed_ = 0.05F;
  float inkGreen_ = 0.10F;
  float inkBlue_ = 0.20F;
  float inkAlpha_ = 1.0F;
  float inkOpacity_ = 1.0F;
  bool inkSizeOverride_ = false;
  bool inkColorOverride_ = false;
  bool inkOpacityOverride_ = false;
  float eraserDiameterLogicalPx_ = 18.0F;
  std::unordered_map<std::uint64_t, std::vector<foundation::WorldPoint>> eraserTraces_;
  std::unordered_map<std::uint64_t, std::uint64_t> eraserPreviewRevisions_;
  std::vector<render::BrushRenderPoint> committedBrushPoints_;
  std::uint64_t brushDigest_ = 0;
  std::uint64_t brushResolvedStateDigest_ = 0;
  std::string brushPackageDigest_;
  std::uint64_t brushPreviewDigest_ = 0;
  mutable std::recursive_mutex previewStateMutex_;
  std::uint64_t brushSealedOutlineDigest_ = 0;
  std::uint64_t brushReplayDigest_ = 0;
  std::vector<BaselineTraceSample> baselineTrace_;
  std::vector<BaselineViewportObservation> baselineViewport_;
  std::unordered_map<input::PointerKey, std::uint64_t, input::PointerKeyHash>
      baselineDownTimestamps_;

  struct PendingBrushCommit final {
    ink::BrushCommitIntent intent{};
    ink::BrushPackage package{};
  };
  semantic::IndexedObjectStore semanticObjects_;
  semantic::AppliedOperationLedger appliedOperations_;
  semantic::SemanticGenerationState semanticGeneration_{semantic::SemanticGeneration{0}};
  semantic::CanonicalCommitClock canonicalCommitClock_{semantic::RuntimeEpoch{1}};
  semantic::OperationEngine operationEngine_;
  semantic::ResourceManifest resourceManifest_{};
  semantic::ResourceManifest* resourceManifestPort_ = &resourceManifest_;
  semantic::DocumentId documentId_{canvas::foundation::ObjectId::fromUint64(1U)};
  std::uint64_t nextHistoryOperationOrdinal_ = (std::uint64_t{1} << 63U);
  foundation::ObjectId operationNamespace_{};
  bool operationNamespaceBound_ = false;
  std::uint64_t nextObjectIdentityOrdinal_ = 1;
    std::uint64_t nextTextIdentity_ = 1;
  bool textTool_=false;
  runtime::TextSessionToken caretToken_{};
  runtime::TextRange caretSelection_{};
  std::optional<runtime::TextRange> caretPreeditSelection_;
  runtime::TextPhase caretPhase_ = runtime::TextPhase::kClosed;
  std::uint64_t caretInputSequence_ = 0;
  std::chrono::steady_clock::time_point caretBlinkStart_{};
  bool textCaretVisible_ = true;
  input::PointerKey textSelectionPointer_{};
  std::uint32_t textSelectionAnchor_=0;
  std::unordered_set<input::PointerKey,input::PointerKeyHash> textPointerKeys_;
  struct TouchTapContact final {
    semantic::ObjectId object{};
    float x = 0.0F, y = 0.0F;
    std::uint64_t timestampNs = 0;
    bool moved = false;
  };
  struct CompletedTouchTap final {
    input::PointerKey key{};
    semantic::ObjectId object{};
    float x = 0.0F, y = 0.0F;
    std::uint64_t timestampNs = 0;
  };
  std::unordered_map<input::PointerKey, TouchTapContact, input::PointerKeyHash>
      touchTapContacts_;
  std::optional<CompletedTouchTap> previousTouchTap_;
  bool touchTapMultiContact_ = false;
  std::unordered_set<input::PointerKey,input::PointerKeyHash> shapeExpandPointerKeys_;
  std::unordered_map<std::uint64_t, PendingBrushCommit> pendingBrushCommits_;
  ink::ArcPreviewSink* arcPreviewSink_ = nullptr;
  ink::CanonicalVisibilitySink* canonicalVisibilitySink_ = nullptr;
  LocalOperationSink* localOperationSink_ = nullptr;
  std::optional<semantic::CanonicalCommitRecord> lastAppliedCommit_;
  bool sectionDefersPublication_ = false;
  struct SectionBoundaryDrag final {
    foundation::ObjectId object{};
    semantic::SectionContent initial{},preview{};
    semantic::Transform2D world{};
    std::uint64_t generation=0,camera=0,metrics=0;
    bool valid=true;
  };
  std::optional<SectionBoundaryDrag> sectionBoundaryDrag_;
  bool sectionBoundaryPointer(const input::PointerKey&,input::PointerPhase,float,float) noexcept;
  std::optional<interaction::SubmitResult> tryCommitSectionDrop(const semantic::SetTransformsOp&) noexcept;
  runtime::SectionOutcome executeSectionCommand(const runtime::SectionIntent&,const semantic::SetTransformsOp* confirmedDrop) noexcept;
  void applyContainerPreview(render::ReferenceDrawList&);
  semantic::SemanticGeneration presentedSemanticGeneration_{};
  render::SurfaceGeneration presentedSurfaceGeneration_{};
  render::MetricsGeneration presentedMetricsGeneration_{};
  std::unique_ptr<canvas::Scene> runtimeSceneHost_;
  std::unique_ptr<canvas::SceneBinding> sceneBinding_;
  std::unique_ptr<canvas::IncrementalRuntimeCoordinator> sceneCoordinator_;
  std::unique_ptr<canvas::ISemanticSceneCompiler> sceneCompiler_;
  text::RichTextLayoutService* textLayoutService_ = nullptr;
  std::unique_ptr<render::RenderViewRuntime> selectionView_;
  // Owner-controlled presentation override; never part of Semantic or Scene.
  std::optional<semantic::ObjectRecord> imageCropPresentation_;
  std::function<void(const input::PointerSample&)> imageCropPointer_;
  interaction::EditorHistory history_;
  std::unique_ptr<semantic::PreparedOperationIntent> explicitIntent_;
  std::optional<interaction::EditorHistory::PreparedEntry> explicitIntentHistory_;
  bool explicitIntentActive_ = false;
  render::SkiaRenderer skiaRenderer_;
  render::SkiaRenderer previewRenderer_;
  mutable std::mutex previewRenderMutex_;
  mutable std::mutex previewProviderMutex_;
  std::uint64_t canonicalFrameCount_ = 0;
  std::uint64_t frameIdHighWater_ = 0;
  std::optional<render::RuntimeFrameIdentity> lastSubmittedCanonicalFrame_;
  std::unique_ptr<render::FrameState> lastSubmittedCanonicalFrameState_;
  std::optional<render::RuntimeFrameIdentity> lastPresentedCanonicalFrame_;
  std::unique_ptr<render::FrameState> lastPresentedCanonicalFrameState_;
  bool canonicalPresentationDirty_ = false;
  bool connectorRoutingPending_ = false;
  semantic::ConnectorRouting connectorRoutingPreset_ = semantic::ConnectorRouting::kStraight;
  std::uint64_t connectorRoutingGeneration_ = std::numeric_limits<std::uint64_t>::max();
  std::vector<semantic::ObjectRecord> connectorRoutingSource_;
  std::vector<std::size_t> connectorRoutingQueue_;
  std::size_t connectorRoutingCursor_ = 0;
  std::unique_ptr<scene::ConnectorRouteJob> connectorRoutingJob_;
  struct ConnectorPreviewRouting;
  std::shared_ptr<ConnectorPreviewRouting> connectorPreviewRouting_;
  bool transientOverlayDirty_ = false;
  // A completed creation/selection gesture keeps its last transient frame
  // visible until the replacement canonical frame is accepted. This avoids
  // exposing the old canonical surface for one compositor refresh.
  bool transientOverlayHandoffPending_ = false;
  bool platformPresentationDeferred_ = false;
  bool canonicalPresentationRetryable_ = false;
  bool previewPresentationRetryable_ = false;
  bool closed_ = false;
  std::uint64_t resizeEvents_ = 0;
  std::uint64_t surfaceLostEvents_ = 0;
  std::uint64_t rebindEvents_ = 0;
  std::optional<ink::CanonicalHandoffIdentity> lastCanonicalIdentity_;
  std::unordered_map<std::uint64_t, ink::CanonicalHandoffIdentity>
      pendingCanonicalIdentities_;
  interaction::SelectionSession selection_;
  interaction::RegionSelectionSession regionSelection_;
  runtime::CanvasSelectionMode selectionAcquisitionMode_=runtime::CanvasSelectionMode::kPoint;
  runtime::SelectionQueryStatus regionQueryStatus_=runtime::SelectionQueryStatus::kIdle;
  std::vector<foundation::ObjectId> regionCandidates_;
  std::vector<std::uint32_t> regionUnsupportedKinds_;
  std::unordered_set<input::PointerKey,input::PointerKeyHash> acquisitionClickPointers_;
  std::uint64_t regionBinding_=1;
  struct RegionQueryResult final {
    runtime::SelectionQueryStatus status=runtime::SelectionQueryStatus::kFailed;
    std::vector<foundation::ObjectId> roots;
    std::vector<std::uint32_t> unsupportedKinds;
  };
  [[nodiscard]] RegionSelectionContext regionContext() const noexcept;
  [[nodiscard]] foundation::Result<HitTestResult> selectionPointHits(foundation::WorldPoint) const;
  [[nodiscard]] RegionQueryResult querySelectionRegion(std::span<const RegionPoint>) const;
  [[nodiscard]] bool regionSelectionPointer(const input::PointerSample&) noexcept;
  [[nodiscard]] foundation::ObjectId resolveSelectionRoot(foundation::ObjectId) const;
  [[nodiscard]] bool publishSelectionAcquisition(std::span<const foundation::ObjectId>,
      SelectionCombineMode, std::uint64_t expectedRevision);
  [[nodiscard]] bool acquisitionPointer(const input::PointerSample&) noexcept;
  [[nodiscard]] bool renderAcquisitionOverlay() noexcept;
  void appendGroupScopeOutline(render::EditingOverlay&) noexcept;
  [[nodiscard]] bool singleSelectionAffordances() const noexcept {
    return selectedObjectCount()==1U && !regionSelectionActive() && !panTool_ &&
        (!selectionMode_||selectionAcquisitionMode_==runtime::CanvasSelectionMode::kPoint);
  }
  bool selectionMode_ = false;
  // Explicit local permission is bound by the owning composition; unknown is denied.
  bool multiSelectionPermissionAllowed_ = false;
  std::uint64_t multiSelectionPermissionRevision_ = 1;
  std::uint64_t editingPermissionRevision_ = 0;
  std::optional<foundation::ObjectId> pendingMultiClick_;
  std::vector<std::pair<foundation::ObjectId, semantic::Transform2D>> editingMultiInitial_;
  interaction::TransientSceneOverride transformOverrides_;
  interaction::TransformHandleDrag transformDrag_{*this, transformOverrides_};
  input::PointerKey editingPointer_{};
  render::HandleKind editingHandle_ = render::HandleKind::kNone;
  foundation::WorldRect editingLocalBounds_{};
  semantic::Transform2D editingInitial_{};
  foundation::WorldPoint editingDownWorld_{};
  semantic::SemanticGeneration editingGeneration_{};
  std::uint64_t editingCameraGeneration_=0,editingMetricsGeneration_=0,editingSurfaceGeneration_=0,editingSelectionRevision_=0;
  foundation::WorldPoint editingDropWorld_{};
  bool editingChanged_ = false;
  interaction::SnapResolver snapResolver_{};
  std::uint64_t snapCandidateCount_ = 0;
  std::vector<render::SnapGuideGeometry> snapGuides_;
  std::uint64_t selectionOverlayUpdates_ = 0;
  QualificationObservation qualificationObservation_{};
  std::unordered_map<input::PointerKey, CreationSession, input::PointerKeyHash> creations_;
  std::unordered_set<input::PointerKey, input::PointerKeyHash> connectorPointerKeys_;
  struct CreationFence final {
    semantic::SemanticGeneration semantic{};
    std::uint64_t camera = 0;
  };
  std::unordered_map<input::PointerKey, CreationFence, input::PointerKeyHash> creationFences_;
  input::PointerKey latestCreationPointer_{};
  CreationPreset creationPreset_ = CreationPreset::kRectangle;
  runtime::CanvasAppearance futureAppearance_{};
  std::uint64_t creationSerial_ = 0;
  bool creationMode_ = false;
  std::thread::id ownerThread_{};
  bool publicationCallbackActive_ = false;
};

}  // namespace canvas::interaction
