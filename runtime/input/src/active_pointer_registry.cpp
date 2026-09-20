#include "canvas/input/active_pointer_registry.hpp"

namespace canvas::input {

PointerKey ActivePointerRegistry::begin(InputSourceId source, PointerId pointer) noexcept {
  if (source == 0 || pointer == 0) return {};
  auto& slot = slots_[SlotKey{source, pointer}];
  if (slot.generation == 0) slot.generation = 1;
  else if (slot.active) return PointerKey{source, pointer, slot.generation};
  else ++slot.generation;
  slot.active = true;
  return PointerKey{source, pointer, slot.generation};
}

bool ActivePointerRegistry::end(const PointerKey& key) noexcept {
  const auto it = slots_.find(SlotKey{key.source, key.pointer});
  if (it == slots_.end() || !it->second.active || it->second.generation != key.generation) return false;
  it->second.active = false;
  return true;
}

bool ActivePointerRegistry::accepts(const PointerKey& key) const noexcept {
  const auto it = slots_.find(SlotKey{key.source, key.pointer});
  return it != slots_.end() && it->second.active && it->second.generation == key.generation;
}

PointerGeneration ActivePointerRegistry::generation(InputSourceId source, PointerId pointer) const noexcept {
  const auto it = slots_.find(SlotKey{source, pointer});
  return it == slots_.end() ? 0 : it->second.generation;
}

}  // namespace canvas::input
