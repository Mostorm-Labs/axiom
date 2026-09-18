#include "canvas/render/render_backend.hpp"

namespace canvas::render {

static_assert(sizeof(BackendSubmissionCode) == sizeof(std::uint8_t));

BackendSubmissionResult FrameOrchestrator::submit(
    IRenderBackend& backend,
    const FramePlan& plan) {
    return backend.submit(plan);
}

} // namespace canvas::render
