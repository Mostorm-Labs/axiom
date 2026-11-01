#pragma once
#include "canvas/foundation/revision.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/foundation/object_id.hpp"
#include "canvas/scene/scene_delta.hpp"
#include <optional>
#include <span>
#include <vector>

namespace canvas::render {
enum class FullRedrawReason { kNone, kInitial, kGenerationGap, kViewChanged, kUnknownExtent };
struct VisualContribution final {
    foundation::WorldRect bounds{};
    bool visible = true;
    // Non-local blend/filter/isolation cannot be bounded by the object alone.
    bool requiresBackdrop = false;
};
struct VisualContributionDelta final {
    foundation::ObjectId objectId{};
    std::optional<VisualContribution> before;
    std::optional<VisualContribution> after;
};
struct RenderDamage final {
    foundation::SceneRevision before{}, after{};
    bool fullRedraw = false;
    FullRedrawReason reason = FullRedrawReason::kNone;
    std::vector<foundation::WorldRect> rects;
    double exactUnionArea = 0, damageArea = 0;
    std::size_t inputRectCount = 0;
};
// Per-render-domain policy consuming Scene/visual output, never Operations.
class DamageTracker final {
  public:
    explicit DamageTracker(std::size_t maxRects = 64) : maxRects_(maxRects ? maxRects : 1) {}
    [[nodiscard]] RenderDamage collect(foundation::SceneRevision before,
        foundation::SceneRevision after, std::span<const VisualContributionDelta> changes,
        FullRedrawReason forced = FullRedrawReason::kNone) const;
    [[nodiscard]] RenderDamage collect(const SceneDelta& delta) const;
  private:
    std::size_t maxRects_;
};
}
