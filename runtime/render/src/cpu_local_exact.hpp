#pragma once
#include "cpu_worker_protocol.hpp"
#include "skia_prepared_geometry.hpp"
#include "canvas/render/selection_region_geometry.hpp"
#include "canvas/render/skia_renderer.hpp"

namespace canvas::render::internal {
// These states are private execution states, never a semantic hit/miss value.
struct LocalMaskIndex;
struct LocalExactStatistics {
    std::uint64_t validated=0,pieceRequests=0,pieceUnions=0,pieceHits=0,fullFallbacks=0;
};
class LocalExactCache final {
 public:
    std::shared_ptr<LocalMaskIndex> request(const semantic::EraseMaskGeometry&,
        const std::shared_ptr<resources::ResourceBudgetManager>&,std::size_t capacity);
    void clear()noexcept;
    void reclaimUnused()noexcept;
 private:
    std::vector<std::shared_ptr<LocalMaskIndex>> entries_;
};
// The entire input/target/history are retained; an imported range can prove only
// its exact range. Full original geometry is still used by the canonical owner.
class LocalExactJob final {
 public:
    LocalExactJob(const RuntimeSceneRecord&,std::shared_ptr<LocalMaskIndex>,
        LocalExactCache&,PreparedGeometryCache&,const RenderCacheOptions&,
        std::shared_ptr<resources::ResourceBudgetManager>);
    LocalExactStatus advance(PreparationAllowance&);
    std::vector<std::uint8_t> encode(const std::array<std::uint64_t,8>&);
    bool import(std::span<const std::uint8_t>,const std::array<std::uint64_t,8>&,bool* resourcePending);
    bool advanceLocalPiece(PreparationAllowance&);
    void setSelectionContact(const SkPath&);
    bool advanceSelectionFallback(PreparationAllowance&);
    void cancel()noexcept;
    bool matches(const RuntimeSceneRecord&,const semantic::EraseMaskGeometry&)const noexcept;
    const LocalExactStatistics& statistics()const noexcept{return work_;}
    const semantic::EraseMaskGeometry& fullGeometry()const noexcept;
 private:
    RuntimeSceneRecord basis_;
    std::shared_ptr<LocalMaskIndex> query_;
    std::vector<std::shared_ptr<LocalMaskIndex>> history_;
    PreparedGeometryCache& bodies_;
    ReferenceTraversalEntry bodyEntry_;
    std::shared_ptr<const PreparedNavigationGeometry> body_;
    RenderCacheOptions limits_;
    std::shared_ptr<resources::ResourceBudgetManager> budget_;
    resources::ResourceLease metadata_;
    bool admitted_=false;
    LocalExactStatus status_=LocalExactStatus::kPending;
    LocalExactStatistics work_;
    LocalExactCache& cache_;
    SkPath selectionContact_;
    bool selection_=false;
    std::unique_ptr<CpuExactPathJob> localPiece_;
    resources::ResourceLease localPieceLease_;
    std::unique_ptr<CpuExactPathJob> fallbackJob_;
    resources::ResourceLease fallbackLease_;
    std::vector<SkPath> fallbackPaths_;
    std::vector<resources::ResourceLease> fallbackPathLeases_;
    std::size_t fallbackComponent_=0,fallbackHistory_=0;
    bool fallbackOverlap_=false;
    std::size_t validating_=0,queryPiece_=0,component_=0,historyMask_=0,historyPiece_=0;
    std::shared_ptr<LocalMaskIndex> waiting_;
    std::size_t waitingPiece_=0;
    std::vector<std::uint8_t> request_;
    std::array<std::uint64_t,8> token_{};
    SkPath overlap_;
    bool haveOverlap_=false;
};
}
