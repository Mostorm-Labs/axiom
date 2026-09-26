#include "ink_playground_host.hpp"

#include "canvas/semantic/brush_engine_codec.hpp"
#include "canvas/scene/direct_render_scene.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/render/visibility_resolver.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/frame_plan.hpp"

#include <cmath>
#include <memory>
#include <limits>
#include <vector>
#include <algorithm>
#include <functional>
#include <cstdio>
#include <array>
#define AXIOM_ANDROID_DIAG(...) std::fprintf(stderr, "[axiom] " __VA_ARGS__), std::fputc('\n', stderr)

namespace canvas::ink_playground {
class PlaygroundSceneCompiler final : public canvas::ISemanticSceneCompiler {
 public:
  foundation::Result<canvas::CompiledSceneSnapshot> compileFull(
      const semantic::SemanticReadView& view) const override {
    canvas::CompiledSceneSnapshot result{canvas::SceneRevision(view.generation().value()), {}};
    const auto records = view.allObjects();
    result.records.reserve(records.size());
    for (const auto& record : records) {
      canvas::SceneObjectKind kind = canvas::SceneObjectKind::kShape;
      switch (record.kind) {
        case semantic::ObjectKind::kImage: kind = canvas::SceneObjectKind::kImage; break;
        case semantic::ObjectKind::kVectorPath: kind = canvas::SceneObjectKind::kVectorPath; break;
        case semantic::ObjectKind::kRichText: kind = canvas::SceneObjectKind::kRichText; break;
        case semantic::ObjectKind::kVectorStroke: kind = canvas::SceneObjectKind::kVectorStroke; break;
        case semantic::ObjectKind::kDabStroke: kind = canvas::SceneObjectKind::kDabStroke; break;
        default: break;
      }
      const auto bounds = canvas::computeBounds(record);
      if (!bounds.finite) return foundation::Result<canvas::CompiledSceneSnapshot>::failure(
          {foundation::ErrorCode::kInvalidRecord, "non-finite brush scene bounds"});
      std::uint64_t order = 0;
      for (const auto byte : record.placement.order_key.bytes()) order = (order << 8U) | byte;
      const auto flags = static_cast<canvas::SceneRecordFlags>(
          static_cast<std::uint32_t>(canvas::SceneRecordFlags::kVisible) |
          static_cast<std::uint32_t>(canvas::SceneRecordFlags::kHitTestable));
      result.records.push_back({record.id, canvas::SceneOrderKey(order), kind, flags,
                                bounds.world, canvas::ContentRevision(record.kind_version),
                                canvas::RenderPayloadRef{static_cast<std::uint32_t>(record.id.bytes[0]), record.kind_version},
                                canvas::HitGeometryRef{static_cast<std::uint32_t>(record.id.bytes[0]), record.kind_version}});
    }
    return foundation::Result<canvas::CompiledSceneSnapshot>::success(std::move(result));
  }
  foundation::Result<canvas::CompiledSceneDelta> compileDelta(
      const semantic::SemanticReadView&, const semantic::ChangeSet&) const override {
    return foundation::Result<canvas::CompiledSceneDelta>::failure(
        {foundation::ErrorCode::kRequiresFullRebuild, "playground uses full scene recovery"});
  }
};
}  // namespace canvas::ink_playground

