#include "web_surface_host.hpp"

namespace canvas::web_demo {

WebSurfaceHost::WebSurfaceHost(render::SurfaceSnapshot initial) noexcept
    : lifecycle_(initial), presentation_(lifecycle_) {
    (void)composition_.open();
    (void)composition_.bindSurface(initial.metrics.physicalWidth,
                                   initial.metrics.physicalHeight);
    // RuntimeAppBinding advances its provider generation during the initial
    // bind. Bring it to the platform's declared generation before exposing
    // frames, so both sides share one identity from the first submission.
    while (composition_.runtime().surface().generation <
           initial.surfaceGeneration.value()) {
        if (!composition_.resizeSurface(initial.metrics.physicalWidth,
                                        initial.metrics.physicalHeight)) break;
    }
    const auto runtimeSurface = composition_.runtime().surface();
    (void)lifecycle_.replace(render::SurfaceSnapshot{
        initial.viewId, render::SurfaceGeneration{runtimeSurface.generation},
        render::MetricsGeneration{runtimeSurface.generation},
        render::SurfaceMetrics{static_cast<float>(runtimeSurface.width),
                               static_cast<float>(runtimeSurface.height),
                               runtimeSurface.width, runtimeSurface.height,
                               initial.metrics.devicePixelRatio,
                               initial.metrics.displayScale}});
}

WebSurfaceDisposition WebSurfaceHost::bind(const render::SurfaceSnapshot& replacement) {
    const auto before = lifecycle_.current();
    const auto result = lifecycle_.replace(replacement);
    if (result == render::SurfaceLifecycleDisposition::kReplaced) {
        const auto runtimeSurface = composition_.runtime().surface();
        if (runtimeSurface.generation != replacement.surfaceGeneration.value() ||
            runtimeSurface.width != replacement.metrics.physicalWidth ||
            runtimeSurface.height != replacement.metrics.physicalHeight) {
            return WebSurfaceDisposition::kRejectedStale;
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

bool WebSurfaceHost::syncRuntimeSurfaceIdentity() noexcept {
    const auto runtimeSurface = composition_.runtime().surface();
    if (!runtimeSurface.available || runtimeSurface.generation == 0U) return false;
    const auto current = lifecycle_.current();
    if (runtimeSurface.generation == current.surfaceGeneration.value()) return true;
    if (runtimeSurface.generation < current.surfaceGeneration.value()) return false;
    const auto result = lifecycle_.replace(render::SurfaceSnapshot{
        current.viewId, render::SurfaceGeneration{runtimeSurface.generation},
        render::MetricsGeneration{runtimeSurface.generation},
        render::SurfaceMetrics{static_cast<float>(runtimeSurface.width),
                               static_cast<float>(runtimeSurface.height),
                               runtimeSurface.width, runtimeSurface.height,
                               current.metrics.devicePixelRatio,
                               current.metrics.displayScale}});
    return result == render::SurfaceLifecycleDisposition::kReplaced;
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
    auto runtimeFeedback = value;
    if (const auto submitted = composition_.runtime().lastSubmittedCanonicalFrame();
        submitted.has_value() && submitted->frameId == value.frameId) {
        runtimeFeedback.surfaceGeneration = submitted->surfaceGeneration;
        runtimeFeedback.metricsGeneration = submitted->metricsGeneration;
    }
    if (!composition_.receivePlatformPresented(runtimeFeedback)) {
        return render::PresentFeedbackDisposition::kUnavailable;
    }
    return disposition;
}

} // namespace canvas::web_demo
