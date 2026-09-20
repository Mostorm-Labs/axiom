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

void surface_test() {
  canvas::ink_playground::InkPlaygroundHost host;
  assert(host.bindSurface(320, 180));
  assert(host.surface().available);
  assert(host.surface().generation == 1);
  assert(host.resizeSurface(640, 360));
  assert(host.surface().generation == 2);
  assert(host.surface().width == 640);
  assert(host.loseSurface());
  assert(!host.surface().available);
  assert(host.resizeSurface(800, 450));
  assert(host.surface().generation == 3);
  assert(host.surface().available);
}

void preview_points_test() {
  canvas::ink_playground::InkPlaygroundHost host;
  assert(host.beginStroke(11));
  canvas::input::PointerSampleBatch batch;
  batch.samples.push_back({1, 1'000'000, 10.0F, 20.0F, 0.5F, false});
  batch.samples.push_back({2, 2'000'000, 30.0F, 40.0F, 0.7F, false});
  assert(host.accept(batch, 2'000'000));
  const auto points = host.previewPoints();
  assert(points.size() == 2);
  assert(points[0].x == 10.0F && points[1].y == 40.0F);
}

void repeated_strokes_test() {
  canvas::ink_playground::InkPlaygroundHost host;
  canvas::input::PointerSampleBatch first;
  first.samples.push_back({1, 1'000'000, 10.0F, 20.0F, 0.5F, false});
  assert(host.beginStroke(21));
  assert(host.accept(first, 1'000'000));
  assert(host.commitStroke(21, 121));

  canvas::input::PointerSampleBatch second;
  second.samples.push_back({2, 2'000'000, 100.0F, 120.0F, 0.7F, false});
  assert(host.beginStroke(22));
  assert(host.accept(second, 2'000'000));
  assert(host.commitStroke(22, 122));
  assert(host.submittedOperationCount() == 2);
  const auto strokes = host.previewStrokes();
  assert(strokes.size() == 2);
  assert(strokes[0].front().x == 10.0F);
  assert(strokes[1].front().x == 100.0F);
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
    (void)contract.entrypoint;
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
  else if (mode == "surface") surface_test();
  else if (mode == "preview") preview_points_test();
  else if (mode == "repeated") repeated_strokes_test();
  else if (mode == "platform") platform_test();
  else return 2;
  return 0;
}
