#pragma once

#include "canvas/input/platform_interaction_ingress.hpp"
#include "canvas/render/render_target.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>

namespace canvas::render {

struct InputSourceRegistration final {
    input::InputSourceId source = 0;
    std::string platform;
};

// Platform-facing composition root. It owns registration and lifecycle
// routing only; interaction/Brush/Scene semantics remain in their runtime
// owners and consume the normalized ingress result.
class RuntimeAppBinding final {
  public:
    RuntimeAppBinding(ViewId viewId, SurfaceProfile initialSurface);

    [[nodiscard]] bool registerInputSource(InputSourceRegistration registration);
    [[nodiscard]] bool unregisterInputSource(input::InputSourceId source);
    [[nodiscard]] bool notifySourceLost(input::InputSourceId source,
                                        std::uint64_t sequence,
                                        std::uint64_t timestampNs) noexcept;
    [[nodiscard]] input::PlatformInteractionIngressResult submit(
        input::InputSourceId source,
        const input::PlatformPointerBatch& batch) noexcept;
    [[nodiscard]] std::optional<input::PointerKey> keyFor(
        input::InputSourceId source, input::PointerId pointer) const noexcept;
    [[nodiscard]] bool hasInputSource(input::InputSourceId source) const noexcept {
        return sources_.contains(source);
    }

    [[nodiscard]] SurfaceProviderDisposition registerSurfaceProfile(
        SurfaceProfile profile);
    [[nodiscard]] SurfaceProviderDisposition selectRenderProfile(
        std::string_view profileId, RenderTargetFormat format);
    [[nodiscard]] SurfaceProviderDisposition resizeSurface(SurfaceMetrics metrics);
    [[nodiscard]] SurfaceProviderDisposition markSurfaceLost() noexcept;
    [[nodiscard]] SurfaceProviderDisposition rebindSurface() noexcept;
    [[nodiscard]] SurfaceProviderDisposition registerPreviewSurfaceProfile(
        SurfaceProfile profile);
    [[nodiscard]] SurfaceProviderDisposition selectPreviewRenderProfile(
        std::string_view profileId, RenderTargetFormat format);
    [[nodiscard]] SurfaceProviderDisposition resizePreviewSurface(SurfaceMetrics metrics);
    [[nodiscard]] SurfaceProviderDisposition markPreviewSurfaceLost() noexcept;
    [[nodiscard]] SurfaceProviderDisposition rebindPreviewSurface() noexcept;
    [[nodiscard]] SurfaceProviderRegistry& surfaces() noexcept { return surfaces_; }
    [[nodiscard]] const SurfaceProviderRegistry& surfaces() const noexcept { return surfaces_; }
    [[nodiscard]] SurfaceProviderRegistry* previewSurfaces() noexcept {
        return previewSurfaces_.get();
    }
    [[nodiscard]] const SurfaceProviderRegistry* previewSurfaces() const noexcept {
        return previewSurfaces_.get();
    }

  private:
    input::PlatformInteractionIngress ingress_;
    std::unordered_set<input::InputSourceId> sources_;
    ViewId viewId_{};
    SurfaceProviderRegistry surfaces_;
    std::unique_ptr<SurfaceProviderRegistry> previewSurfaces_;
};

} // namespace canvas::render
