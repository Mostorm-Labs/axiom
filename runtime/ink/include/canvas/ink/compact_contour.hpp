#pragma once
#include "canvas/runtime/ink_brush.hpp"
#include <optional>
#include <limits>
#include <span>
#include <vector>
namespace canvas::ink {
struct ContourPoint {double x=0,y=0;bool operator==(const ContourPoint&)const=default;};
struct ContourPatch {ContourPoint leftStart,leftControl,leftEnd,rightStart,rightControl,rightEnd;bool operator==(const ContourPatch&)const=default;};
struct ContourCap {double x=0,y=0,radius=0;};
struct CompactContour {std::vector<ContourPatch> patches;std::vector<ContourCap> caps;};
// Incremental paired boundary anchors. One pending input neighbor and one
// quadratic anchor remain replaceable; no expanded geometry is persisted.
class CompactContourCompiler final {
 public:
    explicit CompactContourCompiler(foundation::InkBrushConfig config={}):config_(std::move(config)){}
    void append(std::span<const foundation::InkKnot>,double totalDistance,CompactContour&);
    void finish(double totalDistance,CompactContour&);
    std::uint64_t visits()const noexcept{return visits_;}
    // At most two patches/one corner node per effective knot, sixteen total
    // taper cuts, one carried patch and finish. Saturate before reservations.
    static constexpr std::size_t maxPatches(std::size_t knots) noexcept {
        constexpr auto max=std::numeric_limits<std::size_t>::max();
        return knots>(max-35)/2?max:knots*2+35;
    }
    static constexpr std::size_t maxCaps(std::size_t knots) noexcept {
        constexpr auto max=std::numeric_limits<std::size_t>::max();
        return knots>max-18?max:knots+18;
    }
 private:
    struct Anchor {ContourPoint left,right;};
    foundation::InkBrushConfig config_;
    std::optional<foundation::InkKnot> previous_,current_;
    std::optional<Anchor> anchor_,midpoint_;
    double distance_=0,currentDistance_=0;
    unsigned anchors_=0;
    std::uint64_t visits_=0;
    std::optional<ContourPatch> lastPatch_;
    Anchor makeAnchor(const foundation::InkKnot&,double,double,double,double)const;
    void accept(Anchor,bool,CompactContour&);
    void appendKnot(const foundation::InkKnot&,double,CompactContour&);
    double radius(const foundation::InkKnot&,double,double)const;
};
CompactContour compileCompactContour(const foundation::InkBrushConfig&,std::span<const foundation::InkKnot>);
}
