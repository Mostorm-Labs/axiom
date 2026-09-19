#pragma once

#include "canvas/ink/ink_engine.hpp"
#include "canvas/ink/pointer_trace.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace canvas::ink {

enum class ReplayDisposition : std::uint8_t {
  kCommittedCandidate,
  kCancelled,
  kInvalidTrace,
  kConfirmedOverrun,
  kGenerationMismatch,
};

struct ReplayResult final {
  ReplayDisposition disposition = ReplayDisposition::kInvalidTrace;
  std::uint64_t digest = 0;
  std::size_t processedConfirmedSamples = 0;
  std::optional<StrokeRecord> candidate;
};

ReplayResult replay(const PointerTrace& trace, std::uint64_t strokeId) noexcept;
ReplayResult replayChunked(const PointerTrace& trace, std::uint64_t strokeId,
                           std::span<const std::size_t> chunkSizes) noexcept;

}  // namespace canvas::ink
