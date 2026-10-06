#pragma once

#if defined(_WIN32)
#include "windows_pointer_utils.hpp"
#include <ostream>
#include <vector>

namespace canvas::ink_playground::windows_input {
inline constexpr wchar_t kDiagnosticStreamProperty[] = L"AxiomWindowsInputDiagnosticStream";

[[nodiscard]] inline std::ostream* diagnosticStream(HWND owner) noexcept {
  return reinterpret_cast<std::ostream*>(GetPropW(owner, kDiagnosticStreamProperty));
}

// Observe the current message before forwarding/normalization. This deliberately
// leaves input ownership, capture, and Runtime routing unchanged.
inline void logInputMessage(std::ostream& out, const char* role, HWND window,
                            UINT message, WPARAM wParam) {
  INPUT_MESSAGE_SOURCE source{};
  const auto sourceAvailable = GetCurrentInputMessageSource(&source);
  const auto extraInfo = GetMessageExtraInfo();
  out << "input-origin role=" << role << " hwnd="
      << reinterpret_cast<std::uintptr_t>(window) << " phase=" << message
      << " tick=" << GetTickCount64() << " message_time=" << GetMessageTime()
      << " extra=" << static_cast<std::uintptr_t>(extraInfo)
      << " promoted=" << isPromotedPointerMouseMessage(extraInfo)
      << " source_available=" << sourceAvailable
      << " source_type=" << source.deviceType << " origin=" << source.originId
      << " capture=" << reinterpret_cast<std::uintptr_t>(GetCapture());
  if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE ||
      message == WM_POINTERUP || message == WM_POINTERCAPTURECHANGED) {
    const auto id = GET_POINTERID_WPARAM(wParam);
    POINTER_INFO info{};
    out << " id=" << id;
    if (GetPointerInfo(id, &info)) {
      out << " target=" << reinterpret_cast<std::uintptr_t>(info.hwndTarget)
          << " frame=" << info.frameId << " flags=" << info.pointerFlags
          << " pointer_type=" << info.pointerType
          << " x=" << info.ptPixelLocation.x << " y=" << info.ptPixelLocation.y;
      if (info.pointerType == PT_TOUCH) {
        UINT32 count = 0;
        const auto available = GetPointerFrameTouchInfo(id, &count, nullptr);
        const auto error = GetLastError();
        if (count != 0U && (available || error == ERROR_INSUFFICIENT_BUFFER)) {
          std::vector<POINTER_TOUCH_INFO> frame(count);
          if (GetPointerFrameTouchInfo(id, &count, frame.data())) {
            out << " frame_contacts=" << count << " frame_ids=";
            for (UINT32 i = 0; i < count; ++i) {
              const auto& p = frame[i].pointerInfo;
              out << (i == 0U ? "" : ",") << p.pointerId << ":" << p.pointerFlags;
            }
          }
        }
      }
    } else {
      out << " pointer_error=" << GetLastError();
    }
  }
  out << '\n';
}
}  // namespace canvas::ink_playground::windows_input
#endif
