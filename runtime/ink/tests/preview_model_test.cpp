#include "canvas/ink/arc_preview_bridge.hpp"
#include "canvas/ink/preview_model.hpp"

#include <cassert>

namespace {
class RecordingPort final : public canvas::ink::PreviewPresentationPort {
 public:
  bool available = true;
  bool present(const canvas::ink::PreviewSnapshot& snapshot) noexcept override {
    ++calls;
    last = snapshot;
    return available;
  }
  std::size_t calls = 0;
  canvas::ink::PreviewSnapshot last;
};
}

int main() {
  canvas::ink::PreviewModel model;
  assert(model.begin(7));
  const canvas::ink::StrokePoint confirmedOne[] = {{1.0F, 2.0F, 0.5F}};
  const canvas::ink::StrokePoint predictedOne[] = {{2.0F, 3.0F, 0.5F}};
  assert(model.update(confirmedOne, predictedOne));
  const auto first = model.snapshot();
  assert(first.revision == 1);
  assert(first.confirmed.size() == 1);
  assert(first.predicted.size() == 1);
  const canvas::ink::StrokePoint confirmedTwo[] = {{3.0F, 4.0F, 0.6F}};
  const canvas::ink::StrokePoint predictedTwo[] = {{4.0F, 5.0F, 0.6F}};
  assert(model.update(confirmedTwo, predictedTwo));
  const auto second = model.snapshot();
  assert(second.revision == 2);
  assert(second.confirmed.size() == 2);
  assert(second.predicted.size() == 1);
  assert(second.predicted.front().x == 4.0F);

  RecordingPort port;
  canvas::ink::ArcPreviewBridge bridge(port);
  assert(bridge.present(second) == canvas::ink::PreviewDisposition::kPresented);
  port.available = false;
  assert(bridge.present(second) == canvas::ink::PreviewDisposition::kCanonicalOnly);
  assert(bridge.canonicalOnly());
  assert(bridge.present(second) == canvas::ink::PreviewDisposition::kCanonicalOnly);
  assert(port.calls == 2);
  model.cancel();
  assert(model.snapshot().confirmed.empty());
  return 0;
}
