#pragma once

#include "canvas/ink/programmable_brush.hpp"
#include "canvas/semantic/object_content.hpp"

#include <optional>

namespace canvas::ink {

// Maps a completed persistent runtime artifact into the existing frozen
// StrokeRecord alternatives. Temporal transients intentionally have no
// canonical representation and return nullopt.
[[nodiscard]] std::optional<semantic::StrokeRecord> toCanonicalStroke(
    const BrushProgram& program, const BrushCommitArtifact& artifact);

}  // namespace canvas::ink
