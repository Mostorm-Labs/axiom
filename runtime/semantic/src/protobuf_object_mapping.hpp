#pragma once

#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/operation.hpp"

namespace auditoryworks::axiom::v1 { class ObjectRecord; class Operation; }

namespace canvas::semantic::internal {
[[nodiscard]] bool toProtobufOperation(const Operation&, auditoryworks::axiom::v1::Operation&);
[[nodiscard]] bool toProtobufObjectRecord(const ObjectRecord&, auditoryworks::axiom::v1::ObjectRecord&);
[[nodiscard]] bool fromProtobufObjectRecord(const auditoryworks::axiom::v1::ObjectRecord&, ObjectRecord&);
#if defined(CANVAS_SEMANTIC_PROTOBUF)
// Private mapper seam: performs only the two mechanical domain/DTO mappings.
// Snapshot validation and normalization deliberately remain outside this seam.
[[nodiscard]] bool roundTripProtobufObjectRecord(const ObjectRecord&, ObjectRecord&);
#endif
}
