#include "canvas/render/runtime_frame_coordinator.hpp"

#include <cassert>

using namespace canvas::render;

RuntimeFrameCapture capture(FrameRole role, std::uint64_t frame) {
  return {{ViewId{1}, FrameId{frame}, 7, 8, SurfaceGeneration{3}, MetricsGeneration{4}},
          {320.0F, 240.0F, 640, 480, 2.0F, 2.0F}, role};
}

int main() {
  RuntimeFrameCoordinator coordinator;
  assert(coordinator.request(capture(FrameRole::kPreview, 10)) == RuntimeFrameDisposition::kAccepted);
  assert(coordinator.submit(FrameRole::kPreview, FrameId{10}, true) == RuntimeFrameDisposition::kRetry);
  assert(coordinator.submit(FrameRole::kPreview, FrameId{10}, false) == RuntimeFrameDisposition::kAccepted);
  assert(coordinator.receive({capture(FrameRole::kPreview, 10).identity,
                             FrameRole::kPreview, true, RuntimePresentEvidence::kApproximate}) ==
         RuntimeFrameDisposition::kAccepted);
  assert(coordinator.request(capture(FrameRole::kCanonical, 11)) == RuntimeFrameDisposition::kAccepted);
  assert(coordinator.submit(FrameRole::kCanonical, FrameId{11}, false) == RuntimeFrameDisposition::kAccepted);
  assert(coordinator.receive({capture(FrameRole::kCanonical, 11).identity,
                             FrameRole::kCanonical, true, RuntimePresentEvidence::kPlatformQualified}) ==
         RuntimeFrameDisposition::kAccepted);
  assert(!coordinator.retirePreview(capture(FrameRole::kCanonical, 11).identity));
  assert(coordinator.request(capture(FrameRole::kPreview, 11)) == RuntimeFrameDisposition::kAccepted);
  assert(coordinator.submit(FrameRole::kPreview, FrameId{11}, false) == RuntimeFrameDisposition::kAccepted);
  assert(coordinator.receive({capture(FrameRole::kPreview, 11).identity,
                             FrameRole::kPreview, true, RuntimePresentEvidence::kPlatformQualified}) ==
         RuntimeFrameDisposition::kAccepted);
  assert(coordinator.retirePreview(capture(FrameRole::kCanonical, 11).identity));
  assert(coordinator.request(capture(FrameRole::kCanonical, 9)) == RuntimeFrameDisposition::kStale);
  coordinator.close();
  assert(coordinator.request(capture(FrameRole::kCanonical, 12)) == RuntimeFrameDisposition::kUnavailable);
}
