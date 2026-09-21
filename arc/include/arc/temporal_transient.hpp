#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace arc {

struct TemporalPrimitive final {
  float x = 0.0F;
  float y = 0.0F;
  float radius = 0.0F;
  float opacity = 0.0F;
};

class TemporalTransientLayer final {
 public:
  bool publish(std::uint64_t transientId, std::uint64_t revision,
               std::span<const TemporalPrimitive> primitives,
               std::uint64_t nowMs, std::uint64_t durationMs);
  bool cancel(std::uint64_t transientId) noexcept;
  [[nodiscard]] std::vector<TemporalPrimitive> visible(
      std::uint64_t nowMs) const;
  [[nodiscard]] std::size_t activeCount(std::uint64_t nowMs) const noexcept;

 private:
  struct Entry final {
    std::uint64_t revision = 0;
    std::uint64_t publishedAtMs = 0;
    std::uint64_t durationMs = 0;
    std::vector<TemporalPrimitive> primitives;
  };
  std::unordered_map<std::uint64_t, Entry> entries_;
};

}  // namespace arc
