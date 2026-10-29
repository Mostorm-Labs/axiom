#include <canvas/runtime/runtime.hpp>

#include <algorithm>
#include <cstdio>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (false)

int main() {
  using namespace canvas;
  const auto id = [](std::uint64_t n) { return foundation::ObjectId::fromUint64(n); };
  runtime::Runtime app;
  CHECK(app.open() && app.bindSurface(800, 600));
  auto& facade = app.facade();
  CHECK(facade.setMultiSelectionPermission(runtime::MultiSelectionPermissionMode::kLocalSingleUser));
  std::uint64_t request = 0;
  const auto sectionCommand = [&](runtime::SectionIntent intent) {
    const auto serial = ++request;
    if (facade.submitSectionIntent(intent, serial, 9000).state != runtime::ProductControlState::kQueued || !app.pump()) return false;
    const auto receipt = facade.canvasControlReceipt({9000, serial});
    return receipt && receipt->state == runtime::CanvasControlReceiptState::kApplied;
  };
  semantic::SolidStroke first{{1, 0, 0, 1}, 5, semantic::StrokeCap::kRound,
                              semantic::MiterJoin{4}, semantic::DashPattern{{2, 1}, 0}};
  semantic::SolidStroke second{{0, 0, 1, 1}, 7, semantic::StrokeCap::kSquare,
                               semantic::MiterJoin{7}, semantic::DashPattern{{4, 2}, 0}};
  for (std::uint64_t n = 1; n <= 2; ++n) {
    runtime::SectionIntent section; section.objectId = id(100 + n);
    CHECK(sectionCommand(section));
    runtime::StickyCreateRequest sticky; sticky.objectId = id(n);
    sticky.placement.parent_id = section.objectId;
    sticky.placement.order_key = semantic::OrderKey({1});
    CHECK(facade.createSticky(sticky).state == runtime::ConnectorCommandState::kApplied);
    section.action = runtime::SectionAction::kSelectChildren;
    CHECK(sectionCommand(section));
    const auto projection = facade.readMultiSelection();
    CHECK(projection.available);
    const auto& property = projection.properties.at(0x101U);
    CHECK(property.eligible.size() == 1);
    const semantic::PropertyValue value = semantic::StrokeStyleValue{n == 1 ? first : second};
    const auto serial = ++request;
    CHECK(facade.submitMultiSelectionProperty(0x101U, value, projection.stamp, property.eligible, 9000, serial).queued);
    CHECK(app.pump());
    const auto outcome = facade.multiSelectionReceipt(9000, serial);
    CHECK(outcome && outcome->terminal && outcome->applied);
  }
  runtime::SectionIntent relocate; relocate.action = runtime::SectionAction::kReparent;
  relocate.objects = {id(2)}; relocate.parentId = id(101);
  CHECK(sectionCommand(relocate));
  runtime::SectionIntent select; select.action = runtime::SectionAction::kSelectChildren; select.objectId = id(101);
  CHECK(sectionCommand(select));
  const auto verify = [&](const semantic::SolidStroke& a, const semantic::SolidStroke& b) {
    const auto projection = facade.readMultiSelection();
    if (!projection.available) return false;
    const auto property = projection.properties.find(0x101U);
    if (property == projection.properties.end() || property->second.eligible.size() != 2) return false;
    const auto values = facade.readMultiSelectionPropertyValues(0x101U, projection.stamp, property->second.eligible);
    if (!values || values->size() != 2) return false;
    if ((*values)[0].object == (*values)[1].object) return false;
    for (const auto& item : *values) {
      const auto* expected = item.object == id(1) ? &a : item.object == id(2) ? &b : nullptr;
      if (!expected || item.value != semantic::PropertyValue{semantic::StrokeStyleValue{*expected}}) return false;
    }
    return true;
  };
  CHECK(verify(first, second));
  auto projection = facade.readMultiSelection();
  const auto serial = ++request;
  CHECK(facade.submitMultiSelectionStrokeWidth(19, projection.stamp, projection.properties.at(0x101U).eligible, 9000, serial).queued);
  CHECK(app.pump());
  const auto receipt = facade.multiSelectionReceipt(9000, serial);
  CHECK(receipt && receipt->terminal && receipt->applied && receipt->changed.size() == 2);
  CHECK(std::find(receipt->changed.begin(), receipt->changed.end(), id(1)) != receipt->changed.end());
  CHECK(std::find(receipt->changed.begin(), receipt->changed.end(), id(2)) != receipt->changed.end());
  auto updatedFirst = first; updatedFirst.width = 19;
  auto updatedSecond = second; updatedSecond.width = 19;
  CHECK(verify(updatedFirst, updatedSecond));
  auto generation = facade.readRuntimeState().identity.runtimeGeneration;
  CHECK(facade.undo(++request, generation, 9000).state == runtime::ProductControlState::kQueued);
  CHECK(app.pump() && verify(first, second));
  CHECK(facade.redo(++request, generation, 9000).state == runtime::ProductControlState::kQueued);
  CHECK(app.pump() && verify(updatedFirst, updatedSecond));
  CHECK(app.close());
  std::puts("PASS: external per-object stroke preservation and Undo/Redo");
}
