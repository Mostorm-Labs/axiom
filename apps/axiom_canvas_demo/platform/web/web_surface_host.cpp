#include "web_surface_host.hpp"

namespace canvas::web_demo {

WebSurfaceHost::WebSurfaceHost(render::SurfaceSnapshot initial) noexcept
    : lifecycle_(initial), presentation_(lifecycle_) { (void)composition_.open(); (void)composition_.bindSurface(initial.metrics.physicalWidth, initial.metrics.physicalHeight); }

WebSurfaceDisposition WebSurfaceHost::bind(const render::SurfaceSnapshot& replacement) {
    const auto before = lifecycle_.current();
    const auto result = lifecycle_.replace(replacement);
    if (result == render::SurfaceLifecycleDisposition::kReplaced) {
        if (!composition_.resizeSurface(replacement.metrics.physicalWidth,
                                        replacement.metrics.physicalHeight)) {
            return WebSurfaceDisposition::kRejectedInvalid;
        }
        return WebSurfaceDisposition::kRebound;
    }
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
    const auto disposition = presentation_.submit(frame);
    if (disposition != render::PresentFeedbackDisposition::kSubmitted) return disposition;
    if (!composition_.requestCanonicalFrame(frame.frameId.value())) {
        return render::PresentFeedbackDisposition::kUnavailable;
    }
    return disposition;
}
render::PresentFeedbackDisposition WebSurfaceHost::feedback(const render::PresentedFeedback& value) {
    const auto disposition = presentation_.receive(value);
    if (disposition != render::PresentFeedbackDisposition::kPresented) return disposition;
    if (!composition_.receivePlatformPresented(value)) {
        return render::PresentFeedbackDisposition::kUnavailable;
    }
    return disposition;
}

} // namespace canvas::web_demo
