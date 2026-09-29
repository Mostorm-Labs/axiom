#include "canvas/ink/brush_package_catalog.hpp"

namespace canvas::ink {
namespace {
constexpr std::string_view kManifest = R"json({
  "schemaVersion": 1,
  "packageId": "00000000000000000000000000000042",
  "revision": 1,
  "pipeline": "pipeline.json",
  "resources": [],
  "metadata": {"name": "Vector Ink"}
})json";
constexpr std::string_view kPipeline = R"json({
  "pipelineVersion": 1,
  "defaultsVersion": 1,
  "profile": "vector-solid-v1",
  "stages": {
    "input": {"mode": "on"}, "vector": {"mode": "on"},
    "taper": {"mode": "off"}, "shape": {"mode": "off"},
    "grain": {"mode": "off"}, "rendering": {"mode": "on"},
    "wetMix": {"mode": "off"}
  },
  "vector": {
    "size": 16, "thinning": 0.5, "smoothing": 0.5,
    "streamline": 0.5, "pressureSource": "simulated",
    "missingPressure": "reject", "easing": "linear",
    "startCap": true, "endCap": true, "startTaper": 0, "endTaper": 0
  },
  "paint": {"rgba": [0.05, 0.1, 0.2, 1], "opacity": 1,
            "blend": "source-over", "colorSpace": "srgb"}
})json";
constexpr std::string_view kMarkerManifest = R"json({
  "schemaVersion": 1, "packageId": "00000000000000000000000000000043", "revision": 1,
  "pipeline": "pipeline.json", "resources": [], "metadata": {"name": "Marker Flat"}
})json";
constexpr std::string_view kMarkerPipeline = R"json({
  "pipelineVersion": 1, "defaultsVersion": 1, "profile": "marker-flat-v1",
  "stages": {"input":{"mode":"on"},"vector":{"mode":"on"},"taper":{"mode":"off"},"shape":{"mode":"on"},"grain":{"mode":"off"},"rendering":{"mode":"on"},"wetMix":{"mode":"off"}},
  "vector": {"size": 16, "thinning": 0.5, "smoothing": 0.5, "streamline": 0.5, "pressureSource":"simulated", "missingPressure":"reject", "easing":"linear", "startCap":true, "endCap":true, "startTaper":0, "endTaper":0},
  "marker": {"headAngle": 0.78539816339, "headWidth": 1.0},
  "paint": {"rgba":[0.1,0.2,0.8,0.7],"opacity":0.7,"blend":"source-over","colorSpace":"srgb"}
})json";
constexpr std::string_view kChalkManifest = R"json({
  "schemaVersion": 1, "packageId": "00000000000000000000000000000044", "revision": 1,
  "pipeline": "pipeline.json", "resources": [{"id":"chalk-grain-default","sha256":"d5309d8989c2ad9bcfb0dd61a1ebc1306b706cabd00437ddcd9636599920f8c7","version":1}], "metadata": {"name": "Chalk Grain"}
})json";
constexpr std::string_view kChalkPipeline = R"json({
  "pipelineVersion": 1, "defaultsVersion": 1, "profile": "chalk-grain-v1",
  "stages": {"input":{"mode":"on"},"vector":{"mode":"on"},"taper":{"mode":"off"},"shape":{"mode":"off"},"grain":{"mode":"on"},"rendering":{"mode":"on"},"wetMix":{"mode":"off"}},
  "vector": {"size": 16, "thinning": 0.5, "smoothing": 0.5, "streamline": 0.5, "pressureSource":"simulated", "missingPressure":"reject", "easing":"linear", "startCap":true, "endCap":true, "startTaper":0, "endTaper":0},
  "grain": {"resourceId":"chalk-grain-default","sha256":"d5309d8989c2ad9bcfb0dd61a1ebc1306b706cabd00437ddcd9636599920f8c7","version":1,"density":0.65,"spacing":2.0,"opacity":0.55},
  "paint": {"rgba":[0.12,0.1,0.08,0.8],"opacity":0.8,"blend":"source-over","colorSpace":"srgb"}
})json";
constexpr std::string_view kChalkV2Manifest = R"json({
  "schemaVersion": 1, "packageId": "00000000000000000000000000000045", "revision": 2,
  "pipeline": "pipeline.json", "resources": [{"id":"chalk-grain-default","sha256":"d5309d8989c2ad9bcfb0dd61a1ebc1306b706cabd00437ddcd9636599920f8c7","version":1}], "metadata": {"name": "Chalk Grain Dab"}
})json";
constexpr std::string_view kChalkV3Manifest = R"json({
  "schemaVersion": 1, "packageId": "00000000000000000000000000000046", "revision": 3,
  "pipeline": "pipeline.json", "resources": [
    {"id":"chalk-shape-screenshot-extract-v1","sha256":"4171df4f399e11b7bc44fa9c688c19f7517622b95c845d63eadda91bbd8feaae","version":1,"kind":"shape"},
    {"id":"chalk-grain-screenshot-extract-v1","sha256":"d6c0834bd8e9992c3ca448ca5747b76db4ec7f79cd3d82167273c7d71e1bd520","version":1,"kind":"grain"}
  ], "metadata": {"name": "Chalk Grain Screenshot Extract (provisional)"}
})json";
constexpr std::string_view kChalkV3Pipeline = R"json({
  "pipelineVersion": 1, "defaultsVersion": 1, "profile": "chalk-grain-v1",
  "stages": {"input":{"mode":"on"},"vector":{"mode":"on"},"taper":{"mode":"on"},"shape":{"mode":"on"},"grain":{"mode":"on"},"rendering":{"mode":"on"},"wetMix":{"mode":"off"}},
  "vector": {"size": 16, "thinning": 0.5, "smoothing": 0.4314516, "streamline": 0.4314516, "pressureSource":"simulated", "missingPressure":"reject", "easing":"linear", "startCap":true, "endCap":true, "startTaper":0.3717391, "endTaper":0.2913043},
  "shape": {"resourceId":"chalk-shape-screenshot-extract-v1","sha256":"4171df4f399e11b7bc44fa9c688c19f7517622b95c845d63eadda91bbd8feaae","version":1},
  "grain": {"resourceId":"chalk-grain-screenshot-extract-v1","sha256":"d6c0834bd8e9992c3ca448ca5747b76db4ec7f79cd3d82167273c7d71e1bd520","version":1,"density":0.90,"spacing":2.0,"opacity":0.80},
  "paint": {"rgba":[0.12,0.1,0.08,0.8],"opacity":0.8,"blend":"source-over","colorSpace":"srgb"}
})json";
constexpr std::string_view kChalkV4Manifest = R"json({
  "schemaVersion": 1, "packageId": "00000000000000000000000000000047", "revision": 4,
  "pipeline": "pipeline.json", "resources": [
    {"id":"chalk-shape-screenshot-extract-v2","sha256":"d88a0c58d904a5041bed7f1d79273f4dadcbd92b31e0bb76a0c5dc376ecf594a","version":2,"kind":"shape"},
    {"id":"chalk-grain-screenshot-extract-v2","sha256":"b0770b02eaaf52869ea4dbe16b034c32d59149fbece1a9abf21fec55712a8fe6","version":2,"kind":"grain"}
  ], "metadata": {"name": "Chalk Grain Continuous Coverage"}
})json";
constexpr std::string_view kChalkV4Pipeline = R"json({
  "pipelineVersion": 1, "defaultsVersion": 1, "profile": "chalk-grain-v1",
  "stages": {"input":{"mode":"on"},"vector":{"mode":"on"},"taper":{"mode":"on"},"shape":{"mode":"on"},"grain":{"mode":"on"},"rendering":{"mode":"on"},"wetMix":{"mode":"off"}},
  "vector": {"size": 16, "thinning": 0.5, "smoothing": 0.4314516, "streamline": 0.4314516, "pressureSource":"simulated", "missingPressure":"reject", "easing":"linear", "startCap":true, "endCap":true, "startTaper":0.3717391, "endTaper":0.2913043},
  "shape": {"resourceId":"chalk-shape-screenshot-extract-v2","sha256":"d88a0c58d904a5041bed7f1d79273f4dadcbd92b31e0bb76a0c5dc376ecf594a","version":2},
  "grain": {"resourceId":"chalk-grain-screenshot-extract-v2","sha256":"b0770b02eaaf52869ea4dbe16b034c32d59149fbece1a9abf21fec55712a8fe6","version":2,"density":0.90,"spacing":2.0,"opacity":0.80},
  "paint": {"rgba":[0.12,0.1,0.08,0.8],"opacity":0.8,"blend":"source-over","colorSpace":"srgb"}
})json";
constexpr std::string_view kMembraneManifest = R"json({
  "schemaVersion": 1, "packageId": "00000000000000000000000000000048", "revision": 1,
  "pipeline": "pipeline.json", "resources": [
    {"id":"membrane-shape","sha256":"5660f0e297ff65ccab4e8e9ffcf918523bb44157ff8770c1dffd0379e1e7b34e","version":1,"kind":"shape"},
    {"id":"membrane-grain","sha256":"1e25a30049035541827d7e7b9ea24a785f707a3b9f877a363abcb4efd8ed5007","version":1,"kind":"grain"}
  ], "metadata": {"name": "Membrane"}
})json";
constexpr std::string_view kMembranePipeline = R"json({
  "pipelineVersion": 1, "defaultsVersion": 1, "profile": "membrane-v1",
  "stages": {"input":{"mode":"on"},"vector":{"mode":"on"},"taper":{"mode":"off"},"shape":{"mode":"on"},"grain":{"mode":"on"},"rendering":{"mode":"on"},"wetMix":{"mode":"off"}},
  "vector": {"size":16,"thinning":0,"smoothing":0,"streamline":0,"pressureSource":"device","missingPressure":"half","easing":"linear","startCap":true,"endCap":true,"startTaper":0,"endTaper":0},
  "shape": {"resourceId":"membrane-shape","sha256":"5660f0e297ff65ccab4e8e9ffcf918523bb44157ff8770c1dffd0379e1e7b34e","version":1},
  "grain": {"resourceId":"membrane-grain","sha256":"1e25a30049035541827d7e7b9ea24a785f707a3b9f877a363abcb4efd8ed5007","version":1,"density":1,"spacing":0.017490354098603473,"opacity":1},
  "paint": {"rgba":[0.05,0.1,0.2,1],"opacity":0.5,"blend":"source-over","colorSpace":"srgb"}
})json";
}  // namespace

