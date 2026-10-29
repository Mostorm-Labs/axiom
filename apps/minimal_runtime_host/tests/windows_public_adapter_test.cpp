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
  assert(composition.submit(batch));
  sample.phase = canvas::input::PointerPhase::kMove; sample.x = 60; sample.y = 60;
  sample.sequence = 2; sample.timestampNs = 2000000; batch.samples = {sample};
  assert(composition.submit(batch));
  assert(adapter.prepareFrame(false));
  assert(adapter.frameStatus().previewWorkId != 0);
  assert(adapter.present());
  sample.phase = canvas::input::PointerPhase::kUp; sample.sequence = 3;
  sample.timestampNs = 3000000; batch.samples = {sample};
  assert(composition.submit(batch));
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
