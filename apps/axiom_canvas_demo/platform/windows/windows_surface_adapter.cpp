#include "windows_surface_adapter.hpp"

namespace canvas::windows_demo {

WindowsSurfaceAdapter::WindowsSurfaceAdapter(render::SurfaceSnapshot initial) noexcept
    : lifecycle_(initial), presentation_(lifecycle_) {}

WindowsSurfaceDisposition WindowsSurfaceAdapter::bind(
    const render::SurfaceSnapshot& replacement) {
    const auto before = lifecycle_.current();
    const auto disposition = lifecycle_.replace(replacement);
    if (disposition == render::SurfaceLifecycleDisposition::kReplaced) {
        return WindowsSurfaceDisposition::kRebound;
    }
    if (disposition == render::SurfaceLifecycleDisposition::kStaleGeneration ||
        disposition == render::SurfaceLifecycleDisposition::kNoGenerationAdvance ||
        replacement.surfaceGeneration.value() < before.surfaceGeneration.value() ||
        replacement.metricsGeneration.value() < before.metricsGeneration.value()) {
        return WindowsSurfaceDisposition::kRejectedStale;
    }
    return WindowsSurfaceDisposition::kRejectedInvalid;
}

render::SurfaceLifecycleDisposition WindowsSurfaceAdapter::markLost() noexcept {
    return lifecycle_.markLost(lifecycle_.current().viewId);
}

render::PresentFeedbackDisposition WindowsSurfaceAdapter::submit(
    const render::FrameState& frame) {
    return presentation_.submit(frame);
}

render::PresentFeedbackDisposition WindowsSurfaceAdapter::feedback(
    const render::PresentedFeedback& value) {
    return presentation_.receive(value);
}

} // namespace canvas::windows_demo
