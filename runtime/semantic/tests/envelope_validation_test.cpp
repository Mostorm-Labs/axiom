#include "canvas/semantic/validator.hpp"

#include <gtest/gtest.h>

namespace canvas::semantic {
namespace {

Operation minimalOperation() {
    Operation operation;
    operation.id = OperationId{ObjectId::fromUint64(1U)};
    operation.document_id = DocumentId{ObjectId::fromUint64(2U)};
    operation.payload = DeleteObjectsOp{{ObjectId::fromUint64(3U)}};
    return operation;
}

TEST(EnvelopeValidation, AcceptsOnlyExplicitVersionOne) {
    Operation operation = minimalOperation();
    operation.schema_version = 1U;
    operation.payload_version = 1U;
    EXPECT_TRUE(validateEnvelope(operation, {.schema_version = true, .payload_version = true}).ok());

    EXPECT_FALSE(validateEnvelope(operation, {.schema_version = false, .payload_version = true}).ok());
    EXPECT_FALSE(validateEnvelope(operation, {.schema_version = true, .payload_version = false}).ok());

    operation.schema_version = 0U;
    EXPECT_FALSE(validateEnvelope(operation, {.schema_version = true, .payload_version = true}).ok());
    operation.schema_version = 2U;
    EXPECT_FALSE(validateEnvelope(operation, {.schema_version = true, .payload_version = true}).ok());

    operation.schema_version = 1U;
    operation.payload_version = 0U;
    EXPECT_FALSE(validateEnvelope(operation, {.schema_version = true, .payload_version = true}).ok());
    operation.payload_version = 2U;
    EXPECT_FALSE(validateEnvelope(operation, {.schema_version = true, .payload_version = true}).ok());
}

TEST(EnvelopeValidation, RejectsZeroOperationAndDocumentIds) {
    Operation operation = minimalOperation();
    operation.schema_version = 1U;
    operation.payload_version = 1U;
    const OperationFieldPresence present{true, true};
    operation.id = OperationId{};
    EXPECT_FALSE(validateEnvelope(operation, present).ok());
    operation = minimalOperation();
    operation.schema_version = 1U;
    operation.payload_version = 1U;
    operation.document_id = DocumentId{};
    EXPECT_FALSE(validateEnvelope(operation, present).ok());
}

} // namespace
} // namespace canvas::semantic