namespace {
constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

std::uint64_t hashBytes(std::uint64_t hash, const void* data, std::size_t size) noexcept {
  const auto* bytes = static_cast<const std::uint8_t*>(data);
  for (std::size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= kFnvPrime;
  }
  return hash;
}

template <typename T>
std::uint64_t hashValue(std::uint64_t hash, const T& value) noexcept {
  return hashBytes(hash, &value, sizeof(value));
}

std::int64_t quantize(double value) noexcept {
  return static_cast<std::int64_t>(std::llround(value * 1000.0));
}

std::uint64_t hashOutline(const std::vector<canvas::ink::reference::StrokeOutlinePoint>& outline) noexcept {
  auto hash = kFnvOffset;
  for (const auto& point : outline) {
    const auto x = quantize(point.x);
    const auto y = quantize(point.y);
    hash = hashValue(hash, x);
    hash = hashValue(hash, y);
  }
  return hash;
}

}  // namespace

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
      viewportController_(std::make_unique<interaction::ViewportInteractionController>()),
      appBinding_(std::make_unique<render::RuntimeAppBinding>(
          render::ViewId{1}, render::SurfaceProfile{
              {"cpu-raster", render::RenderTargetKind::kCpuRaster,
               render::RenderTargetBackend::kRaster,
               render::RenderTargetFormat::kRgba8888,
               {1.0F, 1.0F, 1U, 1U, 1.0F, 1.0F},
               {false, false, false, true, false, false}},
              std::make_unique<render::RasterSkiaSurfaceProvider>()})),
      runtimeSceneHost_(std::make_unique<canvas::Scene>(
          std::make_unique<canvas::DirectRenderScene>(),
          std::make_unique<canvas::UniformGridSpatialIndex>())),
      sceneBinding_(std::make_unique<canvas::SceneBinding>(*runtimeSceneHost_)),
      sceneCoordinator_(std::make_unique<canvas::IncrementalRuntimeCoordinator>(*sceneBinding_)),
      sceneCompiler_(std::make_unique<PlaygroundSceneCompiler>()) {}

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
    }
    const input::PointerSample contentSample = trackedContact ? routing.routedSample : sample;
    baselineTrace_.push_back({contentSample.sequence, contentSample.timestampNs,
                              sample.x, sample.y, contentSample.x, contentSample.y,
                              contentSample.pressure,
                              contentSample.key, contentSample.phase});
    baselineViewport_.push_back({contentSample.sequence, coordinator_->viewportClaimed(),
                                 viewportController_->state().scale,
                                 viewportController_->state().translationX,
                                 viewportController_->state().translationY});
    if (trackedContact && !routing.routesToInk) continue;
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
  (void)observationTimeNs;
  if (batch.samples.empty()) return false;
  const auto source = batch.samples.front().source;
  if (appBinding_ == nullptr) return false;
  if (!appBinding_->hasInputSource(source) &&
      !appBinding_->registerInputSource({source, "platform"})) return false;
  const auto routed = appBinding_->submit(source, batch);
  if (!routed.accepted && !routed.terminalCancel) { AXIOM_ANDROID_DIAG("ingress rejected samples=%zu", routed.normalized.samples.size()); return false; }
  bool previewDirty = false;
  const auto viewportBefore = viewportController_->state();
  for (const auto& sample : routed.normalized.samples) {
    if (!sample.key.valid()) continue;
    auto routing = coordinator_->route(sample);
    if (routing.endedViewport) viewportController_->endGesture();
    // Preserve the original AutoIntent handoff boundary: once Runtime claims
    // a viewport gesture, every pending ink session participating in that
    // claim must be cancelled before any further sample can reach BrushSession.
    for (const auto& cancelled : routing.cancelPointers) {
      (void)cancelBrushSession(cancelled.pointer);
    }
    if (routing.cancelsInk && sample.phase != input::PointerPhase::kUp &&
        brushSessions_.contains(sample.key.pointer)) {
      (void)cancelBrushSession(sample.key.pointer);
      continue;
    }
    if (sample.phase == input::PointerPhase::kDown && routing.routesToInk) {
      if (!beginBrushSession(sample.key.pointer, 1U)) { AXIOM_ANDROID_DIAG("begin failed pointer=%llu seq=%llu", static_cast<unsigned long long>(sample.key.pointer), static_cast<unsigned long long>(sample.sequence)); return false; }
    }
    if (sample.phase == input::PointerPhase::kDown) {
      baselineDownTimestamps_[sample.key] = sample.timestampNs;
    }
    const auto downIt = baselineDownTimestamps_.find(sample.key);
    const auto downTimestampNs = downIt == baselineDownTimestamps_.end()
        ? 0U : downIt->second;
    const auto downTimeDeltaNs = downTimestampNs != 0U &&
            sample.timestampNs >= downTimestampNs
        ? sample.timestampNs - downTimestampNs : 0U;
    if (coordinator_->viewportClaimed()) {
      const auto viewportSamples = coordinator_->viewportSamples();
      if (viewportSamples.size() >= 2U) {
        (void)viewportController_->updateGesture(viewportSamples[0], viewportSamples[1]);
      }
    }
    if (sample.key.valid()) {
      const auto content = viewportController_->viewToContent(sample.x, sample.y);
      routing.routedSample.x = content.first;
      routing.routedSample.y = content.second;
    }
    baselineTrace_.push_back({sample.sequence, sample.timestampNs, sample.x, sample.y,
                              routing.routedSample.x, routing.routedSample.y,
                              sample.pressure, sample.key, sample.phase,
                              routing.disposition, routing.viewportClaimed,
                              downTimestampNs, downTimeDeltaNs});
    baselineViewport_.push_back({sample.sequence, coordinator_->viewportClaimed(),
                                 viewportController_->state().scale,
                                 viewportController_->state().translationX,
                                 viewportController_->state().translationY});
    if (routing.routesToInk && sample.phase != input::PointerPhase::kCancel) {
      if (!appendBrushSample(sample.key.pointer, routing.routedSample.x,
                             routing.routedSample.y, sample.pressure,
                             sample.sequence, sample.predicted, false)) { AXIOM_ANDROID_DIAG("append failed pointer=%llu seq=%llu phase=%d", static_cast<unsigned long long>(sample.key.pointer), static_cast<unsigned long long>(sample.sequence), static_cast<int>(sample.phase)); return false; }
      previewDirty = true;
    }
    if (sample.phase == input::PointerPhase::kUp) {
      if (brushSessions_.contains(sample.key.pointer) &&
          !finishBrushSession(sample.key.pointer)) { AXIOM_ANDROID_DIAG("finish failed pointer=%llu seq=%llu", static_cast<unsigned long long>(sample.key.pointer), static_cast<unsigned long long>(sample.sequence)); return false; }
      baselineDownTimestamps_.erase(sample.key);
    } else if (sample.phase == input::PointerPhase::kCancel) {
      (void)cancelBrushSession(sample.key.pointer);
      baselineDownTimestamps_.erase(sample.key);
    }
  }
  const auto viewportAfter = viewportController_->state();
  const bool viewportChanged = viewportBefore.scale != viewportAfter.scale ||
      viewportBefore.translationX != viewportAfter.translationX ||
      viewportBefore.translationY != viewportAfter.translationY;
  if (viewportChanged && previewController_ != nullptr && previewController_->active()) {
    if (!previewController_->updateViewport(viewportAfter.scale,
                                             viewportAfter.translationX,
                                             viewportAfter.translationY)) {
      return false;
    }
  }
  if (viewportChanged && activeSurfaceProvider() != nullptr && surface_.available) {
    // A viewport-only frame redraws the canonical document but is not the
    // canonical-visible receipt for the active brush commit.  Retained amber
    // preview must survive this redraw until the matching commit frame.
    if (!presentCanonicalFrame(canonicalFrameCount_ + 1U, 0.0, false)) return false;
  }
  // Preview submission is display-frame gated by the platform host. The
  // batch only updates Runtime-retained geometry; Java/Choreographer calls
  // presentBrushPreview() once for the latest dirty revision.
  (void)previewDirty;
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
  brushSessionPackages_.clear();
  brushSessionSeeds_.clear();
  brushPreviews_.clear();
  pendingCanonicalIdentities_.clear();
}

