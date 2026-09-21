#include "reference_brush_qualification.hpp"

#include <cassert>

int main() {
  using namespace canvas::ink_playground;
  const auto set = referenceBrushQualificationSet();
  assert(set.presets.size() == 5);
  const auto normal = dualColorDiagnostic(DiagnosticPresentation::kNormal, 77);
  const auto diagnostic = dualColorDiagnostic(DiagnosticPresentation::kDualColor, 77);
  assert(normal.arcRgba != normal.canonicalRgba);
  assert(diagnostic.arcRgba != diagnostic.canonicalRgba);
  assert(normal.arcRgba != diagnostic.arcRgba);
  assert(normal.canonicalRgba != diagnostic.canonicalRgba);
  assert(normal.semanticDigest == diagnostic.semanticDigest);
  assert(normal.presentationOnly && diagnostic.presentationOnly);
}
