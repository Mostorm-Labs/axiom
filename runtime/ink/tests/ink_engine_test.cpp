#include "canvas/ink/ink_engine.hpp"
#include <cassert>
int main() {
  canvas::ink::InkEngine engine(canvas::ink::BrushDescriptor{2.0F});
  assert(engine.begin(17));
  assert(engine.append({1, 10, 1.0F, 2.0F, 0.5F, false}));
  assert(engine.append({2, 20, 3.0F, 4.0F, 0.75F, false}));
  const auto preview = engine.previewPointCount();
  assert(preview == 2);
  const auto stroke = engine.finish();
  assert(stroke.has_value());
  assert(stroke->id == 17);
  assert(stroke->points.size() == 2);
  assert(engine.cancelled() == false);
  return 0;
}
