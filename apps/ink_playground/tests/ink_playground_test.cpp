#include "ink_playground_host.hpp"
#include "platform_contract.hpp"

#include <cassert>
#include <string_view>

namespace {
void composition_test() {
  canvas::ink_playground::InkPlaygroundHost host;
  assert(host.productionPathComplete());
  canvas::input::PointerSampleBatch batch;
  batch.samples.push_back({1, 1'000'000, 2.0F, 3.0F, 0.5F, false});
  batch.samples.push_back({2, 2'000'000, 4.0F, 5.0F, 0.8F, true});
  assert(host.beginStroke(17));
  assert(host.accept(batch, 2'000'000));
  const auto hud = host.hud();
  assert(hud.sampleHz == 1000.0);
  assert(hud.batch == 2);
  assert(hud.queueAgeMs == 0.0);
  assert(hud.inkMs == 1.0);
  assert(hud.previewRevision == 1);
  assert(hud.predictionDepth == 1);
  assert(hud.presentEvidenceKind == "none");
  assert(hud.pendingHandoffCount == 0);
  assert(hud.frameMs == 0.0);
  assert(host.commitStroke(17, 71));
  assert(host.submittedOperationCount() == 1);
}

void hud_test() {
  canvas::ink_playground::InkPlaygroundHost host;
  host.recordPresentation("platform-qualified", 2, 8.25);
  const auto hud = host.hud();
  assert(hud.presentEvidenceKind == "platform-qualified");
  assert(hud.pendingHandoffCount == 2);
  assert(hud.frameMs == 8.25);
}

void platform_test() {
  using namespace canvas::ink_playground;
  const auto windows = platformContract(PlatformKind::kWindows);
  const auto android = platformContract(PlatformKind::kAndroid);
  const auto web = platformContract(PlatformKind::kWeb);
  assert(windows.ingress == IngressKind::kNativeArcInputSource);
  assert(android.ingress == IngressKind::kNativeArcInputSource);
  assert(web.ingress == IngressKind::kPointerEventWasmBatch);
  for (const auto& contract : {windows, android, web}) {
    assert(!contract.platformOwnsBrushSemantics);
    assert(!contract.platformOwnsSelectionSemantics);
    assert(!contract.platformOwnsEraserSemantics);
    assert(!contract.entrypoint.empty());
  }
}
}  // namespace

int main(int argc, char** argv) {
  const std::string_view mode = argc > 1 ? argv[1] : "composition";
  if (mode == "composition") composition_test();
  else if (mode == "hud") hud_test();
  else if (mode == "platform") platform_test();
  else return 2;
  return 0;
}
