#include "canvas/ink/brush_package.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <array>
#include <cstdint>
#include <vector>

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
  out << value.profileId << '|' << value.packageId << '|' << value.revision << '|'
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

std::string sha256(std::string_view input) {
  static constexpr std::array<std::uint32_t, 64> k = {
      0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
      0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
      0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
      0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
      0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
      0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
      0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
      0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};
  auto rotr = [](std::uint32_t x, unsigned n) { return (x >> n) | (x << (32U - n)); };
  std::vector<std::uint8_t> data(input.begin(), input.end());
  const auto bits = static_cast<std::uint64_t>(data.size()) * 8U;
  data.push_back(0x80U); while (data.size() % 64U != 56U) data.push_back(0U);
  for (int s = 56; s >= 0; s -= 8) data.push_back(static_cast<std::uint8_t>(bits >> s));
  std::array<std::uint32_t, 8> h = {0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
  for (std::size_t off = 0; off < data.size(); off += 64U) {
    std::array<std::uint32_t, 64> w{};
    for (unsigned i = 0; i < 16U; ++i) w[i] = (static_cast<std::uint32_t>(data[off + i*4U]) << 24U) | (static_cast<std::uint32_t>(data[off + i*4U + 1U]) << 16U) | (static_cast<std::uint32_t>(data[off + i*4U + 2U]) << 8U) | data[off + i*4U + 3U];
    for (unsigned i = 16; i < 64U; ++i) { const auto s0 = rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3); const auto s1 = rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10); w[i] = w[i-16] + s0 + w[i-7] + s1; }
    auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],z=h[7];
    for (unsigned i = 0; i < 64U; ++i) { const auto s1=rotr(e,6)^rotr(e,11)^rotr(e,25); const auto ch=(e&f)^((~e)&g); const auto t1=z+s1+ch+k[i]+w[i]; const auto s0=rotr(a,2)^rotr(a,13)^rotr(a,22); const auto maj=(a&b)^(a&c)^(b&c); const auto t2=s0+maj; z=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2; }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
  }
  std::ostringstream out; out << std::hex << std::setfill('0'); for (auto value : h) out << std::setw(8) << value; return out.str();
}
}  // namespace

std::string brushPackageCanonicalDigest(const BrushPackage& package) {
  return sha256(canonical(package));
}

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
    result.package.profileId = pipeline.value("profile", std::string());
    result.package.revision = manifest.at("revision").get<std::uint32_t>();
    if (result.package.profileId != "vector-solid-v1" ||
        !validId(result.package.packageId) || result.package.revision == 0 ||
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
    result.canonicalDigest = sha256(result.canonical);
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
  if (!manifest || !pipeline) return {{}, {}, {}, "file_open"};
  std::stringstream manifestText;
  std::stringstream pipelineText;
  manifestText << manifest.rdbuf();
  pipelineText << pipeline.rdbuf();
  return parseBrushPackage(manifestText.str(), pipelineText.str());
}

}  // namespace canvas::ink
