#pragma once

/* REVIEW CANDIDATE ONLY. Not installed, not implemented, not an accepted ABI.
   This header proposes additive functions without changing V1 definitions. */
#include "canvas_runtime_api_v1.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CANVAS_SDK_MULTI_CANDIDATE_VERSION 1u

/* Opaque, process-local observation identity; all-zero is invalid. Never an
   ObjectId, pointer, SemanticGeneration, persistent identifier or client ID. */
typedef struct CanvasSdkMultiTokenV1 {
    uint8_t bytes[16];
} CanvasSdkMultiTokenV1;

typedef uint32_t CanvasSdkPermissionMode;
enum {
    kCanvasSdkPermissionUnconfigured = 0,
    kCanvasSdkPermissionLocalSingleUser = 1,
    kCanvasSdkPermissionContextUnavailable = 2
};

typedef uint32_t CanvasSdkMultiAvailability;
enum {
    kCanvasSdkMultiUnavailable = 0,
    kCanvasSdkMultiReady = 1
};

typedef uint32_t CanvasSdkMultiSupport;
enum {
    kCanvasSdkMultiSupportUnknown = 0,
    kCanvasSdkMultiSupported = 1,
    kCanvasSdkMultiUnsupported = 2
};

typedef uint32_t CanvasSdkMultiPermissionState;
enum {
    kCanvasSdkMultiPermissionUnknown = 0,
    kCanvasSdkMultiPermissionAllowed = 1,
    kCanvasSdkMultiPermissionDenied = 2
};

typedef uint32_t CanvasSdkMultiExcludedReason;
enum {
    kCanvasSdkMultiNotApplicable = 1
};

typedef struct CanvasSdkMultiExcludedV1 {
    CanvasObjectId object_id;
    CanvasSdkMultiExcludedReason reason;
} CanvasSdkMultiExcludedV1;

/* One field is queried at a time. Uniform refers to eligible roots only;
   mixed and partial are independent, so neither is inferred from the other. */
typedef struct CanvasSdkMultiProjectionV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    CanvasSdkMultiTokenV1 token;
    uint32_t field_id;
    CanvasSdkMultiSupport support;
    CanvasSdkMultiAvailability availability;
    CanvasSdkMultiPermissionState permission;
    uint32_t mixed;
    uint32_t partial;
    uint32_t uniform_value_available;
    uint32_t reserved;
    uint64_t total_count;
    uint64_t eligible_count;
    uint64_t excluded_count;
} CanvasSdkMultiProjectionV1;

typedef uint32_t CanvasSdkMultiValueKind;
enum {
    kCanvasSdkMultiBool = 1,
    kCanvasSdkMultiOpacity = 2,
    kCanvasSdkMultiBlendMode = 3,
    kCanvasSdkMultiFillNone = 4,
    kCanvasSdkMultiFillSolid = 5,
    kCanvasSdkMultiStrokeNone = 6,
    kCanvasSdkMultiStrokeSolid = 7,
    kCanvasSdkMultiConnectorDecoration = 8
};

/* Not compiler bool/enums. Values not selected by kind must be zero.
   dash_segments is an input borrowed span; on output it is set to the
   caller-provided dash buffer, never SDK-owned memory. Colors retain f32
   channels and stroke geometry retains f64 precision. */
typedef struct CanvasSdkMultiValueV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    CanvasSdkMultiValueKind kind;
    uint32_t bool_value;
    uint32_t enum_value;
    uint32_t stroke_cap;
    uint32_t stroke_join;
    uint32_t reserved;
    float scalar_value;
    float color_rgba[4];
    double stroke_width;
    double miter_limit;
    double dash_offset;
    const double* dash_segments;
    uint64_t dash_count;
} CanvasSdkMultiValueV1;

typedef uint64_t CanvasSdkMultiPatchMask;
enum {
    kCanvasSdkPatchReplaceField = 1ull << 0,
    kCanvasSdkPatchStrokeColor = 1ull << 1,
    kCanvasSdkPatchStrokeWidth = 1ull << 2,
    kCanvasSdkPatchStrokeCap = 1ull << 3,
    kCanvasSdkPatchStrokeJoin = 1ull << 4,
    kCanvasSdkPatchStrokeDash = 1ull << 5
};

typedef uint32_t CanvasSdkMultiSubmitFlags;
enum {
    kCanvasSdkAcknowledgePartial = 1u << 0
};

typedef uint32_t CanvasSdkMultiReceiptState;
enum {
    kCanvasSdkMultiQueued = 1,
    kCanvasSdkMultiApplied = 2,
    kCanvasSdkMultiRejected = 3,
    kCanvasSdkMultiStale = 4,
    kCanvasSdkMultiDuplicate = 5,
    kCanvasSdkMultiExpired = 6,
    kCanvasSdkMultiQueueFull = 7,
    kCanvasSdkMultiFailed = 8
};

