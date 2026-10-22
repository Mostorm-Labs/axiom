#include "web_surface_host.hpp"

#include "canvas/foundation/revision.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/render/frame_state.hpp"
#include "canvas/render/presentation_tracker.hpp"
#include "canvas/render/surface_lifecycle.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#include "canvas/semantic/semantic_generation.hpp"

#include <cassert>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace {

using namespace canvas::foundation;
using namespace canvas::render;
using namespace canvas::web_demo;

constexpr ViewId kView{301};

SurfaceMetrics metrics(std::uint32_t width = 800U, std::uint32_t height = 600U,
                       float dpr = 1.0F) {
    return SurfaceMetrics{static_cast<float>(width), static_cast<float>(height),
                          width, height, dpr, 1.0F};
}

SurfaceSnapshot surface(std::uint64_t generation,
                        std::uint64_t metricsGeneration,
                        SurfaceMetrics value = metrics()) {
    return SurfaceSnapshot{kView, SurfaceGeneration{generation},
                           MetricsGeneration{metricsGeneration}, value};
}

FrameState frame(std::uint64_t id, std::uint64_t generation,
                 std::uint64_t metricsGeneration,
                 SurfaceMetrics value = metrics()) {
    return FrameState{
        .viewId = kView,
        .camera = CameraState{WorldPoint{0.0F, 0.0F}, 1.0F, 0.0F, CameraGeneration{1}},
        .worldViewport = WorldRect{-400.0F, -300.0F, 400.0F, 300.0F},
        .metrics = value,
        .sceneGeneration = canvas::semantic::SemanticGeneration{17},
        .sceneReadToken = SceneRevision{17},
        .surfaceGeneration = SurfaceGeneration{generation},
        .metricsGeneration = MetricsGeneration{metricsGeneration},
        .frameId = FrameId{id},
    };
}

PresentFeedbackDisposition mappedFeedback(WebSurfaceHost& host, const PresentedFeedback& feedback) {
    const auto work = host.submittedCanonicalWork(feedback.frameId);
    return host.feedback(feedback, work.value_or(canvas::runtime::CanonicalWork{}));
}

void rejectsStaleSurfaceAndAcceptsFreshDprMetrics() {
    WebSurfaceHost host{surface(10, 20)};
    assert(host.bind(host.lifecycle().current()) == WebSurfaceDisposition::kRebound);
    assert(host.bind(surface(9, 20)) == WebSurfaceDisposition::kRejectedStale);
    const auto before = host.lifecycle().current();
    assert(host.bind({before.viewId, before.surfaceGeneration, before.metricsGeneration,
                      metrics(1200U, 900U, 1.5F)}) == WebSurfaceDisposition::kRebound);
    assert(host.lifecycle().current().surfaceGeneration.value() > before.surfaceGeneration.value());
    assert(host.lifecycle().current().metricsGeneration.value() > before.metricsGeneration.value());
}

void lossRequiresRebindAndPresentedFeedbackIsGenerationBound() {
    WebSurfaceHost host{surface(30, 40)};
    const auto before = host.lifecycle().current();
    assert(host.markLost() == SurfaceLifecycleDisposition::kLost);
    assert(host.submit(frame(60, before.surfaceGeneration.value(), before.metricsGeneration.value())) ==
           PresentFeedbackDisposition::kUnavailable);
    assert(host.bind({before.viewId, before.surfaceGeneration, before.metricsGeneration,
                      metrics(1600U, 1200U, 2.0F)}) == WebSurfaceDisposition::kRebound);
    const auto accepted = host.lifecycle().current();
    assert(host.submit(frame(61, accepted.surfaceGeneration.value(), accepted.metricsGeneration.value(),
                             accepted.metrics)) == PresentFeedbackDisposition::kSubmitted);
    assert(mappedFeedback(host, PresentedFeedback{
               kView, FrameId{61}, accepted.surfaceGeneration, before.metricsGeneration,
               PresentOutcome::kPresented, PresentEvidenceKind::kPlatformQualified,
               std::optional<std::uint64_t>{100}}) == PresentFeedbackDisposition::kStaleGeneration);
    assert(mappedFeedback(host, PresentedFeedback{
               kView, FrameId{61}, accepted.surfaceGeneration, accepted.metricsGeneration,
               PresentOutcome::kPresented, PresentEvidenceKind::kPlatformQualified,
               std::optional<std::uint64_t>{101}}) == PresentFeedbackDisposition::kPresented);
}