bool InkPlaygroundHost::beginBrushSession(std::uint64_t pointerId,
                                          std::uint32_t profile) noexcept {
  if (profile != 1U) return false;
  const auto loaded = brushCatalog_.loadDefault("vector-solid-v1", 1U);
  if (!loaded) return false;
  return beginBrushSession(pointerId, loaded.package, 0x4500ULL + pointerId);
}

bool InkPlaygroundHost::beginBrushSession(std::uint64_t pointerId,
                                          const ink::BrushPackage& package,
                                          std::uint64_t seed) noexcept {
  if (pointerId == 0U || package.profileId != "vector-solid-v1" ||
      package.packageId.empty() ||
      brushSessions_.contains(pointerId)) return false;
  auto state = ink::resolveBrushState(package, seed);
  brushPackageDigest_ = ink::brushPackageCanonicalDigest(package);
  brushResolvedStateDigest_ = kFnvOffset;
  brushResolvedStateDigest_ = hashBytes(brushResolvedStateDigest_, package.packageId.data(),
                                        package.packageId.size());
  brushResolvedStateDigest_ = hashValue(brushResolvedStateDigest_, package.revision);
  brushResolvedStateDigest_ = hashValue(brushResolvedStateDigest_, quantize(package.vector.size));
  brushResolvedStateDigest_ = hashValue(brushResolvedStateDigest_, quantize(package.vector.thinning));
  brushResolvedStateDigest_ = hashValue(brushResolvedStateDigest_, quantize(package.vector.smoothing));
  brushResolvedStateDigest_ = hashValue(brushResolvedStateDigest_, quantize(package.vector.streamline));
  brushResolvedStateDigest_ = hashValue(brushResolvedStateDigest_, state.seed);
  auto session = std::make_unique<ink::BrushSession>(pointerId, std::move(state));
  if (!session->begin()) return false;
  brushSessions_.emplace(pointerId, std::move(session));
  brushSessionPackages_[pointerId] = package;
  brushSessionSeeds_[pointerId] = seed;
  brushPreviews_.erase(pointerId);
  if (arcPreviewSink_ != nullptr) {
    ink::BrushPreviewDelta initial;
    const auto disposition = arcPreviewSink_->begin(
        ink::PreviewIdentity{1U, pointerId, 1U}, initial);
    if (disposition == ink::PreviewSubmitResult::kRejected) {
      brushSessions_.erase(pointerId);
      brushSessionPackages_.erase(pointerId);
      brushSessionSeeds_.erase(pointerId);
      return false;
    }
  }
  if (previewProvider_ == nullptr || previewController_ == nullptr ||
      !previewController_->begin(1U, pointerId, 1U, previewProvider_->generation())) {
    return false;
  }
  return true;
}

