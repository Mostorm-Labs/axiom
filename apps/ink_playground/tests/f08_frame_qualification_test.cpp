#include "../common/ink_playground_host.hpp"

#include <cassert>

namespace {
using Host = canvas::ink_playground::InkPlaygroundHost;

void platform_feedback_is_required_for_qualification_and_retirement() {
  Host host;
  assert(host.bindSurface(256U, 256U));
  host.setPlatformPresentationDeferred(true);
  assert(host.beginBrushSession(8801U, 1U));
  assert(host.appendBrushSample(8801U, 20.0, 20.0, 0.5, 1U));
  assert(host.appendBrushSample(8801U, 120.0, 90.0, 0.5, 2U));
  assert(host.presentBrushPreview());
  assert(host.finishBrushSession(8801U));

  assert(host.previewSurfaceProvider()->overlayVisible());
  assert(host.presentCanonicalFrame(1U, 0.0, true));
  const auto submitted = host.lastSubmittedCanonicalFrame();
  assert(submitted.has_value());
  assert(host.operationOutcome().presentedGeneration.value() == 0U);
  assert(host.previewSurfaceProvider()->overlayVisible());

  auto stale = canvas::render::PresentedFeedback{
      submitted->viewId, canvas::render::FrameId{submitted->frameId.value() + 1U},
      submitted->surfaceGeneration, submitted->metricsGeneration,
      canvas::render::PresentOutcome::kPresented,
      canvas::render::PresentEvidenceKind::kPlatformQualified, std::nullopt};
  assert(!host.receivePlatformPresented(stale));
  assert(host.previewSurfaceProvider()->overlayVisible());

  const auto feedback = canvas::render::PresentedFeedback{
      submitted->viewId, submitted->frameId, submitted->surfaceGeneration,
      submitted->metricsGeneration, canvas::render::PresentOutcome::kPresented,
      canvas::render::PresentEvidenceKind::kPlatformQualified, std::nullopt};
  assert(host.receivePlatformPresented(feedback));
  assert(host.operationOutcome().presentedGeneration == host.semanticGeneration());
  // No matching preview frame was presented in this fixture, so even a
  // qualified canonical feedback cannot retire an unrelated overlay.
  assert(host.previewSurfaceProvider()->overlayVisible());
}
}  // namespace

int main() {
  platform_feedback_is_required_for_qualification_and_retirement();
  return 0;
}
