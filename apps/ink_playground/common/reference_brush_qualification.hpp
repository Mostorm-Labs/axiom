#pragma once

#include "reference_brushes.hpp"

#include <cstdint>

namespace canvas::ink_playground {

enum class DiagnosticPresentation : std::uint8_t {
  kNormal = 0,
  kDualColor = 1,
};

struct DualColorDiagnostic final {
  std::uint32_t arcRgba = 0;
  std::uint32_t canonicalRgba = 0;
  std::uint64_t semanticDigest = 0;
  bool presentationOnly = true;
};

[[nodiscard]] DualColorDiagnostic dualColorDiagnostic(
    DiagnosticPresentation presentation, std::uint64_t semanticDigest);
[[nodiscard]] brush_lab::ReferenceBrushSet referenceBrushQualificationSet();

}  // namespace canvas::ink_playground
