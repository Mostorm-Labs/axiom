#pragma once
#include <cstdint>
#include <cstddef>
namespace canvas::runtime {
struct CanvasTargetKey final {
  std::uint64_t runtimeEpoch = 0;
  std::uint64_t viewId = 0;
  std::uint64_t viewEpoch = 0;
  std::uint64_t attachmentEpoch = 0;
  friend constexpr bool operator==(const CanvasTargetKey&, const CanvasTargetKey&) = default;
};

struct CanvasTargetKeyHash final {
  std::size_t operator()(const CanvasTargetKey& value) const noexcept {
    std::size_t hash = static_cast<std::size_t>(value.runtimeEpoch);
    hash = hash * 16777619U ^ static_cast<std::size_t>(value.viewId);
    hash = hash * 16777619U ^ static_cast<std::size_t>(value.viewEpoch);
    return hash * 16777619U ^ static_cast<std::size_t>(value.attachmentEpoch);
  }
};

}
