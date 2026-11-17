#pragma once
#include "skia_prepared_geometry.hpp"
#include "selection_region_geometry_internal.hpp"
#include "skia_mask_history.hpp"
#include "compact_raster_internal.hpp"
#include "include/core/SkImage.h"
#include <map>

namespace canvas::render::internal {
// Disposable device-space ink only. Body pixels never include erasure;
// result pixels are qualified by an exact ordered mask-primitive prefix.
// No surface/framebuffer/receipt validity is inferred from a tile hit.
class SkiaInkLayers final {
 public:
    static constexpr int tileSize=128,maskGuard=8;
    bool draw(SkCanvas& destination,SkSurface& scratch,const ReferenceTraversalEntry&,
        const SkMatrix&,const SkIRect& viewport,const CompactDeviceGrid*,
        const RenderCacheOptions&,const std::shared_ptr<resources::ResourceBudgetManager>&,
        const std::shared_ptr<SourceRetentionPool>&,CompactCoverageTargets&,RenderCacheWork&,bool& painted,bool& resourcePending,std::shared_ptr<PreparedGeometryCache> prepared={},std::shared_ptr<const PreparedNavigationGeometry> ready={},std::shared_ptr<EraseMaskPathCache> histories={},PreparationAllowance* allowance=nullptr);
    struct NavigationBodyWork {std::uint64_t hits=0,misses=0,builds=0,evictions=0,projectionBodyMiss=0,projectionContextMiss=0,projectionFormatMiss=0,projectionDensityMiss=0;std::size_t bytes=0,entries=0;};
    const NavigationBodyWork& navigationBodyWork()const noexcept{return navigationWork_;}
    sk_sp<SkImage> navigationBody(const std::shared_ptr<const PreparedNavigationGeometry>&,SkCanvas&,
        const RenderCacheOptions&,const std::shared_ptr<resources::ResourceBudgetManager>&);
    void rememberNavigationBody(const std::shared_ptr<const PreparedNavigationGeometry>&,SkCanvas&,sk_sp<SkImage>,
        const RenderCacheOptions&,const std::shared_ptr<resources::ResourceBudgetManager>&);
    enum class BodyPreparation {kReady,kPending,kUnavailable};
    BodyPreparation prepareNavigationBody(const ReferenceTraversalEntry&,const std::shared_ptr<const PreparedNavigationGeometry>&,
        const SkMatrix&,SkCanvas&,const RenderCacheOptions&,const std::shared_ptr<resources::ResourceBudgetManager>&,
        CompactCoverageTargets&,RenderCacheWork&,PreparationAllowance&,sk_sp<SkImage>&,SkMatrix&);
    bool projectNavigationBody(const std::shared_ptr<const PreparedNavigationGeometry>&,SkCanvas&,
        const RenderCacheOptions&,const std::shared_ptr<resources::ResourceBudgetManager>&);
    void bindSurfaceBasis(const void*,std::uint64_t resource,std::uint64_t surface,std::uint64_t view,std::uint64_t metrics);
    void clearDeviceLayers()noexcept;
    void clear()noexcept;
    void setValidationPressure(ValidationPressure pressure){pressure_=std::move(pressure);}
    void prune(std::span<const ReferenceTraversalEntry>);
    // Owner-thread preparation opportunity only. Plan values and pixel/GPU
    // leases remain valid; every retained mask result loses qualification.
    std::size_t retireIdleHistoryPreparation() noexcept;
 private:
    struct NavigationBodyKey {
        std::uint64_t body=0;const void* context=nullptr;
        std::array<std::uint32_t,9> matrix{};std::array<int,6> crop{};int colorType=0,alphaType=0;std::uint64_t colorSpace=0;
        bool operator<(const NavigationBodyKey&b)const{return std::tie(body,context,matrix,crop,colorType,alphaType,colorSpace)<std::tie(b.body,b.context,b.matrix,b.crop,b.colorType,b.alphaType,b.colorSpace);}
    };
    struct NavigationBody {std::shared_ptr<const PreparedNavigationGeometry> geometry;sk_sp<SkImage> pixels;resources::ResourceLease lease;std::uint64_t used=0;std::size_t bytes=0;};
    std::map<NavigationBodyKey,NavigationBody> navigationBodies_;
    struct FullBody {std::shared_ptr<const PreparedNavigationGeometry> geometry;sk_sp<SkImage> pixels;sk_sp<SkSurface> building;
        std::unique_ptr<CompactStrokeRasterJob> raster;SkMatrix matrix;resources::ResourceLease lease,staging;
        std::uint64_t used=0;std::size_t bytes=0;};
    std::map<NavigationBodyKey,FullBody> fullBodies_;
    void bindNavigationBudget(const std::shared_ptr<resources::ResourceBudgetManager>&);
    bool evictNavigationBody();
    static NavigationBodyKey navigationKey(const PreparedNavigationGeometry&,const SkImageInfo&,const void*,const SkMatrix&,SkIRect);
    std::weak_ptr<resources::ResourceBudgetManager> navigationBudget_;
    static NavigationBodyKey navigationKey(const PreparedNavigationGeometry&,SkCanvas&);
    NavigationBodyWork navigationWork_{};
    const void* surfaceOwner_=nullptr;
    std::array<std::uint64_t,4> surfaceBasis_{};
    using MaskPrimitive=std::variant<semantic::EraseCubicSegment,semantic::FilledPathMask>;
    struct Source {
        ReferenceCommand command;
        std::shared_ptr<const PreparedNavigationGeometry> geometry;
        SkMatrix matrix;SkIRect viewport;
        std::vector<MaskPrimitive> masks; // Active transient tail only.
        std::shared_ptr<MaskHistoryPlan> history;
        const void* context=nullptr;
        std::array<int,4> grid{};
        resources::ResourceLease metadata;
        std::uint64_t epoch=1;
    };
    struct Key {foundation::ObjectId id;int x,y;bool operator<(const Key& b)const{return std::tie(id,x,y)<std::tie(b.id,b.x,b.y);}};
    struct Image {sk_sp<SkImage> pixels;resources::ResourceLease lease;};
    struct Tile {Image body,result;sk_sp<SkSurface> building;std::unique_ptr<CompactStrokeRasterJob> raster;
        std::uint64_t epoch=0,used=0;std::size_t applied=0,bytes=0;};
    std::map<foundation::ObjectId,Source> sources_;
    std::map<Key,Tile> tiles_;
    std::shared_ptr<PreparedGeometryCache> geometry_;
    std::shared_ptr<EraseMaskPathCache> histories_;
    std::size_t bytes_=0;
    ValidationPressure pressure_;
    std::uint64_t tick_=0;
    unsigned drawDepth_=0;
    bool reserve(std::size_t,const Key&,const RenderCacheOptions&,
        const std::shared_ptr<resources::ResourceBudgetManager>&,resources::ResourceLease&);
    void eraseSource(foundation::ObjectId);
    static void drawMask(SkCanvas&,const MaskPrimitive&);
};
}
