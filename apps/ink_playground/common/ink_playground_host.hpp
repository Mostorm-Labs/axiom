#pragma once

#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "creation_session.hpp"
#include "local_operation_sink.hpp"

namespace canvas::ink_playground {
using interaction::HudSnapshot;
using interaction::QualificationObservation;
using interaction::SurfaceBinding;
using interaction::BaselineTraceSample;
using interaction::BaselineViewportObservation;
using interaction::PreviewPresentationCapture;

// Playground-only resources and qualification scenarios. All editing behavior
// and mutable tool state live in CanvasEditingRuntime, also used by other Hosts.
class InkPlaygroundHost final : public interaction::CanvasEditingRuntime {
 public:
  [[nodiscard]] bool configureTextResources(std::vector<std::uint8_t>,std::vector<std::uint8_t>,text::LayoutContext);
  [[nodiscard]] bool seedTextScenario(std::string_view,std::size_t);
  [[nodiscard]] bool editTextScenario(std::size_t);
  [[nodiscard]] bool applyTextScenarioEdit(std::size_t,std::string_view);
  [[nodiscard]] bool transformTextScenario(std::size_t);
  [[nodiscard]] bool configureBundledTextResources();
  [[nodiscard]] std::string textQualificationJson() const;
  [[nodiscard]] bool setTextFontsAvailable(bool);
  [[nodiscard]] text::LayoutMetrics textLayoutMetrics() const noexcept;

  [[nodiscard]] bool seedQualificationFixture(std::size_t objectCount) noexcept;
 private:
  runtime::MemoryResourceProvider textResources_;
  std::unique_ptr<text::RichTextLayoutService> textLayoutService_;
  std::vector<std::uint8_t> latinTextFont_,cjkTextFont_;
  std::vector<foundation::ObjectId> textScenarioObjects_;
  std::vector<foundation::ObjectId> textScenarioFixtureObjects_;
  std::string textScenarioName_;
  std::uint64_t textScenarioSerial_ = 0;
  bool submitTextScenarioOperation(semantic::OperationPayload);
};
}  // namespace canvas::ink_playground
