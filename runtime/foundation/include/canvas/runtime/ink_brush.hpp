#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace canvas::foundation {
inline constexpr std::string_view kInkMaterialResourceId="ink-compact-material-v1";
inline constexpr std::string_view kInkMaterialResourceSha256="de4f95eed22caea216ba99e425ef40f615b32c113801ff545d9d27e4be41ea29";
enum class InkFamily : std::uint32_t { kRound=1, kMarker=2, kChalk=3, kCrayon=4, kWatercolor=5, kLaser=6 };
enum class InkPressureSource : std::uint32_t { kSimulated=1, kDevice=2 };
enum class InkRoundMode : std::uint32_t { kRegular=1, kHanddrawn=2 };
struct InkBrushConfig final {
    std::uint32_t version=1, algorithmVersion=1, signalVersion=1, materialVersion=1;
    InkFamily family=InkFamily::kRound;
    InkRoundMode roundMode=InkRoundMode::kHanddrawn;
    InkPressureSource pressureSource=InkPressureSource::kSimulated;
    double size=16, opacity=1;
    std::array<double,4> color{.05,.1,.2,1};
    double markerAngle=.7853981633974483, markerFlatness=.25;
    double density=.65, textureScale=8, deposition=.8, wetEdge=.4;
    std::uint32_t laserFadeMs=1000;
    double laserTailLength=300;
    std::string materialResourceId{ kInkMaterialResourceId };
    std::string materialResourceSha256{ kInkMaterialResourceSha256 };
    std::uint32_t materialSamplingVersion=1;
    bool operator==(const InkBrushConfig&) const=default;
};
inline bool validInkBrushConfig(const InkBrushConfig& c, bool persistent=false) noexcept {
    const auto unit=[](double x){return std::isfinite(x)&&x>=0&&x<=1;};
    const auto family=static_cast<unsigned>(c.family);
    const bool material = c.materialResourceId==kInkMaterialResourceId &&
        c.materialResourceSha256==kInkMaterialResourceSha256 && c.materialSamplingVersion==1;
    return c.version==1 && c.algorithmVersion==1 && c.signalVersion==1 && c.materialVersion==1 && material &&
        family>=1 && family<=(persistent?5U:6U) &&
        (c.roundMode==InkRoundMode::kRegular||c.roundMode==InkRoundMode::kHanddrawn) &&
        (c.pressureSource==InkPressureSource::kSimulated||c.pressureSource==InkPressureSource::kDevice) &&
        std::isfinite(c.size)&&c.size>0&&c.size<=4096 && unit(c.opacity) &&
        unit(c.color[0])&&unit(c.color[1])&&unit(c.color[2])&&unit(c.color[3]) &&
        std::isfinite(c.markerAngle)&&std::abs(c.markerAngle)<=6.283185307179586 &&
        std::isfinite(c.markerFlatness)&&c.markerFlatness>=.05&&c.markerFlatness<=1 &&
        unit(c.density)&&std::isfinite(c.textureScale)&&c.textureScale>=.25&&c.textureScale<=1024 &&
        unit(c.deposition)&&unit(c.wetEdge)&&c.laserFadeMs>=50&&c.laserFadeMs<=10000 &&
        std::isfinite(c.laserTailLength)&&c.laserTailLength>0&&c.laserTailLength<=10000;
}
struct InkKnot final {
    double x=0,y=0;
    std::uint16_t pressure=32768;
    bool operator==(const InkKnot&) const=default;
};
} // namespace canvas::foundation