typedef uint32_t CanvasSdkMultiFailure;
enum {
    kCanvasSdkMultiNoFailure = 0,
    kCanvasSdkMultiInvalidValue = 1,
    kCanvasSdkMultiStaleTarget = 2,
    kCanvasSdkMultiPermissionUnavailable = 3,
    kCanvasSdkMultiLocked = 4,
    kCanvasSdkMultiOwnerUnavailable = 5,
    kCanvasSdkMultiCapacityExceeded = 6,
    kCanvasSdkMultiInternalFailure = 7
};

typedef struct CanvasSdkMultiReceiptV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    CanvasSdkMultiReceiptState state;
    CanvasSdkMultiFailure failure;
    uint32_t terminal;
    uint32_t reserved;
    uint64_t changed_count;
    uint64_t unchanged_count;
    uint64_t skipped_count;
} CanvasSdkMultiReceiptV1;

/* Owner-thread calls. CanvasStatus describes invocation, while receipt state
   describes admission/execution. Inputs and output arrays are caller-owned. */
CANVAS_RUNTIME_API CanvasStatus canvas_sdk_runtime_pump(CanvasRuntimeHandle runtime);
CANVAS_RUNTIME_API CanvasStatus canvas_sdk_multi_set_permission(
    CanvasRuntimeHandle runtime, CanvasSdkPermissionMode mode);
CANVAS_RUNTIME_API CanvasStatus canvas_sdk_multi_capture(
    CanvasViewHandle view, uint32_t field_id, CanvasSdkMultiProjectionV1* projection);
CANVAS_RUNTIME_API CanvasStatus canvas_sdk_multi_copy_roots(
    CanvasViewHandle view, const CanvasSdkMultiTokenV1* token,
    CanvasObjectId* objects, uint64_t capacity, uint64_t* required);
CANVAS_RUNTIME_API CanvasStatus canvas_sdk_multi_copy_eligible(
    CanvasViewHandle view, const CanvasSdkMultiTokenV1* token,
    CanvasObjectId* objects, uint64_t capacity, uint64_t* required);
CANVAS_RUNTIME_API CanvasStatus canvas_sdk_multi_copy_excluded(
    CanvasViewHandle view, const CanvasSdkMultiTokenV1* token,
    CanvasSdkMultiExcludedV1* objects, uint64_t capacity, uint64_t* required);
CANVAS_RUNTIME_API CanvasStatus canvas_sdk_multi_copy_uniform_value(
    CanvasViewHandle view, const CanvasSdkMultiTokenV1* token,
    CanvasSdkMultiValueV1* value, double* dash_segments, uint64_t dash_capacity,
    uint64_t* dash_required);
CANVAS_RUNTIME_API CanvasStatus canvas_sdk_multi_copy_root_value(
    CanvasViewHandle view, const CanvasSdkMultiTokenV1* token, CanvasObjectId object_id,
    CanvasSdkMultiValueV1* value, double* dash_segments, uint64_t dash_capacity,
    uint64_t* dash_required);
CANVAS_RUNTIME_API CanvasStatus canvas_sdk_multi_submit(
    CanvasViewHandle view, const CanvasSdkMultiTokenV1* token,
    CanvasClientId client_id, uint64_t request_id, CanvasSdkMultiPatchMask mask,
    CanvasSdkMultiSubmitFlags flags,
    const CanvasSdkMultiValueV1* value, const CanvasObjectId* expected_eligible,
    uint64_t eligible_count, CanvasSdkMultiReceiptV1* admission);
CANVAS_RUNTIME_API CanvasStatus canvas_sdk_multi_receipt(
    CanvasViewHandle view, CanvasClientId client_id, uint64_t request_id,
    CanvasSdkMultiReceiptV1* receipt);
CANVAS_RUNTIME_API CanvasStatus canvas_sdk_multi_copy_changed(
    CanvasViewHandle view, CanvasClientId client_id, uint64_t request_id,
    CanvasObjectId* objects, uint64_t capacity, uint64_t* required);
CANVAS_RUNTIME_API CanvasStatus canvas_sdk_multi_copy_unchanged(
    CanvasViewHandle view, CanvasClientId client_id, uint64_t request_id,
    CanvasObjectId* objects, uint64_t capacity, uint64_t* required);
CANVAS_RUNTIME_API CanvasStatus canvas_sdk_multi_copy_skipped(
    CanvasViewHandle view, CanvasClientId client_id, uint64_t request_id,
    CanvasSdkMultiExcludedV1* objects, uint64_t capacity, uint64_t* required);
CANVAS_RUNTIME_API CanvasStatus canvas_sdk_multi_release_token(
    CanvasViewHandle view, const CanvasSdkMultiTokenV1* token);

#ifdef __cplusplus
}
#endif
