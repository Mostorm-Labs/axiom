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
  const auto marker = catalog.loadDefault("marker-flat-v1", 1);
  assert(marker);
  assert(marker.package.profileId == "marker-flat-v1");
  const auto chalk = catalog.loadDefault("chalk-grain-v1", 1);
  assert(chalk);
  assert(chalk.package.profileId == "chalk-grain-v1");
  const auto chalkV2 = catalog.loadDefault("chalk-grain-v1", 2);
  assert(chalkV2);
  assert(chalkV2.package.revision == 2U);
  assert(chalkV2.canonicalDigest != chalk.canonicalDigest);
  const auto chalkV3 = catalog.loadDefault("chalk-grain-v1", 3);
  assert(chalkV3);
  assert(chalkV3.package.revision == 3U);
  assert(chalkV3.package.shape.resourceId == "chalk-shape-screenshot-extract-v1");
  assert(chalkV3.package.shape.resourceSha256 == "4171df4f399e11b7bc44fa9c688c19f7517622b95c845d63eadda91bbd8feaae");
  assert(chalkV3.package.grain.resourceId == "chalk-grain-screenshot-extract-v1");
  assert(chalkV3.package.grain.resourceSha256 == "d6c0834bd8e9992c3ca448ca5747b76db4ec7f79cd3d82167273c7d71e1bd520");
  assert(chalkV3.package.shape.resourceSha256 != chalkV3.package.grain.resourceSha256);
  assert(chalkV3.canonicalDigest != chalkV2.canonicalDigest);
  const auto chalkV4 = catalog.loadDefault("chalk-grain-v1", 4);
  assert(chalkV4);
  assert(chalkV4.package.revision == 4U);
  assert(chalkV4.package.shape.resourceId == "chalk-shape-screenshot-extract-v2");
  assert(chalkV4.package.grain.resourceId == "chalk-grain-screenshot-extract-v2");
  assert(chalkV4.canonicalDigest != chalkV3.canonicalDigest);
  assert(!catalog.loadDefault("chalk-grain-v1", 5));
  const auto membrane = catalog.loadDefault("membrane-v1", 1);
  assert(membrane);
  assert(membrane.package.profileId == "membrane-v1");
  assert(membrane.package.material == canvas::ink::BrushMaterialMode::kMembrane);
  assert(membrane.package.shape.resourceId == "membrane-shape");
  assert(membrane.package.grain.resourceId == "membrane-grain");
  assert(membrane.package.shape.resourceSha256 == "5660f0e297ff65ccab4e8e9ffcf918523bb44157ff8770c1dffd0379e1e7b34e");
  assert(membrane.package.grain.resourceSha256 == "1e25a30049035541827d7e7b9ea24a785f707a3b9f877a363abcb4efd8ed5007");
  assert(membrane.canonicalDigest != chalkV4.canonicalDigest);
  assert(!catalog.loadDefault("membrane-v1", 2));
  assert(marker.canonicalDigest != chalk.canonicalDigest);
  assert(marker.canonicalDigest != loaded.canonicalDigest);
  assert(!catalog.loadDefault("other", 1));
  return 0;
}
