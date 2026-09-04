#pragma once

#include "canvas/semantic/object_record.hpp"

namespace auditoryworks::axiom::v1 { class ObjectRecord; }

namespace canvas::semantic::internal {
[[nodiscard]] bool toProtobufObjectRecord(const ObjectRecord&, auditoryworks::axiom::v1::ObjectRecord&);
[[nodiscard]] bool fromProtobufObjectRecord(const auditoryworks::axiom::v1::ObjectRecord&, ObjectRecord&);
}
