#include "candidate-multi-selection-v1.h"

#include <string.h>

/* Future real consumer scenario. The caller supplies an actual view with two
   selected Shapes whose solid strokes have different colors, width != 19,
   and at most 16 dash elements. No fixture, Runtime or SDK is mocked here. */
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

static int capture_stroke(CanvasViewHandle view, CanvasSdkMultiProjectionV1* projection) {
    memset(projection, 0, sizeof(*projection));
    projection->struct_size = (uint32_t)sizeof(*projection);
    projection->abi_version = CANVAS_SDK_MULTI_CANDIDATE_VERSION;
    return canvas_sdk_multi_capture(view, 0x101u, projection) == kCanvasStatusOk;
}

static int root_stroke(CanvasViewHandle view, const CanvasSdkMultiTokenV1* token,
                       CanvasObjectId object, CanvasSdkMultiValueV1* value, double dash[16]) {
    uint64_t required = 0;
    memset(value, 0, sizeof(*value));
    value->struct_size = (uint32_t)sizeof(*value);
    value->abi_version = CANVAS_SDK_MULTI_CANDIDATE_VERSION;
    if (canvas_sdk_multi_copy_root_value(view, token, object, value, dash, 16, &required) != kCanvasStatusOk)
        return 0;
    return value->kind == kCanvasSdkMultiStrokeSolid && required == value->dash_count && required <= 16;
}

static int same_stroke_except_width(const CanvasSdkMultiValueV1* first,
                                    const CanvasSdkMultiValueV1* second,
                                    const double first_dash[16], const double second_dash[16]) {
    uint64_t i;
    if (first->kind != second->kind ||
        memcmp(first->color_rgba, second->color_rgba, sizeof(first->color_rgba)) != 0 ||
        first->stroke_cap != second->stroke_cap || first->stroke_join != second->stroke_join ||
        first->miter_limit != second->miter_limit || first->dash_offset != second->dash_offset ||
        first->dash_count != second->dash_count) return 0;
    for (i = 0; i < first->dash_count; ++i) if (first_dash[i] != second_dash[i]) return 0;
    return 1;
}

int sdk01_candidate_stroke_width(CanvasRuntimeHandle runtime, CanvasViewHandle view,
                                 CanvasClientId client, const CanvasObjectId shapes[2]) {
    CanvasSdkMultiProjectionV1 projection;
    CanvasSdkMultiValueV1 before[2];
    CanvasSdkMultiValueV1 observed[2];
    CanvasSdkMultiValueV1 patch = {0};
    CanvasSdkMultiReceiptV1 receipt = {0};
    CanvasObjectId eligible[2] = {0};
    double before_dash[2][16] = {{0}};
    double observed_dash[2][16] = {{0}};
    uint64_t required = 0;
    unsigned i;

    CHECK(canvas_sdk_multi_set_permission(runtime, kCanvasSdkPermissionLocalSingleUser) == kCanvasStatusOk);
    CHECK(capture_stroke(view, &projection));
    CHECK(projection.permission == kCanvasSdkMultiPermissionAllowed);
    CHECK(projection.availability == kCanvasSdkMultiReady);
    CHECK(projection.total_count == 2 && projection.eligible_count == 2 && projection.excluded_count == 0);
    CHECK(canvas_sdk_multi_copy_eligible(view, &projection.token, eligible, 2, &required) == kCanvasStatusOk);
    CHECK(required == 2);
    for (i = 0; i < 2; ++i) {
        CHECK(memcmp(eligible[i].bytes, shapes[i].bytes, sizeof(shapes[i].bytes)) == 0);
        CHECK(root_stroke(view, &projection.token, shapes[i], &before[i], before_dash[i]));
        CHECK(before[i].stroke_width != 19.0);
    }
    CHECK(memcmp(before[0].color_rgba, before[1].color_rgba, sizeof(before[0].color_rgba)) != 0);

    patch.struct_size = (uint32_t)sizeof(patch);
    patch.abi_version = CANVAS_SDK_MULTI_CANDIDATE_VERSION;
    patch.kind = kCanvasSdkMultiStrokeSolid;
    patch.stroke_width = 19.0;
    receipt.struct_size = (uint32_t)sizeof(receipt);
    receipt.abi_version = CANVAS_SDK_MULTI_CANDIDATE_VERSION;
    CHECK(canvas_sdk_multi_submit(view, &projection.token, client, 1, kCanvasSdkPatchStrokeWidth,
                                 0, &patch, eligible, 2, &receipt) == kCanvasStatusOk);
    CHECK(receipt.state == kCanvasSdkMultiQueued && receipt.terminal == 0);
    CHECK(canvas_sdk_runtime_pump(runtime) == kCanvasStatusOk);
    CHECK(canvas_sdk_multi_receipt(view, client, 1, &receipt) == kCanvasStatusOk);
    CHECK(receipt.state == kCanvasSdkMultiApplied && receipt.changed_count == 2);
    CHECK(capture_stroke(view, &projection));
    for (i = 0; i < 2; ++i) {
        CHECK(root_stroke(view, &projection.token, shapes[i], &observed[i], observed_dash[i]));
        CHECK(observed[i].stroke_width == 19.0);
        CHECK(same_stroke_except_width(&before[i], &observed[i], before_dash[i], observed_dash[i]));
    }

    CHECK(canvas_view_undo(view) == kCanvasStatusOk);
    CHECK(canvas_sdk_runtime_pump(runtime) == kCanvasStatusOk);
    CHECK(capture_stroke(view, &projection));
    for (i = 0; i < 2; ++i) {
        CHECK(root_stroke(view, &projection.token, shapes[i], &observed[i], observed_dash[i]));
        CHECK(observed[i].stroke_width == before[i].stroke_width);
        CHECK(same_stroke_except_width(&before[i], &observed[i], before_dash[i], observed_dash[i]));
    }
    CHECK(canvas_view_redo(view) == kCanvasStatusOk);
    CHECK(canvas_sdk_runtime_pump(runtime) == kCanvasStatusOk);
    CHECK(capture_stroke(view, &projection));
    for (i = 0; i < 2; ++i) {
        CHECK(root_stroke(view, &projection.token, shapes[i], &observed[i], observed_dash[i]));
        CHECK(observed[i].stroke_width == 19.0);
        CHECK(same_stroke_except_width(&before[i], &observed[i], before_dash[i], observed_dash[i]));
    }
    return 0;
}
