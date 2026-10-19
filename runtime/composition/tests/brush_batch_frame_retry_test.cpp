#include "canvas/runtime/runtime_composition.hpp"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <limits>
#include <string_view>

namespace {
using namespace canvas;

input::PlatformPointerSample point(std::uint64_t sequence,
                                  input::PointerPhase phase = input::PointerPhase::kMove) {
  return {71U, 1U, sequence, sequence * 1'000'000U,
          20.0F + static_cast<float>(sequence) * 2.0F,
          80.0F + static_cast<float>(sequence % 7U), 0.5F, 0.0F, 0.0F,
          {}, {}, input::SampleProvenance::kConfirmedCurrent, phase};
}

class InspectRuntime final : public interaction::CanvasEditingRuntime {
 public:
  ink::BrushSessionMetrics metrics() const {
    assert(brushSessions_.size() == 1U);
    return brushSessions_.begin()->second->metrics();
  }
  std::size_t sessions() const { return brushSessions_.size(); }
  std::size_t dispatchedSessions() const { return dispatchRuntime_.activeCount(); }
  std::vector<semantic::ObjectRecord> objects() const { return semanticObjects_.allObjects(); }
};

void prepare(InspectRuntime& runtime) {
  assert(runtime.bindSurface(512U, 256U));
  assert(runtime.registerInputSource(71U, "batch-test"));
  runtime.setPlatformPresentationDeferred(true);
}

bool send(InspectRuntime& runtime, const input::PlatformPointerBatch& batch) {
  return runtime.acceptPlatformBatch(batch, batch.samples.back().timestampNs);
}

void sameGeometry(const interaction::PreviewPresentationCapture& a,
                  const interaction::PreviewPresentationCapture& b) {
  assert(!a.geometry.contours.empty());
  assert(a.geometry.contours.size() == b.geometry.contours.size());
  for (std::size_t j = 0U; j < a.geometry.contours.size(); ++j) {
    const auto& x = a.geometry.contours[j];
    const auto& y = b.geometry.contours[j];
    assert(x.outline.size() == y.outline.size() && x.dabs == y.dabs);
    for (std::size_t i = 0U; i < x.outline.size(); ++i) {
      assert(x.outline[i].x == y.outline[i].x && x.outline[i].y == y.outline[i].y);
    }
  }
}

// The retained geometry should be evaluated once per delivered batch, while
// the real ingress and arbiter still observe every sample in order.
void batchEvaluatesOnce() {
  InspectRuntime runtime;
  prepare(runtime);
  input::PlatformPointerBatch batch;
  batch.samples.push_back(point(1U, input::PointerPhase::kDown));
  for (std::uint64_t i = 2U; i <= 64U; ++i) batch.samples.push_back(point(i));
  assert(runtime.acceptPlatformBatch(batch, batch.samples.back().timestampNs));
  assert(runtime.baselineTrace().size() == 64U);
  const auto metrics = runtime.metrics();
  std::printf("64 confirmed points: appendEvaluations=%llu maxInput=%llu\n",
      static_cast<unsigned long long>(metrics.appendEvaluations),
      static_cast<unsigned long long>(metrics.maxAppendInputSamples));
  std::fflush(stdout);
  assert(metrics.appendEvaluations == 1U);
  assert(metrics.maxAppendInputSamples == 64U);
  batch.samples.clear();
  for (std::uint64_t i = 65U; i <= 128U; ++i) batch.samples.push_back(point(i));
  assert(send(runtime, batch));
  assert(runtime.metrics().appendEvaluations == 2U);
  assert(runtime.metrics().maxAppendInputSamples == 128U);
  assert(runtime.baselineTrace().size() == 128U);
}

void batchPreservesGeometryCommitAndTerminals() {
  for (const auto profile : {"vector-solid-v1", "marker-flat-v1", "chalk-grain-v1", "membrane-v1"}) {
    InspectRuntime batched, singles;
    prepare(batched); prepare(singles);
    assert(batched.selectBrushProfile(profile, 1U) && singles.selectBrushProfile(profile, 1U));
    input::PlatformPointerBatch batch;
    batch.samples.push_back(point(1U, input::PointerPhase::kDown));
    for (std::uint64_t i = 2U; i <= 64U; ++i) batch.samples.push_back(point(i));
    const auto start = std::chrono::steady_clock::now();
    assert(send(batched, batch));
    const auto batchedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    const auto singleStart = std::chrono::steady_clock::now();
    for (const auto& sample : batch.samples) assert(send(singles, {{sample}}));
    const auto singleMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - singleStart).count();
    interaction::PreviewPresentationCapture a, b;
    assert(batched.capturePreviewPresentation(a) && singles.capturePreviewPresentation(b));
    sameGeometry(a, b);
    std::printf("%s: 64-point batch=%.3fms single-point delivery=%.3fms evaluations=%llu/%llu\n",
        profile, batchedMs, singleMs,
        static_cast<unsigned long long>(batched.metrics().appendEvaluations),
        static_cast<unsigned long long>(singles.metrics().appendEvaluations));
    batch.samples = {point(65U), point(66U, input::PointerPhase::kUp)};
    assert(send(batched, batch));
    for (const auto& sample : batch.samples) assert(send(singles, {{sample}}));
    assert(batched.sessions() == 0U && singles.sessions() == 0U);
    assert(batched.objects() == singles.objects());
    assert(batched.submittedOperationCount() == 1U);
    const auto objects = batched.objects();
    std::visit([](const auto& content) {
      using T = std::decay_t<decltype(content)>;
      if constexpr (std::is_same_v<T, semantic::BrushStrokeContent> ||
                    std::is_same_v<T, semantic::DabBrushStrokeContent>) {
        assert(content.stroke.confirmed_samples.size() == 66U);
        assert(content.stroke.confirmed_samples.front().position.x == 22.0);
        assert(content.stroke.confirmed_samples.back().position.x == 152.0);
      } else assert(false);
    }, objects.front().content);
    assert(batched.undo() && batched.semanticObjectCount() == 0U);
    assert(batched.redo() && batched.objects() == objects);
  }
  InspectRuntime runtime;
  prepare(runtime);
  input::PlatformPointerBatch batch{{point(1U, input::PointerPhase::kDown), point(2U)}};
  assert(send(runtime, batch));
  auto predicted = point(3U);
  predicted.x = 450.0F;
  predicted.provenance = input::SampleProvenance::kPredicted;
  assert(send(runtime, {{predicted}}));
  batch.samples = {point(4U), point(5U, input::PointerPhase::kUp)};
  assert(send(runtime, batch));
  const auto content = std::get<semantic::BrushStrokeContent>(runtime.objects().front().content);
  assert(content.stroke.confirmed_samples.size() == 4U);
  for (const auto& sample : content.stroke.confirmed_samples) assert(sample.position.x != 450.0);
  const auto before = runtime.submittedOperationCount();
  batch.samples = {point(6U, input::PointerPhase::kDown), point(7U),
                   point(8U, input::PointerPhase::kCancel)};
  assert(send(runtime, batch));
  assert(runtime.sessions() == 0U && runtime.submittedOperationCount() == before);
  assert(runtime.selectTool(interaction::CanvasEditingRuntime::ToolMode::kBrush));
  batch.samples = {point(9U, input::PointerPhase::kDown), point(10U, input::PointerPhase::kUp)};
  assert(send(runtime, batch));
  assert(runtime.submittedOperationCount() == before + 1U);

