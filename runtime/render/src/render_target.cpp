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

SurfaceProviderDisposition SurfaceProviderRegistry::validateUpdate(
    const SurfaceMetrics& metrics, const std::optional<SurfaceProfile>& replacement) const noexcept {
    if (!validMetrics(metrics) || !activeProvider_ ||
        (replacement && (!replacement->provider || replacement->info.profileId.empty()))) {
        return SurfaceProviderDisposition::kInvalidProfile;
    }
    constexpr auto limit = std::numeric_limits<std::uint64_t>::max();
    const auto& current = lifecycle_.current();
    if (current.surfaceGeneration.value() >= limit - 1U ||
        current.metricsGeneration.value() >= limit - 1U) return SurfaceProviderDisposition::kGenerationExhausted;
    const auto* provider = replacement ? replacement->provider.get() : activeProvider_.get();
    if (provider->generation() >= limit - 1U) return SurfaceProviderDisposition::kGenerationExhausted;
    if (replacement && provider->generation() > current.surfaceGeneration.value() + 1U) {
        return SurfaceProviderDisposition::kStaleFrame;
    }
    return SurfaceProviderDisposition::kCommitted;
}

SurfaceProviderDisposition SurfaceProviderRegistry::prepareUpdate(
    SurfaceMetrics metrics, std::optional<SurfaceProfile>& replacement,
    bool restoring, bool replacingPair, bool initialBinding, bool& touchedActive) noexcept {
    abortUpdate();
    const auto validated = validateUpdate(metrics, replacement);
    if (validated != SurfaceProviderDisposition::kCommitted) return validated;
    const auto& current = lifecycle_.current();
    constexpr auto limit = std::numeric_limits<std::uint64_t>::max();
    auto* provider = replacement ? replacement->provider.get() : activeProvider_.get();
    const auto description = provider->describe();
    const bool physicalChanged = description.metrics.physicalWidth != metrics.physicalWidth ||
                                 description.metrics.physicalHeight != metrics.physicalHeight;
    const bool newResource = replacingPair || physicalChanged || restoring ||
                             !lifecycle_.available();
    const bool metricsChanged = metrics != current.metrics;
    auto nextSurface = current.surfaceGeneration.value() + (newResource ? 1U : 0U);
    const auto nextMetrics = current.metricsGeneration.value() +
                             ((!initialBinding && (metricsChanged || replacingPair || restoring)) ? 1U : 0U);
    // A supplied initial provider is already prepared. Adopt its identity
    // once instead of advancing it during registration, selection and bind.
    if (initialBinding && replacement) nextSurface = provider->generation();
    if (initialBinding && !replacement && !physicalChanged) nextSurface = provider->generation();
    if (physicalChanged || (!replacement && restoring) || !lifecycle_.available() || provider->generation() == 0U) {
        if (!replacement) touchedActive = true;
        if (provider->resize(metrics.physicalWidth, metrics.physicalHeight).code !=
            BackendSubmissionCode::kAccepted) return SurfaceProviderDisposition::kProviderUnavailable;
    }
    // A destructive failed preparation can leave a native provider ahead of
    // the last accepted snapshot. Recovery adopts its actual new resource.
    if (!replacement && (restoring || initialBinding)) nextSurface = std::max(nextSurface, provider->generation());
    if (provider->generation() > nextSurface || nextSurface == limit) {
        return SurfaceProviderDisposition::kStaleFrame;
    }
    while (provider->generation() < nextSurface) {
        if (!replacement) touchedActive = true;
        if (provider->advanceGeneration().code != BackendSubmissionCode::kAccepted) {
            return SurfaceProviderDisposition::kProviderUnavailable;
        }
    }
    const auto actual = provider->describe();
    if (actual.metrics.physicalWidth != metrics.physicalWidth ||
        actual.metrics.physicalHeight != metrics.physicalHeight ||
        actual.format != description.format) return SurfaceProviderDisposition::kMetricsMismatch;
    const auto acquired = provider->acquire();
    const bool valid = acquired.code == SkiaSurfaceAcquireCode::kAcquired &&
                       acquired.frame.surface != nullptr && acquired.frame.generation == nextSurface;
    if (!replacement && acquired.code == SkiaSurfaceAcquireCode::kLost) touchedActive = true;
    if (acquired.code == SkiaSurfaceAcquireCode::kAcquired) provider->release();
    if (!valid) return SurfaceProviderDisposition::kProviderUnavailable;
    RenderTargetInfo info{replacement ? replacement->info.profileId : activeInfo_->profileId,
                         actual.kind, actual.backend, actual.format, metrics, actual.capabilities};
    preparedUpdate_.emplace(std::move(info),
        SurfaceSnapshot{current.viewId, SurfaceGeneration{nextSurface},
                        MetricsGeneration{nextMetrics}, metrics},
        replacement ? std::move(replacement->provider) : nullptr, initialBinding);
    replacement.reset();
    return SurfaceProviderDisposition::kCommitted;
}

