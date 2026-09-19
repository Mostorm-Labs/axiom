#pragma once

#include "canvas/render/presentation_tracker.hpp"
#include "canvas/render/surface_lifecycle.hpp"

namespace canvas::windows_demo {

enum class WindowsSurfaceDisposition : unsigned char {
    kRebound,
    kRejectedStale,
    kRejectedInvalid,
};

// Portable ownership seam used by the Windows host and its contract tests.
// It binds a native surface observation to the renderer-owned lifecycle; it
// never traverses semantic or scene state.
class WindowsSurfaceAdapter final {
  public:
    explicit WindowsSurfaceAdapter(render::SurfaceSnapshot initial) noexcept;

    [[nodiscard]] WindowsSurfaceDisposition bind(
        const render::SurfaceSnapshot& replacement);
    [[nodiscard]] render::SurfaceLifecycleDisposition markLost() noexcept;
    [[nodiscard]] render::PresentFeedbackDisposition submit(
        const render::FrameState& frame);
    [[nodiscard]] render::PresentFeedbackDisposition feedback(
        const render::PresentedFeedback& value);
    [[nodiscard]] const render::SurfaceLifecycle& lifecycle() const noexcept {
        return lifecycle_;
    }

  private:
    render::SurfaceLifecycle lifecycle_;
    render::PresentationTracker presentation_;
};

} // namespace canvas::windows_demo
