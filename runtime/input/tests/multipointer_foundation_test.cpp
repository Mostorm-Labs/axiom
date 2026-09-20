#include "canvas/input/active_pointer_registry.hpp"
#include "canvas/input/pointer_capabilities.hpp"

#include <cassert>

int main() {
  canvas::input::ActivePointerRegistry registry;
  const auto first = registry.begin(1, 7);
  assert(first.valid());
  assert(registry.accepts(first));
  assert(registry.end(first));
  assert(!registry.accepts(first));
  const auto reused = registry.begin(1, 7);
  assert(reused.generation > first.generation);
  assert(!registry.accepts(first));
  assert(registry.accepts(reused));
  const canvas::input::ContactGeometry contact{2.0F, 3.0F, 0.0F};
  assert(contact.area() == 6.0F);
  return 0;
}
