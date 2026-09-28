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
}  // namespace

BrushPackageResult BrushPackageCatalog::loadDefault(std::string_view profile,
                                                     std::uint32_t revision) const {
  if (revision != 1U || (profile != "vector-solid-v1" && profile != "marker-flat-v1" && profile != "chalk-grain-v1")) {
    return {{}, {}, {}, "unknown_profile_or_revision"};
  }
  if (profile == "marker-flat-v1") return parseBrushPackage(kMarkerManifest, kMarkerPipeline);
  if (profile == "chalk-grain-v1") return parseBrushPackage(kChalkManifest, kChalkPipeline);
  return parseBrushPackage(kManifest, kPipeline);
}

BrushPackageResult BrushPackageCatalog::loadDirectory(const std::string& manifestPath,
                                                       const std::string& pipelinePath) const {
  return loadBrushPackageFiles(manifestPath, pipelinePath);
}

}  // namespace canvas::ink
