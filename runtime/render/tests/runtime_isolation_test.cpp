#include "canvas/render/runtime_app_binding.hpp"
#include "canvas/render/runtime_frame_coordinator.hpp"
#include "canvas/render/skia_surface_provider.hpp"

#include <cassert>
#include <memory>
#include <vector>

using namespace canvas::render;

namespace {
RenderTargetInfo info(const char* id) {
  return {id, RenderTargetKind::kCpuRaster, RenderTargetBackend::kRaster,
          RenderTargetFormat::kRgba8888,
          {32.0F, 32.0F, 32U, 32U, 1.0F, 1.0F},
          {false, false, false, true, false, false}};
}

struct Sink final : SessionCancelSink {
  int calls = 0;
  void onSessionCancel(const canvas::input::PlatformInteractionIngressResult&) noexcept override {
    ++calls;
  }
};

canvas::input::PlatformPointerBatch down(canvas::input::InputSourceId source,
                                         canvas::input::PointerId pointer) {
  canvas::input::PlatformPointerBatch batch;
  batch.samples.push_back({source, pointer, 1U, 1U, 4.0F, 5.0F, 0.5F, 0.0F, 0.0F,
                           {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent,
                           canvas::input::PointerPhase::kDown});
  return batch;
}

RuntimeFrameCapture frame(ViewId view, FrameRole role, std::uint64_t id) {
  return {{view, FrameId{id}, 7U, 8U, SurfaceGeneration{3U}, MetricsGeneration{4U}},
          {32.0F, 32.0F, 32U, 32U, 1.0F, 1.0F}, role};
}
}

int main() {
  RuntimeAppBinding a{ViewId{1}, SurfaceProfile{info("a"),
      std::make_unique<RasterSkiaSurfaceProvider>()}};
  RuntimeAppBinding b{ViewId{2}, SurfaceProfile{info("b"),
      std::make_unique<RasterSkiaSurfaceProvider>()}};
  Sink sinkA;
  a.setSessionCancelSink(&sinkA);
  assert(a.registerInputSource({9U, "windows"}));
  assert(b.registerInputSource({9U, "windows"}));
  const auto acceptedA = a.submit(9U, down(9U, 3U));
  const auto acceptedB = b.submit(9U, down(9U, 3U));
  assert(acceptedA.accepted && acceptedB.accepted);
  assert(acceptedA.normalized.samples.front().key == acceptedB.normalized.samples.front().key);
  assert(a.notifySourceLost(9U, 2U, 2U));
  assert(sinkA.calls == 1);
  // A can begin a fresh generation after its own source loss; B's active
  // source/session remains independent and accepts its own next sample.
  const auto nextA = a.submit(9U, down(9U, 3U));
  assert(nextA.accepted && nextA.normalized.samples.front().key.generation ==
         acceptedA.normalized.samples.front().key.generation + 1U);
  canvas::input::PlatformPointerBatch moveB;
  moveB.samples.push_back({9U, 3U, 2U, 2U, 6.0F, 7.0F, 0.5F, 0.0F, 0.0F,
                           {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent,
                           canvas::input::PointerPhase::kMove});
  const auto nextB = b.submit(9U, moveB);
  assert(nextB.accepted && nextB.normalized.samples.front().key ==
         acceptedB.normalized.samples.front().key);
  assert(a.resizeSurface({64.0F, 64.0F, 64U, 64U, 2.0F, 2.0F}) ==
         SurfaceProviderDisposition::kCommitted);
  assert(a.surfaces().lifecycle().current().surfaceGeneration.value() == 2U);
  assert(b.surfaces().lifecycle().current().surfaceGeneration.value() == 1U);

  RuntimeFrameCoordinator fa;
  RuntimeFrameCoordinator fb;
  assert(fa.request(frame(ViewId{1}, FrameRole::kCanonical, 1U)) == RuntimeFrameDisposition::kAccepted);
  assert(fb.request(frame(ViewId{2}, FrameRole::kCanonical, 1U)) == RuntimeFrameDisposition::kAccepted);
  fa.close();
  assert(fa.request(frame(ViewId{1}, FrameRole::kCanonical, 2U)) == RuntimeFrameDisposition::kUnavailable);
  assert(fb.submit(FrameRole::kCanonical, FrameId{1U}, false) == RuntimeFrameDisposition::kAccepted);
  assert(fb.receive({frame(ViewId{2}, FrameRole::kCanonical, 1U).identity,
                     FrameRole::kCanonical, true, RuntimePresentEvidence::kPlatformQualified}) ==
         RuntimeFrameDisposition::kAccepted);
  assert(fb.presented(FrameRole::kCanonical));
  return 0;
}
