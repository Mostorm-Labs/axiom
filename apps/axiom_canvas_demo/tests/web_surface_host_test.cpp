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

} // namespace

int main() {
    rejectsStaleSurfaceAndAcceptsFreshDprMetrics();
    lossRequiresRebindAndPresentedFeedbackIsGenerationBound();
    return 0;
}
