#pragma once

#include "canvas/render/presentation_tracker.hpp"
#include "canvas/render/surface_lifecycle.hpp"
#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/canvas_runtime_facade.hpp"

namespace canvas::web_demo {

enum class WebSurfaceDisposition : unsigned char { kRebound, kRejectedStale, kRejectedInvalid };

class WebSurfaceHost final {
  public:
    explicit WebSurfaceHost(render::SurfaceSnapshot initial) noexcept;
    [[nodiscard]] WebSurfaceDisposition bind(const render::SurfaceSnapshot& replacement);
    [[nodiscard]] render::SurfaceLifecycleDisposition markLost() noexcept;
    [[nodiscard]] render::PresentFeedbackDisposition submit(const render::FrameState& frame);
    [[nodiscard]] render::PresentFeedbackDisposition feedback(const render::PresentedFeedback& value);
    [[nodiscard]] bool syncRuntimeSurfaceIdentity() noexcept;
    [[nodiscard]] const render::SurfaceLifecycle& lifecycle() const noexcept { return lifecycle_; }
    [[nodiscard]] runtime::RuntimeComposition& composition() noexcept { return composition_; }
    [[nodiscard]] runtime::CanvasRuntimeFacade& facade() noexcept { return composition_.controlFacade(); }

  private:
    render::SurfaceLifecycle lifecycle_;
    render::PresentationTracker presentation_;
    runtime::RuntimeComposition composition_;
};

} // namespace canvas::web_demo
