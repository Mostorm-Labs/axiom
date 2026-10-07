#include "ink_playground_host.hpp"
#include "ink_playground_history_test_access.hpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <cmath>

using Host = canvas::ink_playground::InkPlaygroundHost;
using Access = canvas::ink_playground::InkPlaygroundHistoryTestAccess;
using Phase = canvas::input::PointerPhase;

void pointer(Host& host, std::uint64_t sequence, float x, float y, Phase phase,
             std::uint64_t id = 77U) {
  canvas::input::PlatformPointerBatch batch;
  batch.samples.push_back({17U, id, sequence, sequence * 1000000U, x, y, 0.5F,
      0.0F, 0.0F, {}, {}, canvas::input::SampleProvenance::kConfirmedCurrent, phase});
  assert(host.acceptPlatformBatch(batch, sequence * 1000000U));
}
std::vector<std::uint8_t> pixels(Host& host) {
  assert(host.presentCanonicalFrame(host.canonicalFrameCount()+1U, 0.0, false));
  std::vector<std::uint8_t> out(256U*256U*4U);
  assert(host.activeSurfaceProvider()->readbackRgba(out).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  return out;
}
void prepare(Host& host) {
  assert(host.bindSurface(256,256));
  assert(host.beginBrushSession(700U));
  assert(host.appendBrushSample(700U,32,64,0.5,1U));
  assert(host.appendBrushSample(700U,160,64,0.5,2U));
  assert(host.finishBrushSession(700U));
  assert(host.setSelectionMode(true));
  assert(host.selectAtViewPoint(80,64));
  assert(host.selectedObjectCount()==1U);
}
void resizeHistory() {
  Host host; prepare(host);
  const auto original=Access::objects(host);
  const auto runtimeRecords=Access::runtimeRecords(host);
  const auto generation=host.semanticGeneration();
  const auto operations=host.submittedOperationCount();
  const auto before=pixels(host);
  const auto handle=host.selectionOverlay()->handle(canvas::render::HandleKind::kRight).center;
  pointer(host,1,handle.x,handle.y,Phase::kDown);
  pointer(host,2,handle.x+40,handle.y,Phase::kMove);
  const auto preview=pixels(host);
  assert(before != preview && "handle movement must change real preview pixels");
  assert(Access::objects(host)==original);
  assert(Access::runtimeRecords(host)==runtimeRecords);
  assert(host.semanticGeneration()==generation);
  assert(host.submittedOperationCount()==operations);
  pointer(host,3,handle.x+40,handle.y,Phase::kUp);
  assert(host.submittedOperationCount()==operations+1U);
  const auto committed=Access::objects(host);
  assert(committed.front().transform != original.front().transform);
  assert(pixels(host)==preview);
  assert(host.undo());
  assert(Access::objects(host)==original);
  assert(pixels(host)==before);
  assert(host.redo());
  assert(Access::objects(host)==committed);
  std::cout << "PASS: real selection-pointer resize preview/commit/history\n";
}

void lifecycle() {
  for (int action=0; action<8; ++action) {
    Host host; prepare(host);
    const auto original=Access::objects(host);
    const auto operations=host.submittedOperationCount();
    const auto before=pixels(host);
    const auto p=host.selectionOverlay()->handle(canvas::render::HandleKind::kRight).center;
    pointer(host,1,p.x,p.y,Phase::kDown);
    pointer(host,2,p.x+40,p.y,Phase::kMove);
    assert(host.selectionTransformActive() && !host.canUndo() && !host.undo());
    switch(action) {
      case 0: pointer(host,3,p.x+40,p.y,Phase::kCancel); break;
      case 1: assert(host.setSelectionMode(false)); assert(host.setSelectionMode(true));
              assert(host.selectAtViewPoint(80,64)); break;
      case 2: assert(host.resizeSurface(256,256)); break;
      case 3: assert(host.loseSurface()); assert(host.rebindSurface()); break;
      case 4: assert(!host.selectionPointer(77U,Phase::kMove,
          std::numeric_limits<float>::quiet_NaN(),p.y)); break;
      case 5: host.cancelAllPointers(); break;
      case 6: {
        auto& port=static_cast<canvas::interaction::OperationSubmitPort&>(host);
        assert(port.submit(canvas::interaction::OperationRequest{555U}).accepted);
        assert(!host.selectionPointer(77U,Phase::kMove,p.x+40,p.y));
        break;
      }
      case 7: assert(host.selectCanonicalSurfaceProfile("cpu-raster",
          host.surface().generation,canvas::render::RenderTargetFormat::kRgba8888)); break;
    }
    assert(!host.selectionTransformActive() && host.transientTransformCount()==0U);
    assert(host.submittedOperationCount()==operations+(action==6?1U:0U) && Access::objects(host)==original);
    const auto after = pixels(host);
    if (after != before) std::cerr << "lifecycle pixel mismatch action=" << action << "\n";
    assert(after==before);
  }
  std::cout << "PASS: cancel/tool/resize/loss/invalid/focus leave zero mutations\n";
}

void noOpSecondaryAndReject() {
  Host host; prepare(host);
  const auto operations=host.submittedOperationCount();
  const auto before=pixels(host);
  const auto p=host.selectionOverlay()->handle(canvas::render::HandleKind::kRight).center;
  pointer(host,1,p.x,p.y,Phase::kDown);
  pointer(host,2,p.x+30,p.y,Phase::kMove);
  pointer(host,3,p.x,p.y,Phase::kDown,88U);
  pointer(host,4,p.x+100,p.y,Phase::kMove,88U);
  pointer(host,5,p.x+100,p.y,Phase::kUp,88U);
  assert(host.selectionTransformActive() && host.submittedOperationCount()==operations);
  pointer(host,6,p.x,p.y,Phase::kUp);
  assert(host.submittedOperationCount()==operations && pixels(host)==before);
  pointer(host,7,p.x,p.y,Phase::kDown);
  pointer(host,8,p.x+30,p.y,Phase::kMove);
  Access::exhaustOperationIds(host);
  pointer(host,9,p.x+30,p.y,Phase::kUp);
  assert(!host.selectionTransformActive() && host.transientTransformCount()==0U);
  assert(host.submittedOperationCount()==operations && pixels(host)==before);
  std::cout << "PASS: secondary cannot steal; no-op/rejected release mutate zero\n";
}

void zoomPanRepeatedHandles() {
  Host host; prepare(host);
  assert(host.applyViewportNavigation({canvas::interaction::ViewportNavigationKind::kBrowserGesture,
      0,0,0,0,1.5F}));
  assert(host.applyViewportNavigation({canvas::interaction::ViewportNavigationKind::kWheelPan,
      0,0,8,8,1.0F}));
  pixels(host);
  std::uint64_t sequence=10;
  using H=canvas::render::HandleKind;
  for (const auto h : {H::kRight,H::kBottomRight,H::kTop,H::kLeft,H::kRotation,H::kNone}) {
    const auto original=Access::objects(host);
    const auto before=pixels(host);
    auto p=h==H::kNone ? canvas::render::ScreenPoint{120,96}
                       : host.selectionOverlay()->handle(h).center;
    if(h==H::kNone) {
      const auto corners=host.selectionOverlay()->selectionOutline().corners;
      // A slim stroke's center can overlap its top/bottom handle hit slop.
      // Use an interior point away from all handles, on the actual stroke.
      p={corners[0].x+(corners[1].x-corners[0].x)/3.0F+
             (corners[3].x-corners[0].x)*0.5F,
         corners[0].y+(corners[1].y-corners[0].y)/3.0F+
             (corners[3].y-corners[0].y)*0.5F};
      assert(host.selectionOverlay()->hitTest(p)==H::kNone);
    }
    pointer(host,sequence++,p.x,p.y,Phase::kDown);
    pointer(host,sequence++,p.x+15,p.y+10,Phase::kMove);
    const auto preview=pixels(host);
    assert(preview!=before && Access::objects(host)==original);
    pointer(host,sequence++,p.x+15,p.y+10,Phase::kUp);
    if (h==H::kNone) {
      const auto transformed=Access::objects(host).front().transform;
      const auto initial=original.front().transform;
      assert(transformed.a==initial.a && transformed.b==initial.b &&
             transformed.c==initial.c && transformed.d==initial.d);
      // View delta (15,10) divided by zoom 1.5 is world delta (10,6 2/3).
      assert(std::abs(transformed.tx-initial.tx-10.0)<1e-4);
      assert(std::abs(transformed.ty-initial.ty-10.0/1.5)<1e-4);
    }
    assert(pixels(host)==preview);
    assert(host.undo() && Access::objects(host)==original && pixels(host)==before);
    assert(host.redo() && pixels(host)==preview);
    const auto stableOperations=host.submittedOperationCount();
    const auto stable=Access::objects(host);
    const auto current=host.selectionOverlay()->handle(H::kRotation).center;
    pointer(host,sequence++,current.x,current.y,Phase::kDown);
    pointer(host,sequence++,current.x,current.y,Phase::kUp);
    assert(host.submittedOperationCount()==stableOperations && Access::objects(host)==stable);
  }
  std::cout << "PASS: viewport zoom/pan, repeated side/corner/rotate/move and exact history pixels\n";
}

void failedPublication() {
  Host host; prepare(host);
  const auto operations=host.submittedOperationCount();
  const auto p=host.selectionOverlay()->handle(canvas::render::HandleKind::kRight).center;
  pointer(host,1,p.x,p.y,Phase::kDown);
  pointer(host,2,p.x+40,p.y,Phase::kMove);
  Access::failPublication(host);
  pointer(host,3,p.x+40,p.y,Phase::kUp);
  assert(host.submittedOperationCount()==operations+1U);
  assert(!host.selectionTransformActive() && !host.canUndo());
  Access::clearFailure(host);
  pixels(host);
  assert(host.canUndo());
  assert(host.undo());
  std::cout << "PASS: accepted commit remains in history when derived publication fails\n";
}

void partialEraseAfterMove() {
  Host host; prepare(host);
  // This is inside the selected stroke, away from the handle hit slop.
  pointer(host,1,80,64,Phase::kDown);
  pointer(host,2,120,96,Phase::kMove);
  pointer(host,3,120,96,Phase::kUp);
  const auto moved=Access::objects(host);
  assert(moved.front().transform.tx==40 && moved.front().transform.ty==32);
  assert(host.setSelectionMode(false));
  const auto before=pixels(host);
  assert(before[(96U*256U+120U)*4U+3U]==255U);
  assert(host.selectTool(Host::ToolMode::kPartialEraser));
  assert(host.eraserBegin(88U));
  assert(host.eraserSample(88U,120,96));
  assert(host.eraserFinish(88U));
  const auto erased=Access::objects(host);
  assert(erased.front().content==moved.front().content);
  assert(erased.front().transform==moved.front().transform);
  assert(erased.front().erase_masks.size()==1U);
  const auto& mask=std::get<canvas::semantic::SweptCircleMask>(
      erased.front().erase_masks.front().geometry);
  // World (120,96), minus the committed object translation (40,32).
  assert(mask.segments.front().p0.position==canvas::semantic::Vec2(80,64));
  assert(mask.segments.front().p0.radius==18.0);
  const auto after=pixels(host);
  assert(after[(96U*256U+120U)*4U+3U]==0U);
  assert(after[(96U*256U+80U)*4U+3U]==255U);
  assert(host.undo() && Access::objects(host)==moved && pixels(host)==before);
  assert(host.redo() && Access::objects(host)==erased && pixels(host)==after);
  std::cout << "PASS: moved Vector partial erase uses local mask coordinates and exact history pixels\n";
}

void partialEraseAffineFootprint() {
  using T=canvas::semantic::Transform2D;
  // Every literal transform maps local (80,64) to world (120,96).
  struct Fixture { T transform; bool swept; double radius; };
  const Fixture fixtures[]={{{2,0,0,2,-40,-32},true,9},
      {{2,0,0,1,-40,32},false,0},{{1,0,0,2,40,-32},false,0},
      {{0,1,-1,0,184,16},true,18},{{1,0,0.5,1,8,32},false,0},
      {{-1,0,0,1,200,32},true,18}};
  for (const auto& fixture:fixtures) {
    const auto& transform=fixture.transform;
    Host host; prepare(host);
    assert(host.selectionPointer(90U,Phase::kDown,80,64));
    auto& port=static_cast<canvas::interaction::TransformSubmitPort&>(host);
    assert(port.submit({{{host.selectedPrimaryObject(),transform}}}).accepted);
    host.cancelSelectionTransform();
    assert(host.setSelectionMode(false));
    const auto before=pixels(host);
    assert(before[(96U*256U+120U)*4U+3U]!=0U);
    assert(host.selectTool(Host::ToolMode::kPartialEraser));
    assert(host.eraserBegin(89U));
    assert(host.eraserSample(89U,108,96));
    assert(host.eraserSample(89U,132,96));
    assert(host.eraserFinish(89U));
    const auto erased=pixels(host);
    assert(erased[(96U*256U+120U)*4U+3U]==0U);
    // A radius-18 world capsule from (108,96) to (132,96) must
    // retain its footprint even when inverse mapping requires an ellipse.
    for (const auto p: {canvas::semantic::Vec2{120,112},{120,80},
                       {92,96},{148,96}}) {
      assert(erased[(static_cast<std::size_t>(p.y)*256U+
                     static_cast<std::size_t>(p.x))*4U+3U]==0U);
    }
    for (const auto p: {canvas::semantic::Vec2{120,120},{120,72},
                       {84,96},{156,96}}) {
      assert(erased[(static_cast<std::size_t>(p.y)*256U+
                     static_cast<std::size_t>(p.x))*4U+3U]==255U);
    }
    const auto records=Access::objects(host);
    const auto& mask=records.front().erase_masks.front();
    assert(fixture.swept == std::holds_alternative<canvas::semantic::SweptCircleMask>(mask.geometry));
    if (fixture.swept) {
      assert(std::get<canvas::semantic::SweptCircleMask>(mask.geometry)
          .segments.front().p0.radius==fixture.radius);
    } else {
      const auto& filled=std::get<canvas::semantic::FilledPathMask>(mask.geometry);
      assert(!filled.path.commands.empty());
    }
    assert(host.undo() && pixels(host)==before);
    assert(host.redo() && pixels(host)==erased);
  }
  std::cout << "PASS: partial erase world footprint after scale/rotate/shear/reflection\n";
}

int main() { resizeHistory(); lifecycle(); noOpSecondaryAndReject(); zoomPanRepeatedHandles(); failedPublication(); partialEraseAfterMove(); partialEraseAffineFootprint(); }
