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
  assert(model.beginKeyed(10));
  assert(model.beginKeyed(11));
  assert(model.updateKeyed(10, confirmedOne, predictedOne));
  assert(model.updateKeyed(11, confirmedTwo, predictedTwo));
  assert(model.snapshot(10)->strokeId == 10);
  assert(model.snapshot(11)->strokeId == 11);
  model.cancelKeyed(10);
  assert(model.snapshot(10) == nullptr);
  assert(model.snapshot(11) != nullptr);
  RecordingPort keyedPort;
  canvas::ink::ArcPreviewBridge keyedBridge(keyedPort);
  assert(keyedBridge.presentKeyed(*model.snapshot(11)) == canvas::ink::PreviewDisposition::kPresented);
  keyedPort.available = false;
  canvas::ink::PreviewSnapshot unavailable{12, 1, {confirmedOne[0]}, {predictedOne[0]}};
  assert(keyedBridge.presentKeyed(unavailable) == canvas::ink::PreviewDisposition::kCanonicalOnly);
  assert(keyedBridge.canonicalOnly(12));
  assert(!keyedBridge.canonicalOnly(11));
  return 0;
}
