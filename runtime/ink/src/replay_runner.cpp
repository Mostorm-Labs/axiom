#include "canvas/ink/replay_runner.hpp"

namespace canvas::ink {

namespace {
ReplayResult replaySamples(const PointerTrace& trace, std::uint64_t strokeId,
                           std::span<const input::PointerSample> samples) noexcept {
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
  for (const auto& sample : samples) {
    if (!engine.append(sample) && !sample.predicted) return result;
  }
  result.candidate = engine.finish();
  result.processedConfirmedSamples = engine.processedSampleCount();
  if (!result.candidate.has_value()) return result;
  result.digest = traceDigest(trace);
  result.disposition = ReplayDisposition::kCommittedCandidate;
  return result;
}
}  // namespace

ReplayResult replay(const PointerTrace& trace, std::uint64_t strokeId) noexcept {
  return replaySamples(trace, strokeId, trace.samples);
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
  ReplayResult result;
  if (trace.confirmedOverrun || trace.surfaceMetricsGeneration != trace.expectedSurfaceMetricsGeneration ||
      trace.cancelled || validate(trace) != TraceValidation::kValid || strokeId == 0) {
    return replay(trace, strokeId);
  }
  // Feed each chunk to the same session in order; chunk boundaries are execution
  // boundaries, not an excuse to concatenate and replay a second representation.
  InkEngine engine(BrushDescriptor{});
  if (!engine.begin(strokeId)) return result;
  std::size_t offset = 0;
  for (const auto size : chunkSizes) {
    for (const auto& sample : std::span<const input::PointerSample>(trace.samples).subspan(offset, size)) {
      if (!engine.append(sample) && !sample.predicted) return result;
    }
    offset += size;
  }
  result.candidate = engine.finish();
  result.processedConfirmedSamples = engine.processedSampleCount();
  if (!result.candidate.has_value()) return result;
  result.digest = traceDigest(trace);
  result.disposition = ReplayDisposition::kCommittedCandidate;
  return result;
}

}  // namespace canvas::ink
