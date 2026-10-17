#include "web_surface_host.hpp"

#include "canvas/foundation/revision.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/render/frame_state.hpp"
#include "canvas/render/presentation_tracker.hpp"
#include "canvas/render/surface_lifecycle.hpp"
#include "canvas/semantic/semantic_generation.hpp"

#include <cassert>
#include <cstdint>
#include <optional>

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

void rejectsStaleSurfaceAndAcceptsFreshDprMetrics() {
    WebSurfaceHost host{surface(10, 20)};
    assert(host.bind(host.lifecycle().current()) == WebSurfaceDisposition::kRebound);
    assert(host.bind(surface(9, 20)) == WebSurfaceDisposition::kRejectedStale);
    assert(host.bind(surface(11, 21, metrics(1200U, 900U, 1.5F))) ==
           WebSurfaceDisposition::kRebound);
    assert(host.lifecycle().current().surfaceGeneration == SurfaceGeneration{11});
    assert(host.lifecycle().current().metricsGeneration == MetricsGeneration{21});
}

void lossRequiresRebindAndPresentedFeedbackIsGenerationBound() {
    WebSurfaceHost host{surface(30, 40)};
    assert(host.markLost() == SurfaceLifecycleDisposition::kLost);
    assert(host.submit(frame(60, 30, 40)) == PresentFeedbackDisposition::kUnavailable);
    assert(host.bind(surface(31, 41, metrics(1600U, 1200U, 2.0F))) ==
           WebSurfaceDisposition::kRebound);
    assert(host.submit(frame(61, 31, 41, metrics(1600U, 1200U, 2.0F))) ==
           PresentFeedbackDisposition::kSubmitted);
    assert(host.feedback(PresentedFeedback{
               kView, FrameId{61}, SurfaceGeneration{31}, MetricsGeneration{40},
               PresentOutcome::kPresented, PresentEvidenceKind::kPlatformQualified,
               std::optional<std::uint64_t>{100}}) ==
           PresentFeedbackDisposition::kStaleGeneration);
    assert(host.feedback(PresentedFeedback{
               kView, FrameId{61}, SurfaceGeneration{31}, MetricsGeneration{41},
               PresentOutcome::kPresented, PresentEvidenceKind::kPlatformQualified,
               std::optional<std::uint64_t>{101}}) ==
           PresentFeedbackDisposition::kPresented);
}

void runtimeCompositionCanonicalFeedbackClosesTheHandoff() {
    WebSurfaceHost host{surface(50, 60)};
    auto& composition = host.composition();
    assert(composition.requestCanonicalFrame(1U));
    const auto submitted = composition.runtime().lastSubmittedCanonicalFrame();
    assert(submitted.has_value());
    assert(composition.readback().canonicalSubmitted);
    assert(composition.receivePlatformPresented(PresentedFeedback{
        submitted->viewId, submitted->frameId, submitted->surfaceGeneration,
        submitted->metricsGeneration, PresentOutcome::kPresented,
        PresentEvidenceKind::kPlatformQualified, std::nullopt}));
    assert(!composition.readback().canonicalSubmitted);
}

void compositionAndPlatformShareSurfaceIdentity() {
    WebSurfaceHost host{surface(1, 1, metrics(320U, 180U))};
    const auto runtimeGeneration = host.composition().runtime().surface().generation;
    assert(runtimeGeneration != 0U);
    assert(host.lifecycle().current().surfaceGeneration.value() == runtimeGeneration);
    const auto frameValue = frame(70U, runtimeGeneration,
                                  host.lifecycle().current().metricsGeneration.value(),
                                  host.lifecycle().current().metrics);
    assert(host.submit(frameValue) == PresentFeedbackDisposition::kSubmitted);
    assert(host.feedback(PresentedFeedback{
               frameValue.viewId, frameValue.frameId, frameValue.surfaceGeneration,
               frameValue.metricsGeneration, PresentOutcome::kPresented,
               PresentEvidenceKind::kPlatformQualified, std::nullopt}) ==
           PresentFeedbackDisposition::kPresented);
}

void invalidReplacementHasNoRuntimeSideEffect() {
    WebSurfaceHost host{surface(1, 1, metrics(320U, 180U))};
    const auto before = host.composition().runtime().surface();
    const auto lifecycleBefore = host.lifecycle().current();
    auto invalid = SurfaceSnapshot{
        lifecycleBefore.viewId,
        SurfaceGeneration{2},
        MetricsGeneration{2},
        SurfaceMetrics{lifecycleBefore.metrics.logicalWidth,
                       lifecycleBefore.metrics.logicalHeight,
                       lifecycleBefore.metrics.physicalWidth,
                       lifecycleBefore.metrics.physicalHeight,
                       0.0F,
                       lifecycleBefore.metrics.displayScale}};
    assert(host.bind(invalid) == WebSurfaceDisposition::kRejectedInvalid);
    const auto after = host.composition().runtime().surface();
    assert(after.generation == before.generation);
    assert(after.width == before.width && after.height == before.height);
    assert(host.lifecycle().current() == lifecycleBefore);
}

} // namespace

int main() {
    rejectsStaleSurfaceAndAcceptsFreshDprMetrics();
    lossRequiresRebindAndPresentedFeedbackIsGenerationBound();
    runtimeCompositionCanonicalFeedbackClosesTheHandoff();
    compositionAndPlatformShareSurfaceIdentity();
    invalidReplacementHasNoRuntimeSideEffect();
    return 0;
}
