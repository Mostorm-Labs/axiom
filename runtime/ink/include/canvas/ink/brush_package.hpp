#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace canvas::ink {

enum class BrushPressureSource : std::uint8_t { kSimulated = 1, kDevice = 2 };
enum class BrushMissingPressure : std::uint8_t { kReject = 1, kHalf = 2 };
enum class BrushStageMode : std::uint8_t { kOff = 1, kAuto = 2, kOn = 3 };

struct BrushVectorConfig final {
  double size = 16.0;
  double thinning = 0.5;
  double smoothing = 0.5;
  double streamline = 0.5;
  BrushPressureSource pressureSource = BrushPressureSource::kSimulated;
  BrushMissingPressure missingPressure = BrushMissingPressure::kReject;
  bool startCap = true;
  bool endCap = true;
};

struct BrushPaintConfig final {
  double red = 0.05;
  double green = 0.10;
  double blue = 0.20;
  double alpha = 1.0;
  double opacity = 1.0;
};

struct BrushPackage final {
  std::string packageId;
  std::uint32_t revision = 1;
  BrushVectorConfig vector;
  BrushPaintConfig paint;
  BrushStageMode inputMode = BrushStageMode::kOn;
  BrushStageMode vectorMode = BrushStageMode::kOn;
  BrushStageMode renderingMode = BrushStageMode::kOn;
};

struct BrushPackageResult final {
  BrushPackage package;
  std::string canonical;
  std::string error;
  explicit operator bool() const noexcept { return error.empty(); }
};

[[nodiscard]] BrushPackageResult parseBrushPackage(std::string_view manifestJson,
                                                   std::string_view pipelineJson);
[[nodiscard]] BrushPackageResult loadBrushPackageFiles(const std::string& manifestPath,
                                                       const std::string& pipelinePath);

}  // namespace canvas::ink
