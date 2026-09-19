#include "web_surface_host.hpp"

namespace canvas::web_demo {

WebSurfaceHost::WebSurfaceHost(render::SurfaceSnapshot initial) noexcept
    : lifecycle_(initial), presentation_(lifecycle_) {}

WebSurfaceDisposition WebSurfaceHost::bind(const render::SurfaceSnapshot& replacement) {
    const auto before = lifecycle_.current();
    const auto result = lifecycle_.replace(replacement);
    if (result == render::SurfaceLifecycleDisposition::kReplaced) return WebSurfaceDisposition::kRebound;
    if (result == render::SurfaceLifecycleDisposition::kStaleGeneration ||
        result == render::SurfaceLifecycleDisposition::kNoGenerationAdvance ||
        replacement.surfaceGeneration.value() < before.surfaceGeneration.value() ||
        replacement.metricsGeneration.value() < before.metricsGeneration.value()) {
        return WebSurfaceDisposition::kRejectedStale;
    }
    return WebSurfaceDisposition::kRejectedInvalid;
}

render::SurfaceLifecycleDisposition WebSurfaceHost::markLost() noexcept {
    return lifecycle_.markLost(lifecycle_.current().viewId);
}
render::PresentFeedbackDisposition WebSurfaceHost::submit(const render::FrameState& frame) {
    return presentation_.submit(frame);
}
render::PresentFeedbackDisposition WebSurfaceHost::feedback(const render::PresentedFeedback& value) {
    return presentation_.receive(value);
}

} // namespace canvas::web_demo