bool InkPlaygroundHost::appendBrushSample(std::uint64_t pointerId, double x, double y,
                                          double pressure, std::uint64_t sequence,
                                          bool predicted, bool renderPreview) noexcept {
  const auto it = brushSessions_.find(pointerId);
  if (it == brushSessions_.end()) return false;
  ink::BrushSample sample{x, y, pressure, true, sequence};
  ink::BrushPreviewDelta delta;
  const auto error = predicted
      ? it->second->append({}, std::span<const ink::BrushSample>(&sample, 1), delta)
      : it->second->append(std::span<const ink::BrushSample>(&sample, 1), {}, delta);
  if (error != ink::BrushSessionError::kNone) { AXIOM_ANDROID_DIAG("BrushSession append error=%d confirmed=%zu predicted=%zu seq=%llu", static_cast<int>(error), static_cast<std::size_t>(predicted ? 0 : 1), static_cast<std::size_t>(predicted ? 1 : 0), static_cast<unsigned long long>(sequence)); return false; }
  if (arcPreviewSink_ != nullptr) {
    const ink::PreviewIdentity identity{1U, pointerId, 1U};
    const auto disposition = arcPreviewSink_->update(identity, delta);
    if (disposition == ink::PreviewSubmitResult::kRejected) return false;
  }
  const auto viewport = viewportController_->state();
  if (previewController_ == nullptr ||
      !previewController_->updateForSession(pointerId, delta, viewport.scale,
                                  viewport.translationX,
                                  viewport.translationY)) { AXIOM_ANDROID_DIAG("preview update failed rev=%llu active=%d state_rev=%llu gen=%llu geom_gen=%llu geom_rev=%llu", static_cast<unsigned long long>(delta.revision), previewController_ != nullptr && previewController_->active() ? 1 : 0, previewController_ == nullptr ? 0ULL : static_cast<unsigned long long>(previewController_->state().contentRevision), previewProvider_ == nullptr ? 0ULL : static_cast<unsigned long long>(previewProvider_->generation()), previewController_ == nullptr ? 0ULL : static_cast<unsigned long long>(previewController_->geometry().surfaceGeneration.value()), previewController_ == nullptr ? 0ULL : static_cast<unsigned long long>(previewController_->geometry().revision)); return false; }
  if (renderPreview && !previewController_->renderIfDirty()) { AXIOM_ANDROID_DIAG("preview render failed rev=%llu", static_cast<unsigned long long>(delta.revision)); return false; }
  brushPreviews_[pointerId] = std::move(delta.outline);
  brushPreviewDigest_ = hashOutline(brushPreviews_[pointerId]);
  return true;
}

bool InkPlaygroundHost::presentBrushPreview() noexcept {
  return previewController_ != nullptr && previewController_->renderIfDirty();
}

bool InkPlaygroundHost::finishBrushSession(std::uint64_t pointerId) noexcept {
  const auto it = brushSessions_.find(pointerId);
  if (it == brushSessions_.end()) return false;
  AXIOM_ANDROID_DIAG("finish begin pointer=%llu objects=%zu scene_gen=%llu", static_cast<unsigned long long>(pointerId), semanticObjects_.size(), static_cast<unsigned long long>(semanticGeneration_.current().value()));
  ink::BrushCommitIntent intent;
  if (it->second->seal(intent) != ink::BrushSessionError::kNone) return false;
  brushSealedOutlineDigest_ = hashOutline(intent.outline);
  const auto package = brushSessionPackages_.at(pointerId);
  const auto seed = brushSessionSeeds_.at(pointerId);
  ink::BrushSession replay(pointerId, ink::resolveBrushState(package, seed));
  ink::BrushPreviewDelta replayPreview;
  ink::BrushCommitIntent replayIntent;
  if (!replay.begin() ||
      replay.append(intent.confirmed, {}, replayPreview) != ink::BrushSessionError::kNone ||
      replay.seal(replayIntent) != ink::BrushSessionError::kNone) return false;
  brushReplayDigest_ = hashOutline(replayIntent.outline);
  if (brushReplayDigest_ != brushSealedOutlineDigest_) return false;
  if (!BrushCommitAdapter::valid(intent, package)) return false;
  brushDigest_ = hashValue(hashValue(hashValue(kFnvOffset, brushSealedOutlineDigest_),
                                     brushReplayDigest_), intent.revision);
  const auto operationId = submittedOperationCount_ + 1U;
  const auto operation = BrushCommitAdapter::build(
      intent, package, operationId, documentId_);
  const auto applied = operationEngine_.apply(
      operation, semantic::ApplySource::kLocalInteraction, semanticObjects_,
      appliedOperations_, semanticGeneration_, canonicalCommitClock_);
  if (applied.disposition != semantic::ApplyDisposition::kApplied &&
      applied.disposition != semantic::ApplyDisposition::kAlreadyApplied) {
    return false;
  }
  if (applied.commit_record.has_value() && sceneCoordinator_ != nullptr &&
      sceneCompiler_ != nullptr) {
    semantic::SemanticReadView view(semanticObjects_, semanticGeneration_.current());
    canvas::SceneCommitInput sceneInput(
        applied.commit_record->before_generation,
        applied.commit_record->after_generation,
        view, &applied.commit_record->change_set);
    const auto synchronized = sceneCoordinator_->apply(*sceneCompiler_, sceneInput);
    AXIOM_ANDROID_DIAG("scene apply pointer=%llu ok=%d objects=%zu scene_gen=%llu", static_cast<unsigned long long>(pointerId), synchronized ? 1 : 0, semanticObjects_.size(), static_cast<unsigned long long>(semanticGeneration_.current().value()));
    if (!synchronized) return false;
  }
  ++submittedOperationCount_;
  if (applied.commit_record.has_value()) {
    const ink::CanonicalHandoffIdentity identity{
        1U, pointerId, 1U, applied.commit_record->operation_id,
        applied.commit_record->commit_stamp,
        render::SurfaceGeneration{previewProvider_ == nullptr
                                      ? surface_.generation
                                      : previewProvider_->generation()}};
    if (canonicalVisibilitySink_ != nullptr &&
        canonicalVisibilitySink_->canonicalCommitted(identity, *applied.commit_record) ==
            ink::HandoffResult::kRejected) return false;
    lastCanonicalIdentity_ = identity;
    pendingCanonicalIdentities_[pointerId] = identity;
  }
  // The semantic commit is now authoritative; submit its scene immediately
  // so platform hosts do not need a second input event to complete the
  // canonical-visible handoff.
  // The Android presenter performs the first canonical frame when pixels are
  // requested; the semantic commit itself only records the handoff. This
  // keeps the PresentationTracker frame identity single-owner.
  brushSessions_.erase(it);
  brushSessionPackages_.erase(pointerId);
  brushSessionSeeds_.erase(pointerId);
  brushPreviews_.erase(pointerId);
  // Canonical visibility is emitted after the next successful canonical
  // present. Keep preview state alive until that receipt arrives.
  return true;
}

