#include "canvas/semantic/operation_fingerprint.hpp"

namespace canvas::semantic {

bool canonicalPayloadEqual(const Operation& lhs, const Operation& rhs) noexcept {
    return lhs.document_id == rhs.document_id &&
           lhs.schema_version == rhs.schema_version &&
           lhs.payload_version == rhs.payload_version &&
           lhs.payload == rhs.payload;
}

} // namespace canvas::semantic
