#include "ink_playground_host.hpp"

#include <cmath>
#include <memory>
#include <limits>
#include <vector>
#include <algorithm>
#include <functional>

namespace canvas::ink_playground {

InkPlaygroundHost::InkPlaygroundHost()
    : input_(std::make_unique<input::InputRouter>()),
      ink_(std::make_unique<ink::InkEngine>(ink::BrushDescriptor{1.0F})),
      preview_(std::make_unique<ink::PreviewModel>()),
      interaction_(std::make_unique<interaction::InteractionRuntime>(
          *this, *this, *this, *this, *this)),
      lifecycle_(std::make_unique<render::SurfaceLifecycle>(render::SurfaceSnapshot{
          render::ViewId{1}, render::SurfaceGeneration{1},
          render::MetricsGeneration{1},
          render::SurfaceMetrics{1.0F, 1.0F, 1, 1, 1.0F, 1.0F}})),
      tracker_(std::make_unique<render::PresentationTracker>(*lifecycle_)),
      coordinator_(std::make_unique<interaction::CanvasInteractionCoordinator>()),
      viewportController_(std::make_unique<interaction::ViewportInteractionController>()) {}

bool InkPlaygroundHost::beginStroke(std::uint64_t strokeId) noexcept {
  strokeStartNs_ = 0;
  return ink_->begin(strokeId) && preview_->begin(strokeId) &&
         interaction_->startSession(strokeId);
}

bool InkPlaygroundHost::beginStroke(const input::PointerKey& key, std::uint64_t strokeId) noexcept {
  if (!key.valid() || keyedStrokeIds_.contains(key)) return false;
  if (!ink_->begin(key, strokeId) || !preview_->beginKeyed(strokeId) ||
      !interaction_->startSession(key, strokeId)) return false;
  keyedStrokeIds_.emplace(key, strokeId);
  return true;
}

bool InkPlaygroundHost::accept(const input::PointerSampleBatch& batch,
                               std::uint64_t observationTimeNs) {
  if (batch.samples.empty() ||
      input_->dispatch(batch) != input::DispatchDisposition::kDelivered) {
    return false;
  }
  std::vector<ink::StrokePoint> confirmed;
  std::vector<ink::StrokePoint> predicted;
  confirmed.reserve(batch.samples.size());
  predicted.reserve(batch.samples.size());
  for (const auto& sample : batch.samples) {
    // Legacy/native ingress may omit phase on already-open sessions. Only
    // feed contacts into the coordinator once a down has established them or
    // the runtime already owns the key; otherwise preserve the keyed ink
    // session's existing append behavior.
    const bool trackedContact = sample.key.valid() &&
        (sample.phase == input::PointerPhase::kDown || coordinator_->hasContact(sample.key));
    interaction::InteractionRoutingResult routing{};
    if (trackedContact) {
      routing = coordinator_->route(sample, *viewportController_);
      if (routing.becameViewport) {
        for (const auto& key : routing.cancelPointers) {
          const auto existing = keyedStrokeIds_.find(key);
          if (existing == keyedStrokeIds_.end()) continue;
          (void)ink_->cancel(key);
          (void)preview_->cancelKeyed(existing->second);
          (void)interaction_->cancel(key, existing->second);
          keyedStrokeIds_.erase(existing);
        }
      }
      const auto viewportSamples = coordinator_->viewportSamples();
      if (viewportSamples.size() >= 2U) {
        (void)viewportController_->updateGesture(viewportSamples[0], viewportSamples[1]);
      }
      if (routing.endedViewport) viewportController_->endGesture();
      if (routing.cancelsInk && sample.phase != input::PointerPhase::kUp &&
          keyedStrokeIds_.contains(sample.key)) {
        const auto ignoredStroke = keyedStrokeIds_.at(sample.key);
        (void)ink_->cancel(sample.key);
        (void)preview_->cancelKeyed(ignoredStroke);
        (void)interaction_->cancel(sample.key, ignoredStroke);
        keyedStrokeIds_.erase(sample.key);
      }
      if (!routing.routesToInk) {
        continue;
      }
    }
    const input::PointerSample contentSample = trackedContact ? routing.routedSample : sample;
    const ink::StrokePoint point{contentSample.x, contentSample.y,
                                 contentSample.pressure};
    if (sample.predicted) {
      predicted.push_back(point);
    } else {
      if (!ink_->append(contentSample)) return false;
      confirmed.push_back(point);
    }
  }
  if (confirmed.empty() && predicted.empty()) {
    hud_.batch = batch.samples.size();
    return true;
  }
  const auto keyed = batch.samples.front().key;
  if (keyed.valid()) {
    const auto it = keyedStrokeIds_.find(keyed);
    if (it == keyedStrokeIds_.end() || !preview_->updateKeyed(it->second, confirmed, predicted)) return false;
  } else if (!preview_->update(confirmed, predicted)) return false;
  const auto firstNs = batch.samples.front().timestampNs;
  const auto lastNs = batch.samples.back().timestampNs;
  strokeStartNs_ = strokeStartNs_ == 0 ? firstNs : strokeStartNs_;
  hud_.batch = batch.samples.size();
  hud_.sampleHz = batch.samples.size() > 1 && lastNs > firstNs
                      ? static_cast<double>(batch.samples.size() - 1) * 1'000'000'000.0 /
                            static_cast<double>(lastNs - firstNs)
                      : 0.0;
  hud_.queueAgeMs = observationTimeNs >= lastNs
                        ? static_cast<double>(observationTimeNs - lastNs) / 1'000'000.0
                        : 0.0;
  hud_.inkMs = observationTimeNs >= strokeStartNs_
                   ? static_cast<double>(observationTimeNs - strokeStartNs_) / 1'000'000.0
                   : 0.0;
  if (keyed.valid()) {
    const auto* state = preview_->snapshot(keyedStrokeIds_.at(keyed));
    hud_.previewRevision = state == nullptr ? 0 : state->revision;
    hud_.predictionDepth = state == nullptr ? 0 : state->predicted.size();
  } else {
    hud_.previewRevision = preview_->snapshot().revision;
    hud_.predictionDepth = preview_->snapshot().predicted.size();
  }
  return true;
}

bool InkPlaygroundHost::acceptPlatformBatch(const input::PlatformPointerBatch& batch,
                                            std::uint64_t observationTimeNs) {
  const auto routed = platformController_.submit(batch);
  if (!routed.accepted && !routed.terminalCancel) return false;
  std::unordered_map<input::PointerKey, input::PointerSampleBatch,
                     input::PointerKeyHash> grouped;
  for (const auto& sample : routed.normalized.samples) {
    if (!sample.key.valid()) continue;
    if (sample.phase == input::PointerPhase::kDown) {
      if (!beginStroke(sample.key, nextPlatformStrokeId_++)) return false;
    }
    if (sample.phase != input::PointerPhase::kCancel) {
      grouped[sample.key].samples.push_back(sample);
    }
  }
  for (auto& [key, drawable] : grouped) {
    (void)key;
    if (!drawable.samples.empty() && !accept(drawable, observationTimeNs)) return false;
  }
  for (const auto& sample : routed.normalized.samples) {
    if (sample.phase == input::PointerPhase::kUp) {
      const auto it = keyedStrokeIds_.find(sample.key);
      if (it != keyedStrokeIds_.end() && !commitStroke(sample.key, it->second, it->second)) {
        return false;
      }
    } else if (sample.phase == input::PointerPhase::kCancel) {
      (void)cancelStroke(sample.key);
    }
  }
  if (routed.terminalCancel) cancelAllPointers();
  return true;
}

bool InkPlaygroundHost::commitStroke(const input::PointerKey& key, std::uint64_t strokeId,
                                     std::uint64_t operationId) noexcept {
  if (!keyedStrokeIds_.contains(key)) return false;
  const auto* state = preview_->snapshot(strokeId);
  if (state == nullptr || !ink_->finish(key).has_value()) return false;
  const auto committedPoints = state->confirmed;
  if (!interaction_->commit(key, strokeId, interaction::OperationRequest{operationId})) return false;
  if (!committedPoints.empty()) committedStrokes_.push_back(committedPoints);
  preview_->cancelKeyed(strokeId);
  keyedStrokeIds_.erase(key);
  return true;
}

bool InkPlaygroundHost::cancelStroke(const input::PointerKey& key) noexcept {
  const auto entry = keyedStrokeIds_.find(key);
  if (entry == keyedStrokeIds_.end()) return false;
  ink_->cancel(key);
  preview_->cancelKeyed(entry->second);
  if (!interaction_->cancel(key, entry->second)) return false;
  keyedStrokeIds_.erase(entry);
  return true;
}

void InkPlaygroundHost::cancelAllPointers() noexcept {
  for (const auto& [key, strokeId] : keyedStrokeIds_) {
    ink_->cancel(key);
    preview_->cancelKeyed(strokeId);
  }
  keyedStrokeIds_.clear();
  coordinator_->reset();
  viewportController_->reset();
  interaction_->cancelKeyedSessions(interaction::CancellationReason::kSourceLost);
  for (auto& [pointer, session] : brushSessions_) {
    (void)pointer;
    session->cancel();
  }
  brushSessions_.clear();
  brushPreviews_.clear();
}

bool InkPlaygroundHost::beginBrushSession(std::uint64_t pointerId,
                                          std::uint32_t profile) noexcept {
  if (pointerId == 0U || brushSessions_.contains(pointerId)) return false;
  ink::BrushPackage package;
  package.packageId = "0123456789abcdef0123456789abcdef";
  package.revision = 1U;
  package.vector.size = 16.0;
  package.vector.thinning = 0.5;
  package.vector.smoothing = 0.5;
  package.vector.streamline = 0.5;
  // Profile is a versioned capability selector, not a product BrushFamily.
  if (profile != 1U) return false;
  auto state = ink::resolveBrushState(package, 0x4500ULL + pointerId);
  auto session = std::make_unique<ink::BrushSession>(pointerId, std::move(state));
  if (!session->begin()) return false;
  brushSessions_.emplace(pointerId, std::move(session));
  brushPreviews_.erase(pointerId);
  return true;
}

bool InkPlaygroundHost::appendBrushSample(std::uint64_t pointerId, double x, double y,
                                          double pressure, std::uint64_t sequence,
                                          bool predicted) noexcept {
  const auto it = brushSessions_.find(pointerId);
  if (it == brushSessions_.end()) return false;
  ink::BrushSample sample{x, y, pressure, true, sequence};
  ink::BrushPreviewDelta delta;
  const auto error = predicted
      ? it->second->append({}, std::span<const ink::BrushSample>(&sample, 1), delta)
      : it->second->append(std::span<const ink::BrushSample>(&sample, 1), {}, delta);
  if (error != ink::BrushSessionError::kNone) return false;
  brushPreviews_[pointerId] = std::move(delta.outline);
  return true;
}

bool InkPlaygroundHost::finishBrushSession(std::uint64_t pointerId) noexcept {
  const auto it = brushSessions_.find(pointerId);
  if (it == brushSessions_.end()) return false;
  ink::BrushCommitIntent intent;
  if (it->second->seal(intent) != ink::BrushSessionError::kNone) return false;
  brushDigest_ = std::hash<std::uint64_t>{}(intent.seed ^ intent.revision ^ intent.session);
  for (const auto& point : intent.outline) {
    committedBrushPoints_.push_back({static_cast<float>(point.x), static_cast<float>(point.y),
                                     1.0F, 0.0F, 1.0F, 1U});
  }
  brushSessions_.erase(it);
  brushPreviews_.erase(pointerId);
  return true;
}

bool InkPlaygroundHost::cancelBrushSession(std::uint64_t pointerId) noexcept {
  const auto it = brushSessions_.find(pointerId);
  if (it == brushSessions_.end()) return false;
  it->second->cancel();
  brushSessions_.erase(it);
  brushPreviews_.erase(pointerId);
  return true;
}

std::vector<render::BrushRenderPoint> InkPlaygroundHost::brushRenderPoints() const {
  auto result = committedBrushPoints_;
  for (const auto& [pointer, outline] : brushPreviews_) {
    (void)pointer;
    for (const auto& point : outline) {
      result.push_back({static_cast<float>(point.x), static_cast<float>(point.y),
                        1.0F, 0.0F, 0.7F, 1U});
    }
  }
  return result;
}

interaction::ContactDisposition InkPlaygroundHost::pointerDisposition(
    const input::PointerKey& key) const noexcept {
  return coordinator_->disposition(key);
}

bool InkPlaygroundHost::applyViewportNavigation(
    const interaction::ViewportNavigationSample& sample) noexcept {
  return viewportController_->applyNavigation(sample);
}

bool InkPlaygroundHost::commitStroke(std::uint64_t strokeId,
                                     std::uint64_t operationId) noexcept {
  const auto committedPoints = preview_->snapshot().confirmed;
  if (!ink_->finish().has_value()) return false;
  if (!interaction_->commit(strokeId, interaction::OperationRequest{operationId})) return false;
  if (!committedPoints.empty()) committedStrokes_.push_back(committedPoints);
  preview_->cancel();
  return true;
}

interaction::SubmitResult InkPlaygroundHost::submit(
    const interaction::OperationRequest&) {
  ++submittedOperationCount_;
  return interaction::SubmitResult::acceptedResult();
}

void InkPlaygroundHost::cancel(std::uint64_t) noexcept {
  ink_->cancel();
  preview_->cancel();
}

void InkPlaygroundHost::recordPresentation(std::string evidenceKind,
                                            std::size_t pendingHandoffs,
                                            double frameMs) {
  hud_.presentEvidenceKind = std::move(evidenceKind);
  hud_.pendingHandoffCount = pendingHandoffs;
  hud_.frameMs = frameMs;
}

bool InkPlaygroundHost::bindSurface(std::uint32_t width,
                                    std::uint32_t height) noexcept {
  if (width == 0U || height == 0U) return false;
  surface_ = SurfaceBinding{1U, width, height, true};
  lifecycle_ = std::make_unique<render::SurfaceLifecycle>(render::SurfaceSnapshot{
      render::ViewId{1}, render::SurfaceGeneration{surface_.generation},
      render::MetricsGeneration{surface_.generation},
      render::SurfaceMetrics{static_cast<float>(width), static_cast<float>(height),
                             width, height, 1.0F, 1.0F}});
  tracker_ = std::make_unique<render::PresentationTracker>(*lifecycle_);
  return true;
}

bool InkPlaygroundHost::resizeSurface(std::uint32_t width,
                                      std::uint32_t height) noexcept {
  if (width == 0U || height == 0U || surface_.generation == 0U ||
      surface_.generation == std::numeric_limits<std::uint64_t>::max()) {
    return false;
  }
  ++surface_.generation;
  surface_.width = width;
  surface_.height = height;
  surface_.available = true;
  lifecycle_ = std::make_unique<render::SurfaceLifecycle>(render::SurfaceSnapshot{
      render::ViewId{1}, render::SurfaceGeneration{surface_.generation},
      render::MetricsGeneration{surface_.generation},
      render::SurfaceMetrics{static_cast<float>(width), static_cast<float>(height),
                             width, height, 1.0F, 1.0F}});
  tracker_ = std::make_unique<render::PresentationTracker>(*lifecycle_);
  return true;
}

bool InkPlaygroundHost::loseSurface() noexcept {
  if (surface_.generation == 0U) return false;
  surface_.available = false;
  (void)lifecycle_->markLost(render::ViewId{1});
  return true;
}

bool InkPlaygroundHost::presentCanonicalFrame(std::uint64_t frameId,
                                              double frameMs) noexcept {
  if (!surface_.available || frameId == 0U) return false;
  const render::FrameState frame{
      render::ViewId{1},
      render::CameraState{foundation::WorldPoint{0.0F, 0.0F}, 1.0F, 0.0F,
                          render::CameraGeneration{1}},
      foundation::WorldRect{0.0F, 0.0F, static_cast<float>(surface_.width),
                            static_cast<float>(surface_.height)},
      render::SurfaceMetrics{static_cast<float>(surface_.width),
                             static_cast<float>(surface_.height), surface_.width,
                             surface_.height, 1.0F, 1.0F},
      semantic::SemanticGeneration{1}, foundation::SceneRevision{frameId},
      render::SurfaceGeneration{surface_.generation},
      render::MetricsGeneration{surface_.generation}, render::FrameId{frameId}};
  if (tracker_->submit(frame) != render::PresentFeedbackDisposition::kSubmitted) return false;
  const auto feedback = tracker_->receive(render::PresentedFeedback{
      frame.viewId, frame.frameId, frame.surfaceGeneration, frame.metricsGeneration,
      render::PresentOutcome::kPresented, render::PresentEvidenceKind::kPlatformQualified,
      std::nullopt});
  if (feedback != render::PresentFeedbackDisposition::kPresented) return false;
  recordPresentation("canonical-presented-platform-qualified", 0U, frameMs);
  return true;
}

std::vector<ink::StrokePoint> InkPlaygroundHost::previewPoints() const {
  return preview_->snapshot().confirmed;
}

std::vector<ink::StrokePoint> InkPlaygroundHost::transientPreviewPoints() const {
  return preview_->snapshot().confirmed;
}

std::vector<std::vector<ink::StrokePoint>> InkPlaygroundHost::previewStrokes() const {
  auto strokes = committedStrokes_;
  const auto active = preview_->snapshot().confirmed;
  if (!active.empty()) strokes.push_back(active);
  return strokes;
}

}  // namespace canvas::ink_playground
