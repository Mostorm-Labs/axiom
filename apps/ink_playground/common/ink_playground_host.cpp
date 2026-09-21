#include "ink_playground_host.hpp"

#include <cmath>
#include <memory>
#include <limits>
#include <vector>

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
      tracker_(std::make_unique<render::PresentationTracker>(*lifecycle_)) {}

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
    const bool trackedContact = sample.key.valid() &&
        (contactDispositions_.contains(sample.key) ||
         sample.phase == input::PointerPhase::kDown);
    if (trackedContact) {
      if (!sample.predicted) contactSamples_[sample.key] = sample;
      const bool viewportWasClaimed = contactCoordinator_.viewportClaimed();
      const auto disposition = contactCoordinator_.update(sample);
      contactDispositions_[sample.key] = disposition;
      if (disposition == interaction::ContactDisposition::kViewportGesture) {
        for (auto& [key, cachedDisposition] : contactDispositions_) {
          cachedDisposition = contactCoordinator_.disposition(key);
        }
        for (const auto& [existing, existingStroke] : keyedStrokeIds_) {
          (void)ink_->cancel(existing);
          (void)preview_->cancelKeyed(existingStroke);
          (void)interaction_->cancel(existing, existingStroke);
        }
        keyedStrokeIds_.clear();
        const input::PointerSample* first = nullptr;
        const input::PointerSample* second = nullptr;
        for (const auto& [key, current] : contactSamples_) {
          if (contactCoordinator_.disposition(key) !=
              interaction::ContactDisposition::kViewportGesture) continue;
          if (first == nullptr) first = &current;
          else { second = &current; break; }
        }
        if (first != nullptr && second != nullptr) {
          if (viewportGesture_.update(*first, *second)) {
            viewportState_ = viewportGesture_.state();
            const float gestureScale = viewportState_.scale;
            viewportState_.scale = gestureScale * committedViewportScale_;
            viewportState_.translationX = gestureScale * committedViewportTranslationX_ +
                viewportState_.translationX;
            viewportState_.translationY = gestureScale * committedViewportTranslationY_ +
                viewportState_.translationY;
          }
        }
      }
      if (viewportWasClaimed && !contactCoordinator_.viewportClaimed()) {
        committedViewportScale_ = viewportState_.scale;
        committedViewportTranslationX_ = viewportState_.translationX;
        committedViewportTranslationY_ = viewportState_.translationY;
        viewportGesture_.reset();
        contactSamples_.clear();
      }
      if (disposition == interaction::ContactDisposition::kIgnored &&
          keyedStrokeIds_.contains(sample.key)) {
        const auto ignoredStroke = keyedStrokeIds_.at(sample.key);
        (void)ink_->cancel(sample.key);
        (void)preview_->cancelKeyed(ignoredStroke);
        (void)interaction_->cancel(sample.key, ignoredStroke);
        keyedStrokeIds_.erase(sample.key);
      }
      if (disposition == interaction::ContactDisposition::kViewportGesture ||
          disposition == interaction::ContactDisposition::kIgnored ||
          !keyedStrokeIds_.contains(sample.key)) {
        continue;
      }
    }
    input::PointerSample contentSample = sample;
    if (sample.key.valid()) {
      if (!std::isfinite(viewportState_.scale) || viewportState_.scale <= 0.0F) {
        return false;
      }
      contentSample.x =
          (sample.x - viewportState_.translationX) / viewportState_.scale;
      contentSample.y =
          (sample.y - viewportState_.translationY) / viewportState_.scale;
    }
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
  contactDispositions_.clear();
  contactSamples_.clear();
  contactCoordinator_.reset();
  viewportGesture_.reset();
  viewportState_ = {};
  committedViewportScale_ = 1.0F;
  committedViewportTranslationX_ = 0.0F;
  committedViewportTranslationY_ = 0.0F;
  interaction_->cancelKeyedSessions(interaction::CancellationReason::kSourceLost);
}

interaction::ContactDisposition InkPlaygroundHost::pointerDisposition(
    const input::PointerKey& key) const noexcept {
  const auto found = contactDispositions_.find(key);
  return found == contactDispositions_.end()
      ? interaction::ContactDisposition::kTerminal : found->second;
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
