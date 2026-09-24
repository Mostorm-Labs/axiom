#pragma once

#include "canvas/input/input_router.hpp"
#include "canvas/input/platform_input_contract.hpp"
#include "canvas/ink/ink_engine.hpp"
#include "canvas/ink/preview_model.hpp"
#include "canvas/ink/brush_session.hpp"
#include "canvas/render/brush_render_point.hpp"
#include "canvas/interaction/interaction_runtime.hpp"
#include "canvas/interaction/canvas_interaction_coordinator.hpp"
#include "canvas/interaction/viewport_interaction_controller.hpp"
#include "canvas/render/presentation_tracker.hpp"
#include "canvas/render/surface_lifecycle.hpp"
#include "canvas/render/render_backend.hpp"
#include "platform_interaction_controller.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <memory>
#include <optional>
#include <unordered_map>
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
    return platformController_.keyFor(source, pointer);
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
  [[nodiscard]] bool appendBrushSample(std::uint64_t pointerId, double x, double y,
                                       double pressure, std::uint64_t sequence,
                                       bool predicted = false) noexcept;
  [[nodiscard]] bool finishBrushSession(std::uint64_t pointerId) noexcept;
  [[nodiscard]] bool cancelBrushSession(std::uint64_t pointerId) noexcept;
  [[nodiscard]] std::vector<render::BrushRenderPoint> brushRenderPoints() const;
  [[nodiscard]] std::size_t brushPrimitiveCount() const noexcept { return committedBrushPoints_.size(); }
  [[nodiscard]] std::uint64_t brushDigest() const noexcept { return brushDigest_; }
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
  [[nodiscard]] bool presentCanonicalFrame(std::uint64_t frameId,
                                           double frameMs) noexcept;
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
  PlatformInteractionController platformController_;
  std::uint64_t nextPlatformStrokeId_ = 1;
  std::unordered_map<std::uint64_t, std::unique_ptr<ink::BrushSession>> brushSessions_;
  std::unordered_map<std::uint64_t, std::vector<ink::reference::StrokeOutlinePoint>> brushPreviews_;
  std::vector<render::BrushRenderPoint> committedBrushPoints_;
  std::uint64_t brushDigest_ = 0;
};

}  // namespace canvas::ink_playground
