#include "canvas/render/runtime_frame_coordinator.hpp"

namespace canvas::render {

RuntimeFrameDisposition RuntimeFrameCoordinator::request(RuntimeFrameCapture capture) noexcept {
  if (closed_) return RuntimeFrameDisposition::kUnavailable;
  if (capture.identity.viewId.value() == 0U ||
      capture.identity.frameId.value() == 0U || capture.identity.surfaceGeneration.value() == 0U ||
      capture.identity.metricsGeneration.value() == 0U) {
    return RuntimeFrameDisposition::kRejected;
  }
  auto& target = slot(capture.role);
  if (target.capture.has_value() &&
      capture.identity.frameId.value() < target.capture->identity.frameId.value()) {
    return RuntimeFrameDisposition::kStale;
  }
  target.capture.reset();
  target.capture.emplace(std::move(capture));
  target.submitted = false;
  target.presented = false;
  target.qualified = false;
  return RuntimeFrameDisposition::kAccepted;
}

RuntimeFrameDisposition RuntimeFrameCoordinator::submit(
    FrameRole role, FrameId frameId, bool busy) noexcept {
  if (closed_) return RuntimeFrameDisposition::kUnavailable;
  auto& target = slot(role);
  if (!target.capture.has_value() || target.capture->identity.frameId != frameId) {
    return RuntimeFrameDisposition::kStale;
  }
  if (busy) return RuntimeFrameDisposition::kRetry;
  target.submitted = true;
  return RuntimeFrameDisposition::kAccepted;
}

RuntimeFrameDisposition RuntimeFrameCoordinator::receive(
    const RuntimePresentFeedback& feedback) noexcept {
  if (closed_) return RuntimeFrameDisposition::kUnavailable;
  auto& target = slot(feedback.role);
  if (!target.capture.has_value() || !target.submitted) return RuntimeFrameDisposition::kStale;
  if (target.capture->identity != feedback.identity) return RuntimeFrameDisposition::kStale;
  if (!feedback.presented) return RuntimeFrameDisposition::kRejected;
  target.presented = true;
  target.qualified = feedback.evidence == RuntimePresentEvidence::kPlatformQualified;
  return RuntimeFrameDisposition::kAccepted;
}

bool RuntimeFrameCoordinator::retirePreview(
    const RuntimeFrameIdentity& canonical) const noexcept {
  if (!canonical_.capture.has_value() || !canonical_.presented || !canonical_.qualified ||
      !preview_.capture.has_value() || !preview_.presented) return false;
  const auto& c = canonical_.capture->identity;
  const auto& p = preview_.capture->identity;
  return c.viewId == canonical.viewId && c.frameId == canonical.frameId &&
         c.documentRevision == canonical.documentRevision &&
         c.sceneGeneration == canonical.sceneGeneration &&
         c.surfaceGeneration == canonical.surfaceGeneration &&
         c.metricsGeneration == canonical.metricsGeneration && p.viewId == c.viewId && p.frameId == c.frameId &&
         p.documentRevision == c.documentRevision && p.sceneGeneration == c.sceneGeneration &&
         p.surfaceGeneration == c.surfaceGeneration && p.metricsGeneration == c.metricsGeneration;
}

bool RuntimeFrameCoordinator::pending(FrameRole role) const noexcept {
  const auto& target = slot(role);
  return target.capture.has_value() && (!target.presented || !target.qualified);
}

bool RuntimeFrameCoordinator::presented(FrameRole role) const noexcept {
  return slot(role).presented;
}

const RuntimeFrameCapture* RuntimeFrameCoordinator::capture(FrameRole role) const noexcept {
  const auto& target = slot(role);
  return target.capture.has_value() ? &*target.capture : nullptr;
}

void RuntimeFrameCoordinator::invalidate(FrameRole role) noexcept {
  auto& target = slot(role);
  target.capture.reset();
  target.submitted = false;
  target.presented = false;
  target.qualified = false;
}

void RuntimeFrameCoordinator::close() noexcept {
  invalidate(FrameRole::kCanonical);
  invalidate(FrameRole::kPreview);
  closed_ = true;
}

}  // namespace canvas::render
