#include "../platform/windows/windows_d3d12_skia_surface_provider.hpp"

#include <iostream>
#include <string_view>

namespace {
struct PopupSearch {
  HWND owner;
  HWND popup = nullptr;
};
BOOL CALLBACK findPopup(HWND window, LPARAM parameter) {
  auto& search = *reinterpret_cast<PopupSearch*>(parameter);
  wchar_t name[128]{};
  GetClassNameW(window, name, 128);
  if (GetWindow(window, GW_OWNER) == search.owner &&
      std::wstring_view(name) == L"AxiomD3D12PreviewOverlay") {
    search.popup = window;
    return FALSE;
  }
  return TRUE;
}

bool verifyPopup(HWND owner) {
  canvas::ink_playground::WindowsD3D12SkiaSurfaceProvider provider(owner);
  using canvas::render::BackendSubmissionCode;
  using canvas::render::SkiaSurfaceAcquireCode;
  if (provider.resize(128, 128).code != BackendSubmissionCode::kAccepted) {
    std::cerr << "Real D3D12 preview surface creation failed\n";
    return false;
  }
  PopupSearch search{owner};
  EnumThreadWindows(GetCurrentThreadId(), findPopup,
                    reinterpret_cast<LPARAM>(&search));
  const auto style = search.popup == nullptr
      ? 0L : GetWindowLongPtrW(search.popup, GWL_EXSTYLE);
  if (!search.popup || (style & WS_EX_TRANSPARENT) == 0 ||
      (style & WS_EX_NOREDIRECTIONBITMAP) == 0 || (style & WS_EX_LAYERED) != 0) {
    std::cerr << "Preview popup is not an isolated DirectComposition target\n";
    return false;
  }
  provider.setOverlayVisible(true);
  if (provider.acquire().code != SkiaSurfaceAcquireCode::kAcquired ||
      provider.present().code != BackendSubmissionCode::kAccepted) return false;
  provider.release();
  provider.setOverlayVisible(false);
  if (provider.resize(192, 192).code != BackendSubmissionCode::kAccepted) return false;
  provider.setOverlayVisible(true);
  if (provider.acquire().code != SkiaSurfaceAcquireCode::kAcquired ||
      provider.present().code != BackendSubmissionCode::kAccepted) return false;
  provider.release();
  return true;
}
}  // namespace

int main() {
  const auto owner = CreateWindowExW(0, L"STATIC", L"Axiom preview popup test",
                                     WS_POPUP, 0, 0, 256, 256, nullptr, nullptr,
                                     GetModuleHandleW(nullptr), nullptr);
  if (!owner) return 1;
  const auto passed = verifyPopup(owner);
  DestroyWindow(owner);
  return passed ? 0 : 2;
}