bool InkPlaygroundHost::cancelBrushSession(std::uint64_t pointerId) noexcept {
  const auto it = brushSessions_.find(pointerId);
  if (it == brushSessions_.end()) return false;
  it->second->cancel();
  if (arcPreviewSink_ != nullptr &&
      arcPreviewSink_->cancel(ink::PreviewIdentity{1U, pointerId, 1U}) ==
          ink::PreviewSubmitResult::kRejected) {
    return false;
  }
  if (previewController_ == nullptr || !previewController_->cancelSession(pointerId) ||
      !previewController_->renderIfDirty()) return false;
  brushSessions_.erase(it);
  brushSessionPackages_.erase(pointerId);
  brushSessionSeeds_.erase(pointerId);
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
  if (viewportController_ == nullptr ||
      !viewportController_->applyNavigation(sample)) {
    return false;
  }

  const auto viewport = viewportController_->state();
  // Navigation is also a preview-surface transform invalidation.  Without
  // this update an already-retained amber outline keeps its pre-gesture
  // matrix until the next brush sample arrives, which makes the pinch appear
  // to be applied one input event late.
  if (previewController_ != nullptr && previewController_->active() &&
      !previewController_->updateViewport(viewport.scale,
                                           viewport.translationX,
                                           viewport.translationY)) {
    return false;
  }

  // Navigation changes the canonical camera even when no new semantic or
  // brush input arrives.  Treat it as an independent canonical-surface
  // invalidation so the Web/Android/Windows display cannot remain at the old
  // transform until the next pointer sample happens to trigger a frame.
  if (surface_.available && activeSurfaceProvider() != nullptr) {
    return presentCanonicalFrame(canonicalFrameCount_ + 1U, 0.0, false);
  }
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
    const interaction::OperationRequest& request) {
  if (request.operationId == 0U) return interaction::SubmitResult::rejected();
  // Legacy interaction callers remain accepted only after they have supplied
  // a real operation identity. BrushSession production commits use the typed
  // adapter above and never pass through this compatibility port.
  const auto operation = semantic::Operation{
      semantic::OperationId(canvas::foundation::ObjectId::fromUint64(request.operationId)),
      documentId_, 1U, 1U,
      semantic::InsertObjectsOp{}};
  const auto applied = operationEngine_.apply(
      operation, semantic::ApplySource::kLocalInteraction, semanticObjects_,
      appliedOperations_, semanticGeneration_, canonicalCommitClock_);
  if (applied.disposition != semantic::ApplyDisposition::kApplied &&
      applied.disposition != semantic::ApplyDisposition::kAlreadyApplied) {
    return interaction::SubmitResult::rejected();
  }
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
  if (appBinding_ == nullptr ||
      appBinding_->resizeSurface(render::SurfaceMetrics{
          static_cast<float>(width), static_cast<float>(height), width, height,
          1.0F, 1.0F}) != render::SurfaceProviderDisposition::kCommitted) {
    return false;
  }
  surface_ = SurfaceBinding{appBinding_->surfaces().lifecycle().current().surfaceGeneration.value(), width, height, true};
  lifecycle_ = std::make_unique<render::SurfaceLifecycle>(render::SurfaceSnapshot{
      render::ViewId{1}, render::SurfaceGeneration{surface_.generation},
      render::MetricsGeneration{surface_.generation},
      render::SurfaceMetrics{static_cast<float>(width), static_cast<float>(height),
                             width, height, 1.0F, 1.0F}});
  tracker_ = std::make_unique<render::PresentationTracker>(*lifecycle_);
#if !defined(__EMSCRIPTEN__)
  // Native hosts use the deterministic transparent overlay until their
  // platform provider is registered. Web owns both canvas/context resources
  // and registers its preview provider explicitly after the canonical one.
  auto previewProvider = std::make_unique<render::TransparentOverlaySkiaSurfaceProvider>();
  if (previewProvider->resize(width, height).code != render::BackendSubmissionCode::kAccepted) {
    return false;
  }
  auto* previewRaw = previewProvider.get();
  auto previewInfo = previewProvider->describe();
  previewInfo.profileId = "arc-preview-surface";
  if (appBinding_->registerPreviewSurfaceProfile(
          render::SurfaceProfile{previewInfo, std::move(previewProvider)}) !=
      render::SurfaceProviderDisposition::kCommitted) {
    return false;
  }
  previewProvider_ = previewRaw;
  previewController_ = std::make_unique<render::PreviewSurfaceController>(
      skiaRenderer_, *previewProvider_);
#endif
  ++resizeEvents_;
  return true;
}

