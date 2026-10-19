#include "canvas/interaction/interaction_dispatch_runtime.hpp"

namespace canvas::interaction {

bool InteractionDispatchRuntime::submit(const input::PointerSample& sample) noexcept {
  if (!sample.key.valid()) return false;
  auto it = sessions_.find(sample.key);
  if (sample.phase == input::PointerPhase::kDown) {
    if (sample.predicted || it != sessions_.end()) return false;
    const auto mode = port_.dispatchMode();
    const auto id = nextSessionId_++;
    if (id == 0) return false;
    it = sessions_.emplace(sample.key, Session{mode, id}).first;
  } else if (it == sessions_.end()) {
    return false;
  }
  const auto mode = it->second.mode;
  const auto id = it->second.id;
  if (sample.phase == input::PointerPhase::kCancel) {
    const auto accepted = port_.cancel(mode, sample, id);
    sessions_.erase(it);
    return accepted;
  }
  bool accepted = false;
  switch (mode) {
    case DispatchMode::kSelection:
      accepted = port_.selection(sample, id); break;
    case DispatchMode::kCreation:
      accepted = port_.creation(sample, id); break;
    case DispatchMode::kPan:
      accepted = port_.pan(sample, id); break;
    case DispatchMode::kObjectEraser:
    case DispatchMode::kPartialEraser:
      accepted = port_.eraser(sample, id); break;
    case DispatchMode::kBrush:
      accepted = port_.brush(sample, id); break;
  }
  if (!accepted) {
    if (sample.phase == input::PointerPhase::kDown) {
      input::PointerSample cancelled = sample;
      cancelled.phase = input::PointerPhase::kCancel;
      (void)port_.cancel(mode, cancelled, id);
      sessions_.erase(sample.key);
    }
    return false;
  }
  if (sample.phase == input::PointerPhase::kUp) sessions_.erase(sample.key);
  return true;
}

bool InteractionDispatchRuntime::submitBatch(
    std::span<const input::PointerSample> samples) noexcept {
  if (samples.empty()) return false;
  for (std::size_t begin = 0U; begin < samples.size();) {
    const auto& first = samples[begin];
    const bool startsSession = first.phase == input::PointerPhase::kDown;
    auto it = sessions_.find(first.key);
    const auto mode = startsSession ? port_.dispatchMode()
        : it != sessions_.end() ? it->second.mode : DispatchMode::kBrush;
    if (!first.key.valid() || mode != DispatchMode::kBrush ||
        first.phase == input::PointerPhase::kCancel ||
        (startsSession ? first.predicted || it != sessions_.end() : it == sessions_.end())) {
      if (!submit(first)) return false;
      ++begin;
      continue;
    }
    std::size_t end = begin + 1U;
    while (samples[end - 1U].phase != input::PointerPhase::kUp && end < samples.size() &&
           samples[end].key == first.key &&
           samples[end].phase != input::PointerPhase::kDown &&
           samples[end].phase != input::PointerPhase::kCancel) {
      ++end;
    }
    if (startsSession) {
      const auto id = nextSessionId_++;
      if (id == 0U) return false;
      it = sessions_.emplace(first.key, Session{mode, id}).first;
    }
    const auto id = it->second.id;
    const bool accepted = port_.brushBatch(samples.subspan(begin, end - begin), id);
    if (!accepted) {
      if (startsSession) {
        auto cancelled = first;
        cancelled.phase = input::PointerPhase::kCancel;
        (void)port_.cancel(mode, cancelled, id);
        sessions_.erase(first.key);
      }
      return false;
    }
    if (samples[end - 1U].phase == input::PointerPhase::kUp) sessions_.erase(first.key);
    begin = end;
  }
  return true;
}

bool InteractionDispatchRuntime::cancel(const input::PointerSample& sample) noexcept {
  if (!sample.key.valid()) return false;
  const auto it = sessions_.find(sample.key);
  if (it == sessions_.end()) return false;
  const auto accepted = port_.cancel(it->second.mode, sample, it->second.id);
  sessions_.erase(it);
  return accepted;
}

void InteractionDispatchRuntime::cancelAll() noexcept {
  for (const auto& [key, session] : sessions_) {
    input::PointerSample sample;
    sample.key = key;
    sample.phase = input::PointerPhase::kCancel;
    (void)port_.cancel(session.mode, sample, session.id);
  }
  sessions_.clear();
}

}  // namespace canvas::interaction
