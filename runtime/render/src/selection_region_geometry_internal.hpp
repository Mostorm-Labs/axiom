#pragma once
#include "canvas/render/selection_region_geometry.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
namespace canvas::render::internal {
// Independent component-order oracle retained for fallback qualification.
// Private source header; no consumer bridge/package export.
SelectionGeometryHit intersectEraseGeometryComponentOracle(const RuntimeSceneRecord&,
    const semantic::EraseMaskGeometry&) noexcept;
class EraseMaskPathCache final {
 public:
    struct Entry;
    struct Work {std::uint64_t builds=0,hits=0,denied=0;std::size_t bytes=0;};
    EraseMaskPathCache();
    ~EraseMaskPathCache();
    SelectionGeometryHit query(const RuntimeSceneRecord&,const semantic::EraseMaskGeometry&,
        std::shared_ptr<resources::ResourceBudgetManager>,std::size_t capacity)noexcept;
    // Private Render translation units only; returned pins retain their quota.
    std::shared_ptr<const Entry> get(const semantic::EraseMaskGeometry&);
    void clear()noexcept;
    Work work()const noexcept;
 private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
