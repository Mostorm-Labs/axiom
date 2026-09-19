#include "../platform/windows/windows_pointer_utils.hpp"

#if defined(_WIN32)

#include <cstdint>

int main() {
  const auto value = canvas::ink_playground::windows_input::deviceIdFromHandle(
      reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(0x1234U)));
  return value == 0x1234U &&
                 canvas::ink_playground::windows_input::deviceIdFromHandle(nullptr) == 0U
             ? 0
             : 1;
}

#else
int main() { return 0; }
#endif