bool InkPlaygroundHost::resizeSurface(std::uint32_t width,
                                      std::uint32_t height) noexcept {
  if (width == 0U || height == 0U || surface_.generation == 0U ||
      surface_.generation == std::numeric_limits<std::uint64_t>::max()) {
    return false;
  }
  if (appBinding_ == nullptr ||
      appBinding_->resizeSurface(render::SurfaceMetrics{
          static_cast<float>(width), static_cast<float>(height), width, height,
          1.0F, 1.0F}) != render::SurfaceProviderDisposition::kCommitted) {
    return false;
  }
  surface_.generation = appBinding_->surfaces().lifecycle().current().surfaceGeneration.value();
  surface_.width = width;
  surface_.height = height;
  surface_.available = true;
  lifecycle_ = std::make_unique<render::SurfaceLifecycle>(render::SurfaceSnapshot{
      render::ViewId{1}, render::SurfaceGeneration{surface_.generation},
      render::MetricsGeneration{surface_.generation},
      render::SurfaceMetrics{static_cast<float>(width), static_cast<float>(height),
                             width, height, 1.0F, 1.0F}});
  tracker_ = std::make_unique<render::PresentationTracker>(*lifecycle_);
  if (previewProvider_ == nullptr || appBinding_->previewSurfaces() == nullptr ||
      appBinding_->resizePreviewSurface(render::SurfaceMetrics{
          static_cast<float>(width), static_cast<float>(height), width, height,
          1.0F, 1.0F}) != render::SurfaceProviderDisposition::kCommitted) {
    return false;
  }
  if (previewController_ != nullptr && previewController_->active() &&
      !previewController_->rebind(previewProvider_->generation())) {
    return false;
  }
  ++resizeEvents_;
  return true;
}

bool InkPlaygroundHost::loseSurface() noexcept {
  if (surface_.generation == 0U) return false;
  if (appBinding_ == nullptr ||
      appBinding_->markSurfaceLost() != render::SurfaceProviderDisposition::kCommitted) {
    return false;
  }
  surface_.available = false;
  if (appBinding_->markPreviewSurfaceLost() != render::SurfaceProviderDisposition::kCommitted) {
    return false;
  }
  (void)lifecycle_->markLost(render::ViewId{1});
  ++surfaceLostEvents_;
  return true;
}

bool InkPlaygroundHost::rebindSurface() noexcept {
  if (appBinding_ == nullptr ||
      appBinding_->rebindSurface() != render::SurfaceProviderDisposition::kCommitted ||
      appBinding_->rebindPreviewSurface() != render::SurfaceProviderDisposition::kCommitted) {
    return false;
  }
  surface_.generation = appBinding_->surfaces().lifecycle().current().surfaceGeneration.value();
  surface_.available = true;
  if (previewController_ != nullptr && !previewController_->rebind(previewProvider_->generation())) return false;
  ++rebindEvents_;
  return true;
}

bool InkPlaygroundHost::rebindPreviewSurface() noexcept {
  if (appBinding_ == nullptr || previewProvider_ == nullptr ||
      appBinding_->rebindPreviewSurface() !=
          render::SurfaceProviderDisposition::kCommitted) {
    return false;
  }
  return previewController_ == nullptr || !previewController_->active() ||
         previewController_->rebind(previewProvider_->generation());
}

bool InkPlaygroundHost::registerSurfaceProvider(
    std::string profileId, std::unique_ptr<render::SkiaSurfaceProvider> provider) noexcept {
  if (appBinding_ == nullptr || provider == nullptr || profileId.empty()) return false;
  auto info = provider->describe();
  info.profileId = profileId;
  if (appBinding_->registerSurfaceProfile(render::SurfaceProfile{info, std::move(provider)}) !=
      render::SurfaceProviderDisposition::kCommitted) return false;
  if (appBinding_->selectRenderProfile(profileId, info.format) !=
      render::SurfaceProviderDisposition::kCommitted) return false;
  surface_.generation = appBinding_->surfaces().lifecycle().current().surfaceGeneration.value();
  surface_.width = appBinding_->surfaces().activeInfo().metrics.physicalWidth;
  surface_.height = appBinding_->surfaces().activeInfo().metrics.physicalHeight;
  surface_.available = true;
  lifecycle_ = std::make_unique<render::SurfaceLifecycle>(render::SurfaceSnapshot{
      render::ViewId{1}, render::SurfaceGeneration{surface_.generation},
      render::MetricsGeneration{surface_.generation},
      appBinding_->surfaces().activeInfo().metrics});
  tracker_ = std::make_unique<render::PresentationTracker>(*lifecycle_);
  return true;
}

