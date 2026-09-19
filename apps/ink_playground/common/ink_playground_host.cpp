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
  if (!preview_->update(confirmed, predicted)) return false;
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
  hud_.previewRevision = preview_->snapshot().revision;
  hud_.predictionDepth = preview_->snapshot().predicted.size();
  return true;
}

bool InkPlaygroundHost::commitStroke(std::uint64_t strokeId,
                                     std::uint64_t operationId) noexcept {
  if (!ink_->finish().has_value()) return false;
  if (!interaction_->commit(strokeId, interaction::OperationRequest{operationId})) return false;
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
  return true;
}

bool InkPlaygroundHost::loseSurface() noexcept {
  if (surface_.generation == 0U) return false;
  surface_.available = false;
  return true;
}

std::vector<ink::StrokePoint> InkPlaygroundHost::previewPoints() const {
  return preview_->snapshot().confirmed;
}

}  // namespace canvas::ink_playground
