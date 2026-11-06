#pragma once
#include "canvas/render/render_group.hpp"
#include "canvas/render/reference_draw_list.hpp"
#include "canvas/render/damage_tracker.hpp"
#include <unordered_map>
#include <optional>

namespace canvas::render {
class RenderGroupManager final {
  public:
    struct EraseDamage final {
        bool active=false,continuous=false;
        std::optional<foundation::WorldRect> bounds;
    };
    struct Update final {std::vector<VisualContributionDelta> contributions;EraseDamage erase;};
    [[nodiscard]] Update update(std::span<const ReferenceTraversalEntry> entries,
        const foundation::WorldRect& clip, std::uint64_t resourceEpoch);
    [[nodiscard]] const RenderGroup* find(foundation::ObjectId id) const noexcept;
    void clear() noexcept {entries_.clear();}
  private:
    struct Entry {RenderGroup group;ReferenceTraversalEntry source;std::size_t ordinal=0;std::uint64_t resourceEpoch=0;};
    std::unordered_map<foundation::ObjectId,Entry,foundation::ObjectIdHash> entries_;
    foundation::WorldRect clip_{};
};
}
