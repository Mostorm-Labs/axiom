#include "canvas_runtime_api_v1.h"

#include <cstdint>

int main() {
  const std::uint32_t actual = canvas_runtime_get_abi_version();
  return actual == CANVAS_RUNTIME_ABI_VERSION ? 0 : 1;
}
