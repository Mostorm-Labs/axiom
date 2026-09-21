#include "reference_brush_qualification.hpp"

namespace canvas::ink_playground {

DualColorDiagnostic dualColorDiagnostic(DiagnosticPresentation presentation,
                                        std::uint64_t semanticDigest) {
  if (presentation == DiagnosticPresentation::kDualColor) {
    return {.arcRgba = 0xff19c7e8U,
            .canonicalRgba = 0xffef3d74U,
            .semanticDigest = semanticDigest,
            .presentationOnly = true};
  }
  return {.arcRgba = 0xff28bee6U,
          .canonicalRgba = 0xff1a5bffU,
          .semanticDigest = semanticDigest,
          .presentationOnly = true};
}

brush_lab::ReferenceBrushSet referenceBrushQualificationSet() {
  return brush_lab::makeReferenceBrushSet();
}

}  // namespace canvas::ink_playground
