#pragma once

#include "canvas/input/pointer_key.hpp"

#include <unordered_map>

namespace canvas::input {

class ActivePointerRegistry final {
 public:
  [[nodiscard]] PointerKey begin(InputSourceId source, PointerId pointer) noexcept;
  [[nodiscard]] bool end(const PointerKey& key) noexcept;
  [[nodiscard]] bool accepts(const PointerKey& key) const noexcept;
  [[nodiscard]] PointerGeneration generation(InputSourceId source, PointerId pointer) const noexcept;

 private:
  struct Slot final { PointerGeneration generation = 0; bool active = false; };
  struct SlotKey final {
    InputSourceId source = 0;
    PointerId pointer = 0;
    friend bool operator==(const SlotKey&, const SlotKey&) = default;
  };
  struct SlotKeyHash final {
    std::size_t operator()(const SlotKey& key) const noexcept {
      return std::hash<std::uint64_t>{}(key.source) ^
             (std::hash<std::uint64_t>{}(key.pointer) << 1U);
    }
  };
  std::unordered_map<SlotKey, Slot, SlotKeyHash> slots_;
};

}  // namespace canvas::input
