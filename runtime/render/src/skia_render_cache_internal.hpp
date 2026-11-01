#pragma once
#include "canvas/render/render_cache.hpp"
#include "canvas/render/render_group_manager.hpp"
#include "canvas/render/frame_plan.hpp"
#include "include/core/SkImage.h"
#include "include/core/SkSurface.h"
#include <unordered_map>

namespace canvas::render::internal {
class SkiaRenderCache final {
  public:
    RenderCacheOptions options{};
    RenderCacheWork work{};
    RenderDamage damage{};
    void begin(const FramePlan&, const void* target, SkSurface&, std::uint64_t resourceEpoch);
    // Called only after full plan validation. False executes permanent Direct.
    bool draw(SkCanvas&,const FramePlan&,const ReferenceTraversalEntry&,bool isolated);
    void clear() noexcept;
    void noteDirect() noexcept {++work.directGroups;++work.rasterizedGroups;}
  private:
    struct Entry {RenderCacheValidity validity;sk_sp<SkImage> image;SkIRect bounds;std::size_t bytes=0;std::uint64_t used=0;};
    std::unordered_map<foundation::ObjectId,Entry,foundation::ObjectIdHash> entries_;
    RenderGroupManager groups_;
    const void* target_=nullptr;
    const SkSurface* surface_=nullptr;
    std::unique_ptr<FrameState> frame_;
    WorldToViewAffine transform_{};
    std::uint64_t targetEpoch_=0,tick_=0;
    void evict(std::size_t bytes);
};
}
