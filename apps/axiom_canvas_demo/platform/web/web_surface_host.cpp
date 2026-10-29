#include "web_surface_host.hpp"

#include <algorithm>
#include <cmath>

namespace canvas::web_demo {

WebSurfaceHost::WebSurfaceHost(render::SurfaceSnapshot initial, bool deferPlatformBinding) noexcept
    : lifecycle_({initial.viewId, render::SurfaceGeneration{1},
                  render::MetricsGeneration{1}, initial.metrics}),
      presentation_(lifecycle_), composition_(initial.viewId.value()) {
    submittedRuntimeFrames_.reserve(8U);
    (void)composition().open();
    if (!deferPlatformBinding) {
        (void)composition().bindSurface(initial.metrics);
        (void)syncRuntimeSurfaceIdentity();
    }
}

WebSurfaceDisposition WebSurfaceHost::bind(const render::SurfaceSnapshot& replacement) {
    const auto& expected = lifecycle_.current();
    // The caller echoes the accepted binding it observed. Runtime allocates
    // the next generations; the Host never manufactures them with resize.
    if (replacement.viewId != expected.viewId ||
        replacement.surfaceGeneration != expected.surfaceGeneration ||
        replacement.metricsGeneration != expected.metricsGeneration) {
        return WebSurfaceDisposition::kRejectedStale;
    }
    return bindMetrics(replacement.metrics);
}

WebSurfaceDisposition WebSurfaceHost::bindMetrics(const render::SurfaceMetrics& metrics) {
    if (!std::isfinite(metrics.logicalWidth) || !std::isfinite(metrics.logicalHeight) ||
        !std::isfinite(metrics.devicePixelRatio) || !std::isfinite(metrics.displayScale) ||
        metrics.logicalWidth <= 0.0F || metrics.logicalHeight <= 0.0F ||
        metrics.devicePixelRatio <= 0.0F || metrics.displayScale <= 0.0F ||
        metrics.physicalWidth == 0U || metrics.physicalHeight == 0U) {
        return WebSurfaceDisposition::kRejectedInvalid;
    }
    const auto before = composition().surfaceSnapshot();
    const bool committed = composition().state() == runtime::RuntimeCompositionState::kBound
        ? composition().bindSurface(metrics)
        : composition().state() == runtime::RuntimeCompositionState::kLost
            ? composition().rebindSurface(metrics) : composition().resizeSurface(metrics);
    if (!committed) {
        if (composition().state() == runtime::RuntimeCompositionState::kLost) {
            submittedRuntimeFrames_.clear();
            (void)lifecycle_.markLost(lifecycle_.current().viewId);
        }
        return WebSurfaceDisposition::kRejectedInvalid;
    }
    const auto accepted = composition().surfaceSnapshot();
    if (accepted.surfaceGeneration != before.surfaceGeneration ||
        accepted.metricsGeneration != before.metricsGeneration) submittedRuntimeFrames_.clear();
    return syncRuntimeSurfaceIdentity() ? WebSurfaceDisposition::kRebound
                                       : WebSurfaceDisposition::kRejectedInvalid;
}

bool WebSurfaceHost::syncRuntimeSurfaceIdentity() noexcept {
    const auto accepted = composition().surfaceSnapshot();
    if (!accepted.available || composition().state() != runtime::RuntimeCompositionState::kReady) return false;
    const render::SurfaceSnapshot snapshot{
        accepted.viewId, render::SurfaceGeneration{accepted.surfaceGeneration},
        render::MetricsGeneration{accepted.metricsGeneration},
        render::SurfaceMetrics{accepted.logicalWidth, accepted.logicalHeight,
            accepted.width, accepted.height, accepted.devicePixelRatio, accepted.displayScale}};
    const auto result = lifecycle_.replace(snapshot);
    // Initial prepared resources may use the first surface identity while
    // supplying complete logical metrics for that same initial binding.
    if (result == render::SurfaceLifecycleDisposition::kMetricsMismatch &&
        lifecycle_.current().surfaceGeneration == snapshot.surfaceGeneration &&
        lifecycle_.current().metricsGeneration == snapshot.metricsGeneration) {
        return lifecycle_.current().metrics == snapshot.metrics;
    }
    return result == render::SurfaceLifecycleDisposition::kReplaced ||
           result == render::SurfaceLifecycleDisposition::kNoGenerationAdvance;
}

render::SurfaceLifecycleDisposition WebSurfaceHost::markLost() noexcept {
    submittedRuntimeFrames_.clear();
    if (composition().state() == runtime::RuntimeCompositionState::kReady &&
        !composition().loseSurface()) {
        return render::SurfaceLifecycleDisposition::kUnavailable;
    }
    return lifecycle_.markLost(lifecycle_.current().viewId);
}
render::PresentFeedbackDisposition WebSurfaceHost::submit(const render::FrameState& frame) {
    // Reserve a bounded platform-to-runtime identity slot before touching the
    // presentation tracker or Runtime.  A rejected submission must be a true
    // no-op so the last accepted Runtime frame keeps its feedback path.
    const bool alreadyMapped = std::find_if(
        submittedRuntimeFrames_.begin(), submittedRuntimeFrames_.end(),
        [&](const FrameMapping& item) { return item.platformFrame == frame.frameId; }) !=
        submittedRuntimeFrames_.end();
    if (!alreadyMapped && submittedRuntimeFrames_.size() >= 8U) {
        return render::PresentFeedbackDisposition::kUnavailable;
    }
    const auto disposition = presentation_.submit(frame);
    if (disposition != render::PresentFeedbackDisposition::kSubmitted) return disposition;
    runtime::RuntimeFrameOpportunity opportunity{};
    if (!composition().serviceFrameOpportunity(true, opportunity) ||
        !opportunity.canonicalFrame.has_value() || !opportunity.canonicalWork ||
        opportunity.canonicalFrame->viewId != frame.viewId ||
        opportunity.canonicalFrame->surfaceGeneration != frame.surfaceGeneration ||
        opportunity.canonicalFrame->metricsGeneration != frame.metricsGeneration) {
        return render::PresentFeedbackDisposition::kUnavailable;
    }
    submittedRuntimeFrames_.push_back({frame.frameId, *opportunity.canonicalFrame,
                                      *opportunity.canonicalWork});
    return disposition;
}
std::optional<runtime::CanonicalWork> WebSurfaceHost::submittedCanonicalWork(
    render::FrameId platformFrame) const noexcept {
    const auto mapping = std::find_if(submittedRuntimeFrames_.begin(), submittedRuntimeFrames_.end(),
        [&](const FrameMapping& item) { return item.platformFrame == platformFrame; });
    return mapping == submittedRuntimeFrames_.end() ? std::nullopt
        : std::optional<runtime::CanonicalWork>{mapping->runtimeWork};
}
render::PresentFeedbackDisposition WebSurfaceHost::feedback(
    const render::PresentedFeedback& value, const runtime::CanonicalWork& work) {
    const auto mapping = std::find_if(
        submittedRuntimeFrames_.begin(), submittedRuntimeFrames_.end(),
        [&](const FrameMapping& item) { return item.platformFrame == value.frameId; });
    if (mapping == submittedRuntimeFrames_.end()) {
        return render::PresentFeedbackDisposition::kUnknownFrame;
    }
    // Never relabel a bare or another instance's callback with this host's
    // current Runtime work. The platform must retain the original token.
    if (mapping->runtimeWork != work) return render::PresentFeedbackDisposition::kStaleGeneration;
    const auto current = composition().lastSubmittedCanonicalWork();
    if (!current.has_value() || current.value() != mapping->runtimeWork) {
        // A replaced/late platform frame cannot acknowledge the newer Runtime
        // frame. Leave the newer mapping and Runtime handoff untouched.
        submittedRuntimeFrames_.erase(mapping);
        return render::PresentFeedbackDisposition::kStaleGeneration;
    }
    const auto disposition = presentation_.receive(value);
    if (disposition != render::PresentFeedbackDisposition::kPresented) return disposition;
    if (!composition().receivePlatformPresented({mapping->runtimeWork,
            value.outcome, value.evidence, value.presentedTimeNs})) {
        return render::PresentFeedbackDisposition::kUnavailable;
    }
    submittedRuntimeFrames_.erase(mapping);
    return disposition;
}

} // namespace canvas::web_demo
