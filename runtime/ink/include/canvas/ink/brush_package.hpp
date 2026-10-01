#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace canvas::ink {

enum class BrushPressureSource : std::uint8_t { kSimulated = 1, kDevice = 2 };
enum class BrushMissingPressure : std::uint8_t { kReject = 1, kHalf = 2 };
enum class BrushStageMode : std::uint8_t { kOff = 1, kAuto = 2, kOn = 3 };
enum class BrushMaterialMode : std::uint8_t {
  kSolidVector = 1,
  kMarkerFlat = 2,
  kChalkGrain = 3,
  kMembrane = 4,
};

struct BrushVectorConfig final {
  double size = 16.0;
  double thinning = 0.5;
  double smoothing = 0.5;
  double streamline = 0.5;
  BrushPressureSource pressureSource = BrushPressureSource::kSimulated;
  BrushMissingPressure missingPressure = BrushMissingPressure::kReject;
  bool startCap = true;
  bool endCap = true;
  double startTaper = 0.0;
  double endTaper = 0.0;
};
struct BrushPaintConfig final {
  double red = 0.05;
  double green = 0.10;
  double blue = 0.20;
  double alpha = 1.0;
  double opacity = 1.0;
};

struct BrushMarkerConfig final {
  double headAngle = 0.0;
  double headWidth = 1.0;
};

struct BrushShapeConfig final {
  std::string resourceId;
  std::string resourceSha256;
  std::uint32_t resourceVersion = 0;
};

struct BrushGrainConfig final {
  std::string resourceId;
  std::string resourceSha256;
  std::uint32_t resourceVersion = 0;
  double density = 0.0;
  double spacing = 0.0;
  double opacity = 1.0;
};

struct BrushPackage final {
  // Stable logical selector. This is intentionally distinct from the
  // canonical 32-character manifest package identity.
  std::string profileId = "vector-solid-v1";
  std::string packageId;
  std::uint32_t revision = 1;
  BrushVectorConfig vector;
  BrushPaintConfig paint;
  BrushMaterialMode material = BrushMaterialMode::kSolidVector;
  BrushMarkerConfig marker;
  BrushShapeConfig shape;
  BrushGrainConfig grain;
  BrushStageMode inputMode = BrushStageMode::kOn;
  BrushStageMode vectorMode = BrushStageMode::kOn;
  BrushStageMode renderingMode = BrushStageMode::kOn;
};

struct BrushPackageResult final {
  BrushPackage package;
  std::string canonical;
  std::string canonicalDigest;
  std::string error;
  explicit operator bool() const noexcept { return error.empty(); }
};

// SHA-256 of the complete canonical package representation. This is the
// cross-platform package identity; it is deliberately not a truncated/FNV
// runtime counter.
[[nodiscard]] std::string brushPackageCanonicalDigest(const BrushPackage& package);

[[nodiscard]] BrushPackageResult parseBrushPackage(std::string_view manifestJson,
                                                   std::string_view pipelineJson);
[[nodiscard]] BrushPackageResult loadBrushPackageFiles(const std::string& manifestPath,
                                                       const std::string& pipelinePath);

}  // namespace canvas::ink
