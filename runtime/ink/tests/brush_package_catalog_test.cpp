#include "canvas/ink/brush_package_catalog.hpp"

#include <cassert>

int main() {
  canvas::ink::BrushPackageCatalog catalog;
  const auto loaded = catalog.loadDefault("vector-solid-v1", 1);
  assert(loaded);
  assert(loaded.package.profileId == "vector-solid-v1");
  assert(loaded.package.packageId == "00000000000000000000000000000042");
  assert(loaded.package.revision == 1);
  assert(loaded.package.vector.pressureSource == canvas::ink::BrushPressureSource::kSimulated);
  assert(loaded.package.vector.missingPressure == canvas::ink::BrushMissingPressure::kReject);
  assert(!loaded.canonical.empty());
  assert(!catalog.loadDefault("vector-solid-v1", 2));
  assert(!catalog.loadDefault("other", 1));
  return 0;
}
