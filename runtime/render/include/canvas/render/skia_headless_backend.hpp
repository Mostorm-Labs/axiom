#pragma once

#include "canvas/render/render_backend.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace canvas::render {

struct HeadlessRasterConfig final {
    std::uint32_t width = 256U;
    std::uint32_t height = 256U;

    bool operator==(const HeadlessRasterConfig&) const = default;
};

enum class HeadlessSubmissionIssue : std::uint8_t {
    kNone = 0,
    kInvalidConfiguration = 1,
    kPlanIdentityMismatch = 2,
    kUnsupportedEraseMask = 3,
    kUnsupportedGeometry = 4,
    kOverflow = 5,
    kRasterFailure = 6,
    kEmptyPlan = 7,
};

struct HeadlessRasterObservation final {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::vector<std::uint8_t> rgba;
    std::string digest;
    std::string sourcePlanDigest;
    std::vector<semantic::ObjectKind> traversedKinds;

    bool operator==(const HeadlessRasterObservation&) const = default;
};

class SkiaHeadlessBackend final : public IRenderBackend {
  public:
    explicit SkiaHeadlessBackend(HeadlessRasterConfig config);

    [[nodiscard]] BackendSubmissionResult submit(const FramePlan& plan) override;
    [[nodiscard]] const std::optional<HeadlessRasterObservation>& observation() const noexcept {
        return _observation;
    }
    [[nodiscard]] HeadlessSubmissionIssue lastIssue() const noexcept { return _lastIssue; }

  private:
    HeadlessRasterConfig _config;
    std::optional<HeadlessRasterObservation> _observation;
    HeadlessSubmissionIssue _lastIssue = HeadlessSubmissionIssue::kNone;
};

} // namespace canvas::render