bool InkPlaygroundHost::registerPreviewSurfaceProvider(
    std::string profileId, std::unique_ptr<render::SkiaSurfaceProvider> provider) noexcept {
  if (appBinding_ == nullptr || provider == nullptr || profileId.empty()) {
    AXIOM_ANDROID_DIAG("preview registration missing binding/provider/profile");
    return false;
  }
  auto info = provider->describe();
  info.profileId = profileId;
  const auto registered = appBinding_->registerPreviewSurfaceProfile(
      render::SurfaceProfile{info, std::move(provider)});
  if (registered != render::SurfaceProviderDisposition::kCommitted) {
    AXIOM_ANDROID_DIAG("preview registration rejected code=%u",
                       static_cast<unsigned>(registered));
    return false;
  }
  const auto selection = appBinding_->selectPreviewRenderProfile(profileId, info.format);
  if (selection != render::SurfaceProviderDisposition::kCommitted) {
    AXIOM_ANDROID_DIAG("preview selection rejected code=%u", static_cast<unsigned>(selection));
    return false;
  }
  auto* selected = appBinding_->previewSurfaces()->activeProvider();
  if (selected == nullptr) return false;
  previewProvider_ = selected;
  previewController_ = std::make_unique<render::PreviewSurfaceController>(skiaRenderer_, *previewProvider_);
  return true;
}

render::SkiaSurfaceProvider* InkPlaygroundHost::activeSurfaceProvider() noexcept {
  return appBinding_ == nullptr ? nullptr : appBinding_->surfaces().activeProvider();
}

render::SkiaSurfaceProvider* InkPlaygroundHost::previewSurfaceProvider() noexcept {
  return previewProvider_;
}

render::CanonicalViewportTransform InkPlaygroundHost::previewViewport() const noexcept {
  return previewController_ == nullptr
      ? render::CanonicalViewportTransform{}
      : previewController_->geometry().viewport;
}

const render::SurfaceRenderState& InkPlaygroundHost::previewRenderState() const noexcept {
  static const render::SurfaceRenderState unavailable{};
  return previewController_ == nullptr ? unavailable : previewController_->state();
}

std::uint64_t InkPlaygroundHost::previewSurfaceGeneration() const noexcept {
  return previewProvider_ == nullptr ? 0U : previewProvider_->generation();
}

std::uint64_t InkPlaygroundHost::previewSubmissionCount() const noexcept {
  return previewProvider_ == nullptr ? 0U : previewProvider_->presentCount();
}

std::uint64_t InkPlaygroundHost::previewPresentCount() const noexcept {
  return previewProvider_ == nullptr ? 0U : previewProvider_->presentCount();
}

bool InkPlaygroundHost::previewActive() const noexcept {
  return previewController_ != nullptr && previewController_->active();
}

