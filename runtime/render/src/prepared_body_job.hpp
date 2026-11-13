#pragma once
#include "skia_prepared_geometry.hpp"

namespace canvas::render::internal {
// World-body preparation only. Immutable source and charged storage belong to
// the existing cache body; no plan, target canvas or borrowed knot span survives.
class PreparedBodyJob final {
 public:
    static std::size_t minimumBytes(std::size_t)noexcept;
    static std::shared_ptr<PreparedNavigationGeometry> create(const ReferenceCommand&,SourceRetentions,
        std::size_t cap,const std::shared_ptr<resources::ResourceBudgetManager>&,
        const std::shared_ptr<PreparedGeometryUsage>&);
    explicit PreparedBodyJob(const foundation::InkBrushConfig& c):compiler_(c){}
    bool advance(PreparedNavigationGeometry&,PreparedGeometryStatistics&);
 private:
    enum class Phase {kMetrics,kCompile,kPaths,kDone};
    Phase phase_=Phase::kMetrics;
    std::shared_ptr<resources::ResourceBudgetManager> budget_;
    bool replay_=false;
    std::size_t cursor_=0,ribbon_=0,cap_=0,blockKnot_=0,blockCount_=0,pathBytes_=0;
    ink::CompactContourCompiler compiler_;
    std::optional<ink::CompactContourCompiler> checkpoint_;
    ink::CompactContour emitted_;
    SkRect blockBounds_=SkRect::MakeEmpty();
};
}
