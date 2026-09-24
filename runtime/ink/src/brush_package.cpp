#include "canvas/ink/brush_package.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace canvas::ink {
namespace {
using Json = nlohmann::json;

bool finite(double value) noexcept { return std::isfinite(value); }

bool validUnit(double value) noexcept {
  return finite(value) && value >= 0.0 && value <= 1.0;
}

bool validId(const std::string& id) noexcept {
  if (id.size() != 32 || id == std::string(32, '0')) return false;
  for (const char value : id) {
    if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'))) return false;
  }
  return true;
}

BrushStageMode parseMode(const Json& stages, const char* name, BrushStageMode fallback) {
  if (!stages.contains(name)) return fallback;
  const auto& stage = stages.at(name);
  if (!stage.is_object() || !stage.contains("mode")) throw std::runtime_error("invalid_stage");
  const auto mode = stage.at("mode").get<std::string>();
  if (mode == "off") return BrushStageMode::kOff;
  if (mode == "auto") return BrushStageMode::kAuto;
  if (mode == "on") return BrushStageMode::kOn;
  throw std::runtime_error("invalid_mode");
}

std::string canonical(const BrushPackage& value) {
  std::ostringstream out;
  out << value.packageId << '|' << value.revision << '|'
      << std::setprecision(17) << value.vector.size << '|'
      << value.vector.thinning << '|' << value.vector.smoothing << '|'
      << value.vector.streamline << '|'
      << static_cast<unsigned>(value.vector.pressureSource) << '|'
      << static_cast<unsigned>(value.vector.missingPressure) << '|'
      << value.vector.startCap << '|' << value.vector.endCap << '|'
      << value.paint.red << '|' << value.paint.green << '|'
      << value.paint.blue << '|' << value.paint.alpha << '|'
      << value.paint.opacity << '|'
      << static_cast<unsigned>(value.inputMode) << '|'
      << static_cast<unsigned>(value.vectorMode) << '|'
      << static_cast<unsigned>(value.renderingMode);
  return out.str();
}
}  // namespace

BrushPackageResult parseBrushPackage(std::string_view manifestJson,
                                     std::string_view pipelineJson) {
  BrushPackageResult result;
  try {
    const auto manifest = Json::parse(manifestJson.begin(), manifestJson.end(), nullptr, true, true);
    const auto pipeline = Json::parse(pipelineJson.begin(), pipelineJson.end(), nullptr, true, true);
    if (!manifest.is_object() || !pipeline.is_object() ||
        manifest.value("schemaVersion", 0) != 1 ||
        pipeline.value("pipelineVersion", 0) != 1 ||
        pipeline.value("defaultsVersion", 0) != 1 ||
        pipeline.value("profile", std::string()) != "vector-solid-v1") {
      result.error = "unsupported_version";
      return result;
    }
    result.package.packageId = manifest.at("packageId").get<std::string>();
    result.package.revision = manifest.at("revision").get<std::uint32_t>();
    if (!validId(result.package.packageId) || result.package.revision == 0 ||
        !manifest.value("resources", Json::array()).empty()) {
      result.error = "invalid_manifest";
      return result;
    }
    const auto stages = pipeline.value("stages", Json::object());
    result.package.inputMode = parseMode(stages, "input", BrushStageMode::kOn);
    result.package.vectorMode = parseMode(stages, "vector", BrushStageMode::kOn);
    result.package.renderingMode = parseMode(stages, "rendering", BrushStageMode::kOn);
    for (const char* name : {"taper", "shape", "grain", "wetMix"}) {
      const auto mode = parseMode(stages, name, BrushStageMode::kOff);
      if (mode == BrushStageMode::kOn) {
        result.error = "unsupported_stage";
        return result;
      }
    }
    const auto vector = pipeline.value("vector", Json::object());
    result.package.vector.size = vector.value("size", 16.0);
    result.package.vector.thinning = vector.value("thinning", 0.5);
    result.package.vector.smoothing = vector.value("smoothing", 0.5);
    result.package.vector.streamline = vector.value("streamline", 0.5);
    const auto pressure = vector.value("pressureSource", std::string("simulated"));
    if (pressure == "simulated") result.package.vector.pressureSource = BrushPressureSource::kSimulated;
    else if (pressure == "device") result.package.vector.pressureSource = BrushPressureSource::kDevice;
    else { result.error = "invalid_pressure_source"; return result; }
    const auto missing = vector.value("missingPressure", std::string("reject"));
    if (missing == "reject") result.package.vector.missingPressure = BrushMissingPressure::kReject;
    else if (missing == "half") result.package.vector.missingPressure = BrushMissingPressure::kHalf;
    else { result.error = "invalid_missing_pressure"; return result; }
    result.package.vector.startCap = vector.value("startCap", true);
    result.package.vector.endCap = vector.value("endCap", true);
    if (vector.value("easing", std::string("linear")) != "linear" ||
        vector.value("startTaper", 0.0) != 0.0 || vector.value("endTaper", 0.0) != 0.0) {
      result.error = "unsupported_vector_option";
      return result;
    }
    const auto paint = pipeline.value("paint", Json::object());
    const auto rgba = paint.value("rgba", std::vector<double>{0.05, 0.1, 0.2, 1.0});
    if (rgba.size() != 4) { result.error = "invalid_paint"; return result; }
    result.package.paint = {rgba[0], rgba[1], rgba[2], rgba[3], paint.value("opacity", 1.0)};
    if (!(finite(result.package.vector.size) && result.package.vector.size > 0.0 &&
          result.package.vector.size <= 4096.0 && finite(result.package.vector.thinning) &&
          result.package.vector.thinning >= -1.0 && result.package.vector.thinning <= 1.0 &&
          validUnit(result.package.vector.smoothing) && validUnit(result.package.vector.streamline) &&
          validUnit(result.package.paint.red) && validUnit(result.package.paint.green) &&
          validUnit(result.package.paint.blue) && validUnit(result.package.paint.alpha) &&
          validUnit(result.package.paint.opacity))) {
      result.error = "invalid_range";
      return result;
    }
    result.canonical = canonical(result.package);
    return result;
  } catch (const std::exception& error) {
    result.error = error.what();
    return result;
  }
}

BrushPackageResult loadBrushPackageFiles(const std::string& manifestPath,
                                         const std::string& pipelinePath) {
  std::ifstream manifest(manifestPath);
  std::ifstream pipeline(pipelinePath);
  if (!manifest || !pipeline) return {{}, {}, "file_open"};
  std::stringstream manifestText;
  std::stringstream pipelineText;
  manifestText << manifest.rdbuf();
  pipelineText << pipeline.rdbuf();
  return parseBrushPackage(manifestText.str(), pipelineText.str());
}

}  // namespace canvas::ink
