#pragma once

#include "canvas/semantic/document_id.hpp"
#include "canvas/semantic/operation_payload.hpp"
#include "canvas/semantic/operation_id.hpp"

#include <cstdint>

namespace canvas::semantic {

enum class OperationKind : std::uint8_t {
    kInsertObjects = 1,
    kDeleteObjects = 2,
    kRestoreObjects = 3,
    kSetPlacements = 4,
    kSetTransforms = 5,
    kPatchProperties = 6,
    kSetObjectSize = 7,
    kSetVectorPathGeometry = 8,
    kSetImageContent = 9,
    kAddStroke = 10,
    kSplitStrokes = 11,
    kAddEraseMasks = 12,
    kRemoveEraseMasks = 13,
    kEditRichText = 14,
    kSetConnectorContent = 15,
};

struct Operation final {
    OperationId id{};
    DocumentId document_id{};
    std::uint32_t schema_version = 0;
    std::uint32_t payload_version = 0;
    OperationPayload payload{};

    [[nodiscard]] OperationKind kind() const noexcept;
};

[[nodiscard]] OperationKind operationKind(const OperationPayload&) noexcept;
[[nodiscard]] bool isKnownOperationKind(OperationKind kind) noexcept;

} // namespace canvas::semantic
