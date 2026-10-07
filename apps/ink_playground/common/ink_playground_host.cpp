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
#include <unordered_set>
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
      sceneCompiler_(std::make_unique<PlaygroundSceneCompiler>()),
      history_(*this) {}

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
  if (selectionMode_) {
    for (const auto& sample : routed.normalized.samples) {
      if (!sample.predicted) (void)selectionPointer(sample.key.pointer, sample.phase, sample.x, sample.y);
    }
    if (routed.terminalCancel) cancelSelectionTransform();
    return routed.accepted || routed.terminalCancel;
  }
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
      if (toolMode_ != ToolMode::kBrush) {
        if (!eraserBegin(sample.key.pointer)) return false;
        // Let the down sample publish the initial eraser footprint below.
      } else {
        const auto profile = selectedBrushProfile_ == "vector-solid-v1" ? 1U
            : selectedBrushProfile_ == "marker-flat-v1" ? 2U
            : selectedBrushProfile_ == "chalk-grain-v1" ? 3U : 4U;
        if (!beginBrushSession(sample.key.pointer, profile)) { AXIOM_ANDROID_DIAG("begin failed pointer=%llu seq=%llu", static_cast<unsigned long long>(sample.key.pointer), static_cast<unsigned long long>(sample.sequence)); return false; }
      }
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
    if (routing.routesToInk && toolMode_ == ToolMode::kBrush && sample.phase != input::PointerPhase::kCancel) {
      if (!appendBrushSample(sample.key.pointer, routing.routedSample.x,
                             routing.routedSample.y, sample.pressure,
                             sample.sequence, sample.predicted, false)) { AXIOM_ANDROID_DIAG("append failed pointer=%llu seq=%llu phase=%d", static_cast<unsigned long long>(sample.key.pointer), static_cast<unsigned long long>(sample.sequence), static_cast<int>(sample.phase)); return false; }
      previewDirty = true;
    }
    if (routing.routesToInk && toolMode_ != ToolMode::kBrush &&
        sample.phase != input::PointerPhase::kCancel) {
      if (!eraserSample(sample.key.pointer, routing.routedSample.x, routing.routedSample.y)) return false;
    }
    if (sample.phase == input::PointerPhase::kUp) {
      if (brushSessions_.contains(sample.key.pointer) &&
          !finishBrushSession(sample.key.pointer)) { AXIOM_ANDROID_DIAG("finish failed pointer=%llu seq=%llu", static_cast<unsigned long long>(sample.key.pointer), static_cast<unsigned long long>(sample.sequence)); return false; }
      if (eraserTraces_.contains(sample.key.pointer) && !eraserFinish(sample.key.pointer)) return false;
      baselineDownTimestamps_.erase(sample.key);
    } else if (sample.phase == input::PointerPhase::kCancel) {
      (void)cancelBrushSession(sample.key.pointer);
      (void)eraserCancel(sample.key.pointer);
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
    if (platformPresentationDeferred_) {
      // Windows consumes this invalidation from its render pump. Keeping it
      // in Runtime makes the scheduling decision explicit without doing a
      // fence wait from the input callback.
      canonicalPresentationDirty_ = true;
    } else if (!presentCanonicalFrame(canonicalFrameCount_ + 1U, 0.0, false)) {
      return false;
    }
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
  cancelSelectionTransform();
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
  eraserTraces_.clear();
  eraserPreviewRevisions_.clear();
}

bool InkPlaygroundHost::beginBrushSession(std::uint64_t pointerId,
                                          std::uint32_t profile) noexcept {
  const auto profileId = profile == 1U ? "vector-solid-v1"
      : profile == 2U ? "marker-flat-v1"
      : profile == 3U ? "chalk-grain-v1" : profile == 4U ? "membrane-v1" : "";
  if (*profileId == '\0') return false;
  const auto loaded = brushCatalog_.loadDefault(profileId,
      profileId == selectedBrushProfile_ ? selectedBrushRevision_ : 1U);
  if (!loaded) return false;
  return beginBrushSession(pointerId, loaded.package, 0x4500ULL + pointerId);
}

