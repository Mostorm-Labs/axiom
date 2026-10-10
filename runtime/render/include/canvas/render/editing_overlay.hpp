#pragma once

#include "canvas/foundation/object_id.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/render/frame_state.hpp"
#include "canvas/semantic/object_record.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace canvas::render {

struct ScreenPoint final {
    float x = 0.0F;
    float y = 0.0F;
    bool operator==(const ScreenPoint&) const = default;
};

enum class SnapGuideAxis : std::uint8_t { kVertical = 0, kHorizontal = 1 };

struct SnapGuideGeometry final {
    SnapGuideAxis axis = SnapGuideAxis::kVertical;
    ScreenPoint start{};
    ScreenPoint end{};
};

enum class HandleKind : std::uint8_t {
    kNone = 0,
    kTopLeft,
    kTop,
    kTopRight,
    kRight,
    kBottomRight,
    kBottom,
    kBottomLeft,
    kLeft,
    kRotation,
};

struct HandleGeometry final {
    HandleKind kind = HandleKind::kNone;
    ScreenPoint center{};
    float visualRadius = 6.0F;
    float hitRadius = 12.0F;
    bool active = false;
};

struct HandleCapabilities final {
    bool corners = true;
    bool sides = true;
    bool rotation = true;
};

struct SelectionOutline final {
    std::array<ScreenPoint, 4> corners{};
    ScreenPoint rotationAnchor{};
    bool visible = false;
    bool operator==(const SelectionOutline&) const = default;
};

struct EditingOverlayInput final {
    foundation::ObjectId selectedObject{};
    foundation::WorldRect worldBounds{};
    semantic::Transform2D objectTransform{};
    bool selected = false;
    bool hovered = false;
    bool active = false;
    HandleCapabilities capabilities{};
    std::span<const SnapGuideGeometry> guides{};
};

class EditingOverlay final {
  public:
    explicit constexpr EditingOverlay(const FrameState& frame) noexcept : frame_(frame) {}

    [[nodiscard]] bool update(const EditingOverlayInput& input) noexcept;
    void clear() noexcept;

    [[nodiscard]] foundation::ObjectId selectedObject() const noexcept {
        return input_.selectedObject;
    }
    [[nodiscard]] bool hovered() const noexcept { return input_.hovered; }
    [[nodiscard]] bool active() const noexcept { return input_.active; }
    [[nodiscard]] std::uint64_t updateCount() const noexcept { return updateCount_; }
    [[nodiscard]] std::uint64_t transientTransformCount() const noexcept {
        return input_.active ? 1U : 0U;
    }
    [[nodiscard]] std::uint64_t viewGeneration() const noexcept {
        return frame_.camera.generation.value();
    }
    [[nodiscard]] const HandleGeometry& handle(HandleKind kind) const noexcept;
    [[nodiscard]] const SelectionOutline& selectionOutline() const noexcept {
        return outline_;
    }
    [[nodiscard]] const std::vector<SnapGuideGeometry>& guides() const noexcept {
        return guides_;
    }
    [[nodiscard]] HandleKind hitTest(ScreenPoint point, float hitSlop = 0.0F) const noexcept;

  private:
    static constexpr std::size_t kHandleCount = 9;
    const FrameState& frame_;
    EditingOverlayInput input_{};
    SelectionOutline outline_{};
    std::array<HandleGeometry, kHandleCount> handles_{};
    std::vector<SnapGuideGeometry> guides_{};
    std::uint64_t updateCount_ = 0;
};

} // namespace canvas::render
