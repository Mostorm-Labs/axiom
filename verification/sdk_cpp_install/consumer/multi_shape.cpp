#include <canvas/runtime/runtime_composition.hpp>

#include <algorithm>
#include <cstdio>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (false)

int main() {
  using namespace canvas;
  runtime::RuntimeComposition app;
  CHECK(app.open() && app.bindSurface(800, 600));
  CHECK(app.registerSource(9100));
  auto& facade = app.controlFacade();
  const auto beforeInvalid = facade.readRuntimeState().control.semanticGeneration;
  input::PlatformPointerBatch invalid;
  input::PlatformPointerSample bad{};
  bad.source = 9100; bad.pointer = 1; bad.sequence = 1; bad.timestampNs = 1000000;
  bad.phase = input::PointerPhase::kDown; bad.x = 10; bad.y = 10;
  invalid.samples.push_back(bad);
  bad.source = 9999;
  invalid.samples.push_back(bad);
  CHECK(!app.submit(invalid));
  CHECK(facade.readRuntimeState().control.semanticGeneration == beforeInvalid);
  auto targets = facade.mountedCanvasTargets();
  CHECK(targets.size() == 1);
  runtime::CanvasControlRequest control{};
  control.clientId = 9101; control.requestId = 1; control.target = targets.front();
  control.payload.kind = runtime::CanvasControlPayloadKind::kSelectCreationPreset;
  control.payload.creationPreset = runtime::CanvasCreationPreset::kRectangle;
  CHECK(facade.submitCanvasControl(control).state == runtime::ProductControlState::kQueued);
  CHECK(app.ownerPump());
  std::uint64_t sequence = 0;
  const auto pointer = [&](input::PointerPhase phase, float x, float y, bool shift = false) {
    input::PlatformPointerSample sample{};
    sample.source = 9100; sample.pointer = 1; sample.sequence = ++sequence;
    sample.timestampNs = sequence * 1000000; sample.phase = phase;
    sample.x = x; sample.y = y;
    sample.modifiers = shift ? input::PointerModifiers::kShift : input::PointerModifiers::kNone;
    input::PlatformPointerBatch batch; batch.samples.push_back(sample);
    return app.submit(batch);
  };
  for (const float x : {100.0F, 260.0F}) {
    CHECK(pointer(input::PointerPhase::kDown, x, 100));
    CHECK(pointer(input::PointerPhase::kMove, x + 80, 160));
    CHECK(pointer(input::PointerPhase::kUp, x + 80, 160));
  }
  CHECK(facade.setSelectionMode(true, 2, facade.readRuntimeState().identity.runtimeGeneration, 9101).state == runtime::ProductControlState::kQueued);
  CHECK(app.ownerPump());
  CHECK(facade.setMultiSelectionPermission(runtime::MultiSelectionPermissionMode::kLocalSingleUser));
  semantic::SolidStroke first{{1, 0, 0, 1}, 5, semantic::StrokeCap::kRound,
                              semantic::MiterJoin{4}, semantic::DashPattern{{2, 1}, 0}};
  semantic::SolidStroke second{{0, 0, 1, 1}, 7, semantic::StrokeCap::kSquare,
                               semantic::MiterJoin{7}, semantic::DashPattern{{4, 2}, 0}};
  foundation::ObjectId ids[2];
  std::uint64_t request = 0;
  for (int n = 0; n < 2; ++n) {
    const auto x = n == 0 ? 140.0F : 300.0F;
    CHECK(pointer(input::PointerPhase::kDown, x, 130));
    CHECK(pointer(input::PointerPhase::kUp, x, 130));
    const auto selection = facade.readSelectionSummary();
    CHECK(selection.count == 1);
    ids[n] = selection.primary;
    for (const std::uint32_t field : {0x100U, 0x101U}) {
      const auto projection = facade.readMultiSelection();
      CHECK(projection.available);
      const semantic::PropertyValue value = field == 0x100U
          ? semantic::PropertyValue{semantic::FillStyleValue{semantic::SolidFill{{0, 0, 1, 1}}}}
          : semantic::PropertyValue{semantic::StrokeStyleValue{n == 0 ? first : second}};
      const auto serial = ++request;
      CHECK(facade.submitMultiSelectionProperty(field, value, projection.stamp, projection.properties.at(field).eligible, 9102, serial).queued);
      CHECK(app.ownerPump());
      const auto receipt = facade.multiSelectionReceipt(9102, serial);
      CHECK(receipt && receipt->terminal && receipt->applied);
    }
  }
  CHECK(ids[0] != ids[1] && !ids[0].isZero() && !ids[1].isZero());
  CHECK(pointer(input::PointerPhase::kDown, 140, 130));
  CHECK(pointer(input::PointerPhase::kUp, 140, 130));
  CHECK(pointer(input::PointerPhase::kDown, 300, 130, true));
  CHECK(pointer(input::PointerPhase::kUp, 300, 130, true));
  const auto verify = [&](const semantic::SolidStroke& a, const semantic::SolidStroke& b) {
    const auto projection = facade.readMultiSelection();
    if (!projection.available || projection.roots.size() != 2) return false;
    for (const auto field : {0x100U, 0x101U}) {
      const auto values = facade.readMultiSelectionPropertyValues(field, projection.stamp, projection.properties.at(field).eligible);
      if (!values || values->size() != 2 || (*values)[0].object == (*values)[1].object) return false;
      for (const auto& item : *values) {
        const auto* expected = item.object == ids[0] ? &a : item.object == ids[1] ? &b : nullptr;
        if (!expected) return false;
        const semantic::PropertyValue value = field == 0x100U
            ? semantic::PropertyValue{semantic::FillStyleValue{semantic::SolidFill{{0, 0, 1, 1}}}}
            : semantic::PropertyValue{semantic::StrokeStyleValue{*expected}};
        if (item.value != value) return false;
      }
    }
    return true;
  };
  CHECK(verify(first, second));
  const auto projection = facade.readMultiSelection();
  const auto serial = ++request;
  CHECK(facade.submitMultiSelectionStrokeWidth(19, projection.stamp, projection.properties.at(0x101U).eligible, 9102, serial).queued);
  CHECK(app.ownerPump());
  const auto receipt = facade.multiSelectionReceipt(9102, serial);
  CHECK(receipt && receipt->terminal && receipt->applied && receipt->changed.size() == 2);
  CHECK(std::find(receipt->changed.begin(), receipt->changed.end(), ids[0]) != receipt->changed.end());
  CHECK(std::find(receipt->changed.begin(), receipt->changed.end(), ids[1]) != receipt->changed.end());
  auto afterFirst = first; afterFirst.width = 19;
  auto afterSecond = second; afterSecond.width = 19;
  CHECK(verify(afterFirst, afterSecond));
  const auto generation = facade.readRuntimeState().identity.runtimeGeneration;
  CHECK(facade.undo(1, generation, 9103).state == runtime::ProductControlState::kQueued);
  CHECK(app.ownerPump() && verify(first, second));
  CHECK(facade.redo(2, generation, 9103).state == runtime::ProductControlState::kQueued);
  CHECK(app.ownerPump() && verify(afterFirst, afterSecond));
  ++sequence;
  CHECK(app.unregisterSource(9100, sequence, sequence * 1000000));
  CHECK(!pointer(input::PointerPhase::kDown, 20, 20));
  CHECK(verify(afterFirst, afterSecond));
  CHECK(app.close());
  std::puts("PASS: external Shape mixed-stroke widths and Undo/Redo");
}
