#include "canvas/ink/replay_runner.hpp"

namespace canvas::ink {

ReplayResult replay(const PointerTrace& trace, std::uint64_t strokeId) noexcept {
  ReplayResult result;
  if (trace.confirmedOverrun) {
    result.disposition = ReplayDisposition::kConfirmedOverrun;
    return result;
  }
  if (trace.surfaceMetricsGeneration != trace.expectedSurfaceMetricsGeneration) {
    result.disposition = ReplayDisposition::kGenerationMismatch;
    return result;
  }
  if (trace.cancelled) {
    result.disposition = ReplayDisposition::kCancelled;
    return result;
  }
  if (validate(trace) != TraceValidation::kValid || strokeId == 0) return result;
  InkEngine engine(BrushDescriptor{});
  if (!engine.begin(strokeId)) return result;
  for (const auto& sample : trace.samples) {
    if (!engine.append(sample) && !sample.predicted) return result;
  }
  result.candidate = engine.finish();
  result.processedConfirmedSamples = engine.processedSampleCount();
  if (!result.candidate.has_value()) return result;
  result.digest = traceDigest(trace);
  result.disposition = ReplayDisposition::kCommittedCandidate;
  return result;
}

ReplayResult replayChunked(const PointerTrace& trace, std::uint64_t strokeId,
                           std::span<const std::size_t> chunkSizes) noexcept {
  if (chunkSizes.empty()) return replay(trace, strokeId);
  std::size_t total = 0;
  for (const auto size : chunkSizes) total += size;
  if (total != trace.samples.size()) {
    ReplayResult result;
    return result;
  }
  return replay(trace, strokeId);
}

}  // namespace canvas::ink