bool InkPlaygroundHost::presentCanonicalFrame(std::uint64_t frameId,
                                              double frameMs,
                                              bool retirePreview) noexcept {
  AXIOM_ANDROID_DIAG("present begin frame=%llu objects=%zu scene_records=%zu", static_cast<unsigned long long>(frameId), semanticObjects_.size(), sceneCoordinator_ == nullptr ? 0U : sceneCoordinator_->runtimeScene().records().size());
  if (!surface_.available || frameId == 0U) { AXIOM_ANDROID_DIAG("present reject surface=%d frame=%llu", surface_.available ? 1 : 0, static_cast<unsigned long long>(frameId)); return false; }
  if (runtimeSceneHost_ == nullptr || sceneCoordinator_ == nullptr ||
      activeSurfaceProvider() == nullptr) { AXIOM_ANDROID_DIAG("present reject missing deps"); return false; }
  const auto semanticGeneration = semanticGeneration_.current();
  if (runtimeSceneHost_->semanticGeneration() != semanticGeneration) { AXIOM_ANDROID_DIAG("present reject scene generation"); return false; }
  const auto viewport = viewportController_->state();
  const float zoom = viewport.scale > 0.0F && std::isfinite(viewport.scale)
      ? viewport.scale : 1.0F;
  const float centerX = static_cast<float>(surface_.width) * 0.5F;
  const float centerY = static_cast<float>(surface_.height) * 0.5F;
  // Visibility queries are expressed in world space, while the platform
  // surface bounds are view-space pixels.  Convert all four view corners
  // through the inverse camera transform before querying the Scene.  Using
  // {0,0,width,height} here silently drops strokes whose entire world-space
  // bounds are outside that screen-space rectangle (the Android "only lines
  // crossing the centre become canonical" symptom).
  const auto viewToWorld = [&](float viewX, float viewY) {
    return foundation::WorldPoint{
        (viewX - viewport.translationX) / zoom,
        (viewY - viewport.translationY) / zoom};
  };
  const std::array<foundation::WorldPoint, 4> worldCorners{
      viewToWorld(0.0F, 0.0F), viewToWorld(static_cast<float>(surface_.width), 0.0F),
      viewToWorld(0.0F, static_cast<float>(surface_.height)),
      viewToWorld(static_cast<float>(surface_.width), static_cast<float>(surface_.height))};
  foundation::WorldRect worldViewport{worldCorners[0].x, worldCorners[0].y,
                                      worldCorners[0].x, worldCorners[0].y};
  for (std::size_t i = 1; i < worldCorners.size(); ++i) {
    worldViewport.left = std::min(worldViewport.left, worldCorners[i].x);
    worldViewport.top = std::min(worldViewport.top, worldCorners[i].y);
    worldViewport.right = std::max(worldViewport.right, worldCorners[i].x);
    worldViewport.bottom = std::max(worldViewport.bottom, worldCorners[i].y);
  }
  const render::FrameState frame{
      render::ViewId{1},
      render::CameraState{foundation::WorldPoint{
                              (centerX - viewport.translationX) / zoom,
                              (centerY - viewport.translationY) / zoom},
                          zoom, 0.0F,
                          render::CameraGeneration{1}},
      // ReferenceDrawList's clip is a view-space contract in the current
      // renderer authority. Keep it covering the physical target; the
      // camera above carries the actual pinch transform.
      worldViewport,
      render::SurfaceMetrics{static_cast<float>(surface_.width),
                             static_cast<float>(surface_.height), surface_.width,
                             surface_.height, 1.0F, 1.0F},
      semanticGeneration, runtimeSceneHost_->revision(),
      render::SurfaceGeneration{surface_.generation},
      render::MetricsGeneration{surface_.generation}, render::FrameId{frameId},
      foundation::WorldRect{0.0F, 0.0F, static_cast<float>(surface_.width),
                            static_cast<float>(surface_.height)}};
  if (!sceneCoordinator_->runtimeScene().records().empty()) {
    const auto visibility = render::VisibilityResolver::resolve(frame, *runtimeSceneHost_);
    if (!visibility) { AXIOM_ANDROID_DIAG("present visibility failed"); return false; }
    const auto references = render::DirectReferenceSource::build(
        frame, visibility.value(), sceneCoordinator_->runtimeScene());
    if (!references) { AXIOM_ANDROID_DIAG("present references failed"); return false; }
    const auto plan = render::FramePlanBuilder::build(frame, references.value());
    if (!plan) { AXIOM_ANDROID_DIAG("present plan failed"); return false; }
    const auto rendered = skiaRenderer_.renderFrame(*activeSurfaceProvider(), plan.value());
    if (rendered.code != render::BackendSubmissionCode::kAccepted) {
      AXIOM_ANDROID_DIAG("present skia failed: %s framegen=%llu providergen=%llu",
                         rendered.message.c_str(),
                         static_cast<unsigned long long>(frame.surfaceGeneration.value()),
                         static_cast<unsigned long long>(activeSurfaceProvider()->generation()));
      return false;
    }
  }
  const auto trackerSubmit = tracker_->submit(frame);
  if (trackerSubmit != render::PresentFeedbackDisposition::kSubmitted) { AXIOM_ANDROID_DIAG("present tracker submit failed code=%d framegen=%llu livegen=%llu metrics=%ux%u", static_cast<int>(trackerSubmit), static_cast<unsigned long long>(frame.surfaceGeneration.value()), static_cast<unsigned long long>(lifecycle_->current().surfaceGeneration.value()), frame.metrics.physicalWidth, frame.metrics.physicalHeight); return false; }
  const auto feedback = tracker_->receive(render::PresentedFeedback{
      frame.viewId, frame.frameId, frame.surfaceGeneration, frame.metricsGeneration,
      render::PresentOutcome::kPresented, render::PresentEvidenceKind::kPlatformQualified,
      std::nullopt});
  if (feedback != render::PresentFeedbackDisposition::kPresented) { AXIOM_ANDROID_DIAG("present feedback failed"); return false; }
  recordPresentation("canonical-presented-platform-qualified", 0U, frameMs);
  ++canonicalFrameCount_;
  if (retirePreview && semanticObjects_.size() != 0U && previewController_ != nullptr &&
      previewController_->active()) {
    std::vector<std::uint64_t> retired;
    for (const auto& [pointer, identity] : pendingCanonicalIdentities_) {
      if (canonicalVisibilitySink_ != nullptr) {
        const auto handoff = canonicalVisibilitySink_->canonicalVisible(identity, frame);
        if (handoff == ink::HandoffResult::kRejected) return false;
        if (handoff != ink::HandoffResult::kAccepted) continue;
      }
      if (!previewController_->retireSession(identity.session,
                                             previewProvider_->generation())) {
        return false;
      }
      retired.push_back(pointer);
    }
    for (const auto pointer : retired) {
      pendingCanonicalIdentities_.erase(pointer);
    }
  }
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