  // Interleaved contacts cannot be merged into the same brush session.
  assert(runtime.setMultiContactPolicy(interaction::MultiContactPolicy::kMultiInk));
  batch.samples = {point(20U, input::PointerPhase::kDown), point(21U),
                   point(22U, input::PointerPhase::kUp)};
  auto otherDown = point(20U, input::PointerPhase::kDown); otherDown.pointer = 2U;
  auto otherMove = point(21U); otherMove.pointer = 2U;
  auto otherUp = point(22U, input::PointerPhase::kUp); otherUp.pointer = 2U;
  batch.samples = {batch.samples[0], otherDown, batch.samples[1], otherMove,
                   batch.samples[2], otherUp};
  assert(send(runtime, batch));
  assert(runtime.sessions() == 0U && runtime.dispatchedSessions() == 0U);
  assert(runtime.submittedOperationCount() == before + 3U);
  const auto& trace = runtime.baselineTrace();
  const auto last = trace.size() - 1U;
  assert(trace[last].key.source == 71U && trace[last - 1U].key.source == 71U);
  assert(trace[last].key.pointer == 2U && trace[last - 1U].key.pointer == 1U);

  assert(runtime.registerInputSource(72U, "second-source"));
  auto secondSource = point(30U, input::PointerPhase::kDown); secondSource.source = 72U;
  assert(send(runtime, {{point(30U, input::PointerPhase::kDown), point(31U)}}));
  assert(send(runtime, {{secondSource}}));
  assert(runtime.sessions() == 2U);
  secondSource = point(32U, input::PointerPhase::kUp); secondSource.source = 72U;
  assert(send(runtime, {{point(32U, input::PointerPhase::kUp)}}));
  assert(send(runtime, {{secondSource}}));
  assert(runtime.sessions() == 0U && runtime.submittedOperationCount() == before + 5U);

