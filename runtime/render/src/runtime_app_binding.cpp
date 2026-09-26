#include "canvas/render/runtime_app_binding.hpp"
#include "canvas/render/skia_surface_provider.hpp"

namespace canvas::render {

RuntimeAppBinding::RuntimeAppBinding(ViewId viewId, SurfaceProfile initialSurface)
    : viewId_(viewId), surfaces_(viewId, std::move(initialSurface)) {}

bool RuntimeAppBinding::registerInputSource(InputSourceRegistration registration) {
    return registration.source != 0U && !registration.platform.empty() &&
           sources_.insert(registration.source).second;
}

bool RuntimeAppBinding::unregisterInputSource(input::InputSourceId source) {
    return sources_.erase(source) != 0U;
}

bool RuntimeAppBinding::notifySourceLost(input::InputSourceId source,
                                         std::uint64_t sequence,
                                         std::uint64_t timestampNs) noexcept {
    if (!sources_.contains(source)) return false;
    (void)ingress_.sourceLost(source, sequence, timestampNs);
    return true;
}

input::PlatformInteractionIngressResult RuntimeAppBinding::submit(
    input::InputSourceId source, const input::PlatformPointerBatch& batch) noexcept {
    if (!sources_.contains(source)) return {};
    for (const auto& sample : batch.samples) {
        if (sample.source != source) return {};
    }
    return ingress_.submit(batch);
}

std::optional<input::PointerKey> RuntimeAppBinding::keyFor(
    input::InputSourceId source, input::PointerId pointer) const noexcept {
    return ingress_.keyFor(source, pointer);
}

SurfaceProviderDisposition RuntimeAppBinding::registerSurfaceProfile(
    SurfaceProfile profile) {
    return surfaces_.registerProfile(std::move(profile));
}

SurfaceProviderDisposition RuntimeAppBinding::selectRenderProfile(
    std::string_view profileId, RenderTargetFormat format) {
    return surfaces_.select(profileId, format);
}

SurfaceProviderDisposition RuntimeAppBinding::resizeSurface(SurfaceMetrics metrics) {
    return surfaces_.resize(metrics);
}

SurfaceProviderDisposition RuntimeAppBinding::markSurfaceLost() noexcept {
    return surfaces_.markLost();
}

SurfaceProviderDisposition RuntimeAppBinding::rebindSurface() noexcept {
    return surfaces_.rebind();
}

SurfaceProviderDisposition RuntimeAppBinding::registerPreviewSurfaceProfile(
    SurfaceProfile profile) {
    if (previewSurfaces_ == nullptr) {
        if (profile.provider == nullptr || profile.info.profileId.empty()) {
            return SurfaceProviderDisposition::kInvalidProfile;
        }
        previewSurfaces_ = std::make_unique<SurfaceProviderRegistry>(
            viewId_, SurfaceProfile{profile.info, std::move(profile.provider)});
        return SurfaceProviderDisposition::kCommitted;
    }
    return previewSurfaces_->registerProfile(std::move(profile));
}

SurfaceProviderDisposition RuntimeAppBinding::selectPreviewRenderProfile(
    std::string_view profileId, RenderTargetFormat format) {
    return previewSurfaces_ == nullptr ? SurfaceProviderDisposition::kProviderUnavailable
                                       : previewSurfaces_->select(profileId, format);
}

SurfaceProviderDisposition RuntimeAppBinding::resizePreviewSurface(SurfaceMetrics metrics) {
    return previewSurfaces_ == nullptr ? SurfaceProviderDisposition::kProviderUnavailable
                                       : previewSurfaces_->resize(metrics);
}

SurfaceProviderDisposition RuntimeAppBinding::markPreviewSurfaceLost() noexcept {
    return previewSurfaces_ == nullptr ? SurfaceProviderDisposition::kProviderUnavailable
                                       : previewSurfaces_->markLost();
}

SurfaceProviderDisposition RuntimeAppBinding::rebindPreviewSurface() noexcept {
    return previewSurfaces_ == nullptr ? SurfaceProviderDisposition::kProviderUnavailable
                                       : previewSurfaces_->rebind();
}

} // namespace canvas::render