void runtimeCompositionCanonicalFeedbackClosesTheHandoff() {
    WebSurfaceHost host{surface(50, 60)};
    auto& composition = host.composition();
    canvas::runtime::RuntimeFrameOpportunity opportunity{};
    assert(composition.serviceFrameOpportunity(true, opportunity));
    assert(opportunity.canonicalRequested);
    const auto submitted = composition.lastSubmittedCanonicalFrame();
    assert(submitted.has_value());
    assert(composition.readback().canonicalSubmitted);
    assert(composition.receivePlatformPresented(canvas::runtime::CanonicalCompletion{
        *opportunity.canonicalWork, PresentOutcome::kPresented,
        PresentEvidenceKind::kPlatformQualified, std::nullopt}));
    assert(!composition.readback().canonicalSubmitted);
}

void compositionAndPlatformShareSurfaceIdentity() {
    WebSurfaceHost host{surface(1, 1, metrics(320U, 180U))};
    const auto runtimeGeneration = host.composition().surfaceSnapshot().surfaceGeneration;
    assert(runtimeGeneration != 0U);
    assert(host.lifecycle().current().surfaceGeneration.value() == runtimeGeneration);
    const auto frameValue = frame(70U, runtimeGeneration,
                                  host.lifecycle().current().metricsGeneration.value(),
                                  host.lifecycle().current().metrics);
    assert(host.submit(frameValue) == PresentFeedbackDisposition::kSubmitted);
    assert(mappedFeedback(host, PresentedFeedback{
               frameValue.viewId, frameValue.frameId, frameValue.surfaceGeneration,
               frameValue.metricsGeneration, PresentOutcome::kPresented,
               PresentEvidenceKind::kPlatformQualified, std::nullopt}) ==
           PresentFeedbackDisposition::kPresented);
}

void feedbackMappingKeepsConcurrentRuntimeFramesIndependent() {
    WebSurfaceHost host{surface(1, 1, metrics(320U, 180U))};
    const auto runtimeSurface = host.composition().surfaceSnapshot();
    const auto first = frame(101U, runtimeSurface.surfaceGeneration,
                             runtimeSurface.metricsGeneration,
                             metrics(runtimeSurface.width, runtimeSurface.height));
    const auto second = frame(102U, runtimeSurface.surfaceGeneration,
                              runtimeSurface.metricsGeneration,
                              metrics(runtimeSurface.width, runtimeSurface.height));
    assert(host.submit(first) == PresentFeedbackDisposition::kSubmitted);
    const auto runtimeFirst = host.composition().lastSubmittedCanonicalFrame();
    assert(runtimeFirst.has_value());
    assert(host.submit(second) == PresentFeedbackDisposition::kSubmitted);
    const auto runtimeSecond = host.composition().lastSubmittedCanonicalFrame();
    assert(runtimeSecond.has_value() && runtimeSecond->frameId != runtimeFirst->frameId);

    assert(mappedFeedback(host, PresentedFeedback{
               first.viewId, first.frameId, first.surfaceGeneration,
               first.metricsGeneration, PresentOutcome::kPresented,
               PresentEvidenceKind::kPlatformQualified, std::nullopt}) ==
           PresentFeedbackDisposition::kStaleGeneration);
    // The late first-frame observation must not consume the second runtime
    // frame's handoff. The second frame remains pending until its own feedback.
    assert(host.composition().lastSubmittedCanonicalFrame().has_value());
    assert(host.composition().lastSubmittedCanonicalFrame()->frameId ==
           runtimeSecond->frameId);
    assert(mappedFeedback(host, PresentedFeedback{
               second.viewId, second.frameId, second.surfaceGeneration,
               second.metricsGeneration, PresentOutcome::kPresented,
               PresentEvidenceKind::kPlatformQualified, std::nullopt}) ==
           PresentFeedbackDisposition::kPresented);
    assert(!host.composition().lastSubmittedCanonicalFrame().has_value());
}

