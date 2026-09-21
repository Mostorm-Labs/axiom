#pragma once

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "arc/protocol.h"
#include "canvas/interaction/multi_contact_coordinator.hpp"

#include <cstdint>
#include <vector>
#include <algorithm>
#include "canvas/ink/ink_engine.hpp"

namespace canvas::ink_playground::windows_input {

struct ViewportPoint final {
  float x = 0.0F;
  float y = 0.0F;
};

[[nodiscard]] inline ViewportPoint toViewportPoint(float x, float y, float scale,
                                                    float translationX,
                                                    float translationY) noexcept {
  return {x * scale + translationX, y * scale + translationY};
}

enum class PlatformPointerAction : std::uint8_t {
  kContinuePreview,
  kSuppressPreview,
  kCommitAndRelease,
  kReleaseWithoutCommit
};

[[nodiscard]] inline PlatformPointerAction platformPointerAction(
    canvas::interaction::ContactDisposition disposition, bool end) noexcept {
  if (disposition == canvas::interaction::ContactDisposition::kViewportGesture ||
      disposition == canvas::interaction::ContactDisposition::kIgnored ||
      disposition == canvas::interaction::ContactDisposition::kTerminal) {
    return end ? PlatformPointerAction::kReleaseWithoutCommit
               : PlatformPointerAction::kSuppressPreview;
  }
  return end ? PlatformPointerAction::kCommitAndRelease
             : PlatformPointerAction::kContinuePreview;
}

[[nodiscard]] inline bool cancelsAllActivePointers(UINT message) noexcept {
  return message == WM_KILLFOCUS || message == WM_CANCELMODE || message == WM_DESTROY;
}

[[nodiscard]] inline canvas::interaction::MultiContactPolicy nextMultiContactPolicy(
    canvas::interaction::MultiContactPolicy policy) noexcept {
  using Policy = canvas::interaction::MultiContactPolicy;
  return policy == Policy::kAutoIntent ? Policy::kMultiInk
       : policy == Policy::kMultiInk ? Policy::kGesturePriority : Policy::kAutoIntent;
}

[[nodiscard]] inline const wchar_t* multiContactPolicyName(
    canvas::interaction::MultiContactPolicy policy) noexcept {
  using Policy = canvas::interaction::MultiContactPolicy;
  return policy == Policy::kAutoIntent ? L"AutoIntent"
       : policy == Policy::kMultiInk ? L"MultiInk" : L"GesturePriority";
}

[[nodiscard]] inline const char* multiContactPolicyNameUtf8(
    canvas::interaction::MultiContactPolicy policy) noexcept {
  using Policy = canvas::interaction::MultiContactPolicy;
  return policy == Policy::kAutoIntent ? "AutoIntent"
       : policy == Policy::kMultiInk ? "MultiInk" : "GesturePriority";
}

[[nodiscard]] inline arc_input_tool_t arcToolForPointerType(
    POINTER_INPUT_TYPE pointerType) noexcept {
  if (pointerType == PT_PEN) return ARC_INPUT_TOOL_PEN;
  if (pointerType == PT_TOUCH) return ARC_INPUT_TOOL_TOUCH;
  return ARC_INPUT_TOOL_MOUSE;
}

enum class PointerLifecycleEvent : std::uint8_t { kIgnore, kBegin, kUpdate, kEnd };

class PointerLifecycle final {
 public:
  [[nodiscard]] PointerLifecycleEvent begin(std::uint32_t pointerId,
                                            std::uint64_t timestampMs) noexcept {
    if (active_) return PointerLifecycleEvent::kIgnore;
    active_ = true;
    pointerId_ = pointerId;
    strokeStartTimeMs_ = timestampMs;
    return PointerLifecycleEvent::kBegin;
  }

  [[nodiscard]] PointerLifecycleEvent update(std::uint32_t pointerId) const noexcept {
    return active_ && pointerId == pointerId_ ? PointerLifecycleEvent::kUpdate
                                              : PointerLifecycleEvent::kIgnore;
  }

  [[nodiscard]] PointerLifecycleEvent end(std::uint32_t pointerId) noexcept {
    if (!active_ || pointerId != pointerId_) return PointerLifecycleEvent::kIgnore;
    active_ = false;
    pointerId_ = 0;
    return PointerLifecycleEvent::kEnd;
  }

  [[nodiscard]] std::uint64_t strokeStartTime() const noexcept { return strokeStartTimeMs_; }

 private:
  bool active_ = false;
  std::uint32_t pointerId_ = 0;
  std::uint64_t strokeStartTimeMs_ = 0;
};

[[nodiscard]] inline std::uint64_t deviceIdFromHandle(HANDLE handle) noexcept {
  return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
}

[[nodiscard]] inline bool keepMouseOnButtonMessages() noexcept {
  return EnableMouseInPointer(FALSE) != FALSE;
}

[[nodiscard]] inline std::uint64_t functionalDeviceId(HANDLE handle,
                                                       std::uint64_t fallback) noexcept {
  const auto value = deviceIdFromHandle(handle);
  return value != 0U ? value : fallback;
}

inline void appendCommittedStroke(std::vector<canvas::ink::StrokePoint>& retained,
                                  const std::vector<canvas::ink::StrokePoint>& stroke) {
  retained.insert(retained.end(), stroke.begin(), stroke.end());
}

[[nodiscard]] inline std::vector<POINTER_INFO> normalizePointerHistory(
    const std::vector<POINTER_INFO>& newestFirst, bool isDown,
    std::uint64_t strokeStartTimeMs = 0U) {
  std::vector<POINTER_INFO> normalized;
  normalized.reserve(newestFirst.size());
  for (auto it = newestFirst.rbegin(); it != newestFirst.rend(); ++it) {
    if ((it->pointerFlags & POINTER_FLAG_INCONTACT) == 0) {
      continue;
    }
    if (strokeStartTimeMs != 0U && static_cast<std::uint64_t>(it->dwTime) < strokeStartTimeMs) {
      continue;
    }
    normalized.push_back(*it);
  }
  if (isDown && !normalized.empty()) {
    const auto down = std::find_if(normalized.begin(), normalized.end(), [](const POINTER_INFO& info) {
      return (info.pointerFlags & POINTER_FLAG_DOWN) != 0;
    });
    if (down != normalized.end()) normalized.erase(normalized.begin(), down);
  }
  return normalized;
}

}  // namespace canvas::ink_playground::windows_input

#endif
