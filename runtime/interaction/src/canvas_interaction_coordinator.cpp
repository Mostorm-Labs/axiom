#include "canvas/interaction/canvas_interaction_coordinator.hpp"
#include "canvas/interaction/viewport_interaction_controller.hpp"

#include <algorithm>

namespace canvas::interaction {

InteractionRoutingResult CanvasInteractionCoordinator::route(
    const input::PointerSample& sample) noexcept {
  InteractionRoutingResult result;
  const bool wasViewport = contacts_.viewportClaimed();
  if (!sample.predicted && sample.key.valid() &&
      sample.phase != input::PointerPhase::kUp &&
      sample.phase != input::PointerPhase::kCancel) {
    samples_[sample.key] = sample;
  }
  result.disposition = contacts_.update(sample);
  result.routedSample = sample;
  if (result.disposition == ContactDisposition::kInk) {
    result.route = InteractionRoute::kInk;
    result.routesToInk = true;
  } else if (result.disposition == ContactDisposition::kViewportGesture) {
    result.route = InteractionRoute::kViewport;
  } else if (result.disposition == ContactDisposition::kPending) {
    // Pending preserves ownership arbitration, but the composition root may
    // still append the sample to an already-open Ink session. Activation is
    // represented by the disposition, not by suppressing the routed sample.
    result.route = InteractionRoute::kInk;
    result.routesToInk = true;
  } else {
    result.route = InteractionRoute::kIgnored;
    result.cancelsInk = true;
  }
  const bool isViewport = contacts_.viewportClaimed();
  result.viewportClaimed = isViewport;
  result.becameViewport = !wasViewport && isViewport;
  result.endedViewport = wasViewport && !isViewport;
  result.canonicalMutation = contacts_.canonicalMutation();
  if (result.becameViewport) {
    for (const auto& [key, cached] : samples_) {
      (void)cached;
      if (contacts_.disposition(key) == ContactDisposition::kViewportGesture) {
        result.cancelPointers.push_back(key);
      }
    }
  }
  if (sample.phase == input::PointerPhase::kUp ||
      sample.phase == input::PointerPhase::kCancel) {
    samples_.erase(sample.key);
  }
  if (result.endedViewport) samples_.clear();
  return result;
}

InteractionRoutingResult CanvasInteractionCoordinator::route(
    const input::PointerSample& sample,
    const ViewportInteractionController& viewport) noexcept {
  auto result = route(sample);
  if (sample.key.valid()) {
    const auto content = viewport.viewToContent(sample.x, sample.y);
    result.routedSample.x = content.first;
    result.routedSample.y = content.second;
  }
  return result;
}

std::vector<input::PointerSample> CanvasInteractionCoordinator::viewportSamples() const {
  std::vector<input::PointerSample> result;
  for (const auto& [key, sample] : samples_) {
    if (contacts_.disposition(key) == ContactDisposition::kViewportGesture) {
      result.push_back(sample);
    }
  }
  std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
    if (left.key.source != right.key.source) return left.key.source < right.key.source;
    if (left.key.pointer != right.key.pointer) return left.key.pointer < right.key.pointer;
    return left.key.generation < right.key.generation;
  });
  return result;
}

void CanvasInteractionCoordinator::reset() noexcept {
  contacts_.reset();
  samples_.clear();
}

}  // namespace canvas::interaction
