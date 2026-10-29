#include "canvas/platform/windows_runtime_adapter.hpp"
#include "canvas/runtime/runtime_composition.hpp"

#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <thread>

int main() {
  WNDCLASSW klass{};
  klass.hInstance = GetModuleHandleW(nullptr);
  klass.lpfnWndProc = DefWindowProcW;
  klass.lpszClassName = L"AxiomPublicAdapterTest";
  assert(RegisterClassW(&klass) != 0U);
  HWND window = CreateWindowExW(0, klass.lpszClassName, L"Axiom adapter test",
      WS_OVERLAPPEDWINDOW, 0, 0, 400, 400, nullptr, nullptr, klass.hInstance, nullptr);
  assert(window);
  canvas::runtime::RuntimeComposition composition;
  assert(composition.open());
  canvas::platform::WindowsRuntimeAdapter adapter(composition, window);
  if (!adapter.bind(256, 256, 2.0F)) {
    std::fprintf(stderr, "BLOCKED_ENVIRONMENT: D3D12 provider unavailable\n");
    DestroyWindow(window);
    return 77;
  }
  assert(adapter.prepareFrame(true));
  assert(adapter.frameStatus().canonicalWorkId != 0);
  assert(adapter.present());
  assert(!adapter.present());
  bool otherThreadAccepted = true;
  std::thread other([&] { otherThreadAccepted = adapter.resize(300, 300, 1.0F); });
  other.join();
  assert(!otherThreadAccepted);
  assert(composition.registerSource(9100));
  canvas::input::PlatformPointerBatch batch;
  canvas::input::PlatformPointerSample sample{};
  sample.source = 9100; sample.pointer = 1; sample.sequence = 1; sample.timestampNs = 1000000;
  sample.phase = canvas::input::PointerPhase::kDown; sample.x = 20; sample.y = 20;
  batch.samples = {sample};
  const auto first = composition.beginContact(batch);
  assert(first);
  sample.phase = canvas::input::PointerPhase::kMove; sample.x = 60; sample.y = 60;
  sample.sequence = 2; sample.timestampNs = 2000000; batch.samples = {sample};
  assert(composition.submitContact(*first, batch));
  assert(adapter.prepareFrame(false));
  assert(adapter.frameStatus().previewWorkId != 0);
  assert(adapter.present());
  sample.phase = canvas::input::PointerPhase::kUp; sample.sequence = 3;
  sample.timestampNs = 3000000; batch.samples = {sample};
  assert(composition.submitContact(*first, batch));
  sample.phase = canvas::input::PointerPhase::kDown;
  sample.sequence = 4; sample.timestampNs = 4000000; batch.samples = {sample};
  const auto second = composition.beginContact(batch);
  assert(second && second->generation != first->generation);
  sample.phase = canvas::input::PointerPhase::kUp;
  sample.sequence = 100; sample.timestampNs = 5000000; batch.samples = {sample};
  assert(!composition.submitContact(*first, batch));
  sample.sequence = 5; sample.timestampNs = 6000000; batch.samples = {sample};
  assert(composition.submitContact(*second, batch));
  sample.phase = canvas::input::PointerPhase::kDown;
  sample.sequence = 6; sample.timestampNs = 7000000; batch.samples = {sample};
  const auto third = composition.beginContact(batch);
  assert(third);
  auto wrongSource = sample;
  wrongSource.source = 9999;
  wrongSource.phase = canvas::input::PointerPhase::kMove;
  batch.samples = {wrongSource};
  assert(!composition.submitContact(*third, batch));
  assert(!composition.cancelContact(*second, 7, 8000000));
  assert(composition.cancelContact(*third, 8, 9000000));
  assert(composition.unregisterSource(9100, 9, 10000000));
  assert(composition.registerSource(9100));
  sample.sequence = 10; sample.timestampNs = 11000000; batch.samples = {sample};
  const auto rebound = composition.beginContact(batch);
  assert(rebound && rebound->generation != third->generation);
  sample.phase = canvas::input::PointerPhase::kUp;
  sample.sequence = 11; sample.timestampNs = 12000000; batch.samples = {sample};
  assert(!composition.submitContact(*third, batch));
  assert(composition.submitContact(*rebound, batch));
  assert(adapter.resize(320, 240, 2.0F));
  assert(adapter.prepareFrame(true));
  assert(adapter.present());
  assert(adapter.loseSurface());
  assert(!adapter.prepareFrame(true));
  assert(adapter.recoverSurface(320, 240, 2.0F));
  assert(adapter.prepareFrame(true));
  assert(adapter.present());
  assert(adapter.shutdown());
  assert(composition.close());
  assert(!adapter.prepareFrame(true));
  DestroyWindow(window);
  std::puts("PASS real HWND adapter bind, frame, resize, shutdown");
}
