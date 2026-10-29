#pragma once

#include "canvas/render/presentation_tracker.hpp"
#include "canvas/render/surface_lifecycle.hpp"
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/runtime/internal/runtime_platform_access.hpp"
#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/runtime_facade.hpp"

#include <optional>
#include <vector>

namespace canvas::web_demo {

enum class WebSurfaceDisposition : unsigned char { kRebound, kRejectedStale, kRejectedInvalid };

class WebSurfaceHost final {
  public:
    explicit WebSurfaceHost(render::SurfaceSnapshot initial, bool deferPlatformBinding = false) noexcept;
    [[nodiscard]] WebSurfaceDisposition bind(const render::SurfaceSnapshot& replacement);
    [[nodiscard]] WebSurfaceDisposition bindMetrics(const render::SurfaceMetrics& metrics);
    [[nodiscard]] render::SurfaceLifecycleDisposition markLost() noexcept;
    [[nodiscard]] render::PresentFeedbackDisposition submit(const render::FrameState& frame);
    [[nodiscard]] render::PresentFeedbackDisposition feedback(
        const render::PresentedFeedback& value, const runtime::CanonicalWork& work);
    [[nodiscard]] std::optional<runtime::CanonicalWork> submittedCanonicalWork(
        render::FrameId platformFrame) const noexcept;
    [[nodiscard]] bool syncRuntimeSurfaceIdentity() noexcept;
    [[nodiscard]] const render::SurfaceLifecycle& lifecycle() const noexcept { return lifecycle_; }
    [[nodiscard]] runtime::RuntimeCompositionCore& composition() noexcept { return runtime::RuntimePlatformAccess::core(composition_); }
    [[nodiscard]] runtime::RuntimeComposition& publicComposition() noexcept { return composition_; }
    [[nodiscard]] runtime::RuntimeFacade& facade() noexcept { return composition_.controlFacade(); }

  private:
    render::SurfaceLifecycle lifecycle_;
    render::PresentationTracker presentation_;
    runtime::RuntimeComposition composition_;
    struct FrameMapping final {
        render::FrameId platformFrame{};
        render::RuntimeFrameIdentity runtimeFrame{};
        runtime::CanonicalWork runtimeWork{};
    };
    std::vector<FrameMapping> submittedRuntimeFrames_;
};

} // namespace canvas::web_demo
