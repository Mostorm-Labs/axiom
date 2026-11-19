#pragma once
#include "canvas/render/frame_plan.hpp"
#include "canvas/render/render_backend.hpp"
namespace canvas::render::internal {
class ReferenceValidationCache;
// Only body/composition validation. Navigation's bounded mask owner must
// separately qualify every committed history before pixel publication/reuse.
BackendSubmissionResult validateNavigationReferencePlan(const FramePlan&,ReferenceValidationCache*);
}
