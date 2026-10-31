#pragma once

#include "canvas/semantic/object_record.hpp"

namespace canvas::semantic::internal {

// Private semantic seam shared by Operation normalization/validation and the
// verification-only Snapshot pipeline.  This deliberately does not enlarge
// the public semantic ABI.
[[nodiscard]] bool normalizeObjectRecord(ObjectRecord& object);
[[nodiscard]] bool validateObjectRecord(const ObjectRecord& object) noexcept;
[[nodiscard]] bool normalizeTableContent(TableContent&);
[[nodiscard]] bool validateTableContent(const TableContent&) noexcept;
[[nodiscard]] bool normalizeSectionContent(SectionContent& content);
[[nodiscard]] bool validateSectionContent(const SectionContent& content) noexcept;

} // namespace canvas::semantic::internal