void SurfaceProviderRegistry::commitUpdate() noexcept {
    if (!preparedUpdate_) return;
    // Preparation and publication execute synchronously on the Runtime owner.
    // No fallible native work remains at this publication boundary.
    (void)lifecycle_.publishPrepared(preparedUpdate_->snapshot, preparedUpdate_->initialBinding);
    if (preparedUpdate_->replacement) activeProvider_ = std::move(preparedUpdate_->replacement);
    activeProfileId_ = preparedUpdate_->info.profileId;
    activeInfo_.emplace(std::move(preparedUpdate_->info));
    preparedUpdate_.reset();
}

bool SurfaceProviderRegistry::activateUpdate(bool& touchedActive) noexcept {
    if (!preparedUpdate_) return false;
    if (preparedUpdate_->replacement && activeProvider_) activeProvider_->deactivateBinding();
    auto* provider = preparedUpdate_->replacement ? preparedUpdate_->replacement.get()
                                                  : activeProvider_.get();
    touchedActive = true;
    return provider && provider->activateBinding().code == BackendSubmissionCode::kAccepted;
}

void SurfaceProviderRegistry::abortUpdate() noexcept { preparedUpdate_.reset(); }

void SurfaceProviderRegistry::invalidate() noexcept {
    abortUpdate();
    if (activeProvider_) { activeProvider_->setOverlayVisible(false); (void)activeProvider_->lose(); }
    (void)lifecycle_.markLost(lifecycle_.current().viewId);
}

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
    if (metrics == activeInfo_->metrics) return SurfaceProviderDisposition::kCommitted;
    const auto currentSurface = lifecycle_.current().surfaceGeneration.value();
    const auto currentMetrics = lifecycle_.current().metricsGeneration.value();
    if (currentSurface == std::numeric_limits<std::uint64_t>::max() ||
        currentMetrics == std::numeric_limits<std::uint64_t>::max()) {
        return SurfaceProviderDisposition::kGenerationExhausted;
    }
    const bool physicalChanged =
        metrics.physicalWidth != activeInfo_->metrics.physicalWidth ||
        metrics.physicalHeight != activeInfo_->metrics.physicalHeight;
    if (!physicalChanged) {
        if (lifecycle_.replace(SurfaceSnapshot{
                lifecycle_.current().viewId, SurfaceGeneration{currentSurface},
                MetricsGeneration{currentMetrics + 1U}, metrics}) !=
            SurfaceLifecycleDisposition::kReplaced) {
            return SurfaceProviderDisposition::kMetricsMismatch;
        }
        activeInfo_.emplace(activeInfo_->profileId, activeInfo_->kind,
                            activeInfo_->backend, activeInfo_->format, metrics,
                            activeInfo_->capabilities);
        return SurfaceProviderDisposition::kCommitted;
    }
    const auto resized = activeProvider_->resize(metrics.physicalWidth, metrics.physicalHeight);
    if (resized.code != BackendSubmissionCode::kAccepted) return SurfaceProviderDisposition::kProviderUnavailable;
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

