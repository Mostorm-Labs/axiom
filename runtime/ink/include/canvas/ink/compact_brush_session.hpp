#pragma once

#include <canvas/ink/brush_package.hpp>

#include <cstdint>
#include <span>
#include <vector>
#include "canvas/ink/brush_preview_delta.hpp"

namespace canvas::ink {

struct TimedBrushSample final {
    double x = 0.0;
    double y = 0.0;
    double pressure = 0.0;
    bool pressurePresent = false;
    std::uint64_t sequence = 0;
    std::uint64_t timestampNs = 0;
    bool pressureCapability = false;
};

struct CompactBrushConfig final {
    BrushPressureSource pressureSource = BrushPressureSource::kSimulated;
    BrushMissingPressure missingPressure = BrushMissingPressure::kReject;
    double pMin = 0.20;
    double pMax = 0.80;
    double vLow = 80.0;
    double vHigh = 1200.0;
    double tauSeconds = 0.032;
    double geometryEpsilon = 0.25;
    std::uint32_t algorithmVersion = 1;
};

// Runtime-owned signal normalizer used by compact authoring and by the
// production BrushSession seam. It retains only the confirmed centreline and
// effective pressure; predicted samples are evaluated on a disposable fork.
class CompactBrushSession final {
public:
    explicit CompactBrushSession(CompactBrushConfig config = {},
                                 std::uint64_t seed = 0) noexcept;

    [[nodiscard]] bool append(std::span<const TimedBrushSample> confirmed,
                              std::span<const TimedBrushSample> predicted) noexcept;
    [[nodiscard]] std::size_t confirmedCount() const noexcept { return confirmedCount_; }
    [[nodiscard]] double latestPressure() const noexcept;
    [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }
    [[nodiscard]] std::vector<foundation::InkKnot> knots() const;
    [[nodiscard]] std::shared_ptr<const BrushPreviewDelta::Chunk> chunks() const noexcept {return preview_;}
    struct Metrics {std::uint64_t samples=0,simplificationVisits=0,copiedKnots=0,generatedKnots=0;};
    Metrics metrics()const noexcept{return metrics_;}

private:
    [[nodiscard]] bool validSample(const TimedBrushSample& sample,
                                   std::uint64_t previousSequence) const noexcept;
    [[nodiscard]] double simulatedPressure(double distance, double dt,
                                           double previousPressure) const noexcept;

    CompactBrushConfig config_{};
    std::uint64_t seed_ = 0;
    std::uint64_t confirmedCount_=0,lastSequence_=0,lastTimestamp_=0;
    std::vector<foundation::InkKnot> pending_;
    double pendingLength_=0;
    Metrics metrics_{};
    foundation::InkKnot lastKnot_{};
    foundation::InkKnot previousControl_{};
    foundation::InkKnot curveStart_{};
    double previousPressure_ = 0.5;
    double previousX_ = 0.0;
    double previousY_ = 0.0;
    std::uint64_t previousTime_ = 0;
    std::shared_ptr<const BrushPreviewDelta::Chunk> chunks_;
    std::shared_ptr<const BrushPreviewDelta::Chunk> preview_;
};

}  // namespace canvas::ink
