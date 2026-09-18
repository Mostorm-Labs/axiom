#pragma once

#include "canvas/render/frame_plan.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace canvas::render {

enum class BackendSubmissionCode : std::uint8_t {
    kAccepted,
    kRejected,
};

struct BackendSubmissionResult final {
    BackendSubmissionCode code = BackendSubmissionCode::kRejected;
    std::string message;

    [[nodiscard]] static BackendSubmissionResult accepted() {
        return BackendSubmissionResult{BackendSubmissionCode::kAccepted, {}};
    }

    [[nodiscard]] static BackendSubmissionResult rejected(std::string message) {
        return BackendSubmissionResult{BackendSubmissionCode::kRejected, std::move(message)};
    }

    bool operator==(const BackendSubmissionResult&) const = default;
};

// Render Core private seam. Implementations consume only the immutable plan;
// platform targets, presentation, and backend-specific types are out of scope.
class IRenderBackend {
  public:
    virtual ~IRenderBackend() = default;
    [[nodiscard]] virtual BackendSubmissionResult submit(const FramePlan& plan) = 0;
};

class FrameOrchestrator final {
  public:
    [[nodiscard]] static BackendSubmissionResult submit(
        IRenderBackend& backend,
        const FramePlan& plan);
};

} // namespace canvas::render
