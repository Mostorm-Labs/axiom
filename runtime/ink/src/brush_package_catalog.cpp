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
}  // namespace

BrushPackageResult BrushPackageCatalog::loadDefault(std::string_view profile,
                                                     std::uint32_t revision) const {
  if (profile != "vector-solid-v1" || revision != 1U) {
    return {{}, {}, {}, "unknown_profile_or_revision"};
  }
  return parseBrushPackage(kManifest, kPipeline);
}

BrushPackageResult BrushPackageCatalog::loadDirectory(const std::string& manifestPath,
                                                       const std::string& pipelinePath) const {
  return loadBrushPackageFiles(manifestPath, pipelinePath);
}

}  // namespace canvas::ink
