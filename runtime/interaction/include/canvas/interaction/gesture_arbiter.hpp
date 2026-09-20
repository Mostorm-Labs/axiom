#pragma once

#include "canvas/input/pointer_key.hpp"

#include <cstdint>
#include <unordered_map>

namespace canvas::interaction {

enum class GestureClaim : std::uint8_t { kNone, kInk, kViewport, kEraser };

class GestureArbiter final {
 public:
  [[nodiscard]] bool claim(const input::PointerKey& key, GestureClaim claim) noexcept;
  [[nodiscard]] bool release(const input::PointerKey& key, GestureClaim claim) noexcept;
  [[nodiscard]] bool owns(const input::PointerKey& key, GestureClaim claim) const noexcept;
  [[nodiscard]] GestureClaim owner(const input::PointerKey& key) const noexcept;
  void cancel(const input::PointerKey& key) noexcept;
  void cancelAll() noexcept { claims_.clear(); }

 private:
  std::unordered_map<input::PointerKey, GestureClaim, input::PointerKeyHash> claims_;
};

}  // namespace canvas::interaction
