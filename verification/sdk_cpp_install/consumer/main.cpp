#include <canvas/runtime/runtime_composition.hpp>

#include <cstdio>
#include <filesystem>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (false)

int main() {
  using namespace canvas;
  CHECK(std::filesystem::exists("icudtl.dat"));
  runtime::RuntimeComposition app;
  CHECK(app.open());
  CHECK(!app.open());
  CHECK(app.bindSurface(256, 256));
  runtime::RuntimeComposition second;
  CHECK(second.open());
  CHECK(second.bindSurface(128, 128));
  auto& facade = app.controlFacade();
  CHECK(&facade != &second.controlFacade());
  runtime::SectionIntent section;
  section.objectId = foundation::ObjectId::fromUint64(5100);
  CHECK(facade.submitSectionIntent(section, 1, 8800).state == runtime::ProductControlState::kQueued);
  CHECK(app.ownerPump());
  const auto receipt = facade.canvasControlReceipt({8800, 1});
  CHECK(receipt && receipt->state == runtime::CanvasControlReceiptState::kApplied);
  CHECK(facade.readSection(section.objectId).available);
  CHECK(!second.controlFacade().readSection(section.objectId).available);
  const auto generation = facade.readRuntimeState().identity.runtimeGeneration;
  CHECK(facade.undo(2, generation, 8800).state == runtime::ProductControlState::kQueued);
  CHECK(app.ownerPump());
  CHECK(!facade.readSection(section.objectId).available);
  CHECK(app.close());
  CHECK(!app.ownerPump());
  CHECK(second.controlFacade().mountedCanvasTargets().size() == 1);
  CHECK(second.close());
  std::puts("PASS: installed C++ Runtime created, edited, read, undone and closed");
}
