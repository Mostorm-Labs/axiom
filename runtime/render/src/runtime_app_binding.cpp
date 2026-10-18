#include "canvas/render/runtime_app_binding.hpp"
#include "canvas/render/skia_surface_provider.hpp"

namespace canvas::render {

RuntimeAppBinding::RuntimeAppBinding(ViewId viewId, SurfaceProfile initialSurface)
    : viewId_(viewId), surfaces_(viewId, std::move(initialSurface)) {}

bool RuntimeAppBinding::registerInputSource(InputSourceRegistration registration) {
    if (registration.source == 0U || registration.platform.empty()) return false;
    retiredSources_.erase(registration.source);
    return sources_.insert(registration.source).second;
}

bool RuntimeAppBinding::unregisterInputSource(input::InputSourceId source,
                                               std::uint64_t sequence,
                                               std::uint64_t timestampNs) {
    if (!sources_.contains(source)) return false;
    const auto result = ingress_.sourceLost(source, sequence, timestampNs);
    if (cancelSink_ != nullptr && result.accepted) cancelSink_->onSessionCancel(result);
    sources_.erase(source);
    retiredSources_.insert(source);
    sourceSequenceFloor_[source] = sequence;
    return true;
}

bool RuntimeAppBinding::notifySourceLost(input::InputSourceId source,
                                         std::uint64_t sequence,
                                         std::uint64_t timestampNs) noexcept {
    if (!sources_.contains(source)) return false;
    const auto result = ingress_.sourceLost(source, sequence, timestampNs);
    if (cancelSink_ != nullptr && result.accepted) cancelSink_->onSessionCancel(result);
    return true;
}

input::PlatformInteractionIngressResult RuntimeAppBinding::submit(
    input::InputSourceId source, const input::PlatformPointerBatch& batch) noexcept {
    if (!sources_.contains(source)) return {};
    for (const auto& sample : batch.samples) {
        if (sample.source != source) return {};
        if (sample.phase == input::PointerPhase::kDown) {
            const auto floor = sourceSequenceFloor_.find(source);
            if (floor != sourceSequenceFloor_.end() && sample.sequence <= floor->second) return {};
        }
    }
    return ingress_.submit(batch);
}

input::PlatformInteractionIngressResult RuntimeAppBinding::cancelPointer(
    const input::PointerKey& key, std::uint64_t sequence,
    std::uint64_t timestampNs) noexcept {
    if (!sources_.contains(key.source)) return {};
    return ingress_.cancelPointer(key, sequence, timestampNs);
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
SurfaceProviderDisposition RuntimeAppBinding::rebindSurface(SurfaceMetrics metrics) noexcept {
    return surfaces_.rebind(metrics);
}
SurfaceProviderDisposition RuntimeAppBinding::adoptPreparedSurface(SurfaceMetrics metrics) noexcept {
    return surfaces_.adoptPrepared(metrics);
}

SurfaceProviderDisposition RuntimeAppBinding::updateSurfacePair(
    SurfaceMetrics metrics, std::optional<SurfaceProfile>& canonical,
    std::optional<SurfaceProfile>& preview, bool restoring, bool initialBinding,
    const std::function<bool()>& commitAdmission) noexcept {
    if (!previewSurfaces_) return SurfaceProviderDisposition::kProviderUnavailable;
    const auto canonicalCheck = surfaces_.validateUpdate(metrics, canonical);
    if (canonicalCheck != SurfaceProviderDisposition::kCommitted) return canonicalCheck;
    const auto previewCheck = previewSurfaces_->validateUpdate(metrics, preview);
    if (previewCheck != SurfaceProviderDisposition::kCommitted) return previewCheck;
    bool touchedActive = false;
    const bool replacingPair = canonical.has_value() || preview.has_value();
    auto result = surfaces_.prepareUpdate(metrics, canonical, restoring, replacingPair, initialBinding, touchedActive);
    if (result == SurfaceProviderDisposition::kCommitted && !commitAdmission()) {
        result = SurfaceProviderDisposition::kStaleFrame;
    }
    if (result == SurfaceProviderDisposition::kCommitted) {
        result = previewSurfaces_->prepareUpdate(metrics, preview, restoring, replacingPair, initialBinding, touchedActive);
    }
    if (result == SurfaceProviderDisposition::kCommitted && !commitAdmission()) {
        result = SurfaceProviderDisposition::kStaleFrame;
    }
    if (result == SurfaceProviderDisposition::kCommitted && !surfaces_.activateUpdate(touchedActive)) {
        result = SurfaceProviderDisposition::kProviderUnavailable;
    }
    if (result == SurfaceProviderDisposition::kCommitted && !commitAdmission()) {
        result = SurfaceProviderDisposition::kStaleFrame;
    }
    if (result == SurfaceProviderDisposition::kCommitted && !previewSurfaces_->activateUpdate(touchedActive)) {
        result = SurfaceProviderDisposition::kProviderUnavailable;
    }
    if (result != SurfaceProviderDisposition::kCommitted || !commitAdmission()) {
        surfaces_.abortUpdate();
        previewSurfaces_->abortUpdate();
        if (touchedActive) invalidateSurfacePair();
        return result == SurfaceProviderDisposition::kCommitted
                   ? SurfaceProviderDisposition::kStaleFrame : result;
    }
    surfaces_.commitUpdate();
    previewSurfaces_->commitUpdate();
    return SurfaceProviderDisposition::kCommitted;
}

void RuntimeAppBinding::invalidateSurfacePair() noexcept {
    surfaces_.invalidate();
    if (previewSurfaces_) previewSurfaces_->invalidate();
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
