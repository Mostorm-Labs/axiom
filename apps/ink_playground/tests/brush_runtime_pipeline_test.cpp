#include "ink_playground_host.hpp"

#include <cassert>
#include <vector>

void partialEraserCommitsRendererNeutralMask() {
  canvas::ink_playground::InkPlaygroundHost host;
  assert(host.bindSurface(256, 256));
  assert(host.beginBrushSession(77U, 1U));
  assert(host.appendBrushSample(77U, 24.0, 64.0, 0.5, 1U));
  assert(host.appendBrushSample(77U, 48.0, 64.0, 0.5, 2U));
  assert(host.appendBrushSample(77U, 80.0, 64.0, 0.5, 3U));
  assert(host.appendBrushSample(77U, 104.0, 64.0, 0.5, 4U));
  assert(host.finishBrushSession(77U));
  assert(host.semanticObjectCount() == 1U);
  const auto beforeGeneration = host.semanticGeneration().value();

  assert(host.selectTool(canvas::ink_playground::InkPlaygroundHost::ToolMode::kPartialEraser));
  assert(host.eraserBegin(88U));
  assert(host.eraserSample(88U, 64.0, 64.0));
  assert(host.eraserFinish(88U));

  // Partial erase retains the source stroke and commits a renderer-neutral
  // mask.  The runtime must not manufacture closed polygon fragments from
  // an outline vertex subset; doing so creates phantom arcs/triangles.
  assert(host.semanticObjectCount() == 1U);
  assert(host.semanticGeneration().value() > beforeGeneration);
  assert(host.presentCanonicalFrame(host.canonicalFrameCount() + 1U, 0.0));
  std::vector<std::uint8_t> pixels(256U * 256U * 4U);
  assert(host.activeSurfaceProvider()->readbackRgba(pixels).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  const auto center = static_cast<std::size_t>((64U * 256U + 64U) * 4U);
  assert(pixels[center + 3U] == 0U);
  const auto far = static_cast<std::size_t>((24U * 256U + 24U) * 4U);
  assert(pixels[far + 3U] != 0U);
}

void partialEraserAllowsTraceToEnterStroke() {
  canvas::ink_playground::InkPlaygroundHost host;
  assert(host.bindSurface(256, 256));
  assert(host.beginBrushSession(177U, 1U));
  assert(host.appendBrushSample(177U, 24.0, 96.0, 0.5, 1U));
  assert(host.appendBrushSample(177U, 104.0, 96.0, 0.5, 2U));
  assert(host.finishBrushSession(177U));
  const auto beforeGeneration = host.semanticGeneration().value();

  assert(host.selectTool(canvas::ink_playground::InkPlaygroundHost::ToolMode::kPartialEraser));
  assert(host.eraserBegin(188U));
  assert(host.eraserSample(188U, -100.0, 96.0));
  assert(host.eraserSample(188U, 64.0, 96.0));
  assert(host.eraserFinish(188U));
  assert(host.semanticGeneration().value() > beforeGeneration);
  assert(host.semanticObjectCount() == 1U);
}

void partialEraserWorksForAllBrushPackages() {
  const char* profiles[] = {"vector-solid-v1", "marker-flat-v1", "chalk-grain-v1"};
  for (const auto* profile : profiles) {
    canvas::ink_playground::InkPlaygroundHost host;
    assert(host.bindSurface(256, 256));
    assert(host.selectBrushProfile(profile, 1U));
    assert(host.beginBrushSession(277U, profile == std::string_view("vector-solid-v1") ? 1U
        : profile == std::string_view("marker-flat-v1") ? 2U : 3U));
    assert(host.appendBrushSample(277U, 24.0, 128.0, 0.5, 1U));
    assert(host.appendBrushSample(277U, 104.0, 128.0, 0.5, 2U));
    assert(host.finishBrushSession(277U));
    const auto before = host.semanticGeneration().value();
    assert(host.selectTool(canvas::ink_playground::InkPlaygroundHost::ToolMode::kPartialEraser));
    assert(host.eraserBegin(288U));
    assert(host.eraserSample(288U, 64.0, 128.0));
    assert(host.eraserFinish(288U));
    assert(host.semanticGeneration().value() > before);
    assert(host.semanticObjectCount() == 1U);
  }
}

void chalkRevisionSelectionIsRetained() {
  canvas::ink_playground::InkPlaygroundHost host;
  assert(host.bindSurface(256, 256));
  assert(host.selectBrushProfile("chalk-grain-v1", 1U));
  assert(host.selectedBrushRevision() == 1U);
  assert(host.beginBrushSession(281U, 3U));
  assert(host.cancelBrushSession(281U));
  assert(host.selectBrushProfile("chalk-grain-v1", 2U));
  assert(host.selectedBrushRevision() == 2U);
}

void membraneProfileSelectionIsRetained() {
  canvas::ink_playground::InkPlaygroundHost host;
  assert(host.bindSurface(256, 256));
  assert(host.selectBrushProfile("membrane-v1", 1U));
  assert(host.selectedBrushProfile() == "membrane-v1");
}

void objectEraserClearsCanonicalSurfaceImmediately() {
  canvas::ink_playground::InkPlaygroundHost host;
  assert(host.bindSurface(256, 256));
  assert(host.beginBrushSession(377U, 1U));
  assert(host.appendBrushSample(377U, 64.0, 64.0, 0.5, 1U));
  assert(host.appendBrushSample(377U, 128.0, 64.0, 0.5, 2U));
  assert(host.finishBrushSession(377U));
  assert(host.presentCanonicalFrame(host.canonicalFrameCount() + 1U, 0.0));
  const auto beforeEraseFrame = host.canonicalFrameCount();
  assert(host.semanticObjectCount() == 1U);

  assert(host.selectTool(canvas::ink_playground::InkPlaygroundHost::ToolMode::kObjectEraser));
  assert(host.eraserBegin(378U));
  assert(host.eraserSample(378U, 96.0, 64.0));
  assert(host.eraserFinish(378U));
  assert(host.semanticObjectCount() == 0U);
  assert(host.canonicalFrameCount() == beforeEraseFrame + 1U);

  std::vector<std::uint8_t> pixels(256U * 256U * 4U);
  assert(host.activeSurfaceProvider()->readbackRgba(pixels).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  const auto center = static_cast<std::size_t>((64U * 256U + 96U) * 4U);
  assert(pixels[center + 0U] == 255U);
  assert(pixels[center + 1U] == 255U);
  assert(pixels[center + 2U] == 255U);
}

void partialEraserPublishesRealtimePreview() {
  canvas::ink_playground::InkPlaygroundHost host;
  assert(host.bindSurface(256, 256));
  assert(host.beginBrushSession(387U, 1U));
  assert(host.appendBrushSample(387U, 32.0, 96.0, 0.5, 1U));
  assert(host.appendBrushSample(387U, 160.0, 96.0, 0.5, 2U));
  assert(host.finishBrushSession(387U));
  assert(host.presentCanonicalFrame(host.canonicalFrameCount() + 1U, 0.0));

  assert(host.selectTool(canvas::ink_playground::InkPlaygroundHost::ToolMode::kPartialEraser));
  const auto beforePreview = host.previewSubmissionCount();
  assert(host.eraserBegin(388U));
  assert(host.eraserSample(388U, 64.0, 96.0));
  assert(host.previewActive());
  assert(host.previewSubmissionCount() > beforePreview);
  std::vector<std::uint8_t> previewPixels(256U * 256U * 4U);
  assert(host.previewSurfaceProvider()->readbackRgba(previewPixels).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  const auto previewCenter = static_cast<std::size_t>((96U * 256U + 64U) * 4U);
  // Partial erase shows the growing final effect on the independent overlay:
  // the swept region is opaque canvas background rather than amber tool ink.
  assert(previewPixels[previewCenter + 0U] == 255U);
  assert(previewPixels[previewCenter + 1U] == 255U);
  assert(previewPixels[previewCenter + 2U] == 255U);
  assert(previewPixels[previewCenter + 3U] == 255U);
  assert(host.eraserCancel(388U));
  assert(!host.previewActive());
}

int main() {
  chalkRevisionSelectionIsRetained();
  membraneProfileSelectionIsRetained();
  partialEraserCommitsRendererNeutralMask();
  partialEraserAllowsTraceToEnterStroke();
  partialEraserWorksForAllBrushPackages();
  objectEraserClearsCanonicalSurfaceImmediately();
  partialEraserPublishesRealtimePreview();
  canvas::ink_playground::InkPlaygroundHost host;
  assert(host.bindSurface(256, 256));
  assert(host.beginBrushSession(7, 1U));
  assert(host.appendBrushSample(7, 8.0, 8.0, 0.5, 1U));
  assert(host.appendBrushSample(7, 24.0, 8.0, 0.5, 2U));
  assert(host.finishBrushSession(7));
  assert(host.semanticObjectCount() == 1U);
  assert(host.semanticGeneration().value() == 1U);
  assert(host.presentCanonicalFrame(1U, 0.0));
  assert(host.canonicalFrameCount() == 1U);

  canvas::ink_playground::InkPlaygroundHost batched;
  assert(batched.bindSurface(256, 256));
  canvas::input::PlatformPointerBatch batch;
  batch.samples.push_back({7U, 11U, 1U, 1'000'000U, 20.0F, 30.0F, 0.5F,
                           0.0F, 0.0F, {}, {},
                           canvas::input::SampleProvenance::kConfirmedCurrent,
                           canvas::input::PointerPhase::kDown});
  batch.samples.push_back({7U, 11U, 2U, 2'000'000U, 40.0F, 50.0F, 0.5F,
                           0.0F, 0.0F, {}, {},
                           canvas::input::SampleProvenance::kConfirmedCurrent,
                           canvas::input::PointerPhase::kMove});
  assert(batched.acceptPlatformBatch(batch, 2'000'000U));
  assert(batched.previewSubmissionCount() == 0U);
  assert(batched.presentBrushPreview());
  assert(batched.previewSubmissionCount() == 1U);
  canvas::input::PlatformPointerBatch finish;
  finish.samples.push_back({7U, 11U, 3U, 3'000'000U, 40.0F, 50.0F, 0.5F,
                            0.0F, 0.0F, {}, {},
                            canvas::input::SampleProvenance::kConfirmedCurrent,
                            canvas::input::PointerPhase::kUp});
  assert(batched.acceptPlatformBatch(finish, 3'000'000U));
  assert(batched.presentCanonicalFrame(1U, 0.0));
  assert(!batched.previewActive());

  // Android commonly reuses pointer id 0 for the next contact.  A second
  // committed stroke must survive the incremental-scene recovery boundary,
  // render alongside the first stroke, and retire its preview only after the
  // second canonical frame is visible.
  canvas::input::PlatformPointerBatch second;
  second.samples.push_back({7U, 11U, 4U, 4'000'000U, 80.0F, 30.0F, 0.5F,
                            0.0F, 0.0F, {}, {},
                            canvas::input::SampleProvenance::kConfirmedCurrent,
                            canvas::input::PointerPhase::kDown});
  second.samples.push_back({7U, 11U, 5U, 5'000'000U, 100.0F, 50.0F, 0.5F,
                            0.0F, 0.0F, {}, {},
                            canvas::input::SampleProvenance::kConfirmedCurrent,
                            canvas::input::PointerPhase::kMove});
  assert(batched.acceptPlatformBatch(second, 5'000'000U));
  assert(batched.presentBrushPreview());
  assert(batched.previewActive());
  // A viewport-only redraw must not retire the retained preview.  The amber
  // geometry remains visible until the matching canonical commit is actually
  // presented.
  assert(batched.presentCanonicalFrame(2U, 0.0, false));
  assert(batched.previewActive());
  canvas::input::PlatformPointerBatch secondFinish;
  secondFinish.samples.push_back({7U, 11U, 6U, 6'000'000U, 100.0F, 50.0F, 0.5F,
                                  0.0F, 0.0F, {}, {},
                                  canvas::input::SampleProvenance::kConfirmedCurrent,
                                  canvas::input::PointerPhase::kUp});
  assert(batched.acceptPlatformBatch(secondFinish, 6'000'000U));
  assert(batched.semanticObjectCount() == 2U);
  assert(batched.presentCanonicalFrame(3U, 0.0));
  assert(!batched.previewActive());

  // Platform ingress must drive the same viewport transform that the Android
  // MotionEvent bridge uses; changing the gesture must result in a canonical
  // redraw without changing the semantic document.
  canvas::ink_playground::InkPlaygroundHost pinch;
  assert(pinch.bindSurface(256, 256));
  auto platformSample = [](std::uint64_t pointer, std::uint64_t seq,
                           float x, float y, canvas::input::PointerPhase phase) {
    return canvas::input::PlatformPointerSample{
        7U, pointer, seq, seq * 1'000'000U, x, y, 0.5F, 0.0F, 0.0F, {}, {},
        canvas::input::SampleProvenance::kConfirmedCurrent, phase};
  };
  canvas::input::PlatformPointerBatch pinchA;
  pinchA.samples.push_back(platformSample(1U, 1U, 20.0F, 20.0F,
                                          canvas::input::PointerPhase::kDown));
  assert(pinch.acceptPlatformBatch(pinchA, 1'000'000U));
  canvas::input::PlatformPointerBatch pinchB;
  pinchB.samples.push_back(platformSample(2U, 2U, 40.0F, 20.0F,
                                          canvas::input::PointerPhase::kDown));
  assert(pinch.acceptPlatformBatch(pinchB, 2'000'000U));
  assert(pinch.viewportGestureClaimed());
  canvas::input::PlatformPointerBatch pinchMove;
  pinchMove.samples.push_back(platformSample(2U, 3U, 80.0F, 20.0F,
                                             canvas::input::PointerPhase::kMove));
  assert(pinch.acceptPlatformBatch(pinchMove, 3'000'000U));
  assert(pinch.viewportGesture().scale == 3.0F);
  assert(pinch.semanticObjectCount() == 0U);

  // The canonical surface must actually change when the camera changes, not
  // merely expose a different coordinate readout.
  canvas::ink_playground::InkPlaygroundHost camera;
  assert(camera.bindSurface(256, 256));
  assert(camera.beginBrushSession(9U, 1U));
  assert(camera.appendBrushSample(9U, 24.0, 24.0, 0.5, 1U));
  assert(camera.appendBrushSample(9U, 56.0, 24.0, 0.5, 2U));
  assert(camera.finishBrushSession(9U));
  assert(camera.presentCanonicalFrame(1U, 0.0));
  const auto frameBeforeNavigation = camera.canonicalFrameCount();
  assert(camera.applyViewportNavigation({
      canvas::interaction::ViewportNavigationKind::kWheelPan,
      12.0F, -8.0F, 0.0F, 0.0F, 1.0F}));
  // Viewport-only navigation is a render invalidation, not just a state
  // update.  The canonical surface must be refreshed before the next brush
  // preview or semantic commit arrives.
  assert(camera.canonicalFrameCount() == frameBeforeNavigation + 1U);
  std::vector<std::uint8_t> before(256U * 256U * 4U);
  assert(camera.activeSurfaceProvider()->readbackRgba(before).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  assert(camera.applyViewportNavigation({
      canvas::interaction::ViewportNavigationKind::kBrowserGesture,
      0.0F, 0.0F, 128.0F, 128.0F, 2.0F}));
  std::vector<std::uint8_t> after(256U * 256U * 4U);
  assert(camera.activeSurfaceProvider()->readbackRgba(after).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  assert(before != after);

  // Real pinch gestures produce fractional zoom (for example 2.127 on
  // Android).  The production Skia path must accept that affine transform;
  // integer-only matrix validation would reject the frame and leave the
  // surface visually unscaled.
  assert(camera.applyViewportNavigation({
      canvas::interaction::ViewportNavigationKind::kBrowserGesture,
      0.0F, 0.0F, 128.0F, 128.0F, 1.0635F}));
  assert(camera.viewportGesture().scale > 2.0F);

  // A new sample after a settled pinch must keep the same viewport on the
  // retained preview geometry.  Passing the append API's default transform
  // would silently reset amber preview to scale=1 while canonical uses the
  // zoomed camera.
  canvas::input::PlatformPointerBatch zoomedStroke;
  zoomedStroke.samples.push_back(platformSample(21U, 10U, 80.0F, 80.0F,
                                                canvas::input::PointerPhase::kDown));
  assert(camera.acceptPlatformBatch(zoomedStroke, 10'000'000U));
  canvas::input::PlatformPointerBatch zoomedMove;
  zoomedMove.samples.push_back(platformSample(21U, 11U, 96.0F, 80.0F,
                                                canvas::input::PointerPhase::kMove));
  assert(camera.acceptPlatformBatch(zoomedMove, 11'000'000U));
  assert(camera.previewRenderState().dirty);
  assert(camera.previewSurfaceProvider() != nullptr);
  assert(camera.previewRenderState().contentRevision > 0U);
  assert(camera.previewActive());
  assert(camera.previewSurfaceGeneration() > 0U);
  const auto previewPresentsBeforeZoomedStroke =
      camera.previewSurfaceProvider()->presentCount();
  const auto previewViewport = camera.previewViewport();
  assert(previewViewport.scale == camera.viewportGesture().scale);
  assert(previewViewport.translationX == camera.viewportGesture().translationX);
  assert(previewViewport.translationY == camera.viewportGesture().translationY);
  assert(camera.presentBrushPreview());
  assert(camera.previewSurfaceProvider()->presentCount() ==
         previewPresentsBeforeZoomedStroke + 1U);

  // AutoIntent contract: a first stroke that has already activated ink lets
  // a later contact join as an independent stroke; two contacts inside the
  // chord window claim viewport instead.
  canvas::ink_playground::InkPlaygroundHost multi;
  assert(multi.bindSurface(256, 256));
  auto platform = [](std::uint64_t pointer, std::uint64_t sequence,
                     std::uint64_t timestamp, float x, float y,
                     canvas::input::PointerPhase phase) {
    return canvas::input::PlatformPointerSample{
        7U, pointer, sequence, timestamp, x, y, 0.5F, 0.0F, 0.0F, {}, {},
        canvas::input::SampleProvenance::kConfirmedCurrent, phase};
  };
  canvas::input::PlatformPointerBatch firstDown;
  firstDown.samples.push_back(platform(31U, 1U, 1'000'000U, 10.0F, 10.0F,
                                       canvas::input::PointerPhase::kDown));
  assert(multi.acceptPlatformBatch(firstDown, 1'000'000U));
  canvas::input::PlatformPointerBatch firstMove;
  firstMove.samples.push_back(platform(31U, 2U, 300'000'000U, 30.0F, 10.0F,
                                       canvas::input::PointerPhase::kMove));
  assert(multi.acceptPlatformBatch(firstMove, 300'000'000U));
  canvas::input::PlatformPointerBatch secondDown;
  secondDown.samples.push_back(platform(32U, 3U, 500'000'000U, 50.0F, 10.0F,
                                         canvas::input::PointerPhase::kDown));
  assert(multi.acceptPlatformBatch(secondDown, 500'000'000U));
  assert(!multi.viewportGestureClaimed());
  canvas::input::PlatformPointerBatch secondMove;
  secondMove.samples.push_back(platform(32U, 4U, 510'000'000U, 70.0F, 10.0F,
                                         canvas::input::PointerPhase::kMove));
  assert(multi.acceptPlatformBatch(secondMove, 510'000'000U));
  canvas::input::PlatformPointerBatch firstUp;
  firstUp.samples.push_back(platform(31U, 5U, 520'000'000U, 30.0F, 10.0F,
                                       canvas::input::PointerPhase::kUp));
  assert(multi.acceptPlatformBatch(firstUp, 520'000'000U));
  canvas::input::PlatformPointerBatch secondUp;
  secondUp.samples.push_back(platform(32U, 6U, 530'000'000U, 70.0F, 10.0F,
                                       canvas::input::PointerPhase::kUp));
  assert(multi.acceptPlatformBatch(secondUp, 530'000'000U));
  assert(multi.semanticObjectCount() == 2U);

  canvas::ink_playground::InkPlaygroundHost pinchAuto;
  assert(pinchAuto.bindSurface(256, 256));
  canvas::input::PlatformPointerBatch pinchDownA;
  pinchDownA.samples.push_back(platform(41U, 1U, 1'000'000U, 10.0F, 10.0F,
                                         canvas::input::PointerPhase::kDown));
  assert(pinchAuto.acceptPlatformBatch(pinchDownA, 1'000'000U));
  canvas::input::PlatformPointerBatch pinchDownB;
  pinchDownB.samples.push_back(platform(42U, 2U, 100'000'000U, 50.0F, 10.0F,
                                         canvas::input::PointerPhase::kDown));
  assert(pinchAuto.acceptPlatformBatch(pinchDownB, 100'000'000U));
  assert(pinchAuto.viewportGestureClaimed());

  // Two ink contacts must retain two independent preview contours. Finishing
  // either contact before the canonical frame is presented must not erase the
  // other contact's amber preview.
  canvas::ink_playground::InkPlaygroundHost concurrent;
  assert(concurrent.bindSurface(256, 256));
  assert(concurrent.beginBrushSession(101U, 1U));
  assert(concurrent.appendBrushSample(101U, 12.0, 12.0, 0.5, 1U, false, false));
  assert(concurrent.beginBrushSession(102U, 1U));
  assert(concurrent.appendBrushSample(102U, 80.0, 80.0, 0.5, 2U, false, false));
  assert(concurrent.previewRenderState().dirty);
  assert(concurrent.brushPreviewOutlines().size() == 2U);
  assert(concurrent.presentBrushPreview());
  assert(concurrent.previewActive());
  assert(concurrent.finishBrushSession(101U));
  assert(concurrent.previewActive());
  assert(concurrent.finishBrushSession(102U));
  assert(concurrent.previewActive());
  assert(concurrent.presentCanonicalFrame(1U, 0.0));
  assert(!concurrent.previewActive());

  // A stroke may live entirely outside the physical 0..surfaceWidth range
  // while still being visible in the camera's world viewport.  Canonical
  // visibility must use that world-space viewport rather than the legacy
  // screen-space rectangle; otherwise only strokes crossing the board centre
  // survive the Scene query.
  canvas::ink_playground::InkPlaygroundHost worldViewport;
  assert(worldViewport.bindSurface(256, 256));
  assert(worldViewport.beginBrushSession(112U, 1U));
  assert(worldViewport.appendBrushSample(112U, 320.0, 320.0, 0.5, 1U));
  assert(worldViewport.appendBrushSample(112U, 360.0, 340.0, 0.5, 2U));
  assert(worldViewport.finishBrushSession(112U));
  assert(worldViewport.applyViewportNavigation({
      canvas::interaction::ViewportNavigationKind::kWheelPan,
      192.0F, 192.0F, 0.0F, 0.0F}));
  assert(worldViewport.canonicalFrameCount() == 1U);
  return 0;
}
