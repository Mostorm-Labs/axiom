#include "canvas/ink/pointer_trace.hpp"
#include "canvas/ink/replay_runner.hpp"

#include <cassert>
#include <cstdint>

namespace {
canvas::ink::PointerTrace straight() {
  canvas::ink::PointerTrace trace;
  trace.samples = {{1, 10, 0.0F, 0.0F, 0.5F, false},
                   {2, 20, 1.0F, 1.0F, 0.6F, false},
                   {3, 30, 2.0F, 2.0F, 0.7F, false}};
  return trace;
}
}

int main() {
  const auto trace = straight();
  assert(canvas::ink::validate(trace) == canvas::ink::TraceValidation::kValid);
  const auto expected = canvas::ink::traceDigest(trace);
  assert(expected != 0);
  const auto replay = canvas::ink::replay(trace, 17);
  assert(replay.disposition == canvas::ink::ReplayDisposition::kCommittedCandidate);
  assert(replay.digest == expected);
  assert(replay.candidate.has_value());
  assert(replay.candidate->id == 17);
  assert(replay.processedConfirmedSamples == 3);
  for (int i = 0; i < 9; ++i) {
    const auto repeat = canvas::ink::replay(trace, 17);
    assert(repeat.disposition == replay.disposition);
    assert(repeat.digest == replay.digest);
    assert(repeat.processedConfirmedSamples == replay.processedConfirmedSamples);
    assert(repeat.candidate.has_value());
    assert(repeat.candidate->points.size() == replay.candidate->points.size());
  }
  const std::size_t chunks[] = {1, 2};
  const auto chunked = canvas::ink::replayChunked(trace, 17, chunks);
  assert(chunked.disposition == replay.disposition);
  assert(chunked.digest == replay.digest);
  assert(chunked.candidate.has_value() == replay.candidate.has_value());
  assert(chunked.candidate->id == replay.candidate->id);
  assert(chunked.candidate->points.size() == replay.candidate->points.size());
  for (std::size_t i = 0; i < replay.candidate->points.size(); ++i) {
    assert(chunked.candidate->points[i] == replay.candidate->points[i]);
  }

  auto duplicate = trace;
  duplicate.samples[2].sequence = 2;
  assert(canvas::ink::validate(duplicate) == canvas::ink::TraceValidation::kNonMonotonicConfirmed);
  auto cancelled = trace;
  cancelled.cancelled = true;
  const auto cancelResult = canvas::ink::replay(cancelled, 18);
  assert(cancelResult.disposition == canvas::ink::ReplayDisposition::kCancelled);
  assert(!cancelResult.candidate.has_value());

  auto generation = trace;
  generation.surfaceMetricsGeneration = 2;
  generation.expectedSurfaceMetricsGeneration = 1;
  const auto generationResult = canvas::ink::replay(generation, 19);
  assert(generationResult.disposition == canvas::ink::ReplayDisposition::kGenerationMismatch);
  assert(!generationResult.candidate.has_value());

  auto overrun = trace;
  overrun.confirmedOverrun = true;
  const auto overrunResult = canvas::ink::replay(overrun, 20);
  assert(overrunResult.disposition == canvas::ink::ReplayDisposition::kConfirmedOverrun);
  assert(!overrunResult.candidate.has_value());

  canvas::ink::PointerTrace longTrace;
  longTrace.samples.reserve(14400);
  for (std::uint64_t i = 1; i <= 14400; ++i) {
    longTrace.samples.push_back({i, i * 4166666, 1.0F, 2.0F, 0.5F, false});
  }
  const auto longReplay = canvas::ink::replay(longTrace, 21);
  assert(longReplay.disposition == canvas::ink::ReplayDisposition::kCommittedCandidate);
  assert(longReplay.processedConfirmedSamples == 14400);
  assert(longReplay.digest == 0x4bd109efefdea9c2ULL);
  assert(longReplay.candidate.has_value());
  assert(longReplay.candidate->points.size() == 14400);

  auto unsupported = trace;
  unsupported.schemaVersion = 2;
  assert(canvas::ink::validate(unsupported) == canvas::ink::TraceValidation::kUnsupportedSchema);
  canvas::ink::PointerTrace empty;
  assert(canvas::ink::validate(empty) == canvas::ink::TraceValidation::kMissingSamples);
  return 0;
}
