#pragma once

#include "canvas/foundation/object_id.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/interaction/viewport_constraints.hpp"
#include "canvas/input/pointer_key.hpp"
#include "canvas/semantic/object_record.hpp"

#include <cstdint>
#include <optional>
#include <vector>
#include <utility>

namespace canvas::interaction {

enum class CreationPreset : std::uint8_t {
  kRectangle, kEllipse, kTriangle, kLine, kArrow, kSticky
};
struct CreationStyle final { semantic::PropertyBag properties{}; bool operator==(const CreationStyle&) const = default; };
struct CreationPreview final {
  bool active = false;
  bool valid = false;
  bool snapped = false;
  CreationPreset preset = CreationPreset::kRectangle;
  foundation::WorldPoint start{};
  foundation::WorldPoint end{};
  foundation::WorldRect viewport{};
  semantic::ObjectRecord record{};
  std::vector<interaction::SnapGuide> guides;
};
struct CreationCommit final { semantic::ObjectRecord record{}; input::PointerKey pointer{}; };

class CreationSession final {
 public:
  [[nodiscard]] bool begin(input::PointerKey, CreationPreset, CreationStyle,
                           foundation::WorldPoint, foundation::WorldRect,
                           std::uint64_t) noexcept;
  [[nodiscard]] bool move(input::PointerKey, foundation::WorldPoint) noexcept;
  [[nodiscard]] std::optional<CreationCommit> finish(input::PointerKey) noexcept;
  [[nodiscard]] bool cancel(input::PointerKey) noexcept;
  [[nodiscard]] bool active() const noexcept { return preview_.active; }
  [[nodiscard]] const CreationPreview& preview() const noexcept { return preview_; }
  // A connector endpoint may be attached to the object whose perimeter
  // accepted the latest snapped pointer position.  This is transient
  // interaction state; only the resulting AttachedEndpoint is canonical.
  [[nodiscard]] std::optional<semantic::AttachedEndpoint> snappedEndpoint() const noexcept {
    return snappedEndpoint_;
  }
  [[nodiscard]] std::optional<semantic::AttachedEndpoint> startAttachment() const noexcept {
    return startAttachment_;
  }
  void setStartAttachment(std::optional<semantic::AttachedEndpoint> value) noexcept {
    startAttachment_ = std::move(value);
  }
  void setEndAttachment(std::optional<semantic::AttachedEndpoint> value) noexcept {
    endAttachment_ = std::move(value);
  }
  void setSnapTargets(std::vector<interaction::SnapTarget> targets) noexcept {
    snapTargets_ = std::move(targets);
  }
  void setSnapScale(float scale) noexcept { snapScale_ = scale > 0.0F ? scale : 1.0F; }
  void setSnapPolicy(SnapPolicy policy) noexcept { snapResolver_.setPolicy(policy); }
  void clear() noexcept;
 private:
  [[nodiscard]] bool rebuild(foundation::WorldPoint) noexcept;
  input::PointerKey pointer_{};
  std::uint64_t objectSeed_ = 0;
  CreationPreset preset_ = CreationPreset::kRectangle;
  CreationStyle style_{};
  foundation::WorldRect viewport_{};
  interaction::SnapResolver snapResolver_{};
  std::vector<interaction::SnapTarget> snapTargets_{};
  float snapScale_ = 1.0F;
  CreationPreview preview_{};
  std::optional<semantic::AttachedEndpoint> snappedEndpoint_{};
  std::optional<semantic::AttachedEndpoint> startAttachment_{};
  std::optional<semantic::AttachedEndpoint> endAttachment_{};
};
}  // namespace canvas::interaction
