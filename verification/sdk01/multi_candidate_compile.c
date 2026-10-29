#include "candidate-multi-selection-v1.h"

#include <stddef.h>

_Static_assert(sizeof(CanvasObjectId) == 16, "object IDs must remain 128 bits");
_Static_assert(sizeof(CanvasSdkMultiTokenV1) == 16, "candidate token must remain 128 bits");
_Static_assert(sizeof(CanvasSdkMultiPatchMask) == 8, "mask uses a fixed-width integer");
_Static_assert(offsetof(CanvasSdkMultiProjectionV1, struct_size) == 0, "versioned prefix");
_Static_assert(offsetof(CanvasSdkMultiProjectionV1, abi_version) == 4, "versioned prefix");
_Static_assert(offsetof(CanvasSdkMultiValueV1, abi_version) == 4, "versioned prefix");
_Static_assert(offsetof(CanvasSdkMultiReceiptV1, abi_version) == 4, "versioned prefix");

/* Compile-only against the draft declaration. No mock SDK is linked or run. */
CanvasStatus sdk01_candidate_c_probe(CanvasViewHandle view) {
    CanvasSdkMultiProjectionV1 projection = {0};
    projection.struct_size = (uint32_t)sizeof(projection);
    projection.abi_version = CANVAS_SDK_MULTI_CANDIDATE_VERSION;
    return canvas_sdk_multi_capture(view, 0x100u, &projection);
}