  // AutoIntent still consumes the chord one observation at a time and
  // cancels the pending brush before claiming the navigation gesture.
  InspectRuntime navigation;
  prepare(navigation);
  assert(navigation.registerInputSource(72U, "second-source"));
  auto firstDown = point(1U, input::PointerPhase::kDown);
  otherDown = firstDown; otherDown.pointer = 2U;
  assert(send(navigation, {{firstDown, otherDown}}));
  assert(navigation.viewportGestureClaimed());
  assert(navigation.sessions() == 0U && navigation.dispatchedSessions() == 0U);
  auto firstUp = point(2U, input::PointerPhase::kUp);
  otherUp = firstUp; otherUp.pointer = 2U;
  assert(send(navigation, {{firstUp, otherUp}}));
  assert(!navigation.viewportGestureClaimed() && navigation.submittedOperationCount() == 0U);
  assert(send(navigation, {{point(3U, input::PointerPhase::kDown), point(4U),
                            point(5U, input::PointerPhase::kUp)}}));
  assert(navigation.submittedOperationCount() == 1U);

  InspectRuntime rejected;
  prepare(rejected);
  auto invalid = point(2U); invalid.x = std::numeric_limits<float>::quiet_NaN();
  assert(!send(rejected, {{point(1U, input::PointerPhase::kDown), invalid}}));
  assert(rejected.sessions() == 0U && rejected.dispatchedSessions() == 0U);
  assert(rejected.semanticObjectCount() == 0U);
  // The dispatcher already rejected and cancelled this session. A late
  // platform Cancel is rejected but must still release ingress ownership.
  assert(!send(rejected, {{point(3U, input::PointerPhase::kCancel)}}));
  assert(send(rejected, {{point(4U, input::PointerPhase::kDown),
                         point(5U, input::PointerPhase::kUp)}}));
  assert(rejected.semanticObjectCount() == 1U);
}

