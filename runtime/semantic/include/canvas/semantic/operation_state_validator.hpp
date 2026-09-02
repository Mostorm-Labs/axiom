#pragma once

#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/property_value.hpp"
#include "canvas/semantic/stateful_validation.hpp"
#include "canvas/semantic/staged_object_view.hpp"

#include <cstdint>

namespace canvas::semantic {

enum class StateRule : std::uint8_t {
    kCreateAbsent,
    kEditExisting,
    kPlacementTarget,
    kTransformTarget,
    kSizeTarget,
    kVectorPathTarget,
    kImageTarget,
    kStrokeTarget,
    kRichTextTarget,
    kConnectorTarget,
};

[[nodiscard]] StatefulResult requireExisting(const StagedObjectView& staged, ObjectId id);
[[nodiscard]] StatefulResult requireAbsent(const StagedObjectView& staged, ObjectId id);
[[nodiscard]] StatefulResult requireKindVersion(
    const ObjectRecord& record, ObjectKind kind, std::uint32_t kind_version);
[[nodiscard]] StatefulResult requirePropertyApplicability(
    const ObjectRecord& record, std::uint32_t field_id, const PropertyValue* value);
[[nodiscard]] StatefulResult validateRecordStateForOperation(const ObjectRecord& record, StateRule rule);

} // namespace canvas::semantic
