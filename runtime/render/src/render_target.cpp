#include "canvas/render/render_target.hpp"
#include "canvas/render/skia_surface_provider.hpp"

#include <algorithm>
#include <limits>
#include <utility>
#include <cmath>

namespace canvas::render {

SurfaceProfile::~SurfaceProfile() = default;
SurfaceProfile::SurfaceProfile(RenderTargetInfo value,
                               std::unique_ptr<SkiaSurfaceProvider> target)
    : info(std::move(value)), provider(std::move(target)) {}
SurfaceProfile::SurfaceProfile(SurfaceProfile&&) noexcept = default;

SurfaceProfileRegistry::SurfaceProfileRegistry(ViewId viewId,
                                               RenderTargetInfo initial)
    : active_(std::move(initial)),
      lifecycle_(SurfaceSnapshot{viewId, SurfaceGeneration{1}, MetricsGeneration{1},
                                 active_->metrics}) {
    (void)registerProfile(*active_);
}

bool SurfaceProfileRegistry::validProfile(const RenderTargetInfo& profile) noexcept {
    return !profile.profileId.empty() && profile.metrics.logicalWidth > 0.0F &&
           profile.metrics.logicalHeight > 0.0F && profile.metrics.physicalWidth > 0U &&
           profile.metrics.physicalHeight > 0U && profile.metrics.devicePixelRatio > 0.0F &&
           profile.metrics.displayScale > 0.0F;
}

bool SurfaceProfileRegistry::registerProfile(RenderTargetInfo profile) {
    if (!validProfile(profile)) return false;
    const auto existing = std::find_if(profiles_.begin(), profiles_.end(),
                                       [&](const RenderTargetInfo& value) {
                                           return value.profileId == profile.profileId;
                                       });
    if (existing != profiles_.end()) return false;
    profiles_.push_back(std::move(profile));
    return true;
}

RenderTargetSwitchDisposition SurfaceProfileRegistry::switchTo(
    std::string_view profileId) {
    const auto found = std::find_if(profiles_.begin(), profiles_.end(),
                                    [&](const RenderTargetInfo& value) {
                                        return value.profileId == profileId;
                                    });
    if (found == profiles_.end()) return RenderTargetSwitchDisposition::kUnknownProfile;
    if (!validProfile(*found)) return RenderTargetSwitchDisposition::kInvalidProfile;
    const auto currentSurface = lifecycle_.current().surfaceGeneration.value();
    const auto currentMetrics = lifecycle_.current().metricsGeneration.value();
    if (currentSurface == std::numeric_limits<std::uint64_t>::max() ||
        currentMetrics == std::numeric_limits<std::uint64_t>::max()) {
        return RenderTargetSwitchDisposition::kGenerationExhausted;
    }
    const auto result = lifecycle_.replace(SurfaceSnapshot{
        lifecycle_.current().viewId, SurfaceGeneration{currentSurface + 1U},
        MetricsGeneration{currentMetrics + 1U}, found->metrics});
    if (result != SurfaceLifecycleDisposition::kReplaced) {
        return RenderTargetSwitchDisposition::kInvalidProfile;
    }
    active_.emplace(*found);
    return RenderTargetSwitchDisposition::kSwitched;
}

RenderTargetSwitchDisposition SurfaceProfileRegistry::markLost() noexcept {
    return lifecycle_.markLost(lifecycle_.current().viewId) ==
                   SurfaceLifecycleDisposition::kLost
               ? RenderTargetSwitchDisposition::kSwitched
               : RenderTargetSwitchDisposition::kInvalidProfile;
}

SurfaceProviderRegistry::SurfaceProviderRegistry(ViewId viewId, SurfaceProfile initial)
    : activeProvider_(std::move(initial.provider)),
      activeProfileId_(initial.info.profileId),
      lifecycle_(SurfaceSnapshot{viewId, SurfaceGeneration{1}, MetricsGeneration{1},
                                 initial.info.metrics}) {
    activeInfo_.emplace(std::move(initial.info));
    if (activeProvider_ != nullptr) {
        profiles_.push_back({*activeInfo_, nullptr});
        (void)activeProvider_->resize(activeInfo_->metrics.physicalWidth,
                                      activeInfo_->metrics.physicalHeight);
    }
}

SurfaceProviderRegistry::~SurfaceProviderRegistry() = default;

bool SurfaceProviderRegistry::validMetrics(const SurfaceMetrics& metrics) noexcept {
    return std::isfinite(metrics.logicalWidth) && metrics.logicalWidth > 0.0F &&
           std::isfinite(metrics.logicalHeight) && metrics.logicalHeight > 0.0F &&
           metrics.physicalWidth > 0U && metrics.physicalHeight > 0U &&
           std::isfinite(metrics.devicePixelRatio) && metrics.devicePixelRatio > 0.0F &&
           std::isfinite(metrics.displayScale) && metrics.displayScale > 0.0F;
}

SurfaceProviderDisposition SurfaceProviderRegistry::registerProfile(SurfaceProfile profile) {
    if (profile.info.profileId.empty() || profile.provider == nullptr ||
        !validMetrics(profile.info.metrics)) return SurfaceProviderDisposition::kInvalidProfile;
    for (const auto& entry : profiles_) {
        if (entry.info.profileId == profile.info.profileId) return SurfaceProviderDisposition::kDuplicateProfile;
    }
    if (profile.provider->resize(profile.info.metrics.physicalWidth,
                                 profile.info.metrics.physicalHeight).code !=
        BackendSubmissionCode::kAccepted) {
        return SurfaceProviderDisposition::kProviderUnavailable;
    }
    profiles_.push_back({std::move(profile.info), std::move(profile.provider)});
    return SurfaceProviderDisposition::kCommitted;
}

SurfaceProviderDisposition SurfaceProviderRegistry::commit(
    RenderTargetInfo info, std::unique_ptr<SkiaSurfaceProvider>& provider) {
    if (provider == nullptr) return SurfaceProviderDisposition::kProviderUnavailable;
    const auto description = provider->describe();
    if (description.format != info.format) {
        return SurfaceProviderDisposition::kFormatMismatch;
    }
    if (description.metrics.physicalWidth != info.metrics.physicalWidth ||
        description.metrics.physicalHeight != info.metrics.physicalHeight) {
        return SurfaceProviderDisposition::kMetricsMismatch;
    }
    const auto acquired = provider->acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired) {
        return SurfaceProviderDisposition::kProviderUnavailable;
    }
    provider->release();
    const auto currentSurface = lifecycle_.current().surfaceGeneration.value();
    const auto currentMetrics = lifecycle_.current().metricsGeneration.value();
    if (currentSurface == std::numeric_limits<std::uint64_t>::max() ||
        currentMetrics == std::numeric_limits<std::uint64_t>::max()) {
        return SurfaceProviderDisposition::kGenerationExhausted;
    }
    const auto nextSurface = currentSurface + 1U;
    if (provider->generation() > nextSurface) {
        return SurfaceProviderDisposition::kStaleFrame;
    }
    while (provider->generation() < nextSurface) {
        if (provider->advanceGeneration().code != BackendSubmissionCode::kAccepted) {
            return SurfaceProviderDisposition::kProviderUnavailable;
        }
    }
    if (lifecycle_.replace(SurfaceSnapshot{lifecycle_.current().viewId,
                                           SurfaceGeneration{nextSurface},
                                           MetricsGeneration{currentMetrics + 1U},
                                           info.metrics}) != SurfaceLifecycleDisposition::kReplaced) {
        return SurfaceProviderDisposition::kMetricsMismatch;
    }
    activeProvider_ = std::move(provider);
    activeInfo_.emplace(std::move(info));
    return SurfaceProviderDisposition::kCommitted;
}

