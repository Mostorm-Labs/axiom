#pragma once

#include "canvas/semantic/object_record.hpp"
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
    OperationKind kind = OperationKind::kInsertObjects;
};

[[nodiscard]] bool isKnownOperationKind(OperationKind kind) noexcept;

} // namespace canvas::semantic
