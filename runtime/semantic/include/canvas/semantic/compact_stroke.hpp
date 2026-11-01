#pragma once
#include "canvas/runtime/ink_brush.hpp"
#include <span>
#include <vector>

namespace canvas::semantic {
struct CompactStrokeRecord final {
    foundation::InkBrushConfig config{};
    std::uint64_t seed=0;
    std::vector<foundation::InkKnot> knots;
    bool operator==(const CompactStrokeRecord&) const=default;
};
struct CompactStrokeContent final {
    CompactStrokeRecord stroke{};
    bool operator==(const CompactStrokeContent&) const=default;
};
inline bool validCompactStroke(const CompactStrokeRecord& s) noexcept {
    if(!foundation::validInkBrushConfig(s.config,true)||s.knots.empty()||s.knots.size()>262144)return false;
    for(const auto& k:s.knots)if(!std::isfinite(k.x)||!std::isfinite(k.y))return false;
    return true;
}
}