SurfaceProviderDisposition SurfaceProviderRegistry::select(
    std::string_view profileId, RenderTargetFormat format) {
    for (auto& entry : profiles_) {
        if (entry.info.profileId != profileId) continue;
        if (entry.info.format != format) return SurfaceProviderDisposition::kFormatMismatch;
        if (entry.info.profileId == activeProfileId_) return SurfaceProviderDisposition::kCommitted;
        if (entry.provider == nullptr) return SurfaceProviderDisposition::kProviderUnavailable;
        Entry* activeEntry = nullptr;
        for (auto& possible : profiles_) {
            if (possible.info.profileId == activeProfileId_) {
                activeEntry = &possible;
                break;
            }
        }
        if (activeEntry == nullptr) return SurfaceProviderDisposition::kProviderUnavailable;
        auto oldProvider = std::move(activeProvider_);
        const auto result = commit(entry.info, entry.provider);
        if (result == SurfaceProviderDisposition::kCommitted) {
            activeEntry->provider = std::move(oldProvider);
            activeProfileId_ = entry.info.profileId;
        } else {
            activeProvider_ = std::move(oldProvider);
        }
        return result;
    }
    return SurfaceProviderDisposition::kUnknownProfile;
}

SurfaceProviderDisposition SurfaceProviderRegistry::resize(SurfaceMetrics metrics) {
    if (activeProvider_ == nullptr || !validMetrics(metrics)) return SurfaceProviderDisposition::kInvalidProfile;
    const auto resized = activeProvider_->resize(metrics.physicalWidth, metrics.physicalHeight);
    if (resized.code != BackendSubmissionCode::kAccepted) return SurfaceProviderDisposition::kProviderUnavailable;
    const auto currentSurface = lifecycle_.current().surfaceGeneration.value();
    const auto currentMetrics = lifecycle_.current().metricsGeneration.value();
    if (currentSurface == std::numeric_limits<std::uint64_t>::max() || currentMetrics == std::numeric_limits<std::uint64_t>::max()) return SurfaceProviderDisposition::kGenerationExhausted;
    const auto nextSurface = currentSurface + 1U;
    while (activeProvider_->generation() < nextSurface) {
        if (activeProvider_->advanceGeneration().code != BackendSubmissionCode::kAccepted) return SurfaceProviderDisposition::kProviderUnavailable;
    }
    if (activeProvider_->generation() > nextSurface) return SurfaceProviderDisposition::kStaleFrame;
    if (lifecycle_.replace(SurfaceSnapshot{lifecycle_.current().viewId, SurfaceGeneration{nextSurface}, MetricsGeneration{currentMetrics + 1U}, metrics}) != SurfaceLifecycleDisposition::kReplaced) return SurfaceProviderDisposition::kMetricsMismatch;
    activeInfo_.emplace(activeInfo_->profileId, activeInfo_->kind, activeInfo_->backend,
                        activeInfo_->format, metrics, activeInfo_->capabilities);
    return SurfaceProviderDisposition::kCommitted;
}

