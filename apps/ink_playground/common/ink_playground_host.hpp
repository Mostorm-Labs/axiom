#pragma once

#include "canvas/input/input_router.hpp"
#include "canvas/input/platform_input_contract.hpp"
#include "canvas/ink/ink_engine.hpp"
#include "canvas/ink/preview_model.hpp"
#include "canvas/ink/brush_session.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/render/brush_render_point.hpp"
#include "canvas/interaction/interaction_runtime.hpp"
#include "canvas/interaction/editor_history.hpp"
#include "canvas/interaction/canvas_interaction_coordinator.hpp"
#include "canvas/interaction/viewport_interaction_controller.hpp"
#include "canvas/interaction/viewport_constraints.hpp"
#include "canvas/interaction/selection_session.hpp"
#include "canvas/interaction/transform_handle_drag.hpp"
#include "canvas/render/presentation_tracker.hpp"
#include "canvas/render/surface_lifecycle.hpp"
#include "canvas/render/runtime_app_binding.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#include "canvas/render/skia_renderer.hpp"
#include "canvas/render/render_view_runtime.hpp"
#include "canvas/render/preview_surface.hpp"
#include "canvas/render/render_backend.hpp"
#include "platform_interaction_controller.hpp"
#include "brush_commit_adapter.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/canonical_commit_clock.hpp"
#include "canvas/semantic/semantic_generation.hpp"
#include "canvas/ink/arc_runtime_sinks.hpp"
#include "canvas/scene/scene.hpp"
#include "canvas/scene/scene_binding.hpp"
#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include "canvas/text/text_layout.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace canvas::ink_playground {

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
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  bool available = false;
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

