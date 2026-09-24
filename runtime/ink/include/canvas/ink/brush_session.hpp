#pragma once
#include "canvas/ink/brush_commit_intent.hpp"
#include "canvas/ink/brush_preview_delta.hpp"
#include "canvas/ink/vector_path_node.hpp"
#include <cstdint>
#include <span>

namespace canvas::ink {
enum class BrushSessionError : std::uint8_t { kNone=0,kInvalid,kSequence,kPressure,kEmpty };
class BrushSession final {
  public:
    BrushSession(std::uint64_t, ResolvedBrushState);
    bool begin();
    BrushSessionError append(std::span<const BrushSample>, std::span<const BrushSample>, BrushPreviewDelta&);
    BrushSessionError seal(BrushCommitIntent&);
    void cancel() noexcept;
    [[nodiscard]] BrushSessionError error() const noexcept { return error_; }
  private:
    std::uint64_t id_;
    ResolvedBrushState state_;
    bool active_ = false;
    std::uint64_t revision_ = 0;
    std::uint64_t lastSequence_ = 0;
    std::vector<BrushSample> confirmed_;
    BrushSessionError error_ = BrushSessionError::kNone;
};
}
