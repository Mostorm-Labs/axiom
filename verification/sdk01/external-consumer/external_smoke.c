#include "canvas_runtime_api_v1.h"

#include <stdio.h>
#include <string.h>

/* Frozen expectations are drawn from the accepted V1 public contract, not
   runtime internals. This fixture is a review candidate, not a product PASS. */
#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
    return 1; \
  } \
} while (0)

int main(void) {
  CanvasRuntimeConfigV1 config;
  CanvasRuntimeHandle first = CANVAS_INVALID_HANDLE;
  CanvasRuntimeHandle second = CANVAS_INVALID_HANDLE;
  CanvasRuntimeHandle untouched = 0x12345678u;
  CanvasCapabilitiesV1 caps;

  CHECK(canvas_runtime_get_abi_version() == CANVAS_RUNTIME_ABI_VERSION);
  CHECK(canvas_runtime_destroy(CANVAS_INVALID_HANDLE) != kCanvasStatusOk);

  memset(&config, 0, sizeof(config));
  config.struct_size = (uint32_t)sizeof(config);
  config.abi_version = CANVAS_RUNTIME_ABI_VERSION + 1u;
  config.requested_backend = kCanvasRendererBackendAuto;
  CHECK(canvas_runtime_create(&config, &untouched) != kCanvasStatusOk);
  CHECK(untouched == 0x12345678u);

  config.abi_version = CANVAS_RUNTIME_ABI_VERSION;
  CHECK(canvas_runtime_create(&config, &first) == kCanvasStatusOk);
  CHECK(first != CANVAS_INVALID_HANDLE);
  CHECK(canvas_runtime_create(&config, &second) == kCanvasStatusOk);
  CHECK(second != CANVAS_INVALID_HANDLE);
  CHECK(first != second);

  memset(&caps, 0, sizeof(caps));
  caps.struct_size = (uint32_t)sizeof(caps);
  caps.abi_version = CANVAS_RUNTIME_ABI_VERSION;
  CHECK(canvas_runtime_get_capabilities(first, &caps) == kCanvasStatusOk);
  CHECK(canvas_runtime_destroy(first) == kCanvasStatusOk);
  CHECK(canvas_runtime_get_capabilities(first, &caps) != kCanvasStatusOk);
  CHECK(canvas_runtime_get_capabilities(second, &caps) == kCanvasStatusOk);
  CHECK(canvas_runtime_destroy(second) == kCanvasStatusOk);
  CHECK(canvas_runtime_destroy(second) != kCanvasStatusOk);
  puts("PASS SDK01ExternalCSmoke ABI and two-runtime lifetime");
  return 0;
}