bool InkPlaygroundHost::beginBrushSession(std::uint64_t pointerId,
                                          const ink::BrushPackage& package,
                                          std::uint64_t seed) noexcept {
  std::lock_guard lock(previewStateMutex_);
  if (pointerId == 0U ||
      (package.profileId != "vector-solid-v1" && package.profileId != "marker-flat-v1" &&
       package.profileId != "chalk-grain-v1" && package.profileId != "membrane-v1") ||
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

bool InkPlaygroundHost::selectTool(ToolMode mode) noexcept {
  if (!brushSessions_.empty()) return false;
  toolMode_ = mode;
  return true;
}

bool InkPlaygroundHost::setSelectionMode(bool enabled) noexcept {
  if (!brushSessions_.empty() || !eraserTraces_.empty() || !keyedStrokeIds_.empty()) {
    return false;
  }
  selectionMode_ = enabled;
  if (!enabled) { cancelSelectionTransform(); selection_.cancel(); selectionView_.reset(); }
  canonicalPresentationDirty_ = true;
  return true;
}

bool InkPlaygroundHost::selectAtViewPoint(float x, float y) noexcept {
  if (!selectionMode_ || runtimeSceneHost_ == nullptr ||
      !std::isfinite(x) || !std::isfinite(y)) return false;
  const auto content = viewportController_->viewToContent(x, y);
  const auto tested = runtimeSceneHost_->hitTest(canvas::HitTestRequest{
      foundation::WorldPoint{content.first, content.second}, 4.0F,
      canvas::HitTestFilter{canvas::HitTestKindMask::kAll, false}, 1U});
  if (!tested || tested.value().frontToBack.empty()) {
    selection_.cancel();
    selectionView_.reset();
    canonicalPresentationDirty_ = true;
    return true;
  }
  const bool selected = selection_.click(tested.value().frontToBack.front());
  if (selected) canonicalPresentationDirty_ = true;
  return selected && renderSelectionOverlay();
}

bool InkPlaygroundHost::renderSelectionOverlay() noexcept {
  if (!selectionMode_ || selection_.primary().isZero() ||
      sceneCoordinator_ == nullptr || surface_.width == 0U || surface_.height == 0U) {
    selectionView_.reset();
    return false;
  }
  const auto* record = sceneCoordinator_->runtimeScene().find(selection_.primary());
  if (record == nullptr || !record->worldBounds.isFiniteAndOrdered()) {
    selectionView_.reset();
    return false;
  }
  const auto viewport = viewportController_->state();
  const float zoom = viewport.scale > 0.0F && std::isfinite(viewport.scale)
      ? viewport.scale : 1.0F;
  const auto center = foundation::WorldPoint{
      (static_cast<float>(surface_.width) * 0.5F - viewport.translationX) / zoom,
      (static_cast<float>(surface_.height) * 0.5F - viewport.translationY) / zoom};
  const auto frame = render::FrameState{
      render::ViewId{1},
      render::CameraState{center, zoom, 0.0F, render::CameraGeneration{1}},
      foundation::WorldRect{-1.0e9F, -1.0e9F, 1.0e9F, 1.0e9F},
      render::SurfaceMetrics{static_cast<float>(surface_.width),
                             static_cast<float>(surface_.height), surface_.width,
                             surface_.height, 1.0F, 1.0F},
      semanticGeneration_.current(), runtimeSceneHost_->revision(),
      render::SurfaceGeneration{surface_.generation},
      render::MetricsGeneration{surface_.generation},
      render::FrameId{canonicalFrameCount_ + 1U},
      foundation::WorldRect{0.0F, 0.0F, static_cast<float>(surface_.width),
                            static_cast<float>(surface_.height)}};
  selectionView_ = std::make_unique<render::RenderViewRuntime>(frame);
  return selectionView_->editingOverlay().update(render::EditingOverlayInput{
      record->objectId, record->visualBounds,
      transformOverrides_.find(record->objectId) == nullptr ? record->transform
          : *transformOverrides_.find(record->objectId),
      true, false, transformDrag_.active(),
      render::HandleCapabilities{true, true, true}});
}

void InkPlaygroundHost::cancelSelectionTransform() noexcept {
  transformDrag_.cancel();
  editingPointer_ = 0;
  editingChanged_ = false;
  canonicalPresentationDirty_ = true;
}

bool InkPlaygroundHost::selectionPointer(std::uint64_t pointer, input::PointerPhase phase,
                                         float x, float y) noexcept {
  if (!selectionMode_ || pointer == 0U) return false;
  if (phase == input::PointerPhase::kCancel) {
    if (editingPointer_ == pointer) cancelSelectionTransform();
    return true;
  }
  if (!std::isfinite(x) || !std::isfinite(y)) {
    if (editingPointer_ == pointer) cancelSelectionTransform();
    return false;
  }
  const auto world = viewportController_->viewToContent(x, y);
  if (phase == input::PointerPhase::kDown) {
    if (editingPointer_ != 0U) return true;
    (void)renderSelectionOverlay();
    editingHandle_ = selectionOverlay() == nullptr ? render::HandleKind::kNone
        : selectionOverlay()->hitTest({x,y});
    bool moveSelected = false;
    if (editingHandle_ == render::HandleKind::kNone && runtimeSceneHost_ != nullptr) {
      const auto hit = runtimeSceneHost_->hitTest(canvas::HitTestRequest{
          {world.first,world.second},4.0F,{canvas::HitTestKindMask::kAll,false},1U});
      moveSelected = hit && !hit.value().frontToBack.empty() &&
          hit.value().frontToBack.front() == selection_.primary();
    }
    if (editingHandle_ == render::HandleKind::kNone && !moveSelected) {
      (void)selectAtViewPoint(x,y);
      editingPointer_ = pointer;
      return true;
    }
    const auto* object = semanticObjects_.find(selection_.primary());
    if (object == nullptr) return false;
    const auto bounds = canvas::computeBounds(*object);
    const auto& t = object->transform;
    if (!bounds.finite || std::abs(t.a*t.d-t.b*t.c) < 1e-12) return false;
    const std::array targets{std::pair{object->id,t}};
    if (!transformDrag_.begin(targets)) return false;
    editingPointer_ = pointer;
    editingInitial_ = t;
    editingLocalBounds_ = bounds.visual;
    editingDownWorld_ = {world.first,world.second};
    editingGeneration_ = semanticGeneration_.current();
    editingChanged_ = false;
    return true;
  }
  if (pointer != editingPointer_) return true;
  if (transformDrag_.active()) {
    if (editingGeneration_ != semanticGeneration_.current()) {
      cancelSelectionTransform(); return false;
    }
    const auto& t = editingInitial_;
    semantic::Transform2D next=t;
    if (editingHandle_ == render::HandleKind::kNone) {
      next.tx += world.first-editingDownWorld_.x;
      next.ty += world.second-editingDownWorld_.y;
    } else if (editingHandle_ == render::HandleKind::kRotation) {
      const double cx=(editingLocalBounds_.left+editingLocalBounds_.right)*0.5;
      const double cy=(editingLocalBounds_.top+editingLocalBounds_.bottom)*0.5;
      const double wx=t.a*cx+t.c*cy+t.tx, wy=t.b*cx+t.d*cy+t.ty;
      const double angle=std::atan2(world.second-wy,world.first-wx) -
          std::atan2(editingDownWorld_.y-wy,editingDownWorld_.x-wx);
      // Preserve the exact initial transform for a no-op release. Computing
      // center + (translation - center) can introduce rounding-only edits.
      if (angle != 0.0) {
        const double c=std::cos(angle),s=std::sin(angle);
        next={c*t.a-s*t.b,s*t.a+c*t.b,c*t.c-s*t.d,s*t.c+c*t.d,
              wx+c*(t.tx-wx)-s*(t.ty-wy),wy+s*(t.tx-wx)+c*(t.ty-wy)};
      }
    } else {
      using H=render::HandleKind;
      const bool left=editingHandle_==H::kLeft||editingHandle_==H::kTopLeft||editingHandle_==H::kBottomLeft;
      const bool right=editingHandle_==H::kRight||editingHandle_==H::kTopRight||editingHandle_==H::kBottomRight;
      const bool top=editingHandle_==H::kTop||editingHandle_==H::kTopLeft||editingHandle_==H::kTopRight;
      const bool bottom=editingHandle_==H::kBottom||editingHandle_==H::kBottomLeft||editingHandle_==H::kBottomRight;
      const double ax=left?editingLocalBounds_.right:editingLocalBounds_.left;
      const double ay=top?editingLocalBounds_.bottom:editingLocalBounds_.top;
      const double det=t.a*t.d-t.b*t.c;
      const double dx=world.first-editingDownWorld_.x,dy=world.second-editingDownWorld_.y;
      const double localDx=(t.d*dx-t.c*dy)/det, localDy=(-t.b*dx+t.a*dy)/det;
      const double width=editingLocalBounds_.right-editingLocalBounds_.left;
      const double height=editingLocalBounds_.bottom-editingLocalBounds_.top;
      const double sx=(left||right)&&width>1e-6 ? std::clamp(1.0+localDx/(left?-width:width),0.01,100.0):1.0;
      const double sy=(top||bottom)&&height>1e-6 ? std::clamp(1.0+localDy/(top?-height:height),0.01,100.0):1.0;
      next={t.a*sx,t.b*sx,t.c*sy,t.d*sy,t.tx+t.a*ax*(1-sx)+t.c*ay*(1-sy),
                                         t.ty+t.b*ax*(1-sx)+t.d*ay*(1-sy)};
    }
    editingChanged_ = !(next==editingInitial_);
    const std::array values{std::pair{selection_.primary(),next}};
    if (!transformDrag_.preview(values)) { cancelSelectionTransform(); return false; }
    canonicalPresentationDirty_ = true;
    if (phase == input::PointerPhase::kUp) {
      const bool accepted=!editingChanged_ || transformDrag_.commit();
      cancelSelectionTransform();
      return accepted;
    }
  }
  if (phase == input::PointerPhase::kUp) editingPointer_=0;
  return true;
}

interaction::SubmitResult InkPlaygroundHost::submit(const semantic::SetTransformsOp& transforms) {
  if (transforms.items.size()!=1U || !historySceneReady() ||
      editingGeneration_!=semanticGeneration_.current()) return interaction::SubmitResult::rejected();
  const auto* before=semanticObjects_.find(transforms.items.front().object_id);
  if (before==nullptr || before->id!=selection_.primary()) return interaction::SubmitResult::rejected();
  const auto forwardId=allocateOperationId(), inverseId=allocateOperationId();
  if (forwardId.isZero() || inverseId.isZero()) return interaction::SubmitResult::rejected();
  const semantic::Operation operation{forwardId,documentId_,1U,1U,transforms};
  semantic::Operation inverse{inverseId,documentId_,1U,1U,
      semantic::SetTransformsOp{{{before->id,before->transform}}}};
  const auto applied=operationEngine_.apply(operation,semantic::ApplySource::kLocalInteraction,
      semanticObjects_,appliedOperations_,semanticGeneration_,canonicalCommitClock_);
  if (applied.disposition!=semantic::ApplyDisposition::kApplied) return interaction::SubmitResult::rejected();
  (void)history_.record(operation,{std::move(inverse)});
  ++submittedOperationCount_;
  if (applied.commit_record.has_value()) {
    const semantic::SemanticReadView view(semanticObjects_,semanticGeneration_.current());
    const canvas::SceneCommitInput input(applied.commit_record->before_generation,
        applied.commit_record->after_generation,view,&applied.commit_record->change_set);
    (void)sceneCoordinator_->apply(*sceneCompiler_,input);
  }
  canonicalPresentationDirty_=true;
  return interaction::SubmitResult::acceptedResult();
}

bool InkPlaygroundHost::selectBrushProfile(std::string_view profileId,
                                           std::uint32_t revision) noexcept {
  if (!brushSessions_.empty() ||
      (revision != 1U && !(profileId == "chalk-grain-v1" &&
                            (revision == 2U || revision == 3U || revision == 4U)))) return false;
  const auto loaded = brushCatalog_.loadDefault(profileId, revision);
  if (!loaded) return false;
  selectedBrushProfile_ = std::string(profileId);
  selectedBrushRevision_ = revision;
  return true;
}

bool InkPlaygroundHost::eraserBegin(std::uint64_t pointerId) noexcept {
  std::lock_guard lock(previewStateMutex_);
  if (toolMode_ == ToolMode::kBrush || pointerId == 0U || eraserTraces_.contains(pointerId)) return false;
  eraserTraces_[pointerId] = {};
  eraserPreviewRevisions_[pointerId] = 0U;
  if (previewController_ == nullptr || previewProvider_ == nullptr ||
      !previewController_->begin(1U, pointerId, 1U, previewProvider_->generation())) {
    eraserTraces_.erase(pointerId);
    eraserPreviewRevisions_.erase(pointerId);
    return false;
  }
  return true;
}

bool InkPlaygroundHost::eraserSample(std::uint64_t pointerId, double x, double y) noexcept {
  std::lock_guard lock(previewStateMutex_);
  const auto it = eraserTraces_.find(pointerId);
  if (it == eraserTraces_.end() || !std::isfinite(x) || !std::isfinite(y)) return false;
  it->second.push_back({static_cast<float>(x), static_cast<float>(y)});
  constexpr std::size_t kSegments = 24U;
  constexpr double kRadius = 18.0;
  ink::BrushPreviewDelta delta;
  delta.revision = ++eraserPreviewRevisions_[pointerId];
  const auto& trace = it->second;
  if (trace.size() == 1U) {
    delta.outline.reserve(kSegments);
    for (std::size_t i = 0; i < kSegments; ++i) {
      const double angle = 2.0 * 3.14159265358979323846 * static_cast<double>(i) /
                           static_cast<double>(kSegments);
      delta.outline.push_back({x + kRadius * std::cos(angle),
                               y + kRadius * std::sin(angle)});
    }
  } else {
    // Build one closed swept-band contour from the retained eraser trace.
    // This keeps the erased region visibly continuous instead of showing
    // only the latest pointer-centered circle.
    std::vector<ink::reference::StrokeOutlinePoint> left;
    std::vector<ink::reference::StrokeOutlinePoint> right;
    left.reserve(trace.size());
    right.reserve(trace.size());
    for (std::size_t i = 0; i < trace.size(); ++i) {
      const auto& current = trace[i];
      const auto& previous = trace[i == 0U ? i : i - 1U];
      const auto& next = trace[i + 1U < trace.size() ? i + 1U : i];
      const double dx = static_cast<double>(next.x - previous.x);
      const double dy = static_cast<double>(next.y - previous.y);
      const double length = std::hypot(dx, dy);
      const double nx = length > 0.0 ? -dy / length : 0.0;
      const double ny = length > 0.0 ? dx / length : 1.0;
      left.push_back({current.x + kRadius * nx, current.y + kRadius * ny});
      right.push_back({current.x - kRadius * nx, current.y - kRadius * ny});
    }
    delta.outline.reserve(left.size() + right.size());
    delta.outline.insert(delta.outline.end(), left.begin(), left.end());
    for (auto reverse = right.rbegin(); reverse != right.rend(); ++reverse) {
      delta.outline.push_back(*reverse);
    }
  }
  const auto viewport = viewportController_->state();
  render::PreviewStyleOverride style;
  if (toolMode_ == ToolMode::kPartialEraser) {
    // Preview is an independent transparent overlay.  For partial erase we
    // paint the swept region with the canvas background so the user sees the
    // final visual effect grow sample-by-sample; semantic masks are still
    // committed only by eraserFinish(). Object eraser keeps the amber tool
    // indicator and its existing commit-time disappearance semantics.
    style.red = 1.0F;
    style.green = 1.0F;
    style.blue = 1.0F;
    style.alpha = 1.0F;
    style.opacityMultiplier = 1.0F;
    style.overrideColor = true;
    style.overrideOpacity = true;
  }
  previewController_->setPresentationStyle(style);
  return previewController_->updateForSession(pointerId, delta, viewport.scale,
                                               viewport.translationX,
                                               viewport.translationY) &&
         (platformPresentationDeferred_ ? true : previewController_->renderIfDirty(style));
}

bool InkPlaygroundHost::eraserFinish(std::uint64_t pointerId) noexcept {
  std::lock_guard lock(previewStateMutex_);
  const auto it = eraserTraces_.find(pointerId);
  if (it == eraserTraces_.end()) return false;
  if (it->second.empty() || runtimeSceneHost_ == nullptr) {
    eraserTraces_.erase(it);
    eraserPreviewRevisions_.erase(pointerId);
    if (previewController_ != nullptr && previewController_->active()) {
      (void)previewController_->cancelSession(pointerId);
      (void)previewController_->retireSession(pointerId, previewProvider_->generation());
    }
    return false;
  }
  std::unordered_set<foundation::ObjectId, foundation::ObjectIdHash> hit;
  for (const auto& point : it->second) {
    const auto tested = runtimeSceneHost_->hitTest(canvas::HitTestRequest{
        point, 18.0F, canvas::HitTestFilter{static_cast<canvas::HitTestKindMask>(
            static_cast<std::uint32_t>(canvas::HitTestKindMask::kVectorStroke) |
            static_cast<std::uint32_t>(canvas::HitTestKindMask::kDabStroke)), false}, 64U});
    if (!tested) {
      eraserTraces_.erase(it);
      eraserPreviewRevisions_.erase(pointerId);
      if (previewController_ != nullptr && previewController_->active() &&
          previewProvider_ != nullptr) {
        (void)previewController_->cancelSession(pointerId);
        (void)previewController_->retireSession(pointerId, previewProvider_->generation());
      }
      return false;
    }
    for (const auto id : tested.value().frontToBack) hit.insert(id);
  }
  if (hit.empty()) {
    eraserTraces_.erase(it);
    eraserPreviewRevisions_.erase(pointerId);
    if (previewController_ != nullptr && previewController_->active()) {
      (void)previewController_->cancelSession(pointerId);
      (void)previewController_->retireSession(pointerId, previewProvider_->generation());
    }
    return true;
  }
  semantic::Operation operation;
  const auto operationOrdinal = localOperationOrdinal();
  if (operationOrdinal == 0U) return false;
  operation.id = semantic::OperationId(foundation::ObjectId::fromUint64(operationOrdinal));
  operation.document_id = documentId_;
  operation.schema_version = 1U;
  operation.payload_version = 1U;
  if (toolMode_ == ToolMode::kObjectEraser) {
    semantic::DeleteObjectsOp payload;
    payload.object_ids.assign(hit.begin(), hit.end());
    std::sort(payload.object_ids.begin(), payload.object_ids.end());
    operation.payload = std::move(payload);
  } else {
    constexpr double radius = 18.0;
    semantic::AddEraseMasksOp maskPayload;
    std::size_t replacementOrdinal = 0U;
    for (const auto id : hit) {
      // Keep the canonical stroke intact and attach a renderer-neutral mask.
      // Splitting by deleting outline vertices and implicitly closing each
      // remainder manufactures long phantom edges (and the visible arcs/
      // triangles reported in the Web UI). SplitStrokes needs a true polygon
      // clipper; until that authority exists, fail over to the same mask
      // representation used by marker/chalk.
      if (it->second.empty()) continue;
      const auto* object = semanticObjects_.find(id);
      if (object == nullptr) return false;
      const auto& transform = object->transform;
      const double determinant = transform.a * transform.d - transform.b * transform.c;
      if (!std::isfinite(determinant) || determinant == 0.0) return false;
      const auto localPoint = [&](const semantic::Vec2& point) {
        const double x = point.x - transform.tx;
        const double y = point.y - transform.ty;
        return semantic::Vec2{(transform.d * x - transform.c * y) / determinant,
                              (transform.a * y - transform.b * x) / determinant};
      };
      // EraseMaskGeometry is object-local. The retained trace and preview
      // are world-space; convert at this operation construction boundary.
      semantic::SweptCircleMask swept;
      for (std::size_t i = 1; i < it->second.size(); ++i) {
        const auto& a = it->second[i - 1U];
        const auto& b = it->second[i];
        swept.segments.push_back({
            {{a.x, a.y}, radius}, {{b.x, b.y}, radius},
            {a.x + (b.x - a.x) / 3.0F, a.y + (b.y - a.y) / 3.0F},
            {a.x + (b.x - a.x) * 2.0F / 3.0F, a.y + (b.y - a.y) * 2.0F / 3.0F}});
      }
      if (swept.segments.empty()) {
        const auto& p = it->second.front();
        swept.segments.push_back({{{p.x, p.y}, radius}, {{p.x, p.y}, radius},
                                  {p.x, p.y}, {p.x, p.y}});
      }
      semantic::EraseMaskGeometry geometry;
      const double scaleX = std::hypot(transform.a, transform.b);
      const double scaleY = std::hypot(transform.c, transform.d);
      const double scale = std::max(scaleX, scaleY);
      const bool similarity = std::abs(scaleX - scaleY) <= 1e-12 * scale &&
          std::abs((transform.a / scale) * (transform.c / scale) +
                   (transform.b / scale) * (transform.d / scale)) <= 1e-12;
      if (similarity) {
        for (auto& segment : swept.segments) {
          segment.p0.position = localPoint(segment.p0.position);
          segment.p1.position = localPoint(segment.p1.position);
          segment.control1 = localPoint(segment.control1);
          segment.control2 = localPoint(segment.control2);
          segment.p0.radius /= scaleX;
          segment.p1.radius /= scaleX;
        }
        geometry = std::move(swept);
      } else {
        // A world circle is an ellipse in a nonuniformly scaled/sheared
        // object's local space. Use the existing FilledPathMask variant for
        // the inverse-mapped union of round caps and connecting rectangles.
        // Never approximate it with a single local radius.
        semantic::FilledPathMask filled;
        auto& commands = filled.path.commands;
        constexpr double kCircleControl = 0.5522847498307936;
        for (const auto& point : it->second) {
          const double x = point.x, y = point.y;
          const double k = radius * kCircleControl;
          commands.push_back(semantic::MoveTo{localPoint({x + radius, y})});
          commands.push_back(semantic::CubicTo{localPoint({x + radius, y + k}),
              localPoint({x + k, y + radius}), localPoint({x, y + radius})});
          commands.push_back(semantic::CubicTo{localPoint({x - k, y + radius}),
              localPoint({x - radius, y + k}), localPoint({x - radius, y})});
          commands.push_back(semantic::CubicTo{localPoint({x - radius, y - k}),
              localPoint({x - k, y - radius}), localPoint({x, y - radius})});
          commands.push_back(semantic::CubicTo{localPoint({x + k, y - radius}),
              localPoint({x + radius, y - k}), localPoint({x + radius, y})});
          commands.push_back(semantic::ClosePath{});
        }
        for (std::size_t i = 1; i < it->second.size(); ++i) {
          const auto& a = it->second[i - 1U];
          const auto& b = it->second[i];
          const double dx = static_cast<double>(b.x) - a.x;
          const double dy = static_cast<double>(b.y) - a.y;
          const double length = std::hypot(dx, dy);
          if (length == 0.0) continue;
          const double nx = -dy * radius / length, ny = dx * radius / length;
          commands.push_back(semantic::MoveTo{localPoint({a.x + nx, a.y + ny})});
          commands.push_back(semantic::LineTo{localPoint({a.x - nx, a.y - ny})});
          commands.push_back(semantic::LineTo{localPoint({b.x - nx, b.y - ny})});
          commands.push_back(semantic::LineTo{localPoint({b.x + nx, b.y + ny})});
          commands.push_back(semantic::ClosePath{});
        }
        geometry = std::move(filled);
      }
      maskPayload.items.push_back({id, {{foundation::ObjectId::fromUint64(
          operationOrdinal * 1000U + ++replacementOrdinal), std::move(geometry)}}});
    }
    operation.payload = std::move(maskPayload);
  }
  semantic::Operation inverse;
  inverse.id = allocateOperationId();
  if (inverse.id.isZero()) return false;
  inverse.document_id = documentId_;
  inverse.schema_version = 1U;
  inverse.payload_version = 1U;
  if (toolMode_ == ToolMode::kObjectEraser) {
    semantic::RestoreObjectsOp restore;
    for (const auto id : std::get<semantic::DeleteObjectsOp>(operation.payload).object_ids) {
      const auto* record = semanticObjects_.find(id);
      if (record != nullptr) restore.objects.push_back(*record);
    }
    inverse.payload = std::move(restore);
  } else {
    semantic::RemoveEraseMasksOp remove;
    for (const auto& item : std::get<semantic::AddEraseMasksOp>(operation.payload).items) {
      semantic::EraseMaskRemoveItem target;
      target.object_id = item.object_id;
      for (const auto& mask : item.masks) target.mask_ids.push_back(mask.id);
      remove.items.push_back(std::move(target));
    }
    inverse.payload = std::move(remove);
  }
  const auto applied = operationEngine_.apply(operation, semantic::ApplySource::kLocalInteraction,
      semanticObjects_, appliedOperations_, semanticGeneration_, canonicalCommitClock_);
  eraserTraces_.erase(it);
  eraserPreviewRevisions_.erase(pointerId);
  if (applied.disposition != semantic::ApplyDisposition::kApplied) return false;
  if (!history_.record(operation, {std::move(inverse)})) return false;
  ++submittedOperationCount_;
  if (applied.commit_record.has_value() && sceneCoordinator_ != nullptr && sceneCompiler_ != nullptr) {
    semantic::SemanticReadView view(semanticObjects_, semanticGeneration_.current());
    canvas::SceneCommitInput input(applied.commit_record->before_generation,
                                   applied.commit_record->after_generation, view,
                                   &applied.commit_record->change_set);
    // Semantic acceptance is final. Scene is derived and can recover at the
    // next frame; a failed projection must not lose the accepted history.
    (void)sceneCoordinator_->apply(*sceneCompiler_, input);
  }
  if (!platformPresentationDeferred_ &&
      !presentCanonicalFrame(canonicalFrameCount_ + 1U, 0.0, false)) return true;
  if (previewController_ != nullptr && previewController_->active()) {
    if (!previewController_->cancelSession(pointerId) || previewProvider_ == nullptr ||
        !previewController_->retireSession(pointerId, previewProvider_->generation())) return false;
  }
  return true;
}

bool InkPlaygroundHost::eraserCancel(std::uint64_t pointerId) noexcept {
  std::lock_guard lock(previewStateMutex_);
  const bool erased = eraserTraces_.erase(pointerId) != 0U;
  eraserPreviewRevisions_.erase(pointerId);
  if (previewController_ != nullptr && previewController_->active()) {
    if (!previewController_->cancelSession(pointerId) || previewProvider_ == nullptr ||
        !previewController_->retireSession(pointerId, previewProvider_->generation())) return false;
  }
  return erased;
}

bool InkPlaygroundHost::appendBrushSample(std::uint64_t pointerId, double x, double y,
                                          double pressure, std::uint64_t sequence,
                                          bool predicted, bool renderPreview) noexcept {
  std::lock_guard lock(previewStateMutex_);
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
  if (renderPreview && !platformPresentationDeferred_ &&
      !previewController_->renderIfDirty()) { AXIOM_ANDROID_DIAG("preview render failed rev=%llu", static_cast<unsigned long long>(delta.revision)); return false; }
  brushPreviews_[pointerId] = std::move(delta.outline);
  brushPreviewDigest_ = hashOutline(brushPreviews_[pointerId]);
  return true;
}

bool InkPlaygroundHost::presentBrushPreview() noexcept {
  std::lock_guard lock(previewStateMutex_);
  return previewController_ != nullptr &&
      previewController_->renderIfDirty(previewController_->presentationStyle());
}

bool InkPlaygroundHost::capturePreviewPresentation(
    PreviewPresentationCapture& capture) noexcept {
  std::lock_guard lock(previewStateMutex_);
  if (previewController_ == nullptr || !previewController_->active() ||
      !previewController_->state().dirty) return false;
  capture.geometry = previewController_->geometry();
  capture.style = previewController_->presentationStyle();
  capture.contentRevision = previewController_->state().contentRevision;
  capture.surfaceGeneration = previewController_->state().generation;
  return true;
}

bool InkPlaygroundHost::renderPreviewPresentation(
    const PreviewPresentationCapture& capture,
    std::uint64_t* presentCount) noexcept {
  std::lock_guard renderLock(previewRenderMutex_);
  std::lock_guard providerLock(previewProviderMutex_);
  render::SkiaSurfaceProvider* provider = nullptr;
  {
    std::lock_guard stateLock(previewStateMutex_);
    if (previewController_ == nullptr || !previewController_->active() ||
        previewProvider_ == nullptr ||
        previewProvider_->generation() != capture.surfaceGeneration) return false;
    provider = previewProvider_;
  }
  const auto result = previewRenderer_.renderPreview(*provider, capture.geometry,
                                                  capture.style);
  if (result.code != render::BackendSubmissionCode::kAccepted) return false;
  if (presentCount != nullptr) *presentCount = provider->presentCount();
  return true;
}

bool InkPlaygroundHost::acknowledgePreviewPresentation(
    const PreviewPresentationCapture& capture, std::uint64_t presentCount) noexcept {
  std::lock_guard providerLock(previewProviderMutex_);
  std::lock_guard stateLock(previewStateMutex_);
  if (previewController_ == nullptr || previewProvider_ == nullptr ||
      previewProvider_->generation() != capture.surfaceGeneration) return false;
  const bool accepted = previewController_->markPresented(
      capture.contentRevision, capture.surfaceGeneration, presentCount);
  if (accepted) previewProvider_->setOverlayVisible(true);
  return accepted;
}

void InkPlaygroundHost::setPreviewOverlayVisible(bool visible) noexcept {
  std::lock_guard providerLock(previewProviderMutex_);
  if (previewProvider_ != nullptr) previewProvider_->setOverlayVisible(visible);
}

void InkPlaygroundHost::setPlatformPresentationDeferred(bool deferred) noexcept {
  platformPresentationDeferred_ = deferred;
  if (previewController_ != nullptr) previewController_->setPresentationDeferred(deferred);
}

bool InkPlaygroundHost::finishBrushSession(std::uint64_t pointerId) noexcept {
  std::lock_guard lock(previewStateMutex_);
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
  const bool usesDabs = (package.profileId == "chalk-grain-v1" &&
                         package.revision >= 2U && package.revision <= 4U) ||
                        package.profileId == "membrane-v1";
  if (usesDabs && replayIntent.dabDigest != intent.dabDigest) return false;
  if (!BrushCommitAdapter::valid(intent, package)) return false;
  brushDigest_ = hashValue(hashValue(hashValue(kFnvOffset, brushSealedOutlineDigest_),
                                     brushReplayDigest_), intent.revision);
  const auto operationId = localOperationOrdinal();
  if (operationId == 0U) return false;
  const auto operation = BrushCommitAdapter::build(
      intent, package, operationId, documentId_);
  semantic::Operation inverse;
  inverse.id = allocateOperationId();
  if (inverse.id.isZero()) return false;
  inverse.document_id = documentId_;
  inverse.schema_version = 1U;
  inverse.payload_version = 1U;
  inverse.payload = semantic::DeleteObjectsOp{{foundation::ObjectId::fromUint64(operationId)}};
  const auto applied = operationEngine_.apply(
      operation, semantic::ApplySource::kLocalInteraction, semanticObjects_,
      appliedOperations_, semanticGeneration_, canonicalCommitClock_);
  if (applied.disposition != semantic::ApplyDisposition::kApplied) {
    return false;
  }
  if (!history_.record(operation, {std::move(inverse)})) return false;
  ++submittedOperationCount_;
  if (applied.commit_record.has_value() && sceneCoordinator_ != nullptr &&
      sceneCompiler_ != nullptr) {
    semantic::SemanticReadView view(semanticObjects_, semanticGeneration_.current());
    canvas::SceneCommitInput sceneInput(
        applied.commit_record->before_generation,
        applied.commit_record->after_generation,
        view, &applied.commit_record->change_set);
    const auto synchronized = sceneCoordinator_->apply(*sceneCompiler_, sceneInput);
    AXIOM_ANDROID_DIAG("scene apply pointer=%llu ok=%d objects=%zu scene_gen=%llu", static_cast<unsigned long long>(pointerId), synchronized ? 1 : 0, semanticObjects_.size(), static_cast<unsigned long long>(semanticGeneration_.current().value()));
  }
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
  std::lock_guard lock(previewStateMutex_);
  const auto it = brushSessions_.find(pointerId);
  if (it == brushSessions_.end()) return false;
  it->second->cancel();
  if (arcPreviewSink_ != nullptr &&
      arcPreviewSink_->cancel(ink::PreviewIdentity{1U, pointerId, 1U}) ==
          ink::PreviewSubmitResult::kRejected) {
    return false;
  }
  if (previewController_ == nullptr || previewProvider_ == nullptr ||
      !previewController_->cancelSession(pointerId) ||
      !previewController_->retireSession(pointerId, previewProvider_->generation()) ||
      (!platformPresentationDeferred_ && !previewController_->renderIfDirty())) return false;
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
  std::lock_guard lock(previewStateMutex_);
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
    if (platformPresentationDeferred_) {
      canonicalPresentationDirty_ = true;
      return true;
    }
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
  // A duplicate operation is idempotently accepted by the semantic engine,
  // but it is not a new local document revision and must not advance the
  // host's product revision counter.
  if (applied.disposition == semantic::ApplyDisposition::kApplied) {
    ++submittedOperationCount_;
  }
  return interaction::SubmitResult::acceptedResult();
}

semantic::OperationId InkPlaygroundHost::allocateOperationId() {
  while (nextHistoryOperationOrdinal_ != 0U) {
    const auto id = semantic::OperationId(
        foundation::ObjectId::fromUint64(nextHistoryOperationOrdinal_++));
    if (!appliedOperations_.find(id).has_value()) return id;
  }
  return {};
}

std::uint64_t InkPlaygroundHost::localOperationOrdinal() const noexcept {
  // Preserve the existing low-ID/order-key sequence for ordinary drawings;
  // the compatibility port can consume arbitrary identities independently.
  auto ordinal = static_cast<std::uint64_t>(submittedOperationCount_ + 1U);
  while (ordinal != 0U && ordinal < (std::uint64_t{1} << 63U)) {
    const auto id = foundation::ObjectId::fromUint64(ordinal);
    if (!appliedOperations_.find(semantic::OperationId(id)).has_value() &&
        !semanticObjects_.contains(id)) return ordinal;
    ++ordinal;
  }
  return 0U;
}

bool InkPlaygroundHost::historySceneReady() const noexcept {
  return runtimeSceneHost_ != nullptr && sceneCoordinator_ != nullptr &&
         runtimeSceneHost_->semanticGeneration() == semanticGeneration_.current() &&
         sceneCoordinator_->runtimeScene().generation() == semanticGeneration_.current();
}

bool InkPlaygroundHost::recoverHistoryScene() noexcept {
  if (historySceneReady()) return true;
  if (sceneCoordinator_ == nullptr || sceneCompiler_ == nullptr) return false;
  const semantic::SemanticReadView view(semanticObjects_, semanticGeneration_.current());
  const canvas::SceneCommitInput recovery(semanticGeneration_.current(), view);
  return static_cast<bool>(sceneCoordinator_->recover(*sceneCompiler_, recovery));
}

interaction::SubmitResult InkPlaygroundHost::submit(
    std::span<const semantic::Operation> operations, semantic::ApplySource source) {
  // This host records only one-operation compensation entries. Reject larger
  // batches before mutation; other EditorHistory owners may support them.
  if (operations.size() != 1U || source != semantic::ApplySource::kUndoRedo ||
      !historySceneReady()) return interaction::SubmitResult::rejected();
  for (const auto& operation : operations) {
    const auto applied = operationEngine_.apply(
        operation, source, semanticObjects_, appliedOperations_, semanticGeneration_,
        canonicalCommitClock_);
    if (applied.disposition != semantic::ApplyDisposition::kApplied) {
      return interaction::SubmitResult::rejected();
    }
    if (applied.commit_record.has_value() && sceneCoordinator_ != nullptr &&
        sceneCompiler_ != nullptr) {
      semantic::SemanticReadView view(semanticObjects_, semanticGeneration_.current());
      canvas::SceneCommitInput input(applied.commit_record->before_generation,
                                     applied.commit_record->after_generation, view,
                                     &applied.commit_record->change_set);
      (void)sceneCoordinator_->apply(*sceneCompiler_, input);
    }
    ++submittedOperationCount_;
  }
  if (!operations.empty()) {
    // Once Semantic has accepted, presentation/projection failure cannot
    // reject that mutation. The next frame recovers the derived Scene.
    if (!platformPresentationDeferred_) {
      (void)presentCanonicalFrame(canonicalFrameCount_ + 1U, 0.0, false);
    }
  }
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
  if (editingPointer_ != 0U) cancelSelectionTransform();
  std::lock_guard providerLock(previewProviderMutex_);
  std::lock_guard lock(previewStateMutex_);
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
  if (editingPointer_ != 0U) cancelSelectionTransform();
  std::lock_guard lock(previewStateMutex_);
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
  if (editingPointer_ != 0U) cancelSelectionTransform();
  std::lock_guard providerLock(previewProviderMutex_);
  std::lock_guard lock(previewStateMutex_);
  if (appBinding_ == nullptr ||
      appBinding_->rebindSurface() != render::SurfaceProviderDisposition::kCommitted ||
      appBinding_->rebindPreviewSurface() != render::SurfaceProviderDisposition::kCommitted) {
    return false;
  }
  surface_.generation = appBinding_->surfaces().lifecycle().current().surfaceGeneration.value();
  surface_.available = true;
  lifecycle_ = std::make_unique<render::SurfaceLifecycle>(
      appBinding_->surfaces().lifecycle().current());
  tracker_ = std::make_unique<render::PresentationTracker>(*lifecycle_);
  if (previewController_ != nullptr && !previewController_->rebind(previewProvider_->generation())) return false;
  ++rebindEvents_;
  return true;
}

bool InkPlaygroundHost::rebindPreviewSurface() noexcept {
  std::lock_guard providerLock(previewProviderMutex_);
  std::lock_guard lock(previewStateMutex_);
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

bool InkPlaygroundHost::selectCanonicalSurfaceProfile(
    std::string_view profileId, std::uint64_t expectedGeneration,
    render::RenderTargetFormat format) noexcept {
  if (appBinding_ == nullptr || profileId.empty()) return false;
  if (appBinding_->surfaces().lifecycle().current().surfaceGeneration.value() !=
      expectedGeneration) return false;
  if (editingPointer_ != 0U) cancelSelectionTransform();
  if (appBinding_->selectRenderProfile(profileId, format) !=
      render::SurfaceProviderDisposition::kCommitted) return false;
  surface_.generation = appBinding_->surfaces().lifecycle().current().surfaceGeneration.value();
  surface_.width = appBinding_->surfaces().activeInfo().metrics.physicalWidth;
  surface_.height = appBinding_->surfaces().activeInfo().metrics.physicalHeight;
  surface_.available = true;
  return true;
}

bool InkPlaygroundHost::rebindCanonicalSurface(std::uint64_t expectedGeneration) noexcept {
  if (appBinding_ == nullptr ||
      appBinding_->surfaces().lifecycle().current().surfaceGeneration.value() !=
          expectedGeneration) return false;
  if (editingPointer_ != 0U) cancelSelectionTransform();
  if (appBinding_->rebindSurface() != render::SurfaceProviderDisposition::kCommitted) return false;
  surface_.generation = appBinding_->surfaces().lifecycle().current().surfaceGeneration.value();
  surface_.available = true;
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
  if (!recoverHistoryScene()) { AXIOM_ANDROID_DIAG("present reject scene generation"); return false; }
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
  auto visibility = render::VisibilityResolver::resolve(frame, *runtimeSceneHost_);
  if (!visibility) { AXIOM_ANDROID_DIAG("present visibility failed"); return false; }
  // A selected object can be dragged into view from outside the canonical
  // visibility set. Extend only this view's transient traversal, never Scene.
  if (transformDrag_.active() &&
      std::find(visibility.value().backToFront.begin(),visibility.value().backToFront.end(),
                selection_.primary())==visibility.value().backToFront.end()) {
    auto& ids=visibility.value().backToFront;
    ids.push_back(selection_.primary());
    std::sort(ids.begin(),ids.end(),[&](auto a,auto b) {
      return sceneCoordinator_->runtimeScene().find(a)->placement.order_key <
             sceneCoordinator_->runtimeScene().find(b)->placement.order_key;
    });
    visibility.value().visibleRecords=ids.size();
  }
  auto references = render::DirectReferenceSource::build(
      frame, visibility.value(), sceneCoordinator_->runtimeScene());
  if (!references) { AXIOM_ANDROID_DIAG("present references failed"); return false; }
  // The copy belongs to this frame only. Canonical storage and the shared
  // RuntimeScene retain their original records throughout a drag.
  for (auto& entry: references.value().entries) {
    if (const auto* transform=transformOverrides_.find(entry.record.objectId)) {
      entry.record.transform=*transform;
      // Canonical digest/encoding does not describe transient editor pixels.
      references.value().canonicalBytes.clear();
      references.value().digest.clear();
    }
  }
  const auto plan = render::FramePlanBuilder::build(frame, references.value());
  if (!plan) { AXIOM_ANDROID_DIAG("present plan failed"); return false; }
  if (selectionMode_) (void)renderSelectionOverlay();
  const auto rendered = skiaRenderer_.renderFrame(*activeSurfaceProvider(), plan.value(),
                                                  selectionOverlay());
  if (rendered.code != render::BackendSubmissionCode::kAccepted) {
    AXIOM_ANDROID_DIAG("present skia failed: %s framegen=%llu providergen=%llu",
                       rendered.message.c_str(),
                       static_cast<unsigned long long>(frame.surfaceGeneration.value()),
                       static_cast<unsigned long long>(activeSurfaceProvider()->generation()));
    return false;
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
  canonicalPresentationDirty_ = false;
  if (retirePreview && semanticObjects_.size() != 0U) {
    std::lock_guard providerLock(previewProviderMutex_);
    std::lock_guard previewLock(previewStateMutex_);
    if (previewController_ == nullptr || !previewController_->active()) return true;
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

std::vector<ink::reference::StrokeOutlinePoint> InkPlaygroundHost::brushPreviewOutline() const {
  if (brushPreviews_.empty()) return {};
  return brushPreviews_.begin()->second;
}

std::vector<std::vector<ink::reference::StrokeOutlinePoint>>
InkPlaygroundHost::brushPreviewOutlines() const {
  std::vector<std::vector<ink::reference::StrokeOutlinePoint>> result;
  result.reserve(brushPreviews_.size());
  for (const auto& [pointer, outline] : brushPreviews_) {
    static_cast<void>(pointer);
    result.push_back(outline);
  }
  return result;
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
