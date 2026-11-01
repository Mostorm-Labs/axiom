#include "candidate-multi-selection-v1.h"

#include <stddef.h>
#include <stdio.h>

/* Measures compiler layout of the draft declarations only. It does not link
   the Runtime or imply an accepted ABI or a product SDK implementation. */
int main(void) {
    printf("{\"pointer_size\":%zu,\"object_id_size\":%zu,\"token_size\":%zu,"
           "\"excluded_size\":%zu,\"excluded_reason_offset\":%zu,"
           "\"projection_size\":%zu,\"projection_token_offset\":%zu,"
           "\"projection_total_offset\":%zu,\"value_size\":%zu,"
           "\"value_dash_pointer_offset\":%zu,\"value_dash_count_offset\":%zu,"
           "\"receipt_size\":%zu,\"receipt_counts_offset\":%zu}\n",
           sizeof(void*), sizeof(CanvasObjectId), sizeof(CanvasSdkMultiTokenV1),
           sizeof(CanvasSdkMultiExcludedV1), offsetof(CanvasSdkMultiExcludedV1, reason),
           sizeof(CanvasSdkMultiProjectionV1), offsetof(CanvasSdkMultiProjectionV1, token),
           offsetof(CanvasSdkMultiProjectionV1, total_count), sizeof(CanvasSdkMultiValueV1),
           offsetof(CanvasSdkMultiValueV1, dash_segments), offsetof(CanvasSdkMultiValueV1, dash_count),
           sizeof(CanvasSdkMultiReceiptV1), offsetof(CanvasSdkMultiReceiptV1, changed_count));
    return 0;
}
