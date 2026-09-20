#include "ink_playground_host.hpp"

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
    const ink::StrokePoint point{sample.x, sample.y, sample.pressure};
    if (sample.predicted) {
      predicted.push_back(point);
    } else {
      if (!ink_->append(sample)) return false;
      confirmed.push_back(point);
    }
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
  const auto* state = preview_->snapshot(strokeId);
  if (state == nullptr || !ink_->finish(key).has_value()) return false;
  const auto committedPoints = state->confirmed;
  if (!interaction_->commit(key, strokeId, interaction::OperationRequest{operationId})) return false;
  if (!committedPoints.empty()) committedStrokes_.push_back(committedPoints);
  preview_->cancelKeyed(strokeId);
  keyedStrokeIds_.erase(key);
  return true;
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
