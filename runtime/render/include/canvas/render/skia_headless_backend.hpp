#pragma once

#include "canvas/render/render_backend.hpp"
#include "canvas/render/skia_text_resources.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace canvas::render {
class ImageResourceResolver;

struct HeadlessRasterConfig final {
    std::uint32_t width = 256U;
    std::uint32_t height = 256U;
    // RGBA8 background for the oracle. Transparent remains the historical
    // fixture default; opaque qualification matches a canonical surface.
    std::uint32_t backgroundRgba = 0U;

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
    explicit SkiaHeadlessBackend(HeadlessRasterConfig config, ImageResourceResolver* images = nullptr);

    [[nodiscard]] BackendSubmissionResult submit(const FramePlan& plan) override;
    [[nodiscard]] const std::optional<HeadlessRasterObservation>& observation() const noexcept {
        return _observation;
    }
    [[nodiscard]] HeadlessSubmissionIssue lastIssue() const noexcept { return _lastIssue; }

  private:
    HeadlessRasterConfig _config;
    ImageResourceResolver* _images = nullptr;
    SkiaTextResources _textResources;
    std::optional<HeadlessRasterObservation> _observation;
    HeadlessSubmissionIssue _lastIssue = HeadlessSubmissionIssue::kNone;
};

} // namespace canvas::render