// Only the OS/GPU acquire/present boundary is controlled. Geometry, canonical
// commits, actual raster pixels and all frame identities remain production.
class BusyProvider final : public render::SkiaSurfaceProvider {
 public:
  render::RasterSkiaSurfaceProvider actual;
  bool busyAcquire = false;
  bool busyPresent = false;
  bool lost = false;
  bool rejectPresent = false;
  render::RenderTargetInfo describe() const noexcept override { return actual.describe(); }
  render::SkiaSurfaceAcquireResult acquire() noexcept override {
    if (lost) return render::SkiaSurfaceAcquireResult::rejected(
        render::SkiaSurfaceAcquireCode::kLost, "fixture device lost");
    return busyAcquire ? render::SkiaSurfaceAcquireResult::rejected(
        render::SkiaSurfaceAcquireCode::kUnavailable, "fixture backbuffer busy") : actual.acquire();
  }
  void release() noexcept override { actual.release(); }
  render::BackendSubmissionResult present() noexcept override {
    if (rejectPresent) return render::BackendSubmissionResult::rejected("fixture device failure");
    return busyPresent ? render::BackendSubmissionResult::retry("fixture present busy")
                       : actual.present();
  }
  render::BackendSubmissionResult resize(std::uint32_t w, std::uint32_t h) noexcept override {
    return actual.resize(w, h);
  }
  render::BackendSubmissionResult readbackRgba(std::span<std::uint8_t> out) noexcept override {
    return actual.readbackRgba(out);
  }
  std::uint64_t generation() const noexcept override { return actual.generation(); }
  render::BackendSubmissionResult advanceGeneration() noexcept override { return actual.advanceGeneration(); }
  std::uint64_t readbackCount() const noexcept override { return actual.readbackCount(); }
  std::uint64_t cpuCopyCount() const noexcept override { return actual.cpuCopyCount(); }
  std::uint64_t presentCount() const noexcept override { return actual.presentCount(); }
};

void busyCanonicalPreservesAlreadySubmittedFeedback() {
  runtime::RuntimeComposition composition;
  assert(composition.open());
  auto canonical = std::make_unique<BusyProvider>();
  auto* raw = canonical.get();
  assert(canonical->resize(256U, 256U).code == render::BackendSubmissionCode::kAccepted);
  assert(composition.registerSurfaceProvider("canonical", std::move(canonical)));
  assert(composition.bindSurface(256U, 256U));
  assert(composition.setPlatformPresentationDeferred(true));
  assert(composition.registerSource(71U));
  runtime::RuntimeFrameOpportunity first;
  assert(composition.serviceFrameOpportunity(true, first) && first.canonicalWork);
  assert(composition.submit({{point(1U, input::PointerPhase::kDown), point(2U)}}));
  raw->busyAcquire = true;
  runtime::RuntimeFrameOpportunity busy;
  assert(composition.serviceFrameOpportunity(true, busy) && busy.previewWork);
  assert(busy.canonicalWork == first.canonicalWork);
  assert(composition.receivePlatformPresented({*first.canonicalWork,
      render::PresentOutcome::kPresented, render::PresentEvidenceKind::kPlatformQualified, {}}));
  assert(composition.finishFrameOpportunity({busy.previewWork, true}));
  assert(composition.canonicalPresentationDirty());
}

void fatalProviderFailureIsNotRetry() {
  for (const bool canonicalFails : {true, false}) {
    for (const bool lose : {true, false}) {
      runtime::RuntimeComposition composition;
      assert(composition.open());
      auto canonical = std::make_unique<BusyProvider>();
      auto preview = std::make_unique<BusyProvider>();
      auto* raw = canonicalFails ? canonical.get() : preview.get();
      assert(canonical->resize(256U, 256U).code == render::BackendSubmissionCode::kAccepted);
      assert(preview->resize(256U, 256U).code == render::BackendSubmissionCode::kAccepted);
      assert(composition.registerSurfaceProvider("canonical", std::move(canonical)));
      assert(composition.registerPreviewSurfaceProvider("preview", std::move(preview)));
      assert(composition.bindSurface(256U, 256U) && composition.registerSource(71U));
      assert(composition.setPlatformPresentationDeferred(true));
      assert(composition.submit({{point(1U, input::PointerPhase::kDown), point(2U)}}));
      raw->lost = lose;
      raw->rejectPresent = !lose;
      runtime::RuntimeFrameOpportunity failed;
      assert(!composition.serviceFrameOpportunity(true, failed));
      assert(!failed.previewWork);
      assert(composition.previewRenderState().platformQualifiedRevision == 0U);
    }
  }
}

