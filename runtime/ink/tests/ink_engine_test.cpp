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
  assert(engine.processedSampleCount() == 2);
  assert(engine.begin(18));
  engine.cancel();
  assert(!engine.finish().has_value());
  assert(engine.cancelled());
  assert(engine.begin(19));
  for (std::uint64_t i = 1; i <= 14400; ++i) {
    assert(engine.append({i, i * 4166666, 1.0F, 2.0F, 0.5F, false}));
  }
  assert(engine.processedSampleCount() == 14400);
  assert(engine.finish().has_value());
  return 0;
}
