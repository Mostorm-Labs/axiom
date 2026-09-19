#pragma once

#include "canvas/render/presentation_tracker.hpp"
#include "canvas/render/surface_lifecycle.hpp"

namespace canvas::web_demo {

enum class WebSurfaceDisposition : unsigned char { kRebound, kRejectedStale, kRejectedInvalid };

class WebSurfaceHost final {
  public:
    explicit WebSurfaceHost(render::SurfaceSnapshot initial) noexcept;
    [[nodiscard]] WebSurfaceDisposition bind(const render::SurfaceSnapshot& replacement);
    [[nodiscard]] render::SurfaceLifecycleDisposition markLost() noexcept;
    [[nodiscard]] render::PresentFeedbackDisposition submit(const render::FrameState& frame);
    [[nodiscard]] render::PresentFeedbackDisposition feedback(const render::PresentedFeedback& value);
    [[nodiscard]] const render::SurfaceLifecycle& lifecycle() const noexcept { return lifecycle_; }

  private:
    render::SurfaceLifecycle lifecycle_;
    render::PresentationTracker presentation_;
};

} // namespace canvas::web_demo
