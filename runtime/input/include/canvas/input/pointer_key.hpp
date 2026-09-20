#pragma once

#include <cstdint>
#include <functional>

namespace canvas::input {

using InputSourceId = std::uint64_t;
using PointerId = std::uint64_t;
using PointerGeneration = std::uint64_t;

struct PointerKey final {
  InputSourceId source = 0;
  PointerId pointer = 0;
  PointerGeneration generation = 0;

  friend bool operator==(const PointerKey&, const PointerKey&) = default;
  [[nodiscard]] bool valid() const noexcept { return source != 0 && pointer != 0 && generation != 0; }
};

struct PointerKeyHash final {
  std::size_t operator()(const PointerKey& key) const noexcept {
    std::size_t h = std::hash<std::uint64_t>{}(key.source);
    h ^= std::hash<std::uint64_t>{}(key.pointer) + (h << 6U) + (h >> 2U);
    h ^= std::hash<std::uint64_t>{}(key.generation) + (h << 6U) + (h >> 2U);
    return h;
  }
};

}  // namespace canvas::input