// Application composition root. It owns no canonical document state: the
// production runtime modules remain the owners of input, ink, interaction,
// render and presentation semantics.
class InkPlaygroundHost final : public interaction::SemanticReadPort,
                                public interaction::SceneQueryPort,
                                public interaction::ViewStatePort,
                                public interaction::OperationSubmitPort,
                                public interaction::HistorySubmitPort,
                                public interaction::TransformSubmitPort,
                                public interaction::TransientPresentationPort {
  friend class InkPlaygroundHistoryTestAccess;
  public:
  enum class ToolMode : std::uint8_t { kBrush = 0, kObjectEraser = 1, kPartialEraser = 2 };
  InkPlaygroundHost();
  [[nodiscard]] bool configureTextResources(std::vector<std::uint8_t>,std::vector<std::uint8_t>,text::LayoutContext);
  [[nodiscard]] bool seedTextScenario(std::string_view,std::size_t);
  [[nodiscard]] bool editTextScenario(std::size_t);
  [[nodiscard]] bool applyTextScenarioEdit(std::size_t,std::string_view);
  [[nodiscard]] bool transformTextScenario(std::size_t);
  [[nodiscard]] bool configureBundledTextResources();
  [[nodiscard]] std::string textQualificationJson() const;
  [[nodiscard]] bool setTextFontsAvailable(bool);
  [[nodiscard]] text::LayoutMetrics textLayoutMetrics() const noexcept;

  [[nodiscard]] bool beginStroke(std::uint64_t strokeId) noexcept;
  [[nodiscard]] bool beginStroke(const input::PointerKey& key, std::uint64_t strokeId) noexcept;
  [[nodiscard]] bool accept(const input::PointerSampleBatch& batch,
                            std::uint64_t observationTimeNs);
  [[nodiscard]] bool acceptPlatformBatch(const input::PlatformPointerBatch& batch,
                                         std::uint64_t observationTimeNs);
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
  [[nodiscard]] bool selectAtViewPoint(float x, float y) noexcept;
  // Qualification/composition seam: toggle an already hit object without
  // introducing a second selection model or changing production pointer
  // routing semantics.
  [[nodiscard]] bool toggleSelectionAtViewPoint(float x, float y) noexcept;
  [[nodiscard]] bool selectionPointer(std::uint64_t pointer, input::PointerPhase phase,
                                      float x, float y) noexcept;
  void cancelSelectionTransform() noexcept;
  [[nodiscard]] bool selectionTransformActive() const noexcept { return transformDrag_.active(); }
  [[nodiscard]] std::size_t transientTransformCount() const noexcept { return transformOverrides_.size(); }
  void clearSelection() noexcept {
    cancelSelectionTransform();
    selection_.cancel();
    selectionView_.reset();
    snapGuides_.clear();
    canonicalPresentationDirty_ = true;
  }
  [[nodiscard]] std::size_t selectedObjectCount() const noexcept {
    return selection_.summary().count;
  }
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
  [[nodiscard]] bool seedQualificationFixture(std::size_t objectCount) noexcept;
  [[nodiscard]] std::size_t qualificationObjectCount() const noexcept {
    return semanticObjects_.size();
  }
  [[nodiscard]] std::uint64_t qualificationSceneGeneration() const noexcept {
    return runtimeSceneHost_ == nullptr ? 0U : runtimeSceneHost_->revision().value();
  }
  [[nodiscard]] std::uint64_t selectionOverlayUpdates() const noexcept {
    return selectionOverlayUpdates_;
  }
  [[nodiscard]] QualificationObservation qualificationObservation() const noexcept {
    return qualificationObservation_;
  }
  [[nodiscard]] const render::EditingOverlay* selectionOverlay() const noexcept {
    return selectionView_ == nullptr ? nullptr : &selectionView_->editingOverlay();
  }
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
  [[nodiscard]] bool eraserSample(std::uint64_t pointerId, double x, double y) noexcept;
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
  [[nodiscard]] const std::vector<BaselineTraceSample>& baselineTrace() const noexcept {
    return baselineTrace_;
  }
  [[nodiscard]] const std::vector<BaselineViewportObservation>& baselineViewport() const noexcept {
    return baselineViewport_;
  }
  [[nodiscard]] bool cancelStroke(const input::PointerKey& key) noexcept;
  void cancelAllPointers() noexcept;
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
  [[nodiscard]] bool resizeSurface(std::uint32_t width, std::uint32_t height) noexcept;
  [[nodiscard]] bool loseSurface() noexcept;
  [[nodiscard]] bool rebindSurface() noexcept;
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
  // A deferred platform presenter uses this bit to schedule a Canonical
  // redraw after a viewport-only change.  It is cleared only after the
  // current viewport has been successfully presented.
  [[nodiscard]] bool canonicalPresentationDirty() const noexcept {
    return canonicalPresentationDirty_;
  }
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
    return historySceneReady() && !transformDrag_.active() && !coordinator_->viewportClaimed() &&
           brushSessions_.empty() && eraserTraces_.empty() &&
           keyedStrokeIds_.empty() && history_.canUndo();
  }
  [[nodiscard]] bool canRedo() const noexcept {
    return historySceneReady() && !transformDrag_.active() && !coordinator_->viewportClaimed() &&
           brushSessions_.empty() && eraserTraces_.empty() &&
           keyedStrokeIds_.empty() && history_.canRedo();
  }
  [[nodiscard]] bool undo() noexcept {
    return canUndo() && history_.undo();
  }
  [[nodiscard]] bool redo() noexcept {
    return canRedo() && history_.redo();
  }
  void recordPresentation(std::string evidenceKind, std::size_t pendingHandoffs,
                          double frameMs);

 private:
  [[nodiscard]] bool attached() const noexcept override { return true; }
  [[nodiscard]] bool available() const noexcept override { return true; }
  [[nodiscard]] std::uint64_t generation() const noexcept override { return 1; }
  [[nodiscard]] interaction::SubmitResult submit(
      const interaction::OperationRequest&) override;
  [[nodiscard]] interaction::SubmitResult submit(const semantic::SetTransformsOp&) override;
  [[nodiscard]] semantic::OperationId allocateOperationId() override;
  [[nodiscard]] std::uint64_t localOperationOrdinal() const noexcept;
  [[nodiscard]] bool historySceneReady() const noexcept;
  [[nodiscard]] bool recoverHistoryScene() noexcept;
  [[nodiscard]] interaction::SubmitResult submit(
      std::span<const semantic::Operation> operations,
      semantic::ApplySource source) override;
  void cancel(std::uint64_t) noexcept override;

  std::unique_ptr<input::InputRouter> input_;
  std::unique_ptr<ink::InkEngine> ink_;
  std::unique_ptr<ink::PreviewModel> preview_;
  std::unique_ptr<interaction::InteractionRuntime> interaction_;
  std::unique_ptr<render::SurfaceLifecycle> lifecycle_;
  std::unique_ptr<render::PresentationTracker> tracker_;
  HudSnapshot hud_;
  std::size_t submittedOperationCount_ = 0;
  std::uint64_t strokeStartNs_ = 0;
  SurfaceBinding surface_{};
  std::vector<std::vector<ink::StrokePoint>> committedStrokes_;
  bool runtimePreviewVisible_ = true;
  std::unordered_map<input::PointerKey, std::uint64_t, input::PointerKeyHash> keyedStrokeIds_;
  std::unique_ptr<interaction::CanvasInteractionCoordinator> coordinator_;
  std::unique_ptr<interaction::ViewportInteractionController> viewportController_;
  std::unique_ptr<render::RuntimeAppBinding> appBinding_;
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
  std::unordered_map<std::uint64_t, std::pair<float, float>> panPointers_;
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
  semantic::DocumentId documentId_{canvas::foundation::ObjectId::fromUint64(1U)};
  std::uint64_t nextHistoryOperationOrdinal_ = (std::uint64_t{1} << 63U);
  std::unordered_map<std::uint64_t, PendingBrushCommit> pendingBrushCommits_;
  ink::ArcPreviewSink* arcPreviewSink_ = nullptr;
  ink::CanonicalVisibilitySink* canonicalVisibilitySink_ = nullptr;
  std::unique_ptr<canvas::Scene> runtimeSceneHost_;
  std::unique_ptr<canvas::SceneBinding> sceneBinding_;
  std::unique_ptr<canvas::IncrementalRuntimeCoordinator> sceneCoordinator_;
  std::unique_ptr<canvas::ISemanticSceneCompiler> sceneCompiler_;
  runtime::MemoryResourceProvider textResources_;
  std::unique_ptr<text::RichTextLayoutService> textLayoutService_;
  std::vector<std::uint8_t> latinTextFont_,cjkTextFont_;
  std::vector<foundation::ObjectId> textScenarioObjects_;
  std::vector<foundation::ObjectId> textScenarioFixtureObjects_;
  std::string textScenarioName_;
  std::uint64_t textScenarioSerial_ = 0;
  bool submitTextScenarioOperation(semantic::OperationPayload);
  std::unique_ptr<render::RenderViewRuntime> selectionView_;
  interaction::EditorHistory history_;
  render::SkiaRenderer skiaRenderer_;
  render::SkiaRenderer previewRenderer_;
  mutable std::mutex previewRenderMutex_;
  mutable std::mutex previewProviderMutex_;
  std::uint64_t canonicalFrameCount_ = 0;
  bool canonicalPresentationDirty_ = false;
  bool platformPresentationDeferred_ = false;
  std::uint64_t resizeEvents_ = 0;
  std::uint64_t surfaceLostEvents_ = 0;
  std::uint64_t rebindEvents_ = 0;
  std::optional<ink::CanonicalHandoffIdentity> lastCanonicalIdentity_;
  std::unordered_map<std::uint64_t, ink::CanonicalHandoffIdentity>
      pendingCanonicalIdentities_;
  interaction::SelectionSession selection_;
  bool selectionMode_ = false;
  interaction::TransientSceneOverride transformOverrides_;
  interaction::TransformHandleDrag transformDrag_{*this, transformOverrides_};
  std::uint64_t editingPointer_ = 0;
  render::HandleKind editingHandle_ = render::HandleKind::kNone;
  foundation::WorldRect editingLocalBounds_{};
  semantic::Transform2D editingInitial_{};
  foundation::WorldPoint editingDownWorld_{};
  semantic::SemanticGeneration editingGeneration_{};
  bool editingChanged_ = false;
  interaction::SnapResolver snapResolver_{};
  std::uint64_t snapCandidateCount_ = 0;
  std::vector<render::SnapGuideGeometry> snapGuides_;
  std::uint64_t selectionOverlayUpdates_ = 0;
  QualificationObservation qualificationObservation_{};
};

}  // namespace canvas::ink_playground
