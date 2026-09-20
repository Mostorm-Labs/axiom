#include "arc/temporal_transient.hpp"

#include <cmath>

namespace arc {

bool TemporalTransientLayer::publish(
    std::uint64_t transientId, std::uint64_t revision,
    std::span<const TemporalPrimitive> primitives, std::uint64_t nowMs,
    std::uint64_t durationMs) {
  if (transientId == 0 || revision == 0 || primitives.empty() ||
      durationMs == 0) {
    return false;
  }
  for (const auto& primitive : primitives) {
    if (!std::isfinite(primitive.x) || !std::isfinite(primitive.y) ||
        !std::isfinite(primitive.radius) || primitive.radius <= 0.0F ||
        !std::isfinite(primitive.opacity) || primitive.opacity < 0.0F ||
        primitive.opacity > 1.0F) {
      return false;
    }
  }
  const auto found = entries_.find(transientId);
  if (found != entries_.end() && revision <= found->second.revision) return false;
  entries_.insert_or_assign(
      transientId,
      Entry{revision, nowMs, durationMs,
            std::vector<TemporalPrimitive>(primitives.begin(), primitives.end())});
  return true;
}

bool TemporalTransientLayer::cancel(std::uint64_t transientId) noexcept {
  return entries_.erase(transientId) == 1U;
}

std::vector<TemporalPrimitive> TemporalTransientLayer::visible(
    std::uint64_t nowMs) const {
  std::vector<TemporalPrimitive> result;
  for (const auto& [id, entry] : entries_) {
    (void)id;
    if (nowMs < entry.publishedAtMs ||
        nowMs >= entry.publishedAtMs + entry.durationMs) {
      continue;
    }
    const float remaining =
        1.0F - static_cast<float>(nowMs - entry.publishedAtMs) /
                   static_cast<float>(entry.durationMs);
    for (auto primitive : entry.primitives) {
      primitive.opacity *= remaining;
      result.push_back(primitive);
    }
  }
  return result;
}

std::size_t TemporalTransientLayer::activeCount(std::uint64_t nowMs) const noexcept {
  std::size_t result = 0;
  for (const auto& [id, entry] : entries_) {
    (void)id;
    if (nowMs >= entry.publishedAtMs &&
        nowMs < entry.publishedAtMs + entry.durationMs) {
      ++result;
    }
  }
  return result;
}

}  // namespace arc
