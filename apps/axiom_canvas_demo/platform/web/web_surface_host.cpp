#include "web_surface_host.hpp"

#include <cmath>

namespace canvas::web_demo {

WebSurfaceHost::WebSurfaceHost(render::SurfaceSnapshot initial) noexcept
    : lifecycle_(initial), presentation_(lifecycle_), composition_(initial.viewId) {
    (void)composition_.open();
    facade_ = std::make_unique<runtime::CanvasRuntimeFacade>(
        composition_.runtime(), {1U, initial.viewId.value(),
                                  initial.surfaceGeneration.value(),
                                  initial.metricsGeneration.value()});
    composition_.attachControlFacade(facade_.get());
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
    const auto& metrics = replacement.metrics;
    if (!std::isfinite(metrics.logicalWidth) || !std::isfinite(metrics.logicalHeight) ||
        !std::isfinite(metrics.devicePixelRatio) || !std::isfinite(metrics.displayScale) ||
        metrics.logicalWidth <= 0.0F || metrics.logicalHeight <= 0.0F ||
        metrics.devicePixelRatio <= 0.0F || metrics.displayScale <= 0.0F ||
        metrics.physicalWidth == 0U || metrics.physicalHeight == 0U) {
        return WebSurfaceDisposition::kRejectedInvalid;
    }
    if (replacement.viewId != before.viewId ||
        replacement.surfaceGeneration.value() < before.surfaceGeneration.value() ||
        replacement.metricsGeneration.value() < before.metricsGeneration.value()) {
        return WebSurfaceDisposition::kRejectedStale;
    }
    auto plannedLifecycle = lifecycle_;
    const auto planned = plannedLifecycle.replace(replacement);
    if (planned == render::SurfaceLifecycleDisposition::kWrongView ||
        planned == render::SurfaceLifecycleDisposition::kInvalidGeneration ||
        planned == render::SurfaceLifecycleDisposition::kGenerationExhausted ||
        planned == render::SurfaceLifecycleDisposition::kInvalidMetrics ||
        planned == render::SurfaceLifecycleDisposition::kMetricsMismatch) {
        return WebSurfaceDisposition::kRejectedInvalid;
    }
    if (planned == render::SurfaceLifecycleDisposition::kStaleGeneration) {
        return WebSurfaceDisposition::kRejectedStale;
    }
    if (planned == render::SurfaceLifecycleDisposition::kNoGenerationAdvance) {
        return WebSurfaceDisposition::kRebound;
    }
    auto runtimeSurface = composition_.runtime().surface();
    // Composition owns the lifecycle transition. A Web bridge may have
    // prepared replacement providers first, which makes the Runtime surface
    // look available while the Composition is still Lost; do not treat that
    // intermediate fact as a completed restore.
    if (composition_.state() == runtime::RuntimeCompositionState::kLost) {
        const auto current = composition_.runtime().surface();
        const bool adopted = current.available &&
            current.generation == replacement.surfaceGeneration.value() &&
            current.width == replacement.metrics.physicalWidth &&
            current.height == replacement.metrics.physicalHeight;
        if (adopted) {
            if (!composition_.adoptPreparedSurface(current.generation, replacement.metrics)) {
                return WebSurfaceDisposition::kRejectedStale;
            }
        } else if (!composition_.rebindSurface(replacement.metrics)) {
            return WebSurfaceDisposition::kRejectedStale;
        }
        runtimeSurface = composition_.runtime().surface();
    }
    if (!runtimeSurface.available ||
        runtimeSurface.generation > replacement.surfaceGeneration.value()) {
        return WebSurfaceDisposition::kRejectedStale;
    }
    while (runtimeSurface.generation < replacement.surfaceGeneration.value()) {
        if (!composition_.resizeSurface(replacement.metrics.physicalWidth,
                                        replacement.metrics.physicalHeight)) {
            return WebSurfaceDisposition::kRejectedStale;
        }
        runtimeSurface = composition_.runtime().surface();
    }
    if (runtimeSurface.generation != replacement.surfaceGeneration.value() ||
        runtimeSurface.width != replacement.metrics.physicalWidth ||
        runtimeSurface.height != replacement.metrics.physicalHeight) {
        return WebSurfaceDisposition::kRejectedStale;
    }
    if (replacement.surfaceGeneration == before.surfaceGeneration &&
        replacement.metricsGeneration == before.metricsGeneration &&
        replacement.metrics == before.metrics) {
        return WebSurfaceDisposition::kRebound;
    }
    const auto result = lifecycle_.replace(replacement);
    if (result == render::SurfaceLifecycleDisposition::kReplaced) {
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
    if (composition_.state() == runtime::RuntimeCompositionState::kReady &&
        !composition_.loseSurface()) {
        return render::SurfaceLifecycleDisposition::kUnavailable;
    }
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
        runtimeFeedback.viewId = submitted->viewId;
        runtimeFeedback.surfaceGeneration = submitted->surfaceGeneration;
        runtimeFeedback.metricsGeneration = submitted->metricsGeneration;
    }
    if (!composition_.receivePlatformPresented(runtimeFeedback)) {
        return render::PresentFeedbackDisposition::kUnavailable;
    }
    return disposition;
}

} // namespace canvas::web_demo
