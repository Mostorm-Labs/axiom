#pragma once
#include "canvas/ink/brush_commit_intent.hpp"
#include "canvas/ink/brush_preview_delta.hpp"
#include "canvas/ink/vector_path_node.hpp"
#include <cstdint>
#include <algorithm>
#include <span>

namespace canvas::ink {
enum class BrushSessionError : std::uint8_t { kNone=0,kInvalid,kSequence,kPressure,kEmpty };
struct BrushSessionMetrics final {
  std::uint64_t appendEvaluations = 0;
  std::uint64_t maxAppendInputSamples = 0;
  std::uint64_t maxCopiedHistoricalSamples = 0;
  std::uint64_t sealEvaluations = 0;
};
class BrushSession final {
  public:
    BrushSession(std::uint64_t, ResolvedBrushState);
    bool begin();
    BrushSessionError append(std::span<const BrushSample>, std::span<const BrushSample>, BrushPreviewDelta&);
    BrushSessionError seal(BrushCommitIntent&);
    void cancel() noexcept;
    [[nodiscard]] BrushSessionError error() const noexcept { return error_; }
    [[nodiscard]] BrushSessionMetrics metrics() const noexcept { return metrics_; }
  private:
    std::uint64_t id_;
    ResolvedBrushState state_;
    bool active_ = false;
    std::uint64_t revision_ = 0;
    std::uint64_t lastSequence_ = 0;
    std::vector<BrushSample> confirmed_;
    BrushSessionError error_ = BrushSessionError::kNone;
    BrushSessionMetrics metrics_{};
};
}
