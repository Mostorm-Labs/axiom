#include <canvas/runtime/runtime_composition.hpp>

#include <cstdio>
#include <type_traits>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (false)

int main() {
  using namespace canvas;
  static_assert(!std::is_copy_constructible_v<runtime::RuntimeComposition>);
  runtime::RuntimeComposition composition;
  CHECK(composition.open());
  CHECK(composition.bindSurface(256, 256));
  auto& product = composition.controlFacade();
  auto& debug = composition.controlFacade();
  CHECK(&product == &debug);
  runtime::SectionIntent section;
  section.objectId = foundation::ObjectId::fromUint64(5100);
  CHECK(product.submitSectionIntent(section, 1, 8800).state == runtime::ProductControlState::kQueued);
  CHECK(composition.ownerPump());
  const auto receipt = debug.canvasControlReceipt({8800, 1});
  CHECK(receipt && receipt->state == runtime::CanvasControlReceiptState::kApplied);
  CHECK(debug.readSection(section.objectId).available);
  CHECK(debug.undo(2, debug.readRuntimeState().identity.runtimeGeneration, 8800).state ==
        runtime::ProductControlState::kQueued);
  CHECK(composition.ownerPump());
  CHECK(!product.readSection(section.objectId).available);
  CHECK(composition.close());
  CHECK(!composition.ownerPump());
  std::puts("PASS: installed Composition and shared product/debug facade");
}
