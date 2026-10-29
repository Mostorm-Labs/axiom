#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/runtime/internal/multi_selection_port.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#define CHECK(condition) do { \
  if (!(condition)) { \
    std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
    return 1; \
  } \
} while (0)

int main(int argc, char** argv) {
  using namespace canvas;
  const bool expectPreserved = argc == 2 && std::strcmp(argv[1], "--expect-preserved") == 0;
  const bool observeReplacement = argc == 2 && std::strcmp(argv[1], "--observe-replacement") == 0;
  CHECK(expectPreserved || observeReplacement);
  const auto id = [](std::uint64_t value) { return foundation::ObjectId::fromUint64(value); };
  runtime::RuntimeCompositionCore composition;
  CHECK(composition.open());
  CHECK(composition.bindSurface(256U, 256U));
  CHECK(composition.multiSelectionPort().setPermissionMode(runtime::MultiSelectionPermissionMode::kLocalSingleUser));

  semantic::InsertObjectsOp insert;
  for (std::uint64_t n = 1; n <= 2; ++n) {
    semantic::ObjectRecord object{};
    object.id = id(n);
    object.kind = semantic::ObjectKind::kShape;
    object.kind_version = 1;
    object.content = semantic::ShapeContent{1, 20, 20};
    object.placement.order_key = semantic::OrderKey({static_cast<std::uint8_t>(n)});
    object.transform.tx = n == 1 ? 10 : 60;
    object.transform.ty = n == 1 ? 20 : 60;
    semantic::SolidStroke stroke{};
    stroke.color = n == 1 ? semantic::ColorValue{1, 0, 0, 1} : semantic::ColorValue{0, 0, 1, 1};
    stroke.width = 2.0;
    stroke.cap = semantic::StrokeCap::kRound;
    stroke.join = semantic::RoundJoin{};
    stroke.dash = semantic::SolidDash{};
    object.properties.entries.push_back({0x101U, semantic::PropertyValue{semantic::StrokeStyleValue{stroke}}});
    insert.objects.push_back(std::move(object));
  }
  const semantic::Operation seed{semantic::OperationId{id(100)}, semantic::DocumentId{id(1)},
                                 1, 1, std::move(insert)};
  CHECK(composition.applyExternalOperation(seed, semantic::ApplySource::kRemoteSync).disposition ==
        semantic::ApplyDisposition::kApplied);
  CHECK(composition.registerSource(7100U));
  auto& facade = composition.controlFacade();
  CHECK(facade.setSelectionMode(true, 1U, facade.readRuntimeState().identity.runtimeGeneration,
                                7100U).state == runtime::ProductControlState::kQueued);
  CHECK(composition.ownerPump());
  std::uint64_t sequence = 0;
  const auto send = [&](input::PointerPhase phase, float x, float y, bool shift = false) {
    const auto snapshot = facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front());
    input::PlatformPointerSample sample{};
    sample.source = 7100U;
    sample.pointer = 1U;
    sample.sequence = ++sequence;
    sample.timestampNs = sequence * 1000000U;
    sample.phase = phase;
    sample.x = x * snapshot.cameraScale + snapshot.cameraTranslationX;
    sample.y = y * snapshot.cameraScale + snapshot.cameraTranslationY;
    sample.modifiers = shift ? input::PointerModifiers::kShift : input::PointerModifiers::kNone;
    input::PlatformPointerBatch batch;
    batch.samples.push_back(sample);
    return composition.submit(batch);
  };
  CHECK(send(input::PointerPhase::kDown, 15, 25));
  CHECK(send(input::PointerPhase::kUp, 15, 25));
  std::printf("selection_after_first=%zu\n", facade.readSelectionSummary().count);
  CHECK(send(input::PointerPhase::kDown, 65, 65, true));
  CHECK(send(input::PointerPhase::kUp, 65, 65, true));
  std::printf("selection_after_second=%zu\n", facade.readSelectionSummary().count);
  CHECK(facade.readSelectionSummary().count == 2U);
  auto& port = composition.multiSelectionPort();
  const auto projection = port.read();
  CHECK(projection.available);
  const auto& property = projection.properties.at(0x101U);
  CHECK(property.mixed && property.eligible.size() == 2U);

  semantic::SolidStroke wholeField{};
  wholeField.color = {1, 0, 0, 1};
  wholeField.width = 19.0;
  wholeField.cap = semantic::StrokeCap::kRound;
  wholeField.join = semantic::RoundJoin{};
  wholeField.dash = semantic::SolidDash{};
  const semantic::PropertyValue value{semantic::StrokeStyleValue{wholeField}};
  CHECK(port.submitProperty(0x101U, value, projection.stamp, property.eligible, 7200U, 1U).queued);
  CHECK(composition.ownerPump());
  const auto outcome = port.receipt(7200U, 1U);
  CHECK(outcome && outcome->applied && outcome->changed.size() == 2U);
  const auto* second = composition.qualificationRuntime().qualificationObjectRecord(id(2));
  CHECK(second != nullptr);
  const auto entry = std::find_if(second->properties.entries.begin(), second->properties.entries.end(),
                                  [](const auto& item) { return item.field_id == 0x101U; });
  CHECK(entry != second->properties.entries.end());
  const auto* style = std::get_if<semantic::StrokeStyleValue>(&entry->value);
  CHECK(style != nullptr);
  const auto* stroke = std::get_if<semantic::SolidStroke>(style);
  CHECK(stroke != nullptr);
  std::printf("second_stroke rgba=(%.1f,%.1f,%.1f,%.1f) width=%.1f\n",
              stroke->color.r, stroke->color.g, stroke->color.b, stroke->color.a, stroke->width);
  CHECK(stroke->width == 19.0);
  if (expectPreserved) {
    CHECK(stroke->color.b == 1.0F && stroke->color.r == 0.0F);
  } else {
    CHECK(stroke->color.r == 1.0F && stroke->color.b == 0.0F);
  }
  CHECK(composition.close());
  return 0;
}
