#include "canvas/render/preview_surface.hpp"

#include "canvas/render/skia_renderer.hpp"

#include <cmath>
#include <algorithm>
#define AXIOM_PREVIEW_DIAG(...) static_cast<void>(0)

namespace canvas::render {

PreviewSurfaceController::PreviewSurfaceController(
    SkiaRenderer& renderer, SkiaSurfaceProvider& provider) noexcept
    : renderer_(renderer), provider_(provider) {
    state_.generation = provider_.generation();
}

bool PreviewSurfaceController::begin(std::uint64_t documentEpoch,
                                      std::uint64_t session,
                                      std::uint64_t sessionGeneration,
                                      std::uint64_t surfaceGeneration) noexcept {
    if (documentEpoch == 0U || session == 0U || sessionGeneration == 0U ||
        surfaceGeneration == 0U || provider_.generation() != surfaceGeneration) {
        return false;
    }
    if (!active_) {
        geometry_ = {};
        geometry_.documentEpoch = documentEpoch;
        geometry_.session = session;
        geometry_.sessionGeneration = sessionGeneration;
        geometry_.surfaceGeneration = SurfaceGeneration{surfaceGeneration};
    } else if (geometry_.documentEpoch != documentEpoch ||
               geometry_.surfaceGeneration.value() != surfaceGeneration) {
        return false;
    }
    geometry_.contours.erase(
        std::remove_if(geometry_.contours.begin(), geometry_.contours.end(),
                       [session](const auto& contour) {
                           return contour.session == session;
                       }),
        geometry_.contours.end());
    geometry_.contours.push_back({session, 0U, {}});
    defaultSession_ = session;
    state_.generation = surfaceGeneration;
    state_.dirty = true;
    active_ = true;
    AXIOM_PREVIEW_DIAG("begin epoch=%llu session=%llu gen=%llu provider=%llu active=%d", static_cast<unsigned long long>(documentEpoch), static_cast<unsigned long long>(session), static_cast<unsigned long long>(surfaceGeneration), static_cast<unsigned long long>(provider_.generation()), active_ ? 1 : 0);
    return true;
}

bool PreviewSurfaceController::update(const ink::BrushPreviewDelta& delta,
                                      float scale, float translationX,
                                      float translationY) noexcept {
    return updateForSession(defaultSession_, delta, scale, translationX,
                            translationY);
}

bool PreviewSurfaceController::updateForSession(
    std::uint64_t session, const ink::BrushPreviewDelta& delta, float scale,
    float translationX, float translationY) noexcept {
    if (!active_ || session == 0U || delta.revision == 0U ||
        !std::isfinite(scale) ||
        scale <= 0.0F || !std::isfinite(translationX) ||
        !std::isfinite(translationY)) {
        AXIOM_PREVIEW_DIAG("update reject active=%d delta=%llu geom=%llu scale=%f", active_ ? 1 : 0, static_cast<unsigned long long>(delta.revision), static_cast<unsigned long long>(geometry_.revision), scale); return false;
    }
    const bool viewportChanged = geometry_.viewport.scale != scale ||
        geometry_.viewport.translationX != translationX ||
        geometry_.viewport.translationY != translationY;
    // A duplicate revision is a no-op unless the viewport itself changed.
    // This is the runtime-side dirty gate: repeated RAF/tick delivery cannot
    // create another preview submission for the same content revision.
    auto contour = std::find_if(geometry_.contours.begin(), geometry_.contours.end(),
                                [session](const auto& value) {
                                    return value.session == session;
                                });
    if (contour == geometry_.contours.end()) return false;
    if (delta.revision == contour->revision && !viewportChanged) return true;
    contour->revision = delta.revision;
    contour->outline = delta.outline;
    geometry_.revision = std::max(geometry_.revision, delta.revision);
    if (session == defaultSession_) geometry_.outline = delta.outline;
    geometry_.viewport = {scale, translationX, translationY};
    state_.contentRevision = delta.revision;
    state_.dirty = true;
    return true;
}

bool PreviewSurfaceController::updateViewport(float scale,
                                              float translationX,
                                              float translationY) noexcept {
    if (!active_ || !std::isfinite(scale) || scale <= 0.0F ||
        !std::isfinite(translationX) || !std::isfinite(translationY)) {
        return false;
    }
    if (geometry_.viewport.scale == scale &&
        geometry_.viewport.translationX == translationX &&
        geometry_.viewport.translationY == translationY) {
        return true;
    }
    geometry_.viewport = {scale, translationX, translationY};
    state_.dirty = true;
    return true;
}

bool PreviewSurfaceController::cancel() noexcept {
    return cancelSession(defaultSession_);
}

bool PreviewSurfaceController::cancelSession(std::uint64_t session) noexcept {
    if (!active_ || session == 0U) return false;
    const auto it = std::find_if(geometry_.contours.begin(), geometry_.contours.end(),
                                 [session](const auto& value) {
                                     return value.session == session;
                                 });
    if (it == geometry_.contours.end()) return false;
    it->outline.clear();
    ++it->revision;
    if (session == defaultSession_) geometry_.outline.clear();
    ++geometry_.revision;
    state_.contentRevision = geometry_.revision;
    state_.dirty = true;
    return true;
}

bool PreviewSurfaceController::retireSession(
    std::uint64_t session, std::uint64_t surfaceGeneration) noexcept {
    if (!active_ || session == 0U ||
        surfaceGeneration != geometry_.surfaceGeneration.value() ||
        provider_.generation() != surfaceGeneration) return false;
    const auto before = geometry_.contours.size();
    geometry_.contours.erase(
        std::remove_if(geometry_.contours.begin(), geometry_.contours.end(),
                       [session](const auto& contour) {
                           return contour.session == session;
                       }),
        geometry_.contours.end());
    if (geometry_.contours.size() == before) return false;
    ++geometry_.revision;
    state_.contentRevision = geometry_.revision;
    state_.dirty = true;
    if (!renderIfDirty()) return false;
    if (geometry_.contours.empty()) {
        provider_.setOverlayVisible(false);
        active_ = false;
    }
    return true;
}

bool PreviewSurfaceController::rebind(std::uint64_t surfaceGeneration) noexcept {
    if (!active_ || surfaceGeneration == 0U ||
        provider_.generation() != surfaceGeneration) return false;
    geometry_.surfaceGeneration = SurfaceGeneration{surfaceGeneration};
    state_.generation = surfaceGeneration;
    state_.dirty = true;
    return true;
}

bool PreviewSurfaceController::renderIfDirty(const PreviewStyleOverride& style) noexcept {
    AXIOM_PREVIEW_DIAG("render active=%d dirty=%d rev=%llu contours=%zu provider=%llu geomgen=%llu", active_ ? 1 : 0, state_.dirty ? 1 : 0, static_cast<unsigned long long>(geometry_.revision), geometry_.contours.size(), static_cast<unsigned long long>(provider_.generation()), static_cast<unsigned long long>(geometry_.surfaceGeneration.value()));
    if (!active_ || !state_.dirty) return true;
    if (provider_.generation() != geometry_.surfaceGeneration.value()) return false;
    bool hasGeometry = false;
    for (const auto& contour : geometry_.contours) hasGeometry |= !contour.outline.empty();
    const auto result = !hasGeometry
        ? renderer_.clearPreview(provider_, geometry_.surfaceGeneration)
        : renderer_.renderPreview(provider_, geometry_, style);
    if (result.code != BackendSubmissionCode::kAccepted) return false;
    provider_.setOverlayVisible(hasGeometry);
    ++state_.submissionCount;
    state_.presentCount = provider_.presentCount();
    state_.submittedRevision = state_.contentRevision;
    state_.dirty = false;
    return true;
}

bool PreviewSurfaceController::clearAfterCanonicalVisible(
    std::uint64_t surfaceGeneration) noexcept {
    if (!active_ || surfaceGeneration != geometry_.surfaceGeneration.value() ||
        provider_.generation() != surfaceGeneration) return false;
    geometry_.contours.clear();
    geometry_.outline.clear();
    ++geometry_.revision;
    state_.contentRevision = geometry_.revision;
    state_.dirty = true;
    if (!renderIfDirty()) return false;
    provider_.setOverlayVisible(false);
    active_ = false;
    return true;
}

bool PreviewSurfaceController::clearAfterCanonicalVisible(
    std::uint64_t documentEpoch, std::uint64_t session,
    std::uint64_t sessionGeneration, std::uint64_t surfaceGeneration) noexcept {
    if (!active_ || geometry_.documentEpoch != documentEpoch ||
        geometry_.session != session ||
        geometry_.sessionGeneration != sessionGeneration) return false;
    return clearAfterCanonicalVisible(surfaceGeneration);
}

} // namespace canvas::render