void invalidReplacementHasNoRuntimeSideEffect() {
    WebSurfaceHost host{surface(1, 1, metrics(320U, 180U))};
    const auto before = host.composition().surfaceSnapshot();
    const auto lifecycleBefore = host.lifecycle().current();
    auto invalid = SurfaceSnapshot{
        lifecycleBefore.viewId,
        lifecycleBefore.surfaceGeneration,
        lifecycleBefore.metricsGeneration,
        SurfaceMetrics{lifecycleBefore.metrics.logicalWidth,
                       lifecycleBefore.metrics.logicalHeight,
                       lifecycleBefore.metrics.physicalWidth,
                       lifecycleBefore.metrics.physicalHeight,
                       0.0F,
                       lifecycleBefore.metrics.displayScale}};
    assert(host.bind(invalid) == WebSurfaceDisposition::kRejectedInvalid);
    const auto after = host.composition().surfaceSnapshot();
    assert(after.surfaceGeneration == before.surfaceGeneration);
    assert(after.width == before.width && after.height == before.height);
    assert(host.lifecycle().current() == lifecycleBefore);
}

void preparedBridgeProvidersAreAdoptedWithoutDoubleAdvance() {
    WebSurfaceHost host{surface(70, 70, metrics(320U, 180U))};
    assert(host.markLost() == SurfaceLifecycleDisposition::kLost);
    auto canonical = std::make_unique<RasterSkiaSurfaceProvider>();
    assert(canonical->resize(640U, 360U).code == BackendSubmissionCode::kAccepted);
    assert(host.composition().registerSurfaceProvider(
        "prepared-web-canonical", std::move(canonical)));
    auto preview = std::make_unique<RasterSkiaSurfaceProvider>();
    assert(preview->resize(640U, 360U).code == BackendSubmissionCode::kAccepted);
    assert(host.composition().registerPreviewSurfaceProvider(
        "prepared-web-preview", std::move(preview)));
    const auto prepared = host.composition().surfaceSnapshot();
    assert(!prepared.available);
    assert(host.bind({kView, SurfaceGeneration{prepared.surfaceGeneration},
                     MetricsGeneration{prepared.metricsGeneration}, metrics(640U, 360U)}) ==
           WebSurfaceDisposition::kRebound);
    assert(host.composition().state() == canvas::runtime::RuntimeCompositionState::kReady);
    const auto committed = host.composition().surfaceSnapshot();
    assert(committed.surfaceGeneration == prepared.surfaceGeneration + 1U);
    assert(committed.width == 640U && committed.height == 360U);
    assert(host.bind(host.lifecycle().current()) == WebSurfaceDisposition::kRebound);
    assert(host.composition().surfaceSnapshot().surfaceGeneration == committed.surfaceGeneration);
}

void mappingCapacityRejectsBeforeRuntimeSubmission() {
    WebSurfaceHost host{surface(90, 90, metrics(320U, 180U))};
    const auto runtimeSurface = host.composition().surfaceSnapshot();
    for (std::uint64_t id = 1U; id <= 8U; ++id) {
        assert(host.submit(frame(id, runtimeSurface.surfaceGeneration,
                                runtimeSurface.metricsGeneration,
                                metrics(runtimeSurface.width, runtimeSurface.height))) ==
               PresentFeedbackDisposition::kSubmitted);
    }
    const auto pendingBefore = host.composition().lastSubmittedCanonicalFrame();
    assert(pendingBefore.has_value());
    assert(host.submit(frame(9U, runtimeSurface.surfaceGeneration,
                            runtimeSurface.metricsGeneration,
                            metrics(runtimeSurface.width, runtimeSurface.height))) ==
           PresentFeedbackDisposition::kUnavailable);
    const auto pendingAfter = host.composition().lastSubmittedCanonicalFrame();
    assert(pendingAfter == pendingBefore);
    assert(mappedFeedback(host, PresentedFeedback{
               kView, FrameId{8U}, SurfaceGeneration{runtimeSurface.surfaceGeneration},
               MetricsGeneration{runtimeSurface.metricsGeneration}, PresentOutcome::kPresented,
               PresentEvidenceKind::kPlatformQualified, std::nullopt}) ==
           PresentFeedbackDisposition::kPresented);
}

