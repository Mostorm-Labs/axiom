#pragma once
#include "canvas/ink/compact_contour.hpp"
#include "include/core/SkPathBuilder.h"

namespace canvas::render::internal {
// The same paired quadratic ribbon is used for coverage and acquisition.
inline SkPath compactContourRibbon(std::span<const ink::ContourPatch> patches) {
    SkPathBuilder path;
    if(patches.empty())return path.detach();
    const auto scalar=[](double v){return static_cast<SkScalar>(v);};
    const auto& start=patches.front();
    path.moveTo(scalar(start.leftStart.x),scalar(start.leftStart.y));
    for(const auto& p:patches)
        path.quadTo(scalar(p.leftControl.x),scalar(p.leftControl.y),scalar(p.leftEnd.x),scalar(p.leftEnd.y));
    const auto& last=patches.back();path.lineTo(scalar(last.rightEnd.x),scalar(last.rightEnd.y));
    for(auto i=patches.size();i-->0;){const auto& p=patches[i];
        path.quadTo(scalar(p.rightControl.x),scalar(p.rightControl.y),scalar(p.rightStart.x),scalar(p.rightStart.y));}
    path.close();return path.detach();
}
}
