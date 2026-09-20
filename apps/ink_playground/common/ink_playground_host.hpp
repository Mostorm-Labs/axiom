#pragma once

#include "canvas/input/input_router.hpp"
#include "canvas/ink/ink_engine.hpp"
#include "canvas/ink/preview_model.hpp"
#include "canvas/interaction/interaction_runtime.hpp"
#include "canvas/render/presentation_tracker.hpp"
#include "canvas/render/surface_lifecycle.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <memory>

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
  [[nodiscard]] bool accept(const input::PointerSampleBatch& batch,
                            std::uint64_t observationTimeNs);
  [[nodiscard]] bool commitStroke(std::uint64_t strokeId,
                                  std::uint64_t operationId) noexcept;

  [[nodiscard]] bool bindSurface(std::uint32_t width, std::uint32_t height) noexcept;
  [[nodiscard]] bool resizeSurface(std::uint32_t width, std::uint32_t height) noexcept;
  [[nodiscard]] bool loseSurface() noexcept;
  [[nodiscard]] const SurfaceBinding& surface() const noexcept { return surface_; }
  [[nodiscard]] std::vector<ink::StrokePoint> previewPoints() const;
  [[nodiscard]] std::vector<std::vector<ink::StrokePoint>> previewStrokes() const;
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
};

}  // namespace canvas::ink_playground
