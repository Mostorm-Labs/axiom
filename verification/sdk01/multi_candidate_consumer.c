#include "candidate-multi-selection-v1.h"

#include <string.h>

/* Compile-only candidate for a future external SDK test. The caller must
   supply a real runtime/view with exactly three selected Shapes followed by
   two selected Images; each Shape must initially have a non-red fill. This
   function creates no objects, implements no SDK symbol and is not a mock.
   A surrounding real consumer must independently verify object contents,
   History/Undo and the native/Web host migration before it can be a P20
   acceptance oracle. */
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

static int same_id(CanvasObjectId left, CanvasObjectId right) {
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes)) == 0;
}

int sdk01_candidate_partial_fill(CanvasRuntimeHandle runtime, CanvasDocumentHandle document,
                                 CanvasViewHandle view,
                                 CanvasClientId client,
                                 const CanvasObjectId shapes[3],
                                 const CanvasObjectId images[2]) {
    CanvasSdkMultiProjectionV1 projection = {0};
    CanvasSdkMultiReceiptV1 receipt = {0};
    CanvasSdkMultiValueV1 red = {0};
    CanvasObjectId eligible[3] = {0};
    CanvasObjectId changed[3] = {0};
    CanvasSdkMultiExcludedV1 excluded[2] = {0};
    CanvasSdkMultiExcludedV1 skipped[2] = {0};
    CanvasSdkMultiTokenV1 old_token;
    CanvasDocumentStateV1 before = {0};
    CanvasDocumentStateV1 after = {0};
    uint64_t required = 0;
    unsigned i;

    before.struct_size = (uint32_t)sizeof(before);
    before.abi_version = CANVAS_RUNTIME_ABI_VERSION;
    CHECK(canvas_document_get_state(document, &before) == kCanvasStatusOk);
    CHECK(canvas_sdk_multi_set_permission(runtime, kCanvasSdkPermissionUnconfigured) == kCanvasStatusOk);
    projection.struct_size = (uint32_t)sizeof(projection);
    projection.abi_version = CANVAS_SDK_MULTI_CANDIDATE_VERSION;
    CHECK(canvas_sdk_multi_capture(view, 0x100u, &projection) == kCanvasStatusOk);
    CHECK(projection.support == kCanvasSdkMultiSupported);
    CHECK(projection.permission == kCanvasSdkMultiPermissionUnknown);
    CHECK(projection.total_count == 5 && projection.eligible_count == 3 && projection.excluded_count == 2);
    CHECK(projection.partial == 1);
    CHECK(canvas_sdk_multi_copy_eligible(view, &projection.token, eligible, 3, &required) == kCanvasStatusOk);
    CHECK(required == 3);
    for (i = 0; i < 3; ++i) CHECK(same_id(eligible[i], shapes[i]));
    CHECK(canvas_sdk_multi_copy_excluded(view, &projection.token, excluded, 2, &required) == kCanvasStatusOk);
    CHECK(required == 2);
    for (i = 0; i < 2; ++i) {
        CHECK(same_id(excluded[i].object_id, images[i]));
        CHECK(excluded[i].reason == kCanvasSdkMultiNotApplicable);
    }

    red.struct_size = (uint32_t)sizeof(red);
    red.abi_version = CANVAS_SDK_MULTI_CANDIDATE_VERSION;
    red.kind = kCanvasSdkMultiFillSolid;
    red.color_rgba[0] = 1.0f;
    red.color_rgba[3] = 1.0f;
    receipt.struct_size = (uint32_t)sizeof(receipt);
    receipt.abi_version = CANVAS_SDK_MULTI_CANDIDATE_VERSION;
    CHECK(canvas_sdk_multi_submit(view, &projection.token, client, 1,
                                 kCanvasSdkPatchReplaceField, kCanvasSdkAcknowledgePartial,
                                 &red, eligible, 3,
                                 &receipt) == kCanvasStatusOk);
    CHECK(receipt.state == kCanvasSdkMultiRejected);
    CHECK(receipt.failure == kCanvasSdkMultiPermissionUnavailable);
    after.struct_size = (uint32_t)sizeof(after);
    after.abi_version = CANVAS_RUNTIME_ABI_VERSION;
    CHECK(canvas_document_get_state(document, &after) == kCanvasStatusOk);
    CHECK(after.revision == before.revision && after.local_operation_count == before.local_operation_count);

    old_token = projection.token;
    CHECK(canvas_sdk_multi_set_permission(runtime, kCanvasSdkPermissionLocalSingleUser) == kCanvasStatusOk);
    CHECK(canvas_sdk_multi_copy_eligible(view, &old_token, eligible, 3, &required) == kCanvasStatusSequenceError);
    memset(&projection, 0, sizeof(projection));
    projection.struct_size = (uint32_t)sizeof(projection);
    projection.abi_version = CANVAS_SDK_MULTI_CANDIDATE_VERSION;
    CHECK(canvas_sdk_multi_capture(view, 0x100u, &projection) == kCanvasStatusOk);
    CHECK(projection.permission == kCanvasSdkMultiPermissionAllowed);
    CHECK(projection.availability == kCanvasSdkMultiReady);
    CHECK(canvas_sdk_multi_copy_eligible(view, &projection.token, eligible, 3, &required) == kCanvasStatusOk);
    memset(&receipt, 0, sizeof(receipt));
    receipt.struct_size = (uint32_t)sizeof(receipt);
    receipt.abi_version = CANVAS_SDK_MULTI_CANDIDATE_VERSION;
    CHECK(canvas_sdk_multi_submit(view, &projection.token, client, 2,
                                 kCanvasSdkPatchReplaceField, 0, &red, eligible, 3,
                                 &receipt) == kCanvasStatusOk);
    CHECK(receipt.state == kCanvasSdkMultiRejected);
    CHECK(receipt.failure == kCanvasSdkMultiInvalidValue);
    CHECK(canvas_document_get_state(document, &after) == kCanvasStatusOk);
    CHECK(after.revision == before.revision && after.local_operation_count == before.local_operation_count);
    memset(&receipt, 0, sizeof(receipt));
    receipt.struct_size = (uint32_t)sizeof(receipt);
    receipt.abi_version = CANVAS_SDK_MULTI_CANDIDATE_VERSION;
    CHECK(canvas_sdk_multi_submit(view, &projection.token, client, 3,
                                 kCanvasSdkPatchReplaceField, kCanvasSdkAcknowledgePartial,
                                 &red, eligible, 3,
                                 &receipt) == kCanvasStatusOk);
    CHECK(receipt.state == kCanvasSdkMultiQueued && receipt.terminal == 0 && receipt.changed_count == 0);
    CHECK(canvas_sdk_runtime_pump(runtime) == kCanvasStatusOk);
    memset(&receipt, 0, sizeof(receipt));
    receipt.struct_size = (uint32_t)sizeof(receipt);
    receipt.abi_version = CANVAS_SDK_MULTI_CANDIDATE_VERSION;
    CHECK(canvas_sdk_multi_receipt(view, client, 3, &receipt) == kCanvasStatusOk);
    CHECK(receipt.state == kCanvasSdkMultiApplied && receipt.terminal == 1);
    CHECK(receipt.changed_count == 3 && receipt.unchanged_count == 0 && receipt.skipped_count == 2);
    CHECK(canvas_sdk_multi_copy_changed(view, client, 3, changed, 3, &required) == kCanvasStatusOk);
    CHECK(required == 3);
    for (i = 0; i < 3; ++i) CHECK(same_id(changed[i], shapes[i]));
    CHECK(canvas_sdk_multi_copy_skipped(view, client, 3, skipped, 2, &required) == kCanvasStatusOk);
    CHECK(required == 2);
    for (i = 0; i < 2; ++i) {
        CHECK(same_id(skipped[i].object_id, images[i]));
        CHECK(skipped[i].reason == kCanvasSdkMultiNotApplicable);
    }
    return 0;
}
