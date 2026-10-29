#include <canvas/platform/windows_runtime_adapter.hpp>
#include <canvas/runtime/runtime_composition.hpp>

#define NOMINMAX
#include <windows.h>
#include <cstdio>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (false)

int main() {
  WNDCLASSW klass{};
  klass.hInstance = GetModuleHandleW(nullptr);
  klass.lpfnWndProc = DefWindowProcW;
  klass.lpszClassName = L"AxiomExternalSdkWindow";
  CHECK(RegisterClassW(&klass));
  HWND window = CreateWindowExW(0, klass.lpszClassName, L"Axiom SDK integration test",
      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 0, 0, 600, 400, nullptr, nullptr, klass.hInstance, nullptr);
  CHECK(window);
  ShowWindow(window, SW_SHOWNOACTIVATE);
  UpdateWindow(window);
  canvas::runtime::RuntimeComposition owner;
  CHECK(owner.open());
  canvas::platform::WindowsRuntimeAdapter platform(owner, window);
  if (!platform.bind(400, 300, 1.0F)) return 77;
  auto& product = owner.controlFacade();
  auto& debug = owner.controlFacade();
  CHECK(&product == &debug);
  CHECK(owner.registerSource(9000));
  canvas::input::PlatformPointerSample sample{};
  sample.source = 9000; sample.pointer = 1; sample.sequence = 1; sample.timestampNs = 1000000;
  sample.phase = canvas::input::PointerPhase::kDown; sample.x = 10; sample.y = 10;
  canvas::input::PlatformPointerBatch batch; batch.samples = {sample};
  auto contact = owner.beginContact(batch); CHECK(contact);
  sample.phase = canvas::input::PointerPhase::kMove; sample.x = 30; sample.y = 30;
  sample.sequence = 2; sample.timestampNs = 2000000; batch.samples = {sample};
  CHECK(owner.submitContact(*contact, batch));
  CHECK(platform.prepareFrame(true));
  const auto inkPreview = platform.frameStatus();
  CHECK(inkPreview.canonicalWorkId != 0 && inkPreview.previewWorkId != 0);
  CHECK(platform.present());
  sample.phase = canvas::input::PointerPhase::kUp; sample.sequence = 3; sample.timestampNs = 3000000;
  batch.samples = {sample}; CHECK(owner.submitContact(*contact, batch));
  CHECK(platform.prepareFrame(true)); CHECK(platform.present());
  canvas::runtime::CanvasControlRequest control{};
  control.clientId = 9001; control.requestId = 1; control.target = product.mountedCanvasTargets().front();
  control.payload.kind = canvas::runtime::CanvasControlPayloadKind::kSelectCreationPreset;
  control.payload.creationPreset = canvas::runtime::CanvasCreationPreset::kRectangle;
  CHECK(product.submitCanvasControl(control).state == canvas::runtime::ProductControlState::kQueued);
  CHECK(owner.ownerPump());
  sample.sequence = 4; sample.timestampNs = 4000000;
  sample.phase = canvas::input::PointerPhase::kDown; sample.x = 40; sample.y = 40;
  batch.samples = {sample};
  contact = owner.beginContact(batch); CHECK(contact);
  sample.phase = canvas::input::PointerPhase::kMove; sample.x = 120; sample.y = 120;
  sample.sequence = 5; sample.timestampNs = 5000000; batch.samples = {sample};
  CHECK(owner.submitContact(*contact, batch));
  CHECK(platform.prepareFrame(true));
  const auto shapeFrame = platform.frameStatus();
  CHECK(shapeFrame.canonicalWorkId != 0);
  CHECK(platform.present());
  sample.phase = canvas::input::PointerPhase::kUp; sample.sequence = 6; sample.timestampNs = 6000000;
  batch.samples = {sample}; CHECK(owner.submitContact(*contact, batch));
  CHECK(product.setSelectionMode(true, 2, product.readRuntimeState().identity.runtimeGeneration, 9001).state == canvas::runtime::ProductControlState::kQueued);
  CHECK(owner.ownerPump());
  sample.phase = canvas::input::PointerPhase::kDown; sample.x = 80; sample.y = 80;
  sample.sequence = 7; sample.timestampNs = 7000000; batch.samples = {sample}; contact = owner.beginContact(batch); CHECK(contact);
  sample.phase = canvas::input::PointerPhase::kUp; sample.sequence = 8; sample.timestampNs = 8000000;
  batch.samples = {sample}; CHECK(owner.submitContact(*contact, batch));
  CHECK(debug.readSelectionSummary().count == 1);
  const auto previousContact = *contact;
  sample.phase = canvas::input::PointerPhase::kDown; sample.sequence = 9;
  sample.timestampNs = 9000000; batch.samples = {sample};
  contact = owner.beginContact(batch); CHECK(contact);
  CHECK(contact->generation != previousContact.generation);
  sample.phase = canvas::input::PointerPhase::kUp; sample.sequence = 100;
  sample.timestampNs = 10000000; batch.samples = {sample};
  CHECK(!owner.submitContact(previousContact, batch));
  sample.sequence = 10; sample.timestampNs = 11000000; batch.samples = {sample};
  CHECK(owner.submitContact(*contact, batch));
  CHECK(platform.resize(320, 240, 1.0F));
  CHECK(platform.prepareFrame(true));
  CHECK(platform.frameStatus().canonicalWorkId != 0);
  CHECK(platform.present());
  CHECK(platform.loseSurface());
  CHECK(!platform.prepareFrame(true));
  CHECK(platform.recoverSurface(320, 240, 1.0F));
  CHECK(platform.prepareFrame(true));
  CHECK(platform.present());
  CHECK(platform.shutdown()); CHECK(owner.close());
  CHECK(!platform.prepareFrame(true));
  DestroyWindow(window);
  std::puts("PASS external window, two surfaces, input, shared selection and feedback");
}
