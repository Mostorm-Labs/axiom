#pragma once
#include "canvas/render/reference_draw_list.hpp"
#include <array>
#include <optional>
#include <span>

namespace canvas::render::internal {
// Derived feedback validity only. Exhaustion drops validity, never fills holes.
struct NavigationRegions final {
    std::array<foundation::WorldRect,8> areas{};
    std::size_t count=0;
    void reset(foundation::WorldRect r){areas[0]=r;count=1;}
    bool full(foundation::WorldRect r)const{return count==1&&areas[0]==r;}
    bool add(foundation::WorldRect);
    bool subtract(foundation::WorldRect);
};
struct MaskPixelDifference final {
    bool compatible=false;
    std::optional<foundation::WorldRect> damage;
};
// Both lists must have passed production reference validation and describe the
// same queried world region. Full body/effect/order equality is required; only
// permanent mask deltas on whole-geometry v4/outline sources may be localized.
// No geometry/raster algorithm is implemented here: bounds are conservative
// affine images of mask control hulls. Any failed/bounded proof invalidates.
MaskPixelDifference maskPixelDifference(std::span<const ReferenceTraversalEntry> before,
                                       std::span<const ReferenceTraversalEntry> after) noexcept;
std::optional<foundation::WorldRect> maskSamplingDamage(foundation::WorldRect,double density,foundation::WorldRect rasterWorld={}) noexcept;
}
