#include "ink_playground_host.hpp"
#include "platform_contract.hpp"
#include "platform/android/android_pointer_identity.hpp"

#include <cassert>
#include <limits>
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
  assert(host.presentCanonicalFrame(1, 0.0));
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
  assert(host.canonicalStrokes().empty());
  assert(host.runtimePreviewVisible());
  host.setRuntimePreviewVisible(false);
  assert(!host.runtimePreviewVisible());
  assert(host.transientPreviewPoints().size() == 2);
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
  const auto strokes = host.canonicalStrokes();
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

void multipointer_test() {
  canvas::ink_playground::InkPlaygroundHost host;
  const canvas::input::PointerKey first{7, 1, 1};
  const canvas::input::PointerKey second{7, 2, 1};
  assert(host.beginStroke(first, 101));
  assert(host.beginStroke(second, 102));
  canvas::input::PointerSampleBatch firstBatch;
  firstBatch.samples.push_back({1, 1'000'000, 10.0F, 10.0F, 0.5F, false, first});
  canvas::input::PointerSampleBatch secondBatch;
  secondBatch.samples.push_back({1, 1'000'000, 20.0F, 20.0F, 0.5F, false, second});
  assert(host.accept(firstBatch, 1'000'000));
  assert(host.accept(secondBatch, 1'000'000));
  assert(host.cancelStroke(first));
  assert(host.commitStroke(second, 102, 202));
  assert(!host.commitStroke(first, 101, 201));
  assert(host.canonicalStrokes().size() == 1);
}

void provisional_zero_mutation_test() {
  canvas::ink_playground::InkPlaygroundHost host;
  const canvas::input::PointerKey first{9, 1, 1};
  const canvas::input::PointerKey second{9, 2, 1};
  assert(host.beginStroke(first, 301));
  canvas::input::PointerSampleBatch downA;
  downA.samples.push_back({1, 1'000'000, 10.0F, 10.0F, 0.5F, false,
                           first, {}, {}, canvas::input::PointerPhase::kDown});
  assert(host.accept(downA, 1'000'000));
  canvas::input::PointerSampleBatch moveA;
  moveA.samples.push_back({2, 2'000'000, 11.0F, 11.0F, 0.5F, false,
                           first, {}, {}, canvas::input::PointerPhase::kMove});
  assert(host.accept(moveA, 2'000'000));
  assert(host.canonicalStrokes().empty());
  assert(host.submittedOperationCount() == 0);
  assert(host.beginStroke(second, 302));
  canvas::input::PointerSampleBatch downB;
  downB.samples.push_back({3, 3'000'000, 30.0F, 30.0F, 0.5F, false,
                           second, {}, {}, canvas::input::PointerPhase::kDown});
  assert(host.accept(downB, 3'000'000));
  assert(host.canonicalStrokes().empty());
  assert(host.submittedOperationCount() == 0);
  assert(host.pointerDisposition(first) == canvas::interaction::ContactDisposition::kViewportGesture);
  assert(host.pointerDisposition(second) == canvas::interaction::ContactDisposition::kViewportGesture);
}

void viewport_gesture_state_test() {
  canvas::ink_playground::InkPlaygroundHost host;
  const canvas::input::PointerKey first{10, 1, 1};
  const canvas::input::PointerKey second{10, 2, 1};
  assert(host.beginStroke(first, 401));
  assert(host.beginStroke(second, 402));
  canvas::input::PointerSampleBatch downA;
  downA.samples.push_back({1, 1, 0.0F, 0.0F, 0.0F, false, first, {}, {}, canvas::input::PointerPhase::kDown});
  assert(host.accept(downA, 1));
  canvas::input::PointerSampleBatch downB;
  downB.samples.push_back({2, 2, 20.0F, 0.0F, 0.0F, false, second, {}, {}, canvas::input::PointerPhase::kDown});
  assert(host.accept(downB, 2));
  assert(host.viewportGestureClaimed());
  assert(host.viewportGesture().scale == 1.0F);
  canvas::input::PointerSampleBatch moveB;
  moveB.samples.push_back({3, 3, 40.0F, 0.0F, 0.0F, false, second, {}, {}, canvas::input::PointerPhase::kMove});
  assert(host.accept(moveB, 3));
  assert(host.viewportGesture().scale == 2.0F);
  assert(host.viewportGesture().centerX == 20.0F);
  assert(host.viewportGesture().translationX == 0.0F);
  canvas::input::PointerSampleBatch upA;
  upA.samples.push_back({4, 4, 0.0F, 0.0F, 0.0F, false, first, {}, {}, canvas::input::PointerPhase::kUp});
  assert(host.accept(upA, 4));
  canvas::input::PointerSampleBatch upB;
  upB.samples.push_back({5, 5, 40.0F, 0.0F, 0.0F, false, second, {}, {}, canvas::input::PointerPhase::kUp});
  assert(host.accept(upB, 5));
  assert(!host.viewportGestureClaimed());
  assert(host.viewportGesture().scale == 2.0F);
  const canvas::input::PointerKey third{10, 3, 1};
  assert(host.beginStroke(third, 403));
  canvas::input::PointerSampleBatch downC;
  downC.samples.push_back({6, 6, 20.0F, 20.0F, 0.0F, false, third, {}, {}, canvas::input::PointerPhase::kDown});
  assert(host.accept(downC, 6));
  canvas::input::PointerSampleBatch upC;
  upC.samples.push_back({7, 7, 24.0F, 24.0F, 0.0F, false, third, {}, {}, canvas::input::PointerPhase::kUp});
  assert(host.accept(upC, 7));
  assert(host.commitStroke(third, 403, 403));
  assert(host.canonicalStrokes().size() == 1);
  assert(host.canonicalStrokes().front().front().x == 10.0F);
  assert(host.canonicalStrokes().front().front().y == 10.0F);
}

void android_pointer_identity_test() {
  using canvas::ink_playground::androidPointerIdentity;
  assert(androidPointerIdentity(0).has_value());
  assert(*androidPointerIdentity(0) == 1);
  assert(*androidPointerIdentity(1) == 2);
  assert(*androidPointerIdentity(31) == 32);
  assert(!androidPointerIdentity(std::numeric_limits<std::uint64_t>::max()).has_value());
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
  else if (mode == "multipointer") multipointer_test();
  else if (mode == "provisional") provisional_zero_mutation_test();
  else if (mode == "viewport") viewport_gesture_state_test();
  else if (mode == "android-pointer-identity") android_pointer_identity_test();
  else return 2;
  return 0;
}
