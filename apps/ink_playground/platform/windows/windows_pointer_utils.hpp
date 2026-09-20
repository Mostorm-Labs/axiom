#pragma once

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <vector>
#include "canvas/ink/ink_engine.hpp"

namespace canvas::ink_playground::windows_input {

[[nodiscard]] inline std::uint64_t deviceIdFromHandle(HANDLE handle) noexcept {
  return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
}

[[nodiscard]] inline bool enableFunctionalPointerIngress() noexcept {
  return EnableMouseInPointer(TRUE) != FALSE;
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

}  // namespace canvas::ink_playground::windows_input

#endif
