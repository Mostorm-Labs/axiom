#pragma once

#include "canvas/semantic/object_record.hpp"

namespace canvas::semantic::internal {

// Private semantic seam shared by Operation normalization/validation and the
// verification-only Snapshot pipeline.  This deliberately does not enlarge
// the public semantic ABI.
[[nodiscard]] bool normalizeObjectRecord(ObjectRecord& object);
[[nodiscard]] bool validateObjectRecord(const ObjectRecord& object) noexcept;

} // namespace canvas::semantic::internal
