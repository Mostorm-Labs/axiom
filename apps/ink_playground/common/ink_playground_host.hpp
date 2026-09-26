#pragma once

#include "canvas/input/input_router.hpp"
#include "canvas/input/platform_input_contract.hpp"
#include "canvas/ink/ink_engine.hpp"
#include "canvas/ink/preview_model.hpp"
#include "canvas/ink/brush_session.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/render/brush_render_point.hpp"
#include "canvas/interaction/interaction_runtime.hpp"
#include "canvas/interaction/canvas_interaction_coordinator.hpp"
#include "canvas/interaction/viewport_interaction_controller.hpp"
#include "canvas/render/presentation_tracker.hpp"
#include "canvas/render/surface_lifecycle.hpp"
#include "canvas/render/runtime_app_binding.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#include "canvas/render/skia_renderer.hpp"
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

#include <cstddef>
#include <cstdint>
#include <string>
#include <memory>
#include <optional>
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

// Application composition root. It owns no canonical document state: the
// production runtime modules remain the owners of input, ink, interaction,
// render and presentation semantics.
class InkPlaygroundHost final : public interaction::SemanticReadPort,
                                public interaction::SceneQueryPort,
                                public interaction::ViewStatePort,
                                public interaction::OperationSubmitPort,
                                public interaction::TransientPresentationPort {
  public:
  InkPlaygroundHost();

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
  [[nodiscard]] bool beginBrushSession(std::uint64_t pointerId,
                                       const ink::BrushPackage& package,
                                       std::uint64_t seed) noexcept;
  [[nodiscard]] bool appendBrushSample(std::uint64_t pointerId, double x, double y,
                                       double pressure, std::uint64_t sequence,
                                       bool predicted = false,
                                       bool renderPreview = true) noexcept;
  [[nodiscard]] bool presentBrushPreview() noexcept;
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
    return lastCanonicalIdentity_.has_value()
        ? lastCanonicalIdentity_->commit.ordinal.value() : 0U;
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
  [[nodiscard]] bool presentCanonicalFrame(std::uint64_t frameId,
                                           double frameMs,
                                           bool retirePreview = true) noexcept;
  [[nodiscard]] std::uint64_t canonicalFrameCount() const noexcept {
    return canonicalFrameCount_;
  }
  [[nodiscard]] const SurfaceBinding& surface() const noexcept { return surface_; }
  [[nodiscard]] std::vector<ink::StrokePoint> previewPoints() const;
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
  void recordPresentation(std::string evidenceKind, std::size_t pendingHandoffs,
                          double frameMs);

 private:
  [[nodiscard]] bool attached() const noexcept override { return true; }
  [[nodiscard]] bool available() const noexcept override { return true; }
  [[nodiscard]] std::uint64_t generation() const noexcept override { return 1; }
  [[nodiscard]] interaction::SubmitResult submit(
      const interaction::OperationRequest&) override;
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
  std::vector<render::BrushRenderPoint> committedBrushPoints_;
  std::uint64_t brushDigest_ = 0;
  std::uint64_t brushResolvedStateDigest_ = 0;
  std::string brushPackageDigest_;
  std::uint64_t brushPreviewDigest_ = 0;
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
  std::unordered_map<std::uint64_t, PendingBrushCommit> pendingBrushCommits_;
  ink::ArcPreviewSink* arcPreviewSink_ = nullptr;
  ink::CanonicalVisibilitySink* canonicalVisibilitySink_ = nullptr;
  std::unique_ptr<canvas::Scene> runtimeSceneHost_;
  std::unique_ptr<canvas::SceneBinding> sceneBinding_;
  std::unique_ptr<canvas::IncrementalRuntimeCoordinator> sceneCoordinator_;
  std::unique_ptr<canvas::ISemanticSceneCompiler> sceneCompiler_;
  render::SkiaRenderer skiaRenderer_;
  std::uint64_t canonicalFrameCount_ = 0;
  std::uint64_t resizeEvents_ = 0;
  std::uint64_t surfaceLostEvents_ = 0;
  std::uint64_t rebindEvents_ = 0;
  std::optional<ink::CanonicalHandoffIdentity> lastCanonicalIdentity_;
  std::unordered_map<std::uint64_t, ink::CanonicalHandoffIdentity>
      pendingCanonicalIdentities_;
};

}  // namespace canvas::ink_playground
