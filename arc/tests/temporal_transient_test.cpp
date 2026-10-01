#include "arc/temporal_transient.hpp"

#include <array>
#include <cassert>

int main() {
  arc::TemporalTransientLayer layer;
  const std::array primitives{
      arc::TemporalPrimitive{1.0F, 2.0F, 3.0F, 0.8F},
      arc::TemporalPrimitive{4.0F, 5.0F, 2.0F, 0.4F}};
  assert(!layer.publish(0, 1, primitives, 1000, 600));
  assert(layer.publish(42, 1, primitives, 1000, 600));
  assert(!layer.publish(42, 1, primitives, 1000, 600));
  assert(layer.publish(42, 2, primitives, 1050, 600));

  const auto initial = layer.visible(1050);
  assert(initial.size() == 2);
  assert(initial[0].opacity == 0.8F);
  const auto halfway = layer.visible(1350);
  assert(halfway.size() == 2);
  assert(halfway[0].opacity < initial[0].opacity);
  assert(halfway[0].opacity > 0.0F);
  assert(layer.visible(1650).empty());
  assert(layer.activeCount(1650) == 0);

  assert(layer.publish(43, 1, primitives, 2000, 500));
  assert(layer.cancel(43));
  assert(layer.visible(2001).empty());
  return 0;
}