SurfaceProviderDisposition SurfaceProviderRegistry::markLost() noexcept {
    if (activeProvider_ == nullptr) return SurfaceProviderDisposition::kProviderUnavailable;
    if (activeProvider_->lose().code != BackendSubmissionCode::kAccepted) return SurfaceProviderDisposition::kProviderUnavailable;
    return lifecycle_.markLost(lifecycle_.current().viewId) == SurfaceLifecycleDisposition::kLost ? SurfaceProviderDisposition::kCommitted : SurfaceProviderDisposition::kProviderUnavailable;
}

SurfaceProviderDisposition SurfaceProviderRegistry::rebind() noexcept {
    if (activeProvider_ == nullptr) return SurfaceProviderDisposition::kProviderUnavailable;
    const auto currentSurface = lifecycle_.current().surfaceGeneration.value();
    const auto currentMetrics = lifecycle_.current().metricsGeneration.value();
    if (currentSurface == std::numeric_limits<std::uint64_t>::max() || currentMetrics == std::numeric_limits<std::uint64_t>::max()) return SurfaceProviderDisposition::kGenerationExhausted;
    const auto nextSurface = currentSurface + 1U;
    while (activeProvider_->generation() < nextSurface) {
        if (activeProvider_->advanceGeneration().code != BackendSubmissionCode::kAccepted) return SurfaceProviderDisposition::kProviderUnavailable;
    }
    if (activeProvider_->generation() > nextSurface) return SurfaceProviderDisposition::kStaleFrame;
    const auto acquired = activeProvider_->acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired) return SurfaceProviderDisposition::kProviderUnavailable;
    activeProvider_->release();
    if (lifecycle_.replace(SurfaceSnapshot{lifecycle_.current().viewId, SurfaceGeneration{nextSurface}, MetricsGeneration{currentMetrics + 1U}, activeInfo_->metrics}) != SurfaceLifecycleDisposition::kReplaced) return SurfaceProviderDisposition::kMetricsMismatch;
    return SurfaceProviderDisposition::kCommitted;
}

} // namespace canvas::render
