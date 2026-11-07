#pragma once
#include "skia_prepared_geometry.hpp"
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
        const std::shared_ptr<SourceRetentionPool>&,CompactCoverageTargets&,RenderCacheWork&,bool& painted,bool& resourcePending);
    void clear()noexcept;
    void prune(std::span<const ReferenceTraversalEntry>);
 private:
    using MaskPrimitive=std::variant<semantic::EraseCubicSegment,semantic::FilledPathMask>;
    struct Source {
        ReferenceCommand command;
        std::shared_ptr<const PreparedNavigationGeometry> geometry;
        SkMatrix matrix;SkIRect viewport;
        std::vector<MaskPrimitive> masks;
        resources::ResourceLease metadata;
        std::uint64_t epoch=1;
    };
    struct Key {foundation::ObjectId id;int x,y;bool operator<(const Key& b)const{return std::tie(id,x,y)<std::tie(b.id,b.x,b.y);}};
    struct Image {sk_sp<SkImage> pixels;resources::ResourceLease lease;};
    struct Tile {Image body,result;std::uint64_t epoch=0,used=0;std::size_t applied=0,bytes=0;};
    std::map<foundation::ObjectId,Source> sources_;
    std::map<Key,Tile> tiles_;
    PreparedGeometryCache geometry_;
    std::size_t bytes_=0;
    std::uint64_t tick_=0;
    bool reserve(std::size_t,const Key&,const RenderCacheOptions&,
        const std::shared_ptr<resources::ResourceBudgetManager>&,resources::ResourceLease&);
    void eraseSource(foundation::ObjectId);
    static void drawMask(SkCanvas&,const MaskPrimitive&);
};
}