SurfaceProviderDisposition SurfaceProviderRegistry::rebind(SurfaceMetrics metrics) noexcept {
    if (activeProvider_ == nullptr || !validMetrics(metrics)) {
        return SurfaceProviderDisposition::kInvalidProfile;
    }
    const auto currentSurface = lifecycle_.current().surfaceGeneration.value();
    const auto currentMetrics = lifecycle_.current().metricsGeneration.value();
    if (currentSurface == std::numeric_limits<std::uint64_t>::max() ||
        currentMetrics == std::numeric_limits<std::uint64_t>::max()) {
        return SurfaceProviderDisposition::kGenerationExhausted;
    }
    if (activeProvider_->resize(metrics.physicalWidth, metrics.physicalHeight).code !=
        BackendSubmissionCode::kAccepted) {
        return SurfaceProviderDisposition::kProviderUnavailable;
    }
    const auto nextSurface = currentSurface + 1U;
    while (activeProvider_->generation() < nextSurface) {
        if (activeProvider_->advanceGeneration().code != BackendSubmissionCode::kAccepted) {
            return SurfaceProviderDisposition::kProviderUnavailable;
        }
    }
    if (activeProvider_->generation() > nextSurface) return SurfaceProviderDisposition::kStaleFrame;
    const auto acquired = activeProvider_->acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired) {
        return SurfaceProviderDisposition::kProviderUnavailable;
    }
    activeProvider_->release();
    if (lifecycle_.replace(SurfaceSnapshot{lifecycle_.current().viewId,
                                           SurfaceGeneration{nextSurface},
                                           MetricsGeneration{currentMetrics + 1U},
                                           metrics}) != SurfaceLifecycleDisposition::kReplaced) {
        return SurfaceProviderDisposition::kMetricsMismatch;
    }
    activeInfo_.emplace(activeInfo_->profileId, activeInfo_->kind, activeInfo_->backend,
                        activeInfo_->format, metrics, activeInfo_->capabilities);
    return SurfaceProviderDisposition::kCommitted;
}

SurfaceProviderDisposition SurfaceProviderRegistry::adoptPrepared(SurfaceMetrics metrics) noexcept {
    if (activeProvider_ == nullptr || !validMetrics(metrics)) {
        return SurfaceProviderDisposition::kInvalidProfile;
    }
    if (activeProvider_->generation() != lifecycle_.current().surfaceGeneration.value()) {
        return SurfaceProviderDisposition::kStaleFrame;
    }
    if (activeInfo_->metrics.physicalWidth != metrics.physicalWidth ||
        activeInfo_->metrics.physicalHeight != metrics.physicalHeight) {
        return SurfaceProviderDisposition::kMetricsMismatch;
    }
    const auto acquired = activeProvider_->acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired) {
        return SurfaceProviderDisposition::kProviderUnavailable;
    }
    activeProvider_->release();
    // A prepared provider already owns the current surface generation. Adopt
    // the complete metrics atomically so tracker/frame identities see the same
    // logical and physical dimensions as the provider descriptor.
    const auto lifecycleResult = lifecycle_.adoptPrepared(metrics);
    if (lifecycleResult != SurfaceLifecycleDisposition::kReplaced &&
        lifecycleResult != SurfaceLifecycleDisposition::kNoGenerationAdvance) {
        return SurfaceProviderDisposition::kMetricsMismatch;
    }
    activeInfo_.emplace(activeInfo_->profileId, activeInfo_->kind, activeInfo_->backend,
                        activeInfo_->format, metrics, activeInfo_->capabilities);
    return SurfaceProviderDisposition::kCommitted;
}

} // namespace canvas::render