void imageFacadeIsExposedThroughWebHost() {
    WebSurfaceHost host{surface(101U, 101U, metrics(320U, 180U))};
    canvas::runtime::RuntimeFacade& facade = host.facade();
    static_assert(std::is_same_v<decltype(&canvas::runtime::RuntimeFacade::importImage),
                                 canvas::runtime::ImageCommandReceipt (canvas::runtime::RuntimeFacade::*)(const canvas::runtime::ImageImportRequest&)>);
    const auto rejected = facade.importImage({});
    assert(rejected.state == canvas::runtime::ImageCommandState::kRejected);
}

void mappedFeedbackCannotRelabelAnotherRuntimeWork() {
    WebSurfaceHost a{surface(1U, 1U, metrics(128U, 128U))};
    WebSurfaceHost b{surface(1U, 1U, metrics(128U, 128U))};
    const auto sa = a.lifecycle().current();
    const auto sb = b.lifecycle().current();
    const auto fa = frame(1U, sa.surfaceGeneration.value(), sa.metricsGeneration.value(), sa.metrics);
    const auto fb = frame(1U, sb.surfaceGeneration.value(), sb.metricsGeneration.value(), sb.metrics);
    assert(a.submit(fa) == PresentFeedbackDisposition::kSubmitted);
    assert(b.submit(fb) == PresentFeedbackDisposition::kSubmitted);
    const auto wa = a.submittedCanonicalWork(FrameId{1U});
    const auto wb = b.submittedCanonicalWork(FrameId{1U});
    assert(wa && wb && wa != wb);
    const PresentedFeedback feedback{kView, FrameId{1U}, sb.surfaceGeneration,
        sb.metricsGeneration, PresentOutcome::kPresented, PresentEvidenceKind::kPlatformQualified,
        std::nullopt};
    const auto before = b.composition().lastSubmittedCanonicalWork();
    assert(b.feedback(feedback, *wa) == PresentFeedbackDisposition::kStaleGeneration);
    assert(b.composition().lastSubmittedCanonicalWork() == before);
    assert(b.feedback(feedback, *wb) == PresentFeedbackDisposition::kPresented);
    assert(!b.composition().lastSubmittedCanonicalWork());
    assert(b.feedback(feedback, *wb) == PresentFeedbackDisposition::kUnknownFrame);
}

} // namespace

int main() {
    mappedFeedbackCannotRelabelAnotherRuntimeWork();
    {
        WebSurfaceHost host{surface(1U, 1U, SurfaceMetrics{100.0F, 80.0F, 200U, 160U, 2.0F, 1.25F})};
        const auto accepted = host.composition().surfaceSnapshot();
        assert(accepted.logicalWidth == 100.0F && accepted.logicalHeight == 80.0F);
        assert(accepted.width == 200U && accepted.height == 160U);
        assert(accepted.devicePixelRatio == 2.0F && accepted.displayScale == 1.25F);
        assert(host.lifecycle().current().metrics == SurfaceMetrics(100.0F, 80.0F, 200U, 160U, 2.0F, 1.25F));
    }
    rejectsStaleSurfaceAndAcceptsFreshDprMetrics();
    lossRequiresRebindAndPresentedFeedbackIsGenerationBound();
    runtimeCompositionCanonicalFeedbackClosesTheHandoff();
    compositionAndPlatformShareSurfaceIdentity();
    feedbackMappingKeepsConcurrentRuntimeFramesIndependent();
    invalidReplacementHasNoRuntimeSideEffect();
    preparedBridgeProvidersAreAdoptedWithoutDoubleAdvance();
    mappingCapacityRejectsBeforeRuntimeSubmission();
    imageFacadeIsExposedThroughWebHost();
    return 0;
}
