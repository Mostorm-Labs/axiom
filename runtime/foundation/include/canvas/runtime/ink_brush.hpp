#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace canvas::foundation {
inline constexpr std::string_view kInkMaterialResourceId="ink-compact-material-v1";
inline constexpr std::string_view kInkMaterialResourceSha256="de4f95eed22caea216ba99e425ef40f615b32c113801ff545d9d27e4be41ea29";
inline constexpr std::string_view kInkMaterialV2ResourceId="ink-compact-material-v2";
inline constexpr std::string_view kInkMaterialV2ResourceSha256="b0e6138c822bd95da1fdf6b6f87d4b3d3e55f83d7dbbbb2475a993b6e28db104";
inline constexpr std::string_view kInkCrayonV3ResourceId="ink-crayon-material-v3";
inline constexpr std::string_view kInkCrayonV3ResourceSha256="dd0b39a4689fa850609a71f3f8453ca318acd1850cc062ecda9bd0397a5db2e9";
inline constexpr std::string_view kInkDryWetV3ResourceId="ink-chalk-watercolor-v3";
inline constexpr std::string_view kInkDryWetV3ResourceSha256="ad598308c466463170e0841ef6ecb0a144097763057d31cfd0fde1076a40d3e1";
enum class InkFamily : std::uint32_t { kRound=1, kMarker=2, kChalk=3, kCrayon=4, kWatercolor=5, kLaser=6 };
enum class InkPressureSource : std::uint32_t { kSimulated=1, kDevice=2 };
enum class InkEasing : std::uint32_t { kLinear=1, kEaseIn=2, kEaseOut=3, kSmoothstep=4 };
enum class InkPressureModel : std::uint32_t { kTimedSpeed=1, kDistance=2 };
enum class InkRoundMode : std::uint32_t { kRegular=1, kHanddrawn=2 };
struct InkBrushConfig final {
    std::uint32_t version=1, algorithmVersion=1, signalVersion=1, materialVersion=1;
    InkFamily family=InkFamily::kRound;
    InkRoundMode roundMode=InkRoundMode::kHanddrawn;
    InkPressureSource pressureSource=InkPressureSource::kSimulated;
    double size=16, opacity=1;
    double thinning=.5, streamline=.5, smoothing=.5;
    InkEasing easing=InkEasing::kLinear;
    InkPressureModel pressureModel=InkPressureModel::kTimedSpeed;
    double startTaper=0, endTaper=0;
    bool startCap=true, endCap=true;
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
        c.materialResourceSha256==kInkMaterialResourceSha256 && c.materialSamplingVersion==1 && c.materialVersion==1;
    const bool materialV2 = c.materialResourceId==kInkMaterialV2ResourceId &&
        c.materialResourceSha256==kInkMaterialV2ResourceSha256 && c.materialSamplingVersion==1 && c.materialVersion==2;
    const bool crayon=c.family==InkFamily::kCrayon&&c.algorithmVersion==5&&c.signalVersion==3&&c.smoothing==.5&&
        c.materialVersion==3&&c.materialSamplingVersion==1&&c.materialResourceId==kInkCrayonV3ResourceId&&
        c.materialResourceSha256==kInkCrayonV3ResourceSha256;
    const bool drywet=(c.family==InkFamily::kChalk||c.family==InkFamily::kWatercolor)&&c.algorithmVersion==6&&
        c.signalVersion==3&&c.smoothing==.5&&c.materialVersion==4&&c.materialSamplingVersion==1&&
        c.materialResourceId==kInkDryWetV3ResourceId&&c.materialResourceSha256==kInkDryWetV3ResourceSha256;
    const bool legacy=c.version==1 && (c.algorithmVersion==1||c.algorithmVersion==2) && c.signalVersion==1 &&
        c.thinning==.5 && c.streamline==.5 && c.smoothing==.5 && c.easing==InkEasing::kLinear &&
        c.pressureModel==InkPressureModel::kTimedSpeed && c.startTaper==0 && c.endTaper==0 && c.startCap && c.endCap;
    const bool tuned=c.version==2 && ((c.algorithmVersion==3 && c.signalVersion==2)||(c.algorithmVersion==4 && c.signalVersion==3)||crayon||drywet) &&
        (c.family==InkFamily::kRound||c.family==InkFamily::kMarker||crayon||drywet) &&
        std::isfinite(c.thinning)&&c.thinning>=-1&&c.thinning<=1 && unit(c.streamline)&&unit(c.smoothing) &&
        static_cast<unsigned>(c.easing)>=1&&static_cast<unsigned>(c.easing)<=4 &&
        (c.pressureModel==InkPressureModel::kTimedSpeed||c.pressureModel==InkPressureModel::kDistance) &&
        std::isfinite(c.startTaper)&&c.startTaper>=0&&c.startTaper<=4096 &&
        std::isfinite(c.endTaper)&&c.endTaper>=0&&c.endTaper<=4096;
    return (legacy||tuned) && (material||materialV2||crayon||drywet) &&
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