void busyRoleDoesNotBlockOtherRole(bool canonicalBusy) {
  for (const bool failAcquire : {true, false}) {
    runtime::RuntimeComposition composition;
    assert(composition.open());
    auto canonical = std::make_unique<BusyProvider>();
    auto preview = std::make_unique<BusyProvider>();
    auto* canonicalRaw = canonical.get();
    auto* previewRaw = preview.get();
    assert(canonical->resize(256U, 256U).code == render::BackendSubmissionCode::kAccepted);
    assert(preview->resize(256U, 256U).code == render::BackendSubmissionCode::kAccepted);
    assert(composition.registerSurfaceProvider("canonical", std::move(canonical)));
    assert(composition.registerPreviewSurfaceProvider("preview", std::move(preview)));
    assert(composition.bindSurface(256U, 256U));
    assert(composition.registerSource(71U));
    assert(composition.setPlatformPresentationDeferred(true));
    input::PlatformPointerBatch batch;
    batch.samples = {point(1U, input::PointerPhase::kDown), point(2U), point(3U)};
    assert(composition.submit(batch));
    auto* busy = canonicalBusy ? canonicalRaw : previewRaw;
    busy->busyAcquire = failAcquire;
    busy->busyPresent = !failAcquire;
    runtime::RuntimeFrameOpportunity first;
    const bool progressed = composition.serviceFrameOpportunity(true, first);
    std::printf("busy=%s boundary=%s progressed=%d canonical=%d preview=%d\n",
        canonicalBusy ? "canonical" : "preview", failAcquire ? "acquire" : "present",
        progressed, first.canonicalRequested, first.previewRendered);
    std::fflush(stdout);
    assert(progressed);
    if (canonicalBusy) {
      assert(!first.canonicalRequested && !first.canonicalWork);
      assert(first.previewRendered && first.previewWork);
      assert(composition.finishFrameOpportunity({first.previewWork, true}));
      assert(composition.qualificationRuntime().canonicalPresentationDirty());
    } else {
      assert(first.canonicalRequested && first.canonicalWork);
      assert(!first.previewRendered && !first.previewWork);
      assert(composition.receivePlatformPresented({*first.canonicalWork,
          render::PresentOutcome::kPresented, render::PresentEvidenceKind::kPlatformQualified, {}}));
      assert(composition.previewRenderState().dirty);
      assert(previewRaw->presentCount() == 0U);
    }
    // More input while busy supersedes stale geometry; the retry renders the
    // newest retained preview, with no synthetic acknowledgement or spin.
    batch.samples = {point(4U)};
    assert(composition.submit(batch));
    busy->busyAcquire = busy->busyPresent = false;
    runtime::RuntimeFrameOpportunity retry;
    assert(composition.serviceFrameOpportunity(false, retry));
    assert(retry.previewWork && retry.previewRevision > first.previewRevision);
    assert(composition.finishFrameOpportunity({retry.previewWork, true}));
    if (retry.canonicalWork) assert(composition.receivePlatformPresented({*retry.canonicalWork,
        render::PresentOutcome::kPresented, render::PresentEvidenceKind::kPlatformQualified, {}}));
    const auto count = previewRaw->presentCount();
    runtime::RuntimeFrameOpportunity idle;
    assert(composition.serviceFrameOpportunity(false, idle));
    assert(!idle.previewRendered && !idle.previewWork);
    assert(previewRaw->presentCount() == count);
  }
}
}  // namespace

int main(int argc, char** argv) {
  const std::string_view mode = argc > 1 ? argv[1] : "batch";
  if (mode == "canonical-busy") busyRoleDoesNotBlockOtherRole(true);
  else if (mode == "preview-busy") busyRoleDoesNotBlockOtherRole(false);
  else if (mode == "semantics") batchPreservesGeometryCommitAndTerminals();
  else if (mode == "pending-feedback") busyCanonicalPreservesAlreadySubmittedFeedback();
  else if (mode == "fatal") fatalProviderFailureIsNotRetry();
  else batchEvaluatesOnce();
}
