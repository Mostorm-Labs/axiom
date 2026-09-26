#pragma once

#include "canvas/ink/brush_commit_intent.hpp"
#include "canvas/ink/brush_package.hpp"
#include "canvas/ink/resolved_brush_state.hpp"
#include "canvas/semantic/document_id.hpp"
#include "canvas/semantic/operation.hpp"

#include <cstdint>

namespace canvas::ink_playground {

// Runtime-core-only adapter. BrushSession produces a transient, confirmed-only
// intent; this adapter is the sole place that materializes semantic identity
// and the AddStroke v2 envelope.
class BrushCommitAdapter final {
 public:
  [[nodiscard]] static bool valid(
      const ink::BrushCommitIntent& intent,
      const ink::BrushPackage& package) noexcept;
  [[nodiscard]] static semantic::Operation build(
      const ink::BrushCommitIntent& intent,
      const ink::BrushPackage& package,
      std::uint64_t operationId,
      semantic::DocumentId documentId) noexcept;
};

}  // namespace canvas::ink_playground
