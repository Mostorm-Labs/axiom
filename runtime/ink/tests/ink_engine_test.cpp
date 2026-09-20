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
  const canvas::input::PointerKey keyA{3, 1, 1};
  const canvas::input::PointerKey keyB{3, 2, 1};
  assert(engine.begin(keyA, 30));
  assert(engine.begin(keyB, 31));
  assert(engine.activeStrokeCount() == 2);
  assert(engine.append(keyA, {1, 100, 4.0F, 4.0F, 0.5F, false, keyA}));
  assert(engine.append(keyB, {1, 100, 8.0F, 8.0F, 0.5F, false, keyB}));
  assert(engine.finish(keyA)->id == 30);
  assert(engine.finish(keyB)->id == 31);
  return 0;
}
