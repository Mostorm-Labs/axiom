#include "canvas/input/platform_interaction_ingress.hpp"

namespace canvas::input {

PointerSample PlatformInteractionIngress::normalize(
    const PlatformPointerSample& sample, const PointerKey& key) const noexcept {
  PointerSample normalized{};
  normalized.sequence = sample.sequence;
  normalized.timestampNs = sample.timestampNs;
  normalized.x = sample.x;
  normalized.y = sample.y;
  normalized.pressure = sample.pressure;
  normalized.predicted = sample.provenance == SampleProvenance::kPredicted;
  normalized.key = key;
  normalized.contact = sample.contact;
  normalized.capabilities = sample.capabilities;
  normalized.phase = sample.phase;
  normalized.modifiers = sample.modifiers;
  return normalized;
}

void PlatformInteractionIngress::cancelAll(
    PlatformInteractionIngressResult& result) noexcept {
  for (const auto& [identity, key] : active_) {
    (void)identity;
    PointerSample cancelled{};
    cancelled.key = key;
    cancelled.phase = PointerPhase::kCancel;
    result.normalized.samples.push_back(cancelled);
    if (registry_.end(key)) ++result.ended;
  }
  active_.clear();
  lastSequence_.clear();
  result.normalized.terminalCancel = true;
  result.terminalCancel = true;
}

PlatformInteractionIngressResult PlatformInteractionIngress::submit(
    const PlatformPointerBatch& batch) noexcept {
  PlatformInteractionIngressResult result{};
  // A batch is one device/source boundary. Validate before begin/end so a
  // malformed batch cannot partially admit contacts or cancel another source.
  if (!batch.samples.empty()) {
    const auto source = batch.samples.front().source;
    for (const auto& sample : batch.samples) if (sample.source != source) return result;
  }
  for (const auto& sample : batch.samples) {
    if (sample.source == 0 || sample.pointer == 0 || sample.sequence == 0 ||
        sample.timestampNs == 0) {
      continue;
    }
    const Identity identity{sample.source, sample.pointer};
    PointerKey key{};
    if (sample.phase == PointerPhase::kDown) {
      if (active_.contains(identity)) continue;
      key = registry_.begin(sample.source, sample.pointer);
      if (!key.valid()) continue;
      active_[identity] = key;
      lastSequence_[identity] = sample.sequence;
      ++result.began;
    } else {
      const auto it = active_.find(identity);
      if (it == active_.end() || !registry_.accepts(it->second)) continue;
      const auto sequence = lastSequence_.find(identity);
      if (sequence != lastSequence_.end() && sample.sequence <= sequence->second) continue;
      key = it->second;
      lastSequence_[identity] = sample.sequence;
    }
    result.normalized.samples.push_back(normalize(sample, key));
    if (sample.phase == PointerPhase::kUp || sample.phase == PointerPhase::kCancel) {
      if (registry_.end(key)) ++result.ended;
      active_.erase(identity);
      lastSequence_.erase(identity);
    }
  }
  if (batch.sourceLost && !batch.samples.empty()) {
    const auto& first = batch.samples.front();
    auto cancelled = sourceLost(first.source, first.sequence, first.timestampNs);
    result.normalized.samples.insert(result.normalized.samples.end(),
        cancelled.normalized.samples.begin(), cancelled.normalized.samples.end());
    result.terminalCancel = cancelled.terminalCancel;
    result.normalized.terminalCancel = cancelled.terminalCancel;
    result.ended += cancelled.ended;
  } else if (batch.terminalCancel) cancelAll(result);
  result.accepted = !result.normalized.samples.empty() || result.terminalCancel;
  return result;
}

PlatformInteractionIngressResult PlatformInteractionIngress::sourceLost(
    InputSourceId source, std::uint64_t sequence, std::uint64_t timestampNs) noexcept {
  PlatformInteractionIngressResult result{};
  bool matched = false;
  for (const auto& [identity, key] : active_) {
    if (identity.source != source) continue;
    matched = true;
    PointerSample cancelled{};
    cancelled.sequence = sequence;
    cancelled.timestampNs = timestampNs;
    cancelled.key = key;
    cancelled.phase = PointerPhase::kCancel;
    result.normalized.samples.push_back(cancelled);
    if (registry_.end(key)) ++result.ended;
  }
  for (auto it = active_.begin(); it != active_.end();) {
    if (it->first.source == source) it = active_.erase(it);
    else ++it;
  }
  for (auto it = lastSequence_.begin(); it != lastSequence_.end();) {
    if (it->first.source == source) it = lastSequence_.erase(it);
    else ++it;
  }
  // A source loss is a keyed cancel when another source is still active.  A
  // terminal cancel is reserved for the legacy all-input boundary, which
  // preserves the existing single-source cancellation behavior without
  // cascading A's loss into B's live session.
  const bool terminal = matched && active_.empty();
  result.normalized.terminalCancel = terminal;
  result.terminalCancel = terminal;
  result.accepted = !result.normalized.samples.empty() || result.terminalCancel;
  return result;
}

PlatformInteractionIngressResult PlatformInteractionIngress::cancelPointer(
    const PointerKey& key, std::uint64_t sequence, std::uint64_t timestampNs) noexcept {
  PlatformInteractionIngressResult result{};
  if (!key.valid() || !registry_.accepts(key)) return result;
  const Identity identity{key.source, key.pointer};
  const auto active = active_.find(identity);
  if (active == active_.end() || active->second != key) return result;
  PointerSample cancelled{};
  cancelled.sequence = sequence;
  cancelled.timestampNs = timestampNs;
  cancelled.key = key;
  cancelled.phase = PointerPhase::kCancel;
  result.normalized.samples.push_back(cancelled);
  if (registry_.end(key)) ++result.ended;
  active_.erase(active);
  lastSequence_.erase(identity);
  result.accepted = true;
  return result;
}

}  // namespace canvas::input
