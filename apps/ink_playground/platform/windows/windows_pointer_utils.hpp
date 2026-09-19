#pragma once

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>

namespace canvas::ink_playground::windows_input {

[[nodiscard]] inline std::uint64_t deviceIdFromHandle(HANDLE handle) noexcept {
  return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
}

}  // namespace canvas::ink_playground::windows_input

#endif
