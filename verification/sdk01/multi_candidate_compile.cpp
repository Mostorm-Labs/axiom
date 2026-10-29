#include "candidate-multi-selection-v1.h"

#include <type_traits>

static_assert(sizeof(CanvasObjectId) == 16);
static_assert(sizeof(CanvasSdkMultiTokenV1) == 16);
static_assert(std::is_standard_layout_v<CanvasSdkMultiProjectionV1>);
static_assert(std::is_standard_layout_v<CanvasSdkMultiValueV1>);
static_assert(std::is_standard_layout_v<CanvasSdkMultiReceiptV1>);

/* Compile-only: a real SDK library and product consumer do not exist yet. */
CanvasStatus sdk01_candidate_cpp_probe(CanvasViewHandle view,
                                      CanvasSdkMultiProjectionV1* projection) {
  return canvas_sdk_multi_capture(view, 0x100u, projection);
}
