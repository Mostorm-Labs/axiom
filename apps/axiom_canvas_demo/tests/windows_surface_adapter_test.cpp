#include "windows_surface_adapter.hpp"

#include "canvas/foundation/revision.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/render/frame_state.hpp"
#include "canvas/render/presentation_tracker.hpp"
#include "canvas/render/surface_lifecycle.hpp"
#include "canvas/semantic/semantic_generation.hpp"

#include <cassert>
#include <optional>

namespace {

using namespace canvas::foundation;
using namespace canvas::render;
using namespace canvas::windows_demo;

constexpr ViewId kView{91};

SurfaceMetrics metrics(std::uint32_t width = 1600U, std::uint32_t height = 1200U) {
    return SurfaceMetrics{800.0F, 600.0F, width, height, 2.0F, 1.0F};
}

SurfaceSnapshot surface(std::uint64_t generation,
                        std::uint64_t metricsGeneration,
                        SurfaceMetrics value = metrics()) {
    return SurfaceSnapshot{kView, SurfaceGeneration{generation},
                           MetricsGeneration{metricsGeneration}, value};
}

FrameState frame(std::uint64_t id,
                 std::uint64_t generation,
                 std::uint64_t metricsGeneration,
                 SurfaceMetrics value = metrics()) {
    return FrameState{
        .viewId = kView,
        .camera = CameraState{WorldPoint{0.0F, 0.0F}, 1.0F, 0.0F, CameraGeneration{1}},
        .worldViewport = WorldRect{-400.0F, -300.0F, 400.0F, 300.0F},
        .metrics = value,
        .sceneGeneration = canvas::semantic::SemanticGeneration{1},
        .sceneReadToken = SceneRevision{1},
        .surfaceGeneration = SurfaceGeneration{generation},
        .metricsGeneration = MetricsGeneration{metricsGeneration},
        .frameId = FrameId{id},
    };
}

void rejectsStaleSurfaceAndAcceptsFreshPresentation() {
    WindowsSurfaceAdapter adapter{surface(10, 20)};
    assert(adapter.bind(surface(9, 20)) == WindowsSurfaceDisposition::kRejectedStale);
    assert(adapter.bind(surface(10, 21, metrics(1800, 1350))) ==
           WindowsSurfaceDisposition::kRebound);

    const FrameState submitted = frame(50, 10, 21, metrics(1800, 1350));
    assert(adapter.submit(submitted) == PresentFeedbackDisposition::kSubmitted);
    assert(adapter.feedback(PresentedFeedback{
               kView, FrameId{50}, SurfaceGeneration{10}, MetricsGeneration{20},
               PresentOutcome::kPresented, PresentEvidenceKind::kPlatformQualified,
               std::optional<std::uint64_t>{100}}) ==
           PresentFeedbackDisposition::kStaleGeneration);
    assert(adapter.feedback(PresentedFeedback{
               kView, FrameId{50}, SurfaceGeneration{10}, MetricsGeneration{21},
               PresentOutcome::kPresented, PresentEvidenceKind::kPlatformQualified,
               std::optional<std::uint64_t>{101}}) ==
           PresentFeedbackDisposition::kPresented);
}

void lossRequiresFreshRebindBeforeSubmission() {
    WindowsSurfaceAdapter adapter{surface(30, 40)};
    assert(adapter.markLost() == SurfaceLifecycleDisposition::kLost);
    assert(adapter.submit(frame(60, 30, 40)) == PresentFeedbackDisposition::kUnavailable);
    assert(adapter.bind(surface(31, 41, metrics(1920, 1080))) ==
           WindowsSurfaceDisposition::kRebound);
    assert(adapter.submit(frame(61, 31, 41, metrics(1920, 1080))) ==
           PresentFeedbackDisposition::kSubmitted);
}

} // namespace

int main() {
    rejectsStaleSurfaceAndAcceptsFreshPresentation();
    lossRequiresFreshRebindBeforeSubmission();
    return 0;
}
