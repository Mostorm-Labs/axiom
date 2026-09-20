#include "canvas/ink/multipointer_replay.hpp"

#include <algorithm>

namespace canvas::ink {

MultiPointerReplayResult replayMultiPointer(
    std::span<const std::pair<input::PointerKey, std::vector<input::PointerSample>>> streams) noexcept {
  MultiPointerReplayResult result;
  InkEngine engine(BrushDescriptor{});
  std::uint64_t nextStrokeId = 1;
  for (const auto& stream : streams) {
    if (!stream.first.valid() || !engine.begin(stream.first, nextStrokeId++)) {
      result.cancelled.push_back(stream.first);
      continue;
    }
    bool valid = true;
    for (const auto& sample : stream.second) {
      if (!engine.append(stream.first, sample) && !sample.predicted) {
        engine.cancel(stream.first);
        result.cancelled.push_back(stream.first);
        valid = false;
        break;
      }
    }
    if (!valid) continue;
  }
  for (const auto& stream : streams) {
    auto record = engine.finish(stream.first);
    if (!record.has_value()) continue;
    result.committed.push_back(std::move(*record));
  }
  std::sort(result.committed.begin(), result.committed.end(),
            [](const StrokeRecord& lhs, const StrokeRecord& rhs) { return lhs.id < rhs.id; });
  return result;
}

}  // namespace canvas::ink
