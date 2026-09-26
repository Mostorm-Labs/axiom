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
  result.normalized.terminalCancel = true;
  result.terminalCancel = true;
}

PlatformInteractionIngressResult PlatformInteractionIngress::submit(
    const PlatformPointerBatch& batch) noexcept {
  PlatformInteractionIngressResult result{};
  for (const auto& sample : batch.samples) {
    if (sample.source == 0 || sample.pointer == 0 || sample.sequence == 0 ||
        sample.timestampNs == 0) {
      continue;
    }
    const Identity identity{sample.source, sample.pointer};
    PointerKey key{};
    if (sample.phase == PointerPhase::kDown) {
      key = registry_.begin(sample.source, sample.pointer);
      if (!key.valid()) continue;
      active_[identity] = key;
      ++result.began;
    } else {
      const auto it = active_.find(identity);
      if (it == active_.end() || !registry_.accepts(it->second)) continue;
      key = it->second;
    }
    result.normalized.samples.push_back(normalize(sample, key));
    if (sample.phase == PointerPhase::kUp || sample.phase == PointerPhase::kCancel) {
      if (registry_.end(key)) ++result.ended;
      active_.erase(identity);
    }
  }
  if (batch.terminalCancel || batch.sourceLost) cancelAll(result);
  result.accepted = !result.normalized.samples.empty() || result.terminalCancel;
  return result;
}

PlatformInteractionIngressResult PlatformInteractionIngress::sourceLost(
    InputSourceId source, std::uint64_t sequence, std::uint64_t timestampNs) noexcept {
  PlatformPointerBatch batch{};
  batch.sourceLost = true;
  for (const auto& [identity, key] : active_) {
    if (identity.source != source) continue;
    batch.samples.push_back({source, key.pointer, sequence, timestampNs, 0.0F, 0.0F,
                             0.0F, 0.0F, 0.0F, {}, {},
                             SampleProvenance::kConfirmedCurrent, PointerPhase::kCancel});
  }
  return submit(batch);
}

}  // namespace canvas::input
