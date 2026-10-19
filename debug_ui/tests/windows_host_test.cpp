#include "canvas/debug_ui/windows_host.hpp"
#include "canvas/debug_ui/controller.hpp"

#include <cassert>
#include <cstdint>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {
LRESULT CALLBACK ownerProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  return DefWindowProcW(window, message, wParam, lParam);
}
}

int main() {
  const auto instance = GetModuleHandleW(nullptr);
  WNDCLASSW klass{};
  klass.hInstance = instance;
  klass.lpfnWndProc = ownerProc;
  klass.lpszClassName = L"AxiomDebugUiHostRegressionOwner";
  RegisterClassW(&klass);
  const auto owner = CreateWindowExW(0, klass.lpszClassName, L"owner",
      WS_OVERLAPPEDWINDOW, 0, 0, 800, 600, nullptr, nullptr, instance, nullptr);
  assert(owner != nullptr);

  canvas::debug_ui::DebugController controller({});
  canvas::debug_ui::WindowsDebugUiHost host;
  assert(host.initialize(owner));
  host.setController(&controller);
  host.toggle();
  const auto afterToggle = host.renderCountForTesting();
  assert(afterToggle == 1U);

  const auto overlay = host.overlayForTesting();
  assert(overlay != nullptr);
  SendMessageW(overlay, WM_MOUSEMOVE, 0, MAKELPARAM(20, 20));
  // Input dispatch must only queue a refresh. Rendering is owned by the
  // normal timer/idle refresh path so an input burst cannot hang the UI thread.
  assert(host.renderCountForTesting() == afterToggle);
  host.refresh();
  assert(host.renderCountForTesting() == afterToggle + 1U);

  host.shutdown();
  DestroyWindow(owner);
  UnregisterClassW(klass.lpszClassName, instance);
  return 0;
}
#else
int main() { return 0; }
#endif
