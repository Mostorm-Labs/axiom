#pragma once

#include "canvas/foundation/object_id.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/render/frame_state.hpp"
#include "canvas/semantic/object_record.hpp"
#include "canvas/scene/derived_geometry.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>
#include <optional>

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

struct AttachmentPointGeometry final {
    ScreenPoint center{};
    bool active = false;
    bool hovered = false;
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
    std::span<const AttachmentPointGeometry> attachmentPoints{};
};

class EditingOverlay final {
  public:
    explicit constexpr EditingOverlay(const FrameState& frame) noexcept : frame_(frame) {}

    [[nodiscard]] bool update(const EditingOverlayInput& input) noexcept;
    void clear() noexcept;
    void setCreationPreview(const semantic::ObjectRecord& record,
                            std::span<const SnapGuideGeometry> guides,
                            std::optional<scene::ConnectorGeometry> route = std::nullopt) {
        creationPreview_ = record;
        creationRoute_ = std::move(route);
        guides_.assign(guides.begin(), guides.end());
    }
    [[nodiscard]] const std::optional<semantic::ObjectRecord>& creationPreview() const noexcept {
        return creationPreview_;
    }
    [[nodiscard]] const scene::ConnectorGeometry* creationRoute() const noexcept {
        return creationRoute_?&*creationRoute_:nullptr;
    }

    std::vector<std::array<float,8>> textSelection;
    std::optional<std::array<float,8>> textCaret;
    bool textCaretVisible = true;
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
    [[nodiscard]] const std::vector<AttachmentPointGeometry>& attachmentPoints() const noexcept {
        return attachmentPoints_;
    }
    [[nodiscard]] std::vector<AttachmentPointGeometry>& mutableAttachmentPoints() noexcept {
        return attachmentPoints_;
    }
    [[nodiscard]] HandleKind hitTest(ScreenPoint point, float hitSlop = 0.0F) const noexcept;

  private:
    static constexpr std::size_t kHandleCount = 9;
    const FrameState& frame_;
    EditingOverlayInput input_{};
    SelectionOutline outline_{};
    std::array<HandleGeometry, kHandleCount> handles_{};
    std::vector<SnapGuideGeometry> guides_{};
    std::vector<AttachmentPointGeometry> attachmentPoints_{};
    std::uint64_t updateCount_ = 0;
    std::optional<semantic::ObjectRecord> creationPreview_;
    std::optional<scene::ConnectorGeometry> creationRoute_;
};

} // namespace canvas::render