BrushPackageResult BrushPackageCatalog::loadDefault(std::string_view profile,
                                                     std::uint32_t revision) const {
  if ((revision != 1U && !(profile == "chalk-grain-v1" && (revision == 2U || revision == 3U || revision == 4U))) ||
      (profile != "vector-solid-v1" && profile != "marker-flat-v1" && profile != "chalk-grain-v1" && profile != "membrane-v1")) {
    return {{}, {}, {}, "unknown_profile_or_revision"};
  }
  if (profile == "marker-flat-v1") return parseBrushPackage(kMarkerManifest, kMarkerPipeline);
  if (profile == "membrane-v1") return parseBrushPackage(kMembraneManifest, kMembranePipeline);
  if (profile == "chalk-grain-v1" && revision == 2U) return parseBrushPackage(kChalkV2Manifest, kChalkPipeline);
  if (profile == "chalk-grain-v1" && revision == 3U) return parseBrushPackage(kChalkV3Manifest, kChalkV3Pipeline);
  if (profile == "chalk-grain-v1" && revision == 4U) return parseBrushPackage(kChalkV4Manifest, kChalkV4Pipeline);
  if (profile == "chalk-grain-v1") return parseBrushPackage(kChalkManifest, kChalkPipeline);
  return parseBrushPackage(kManifest, kPipeline);
}

BrushPackageResult BrushPackageCatalog::loadDirectory(const std::string& manifestPath,
                                                       const std::string& pipelinePath) const {
  return loadBrushPackageFiles(manifestPath, pipelinePath);
}

}  // namespace canvas::ink
