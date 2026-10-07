#include "canvas/render/editing_overlay.hpp"

#include <algorithm>
#include <cmath>

namespace canvas::render {
namespace {

ScreenPoint mapWorld(const FrameState& frame, float x, float y) noexcept {
    const double zoom = static_cast<double>(frame.camera.zoom);
    const double cosine = std::cos(static_cast<double>(frame.camera.rotationRadians));
    const double sine = std::sin(static_cast<double>(frame.camera.rotationRadians));
    const double a = zoom * cosine;
    const double b = -zoom * sine;
    const double c = zoom * sine;
    const double d = zoom * cosine;
    const double centerX = static_cast<double>(frame.metrics.logicalWidth) / 2.0;
    const double centerY = static_cast<double>(frame.metrics.logicalHeight) / 2.0;
    return {static_cast<float>(centerX + a * (x - frame.camera.worldCenter.x) +
                               c * (y - frame.camera.worldCenter.y)),
            static_cast<float>(centerY + b * (x - frame.camera.worldCenter.x) +
                               d * (y - frame.camera.worldCenter.y))};
}

ScreenPoint applyTransform(const semantic::Transform2D& transform, ScreenPoint point) noexcept {
    return {static_cast<float>(transform.a * point.x + transform.c * point.y + transform.tx),
            static_cast<float>(transform.b * point.x + transform.d * point.y + transform.ty)};
}

float distanceSquared(ScreenPoint first, ScreenPoint second) noexcept {
    const float dx = first.x - second.x;
    const float dy = first.y - second.y;
    return dx * dx + dy * dy;
}

} // namespace

bool EditingOverlay::update(const EditingOverlayInput& input) noexcept {
    if (input.selectedObject.isZero() || !input.worldBounds.isFiniteAndOrdered()) {
        clear();
        return false;
    }
    input_ = input;
    guides_.assign(input.guides.begin(), input.guides.end());
    ++updateCount_;

    const ScreenPoint localTopLeft{input.worldBounds.left, input.worldBounds.top};
    const ScreenPoint localTopRight{input.worldBounds.right, input.worldBounds.top};
    const ScreenPoint localBottomRight{input.worldBounds.right, input.worldBounds.bottom};
    const ScreenPoint localBottomLeft{input.worldBounds.left, input.worldBounds.bottom};
    const auto map = [&](ScreenPoint point) {
        const ScreenPoint world = applyTransform(input.objectTransform, point);
        return mapWorld(frame_, world.x, world.y);
    };
    const std::array<ScreenPoint, 4> corners{
        map(localTopLeft), map(localTopRight), map(localBottomRight), map(localBottomLeft)};
    const ScreenPoint top{(corners[0].x + corners[1].x) * 0.5F,
                          (corners[0].y + corners[1].y) * 0.5F};
    const ScreenPoint right{(corners[1].x + corners[2].x) * 0.5F,
                            (corners[1].y + corners[2].y) * 0.5F};
    const ScreenPoint bottom{(corners[2].x + corners[3].x) * 0.5F,
                             (corners[2].y + corners[3].y) * 0.5F};
    const ScreenPoint left{(corners[3].x + corners[0].x) * 0.5F,
                           (corners[3].y + corners[0].y) * 0.5F};
    const ScreenPoint rotation{top.x, top.y - 24.0F};
    outline_ = SelectionOutline{corners, rotation, input.selected};
    const std::array<std::pair<HandleKind, ScreenPoint>, kHandleCount> geometry{{
        {HandleKind::kTopLeft, corners[0]},
        {HandleKind::kTop, top},
        {HandleKind::kTopRight, corners[1]},
        {HandleKind::kRight, right},
        {HandleKind::kBottomRight, corners[2]},
        {HandleKind::kBottom, bottom},
        {HandleKind::kBottomLeft, corners[3]},
        {HandleKind::kLeft, left},
        {HandleKind::kRotation, rotation},
    }};
    for (std::size_t index = 0; index < handles_.size(); ++index) {
        const auto kind = geometry[index].first;
        const bool enabled =
            (kind == HandleKind::kRotation && input.capabilities.rotation) ||
            ((kind == HandleKind::kTopLeft || kind == HandleKind::kTopRight ||
              kind == HandleKind::kBottomLeft || kind == HandleKind::kBottomRight) &&
             input.capabilities.corners) ||
            ((kind == HandleKind::kTop || kind == HandleKind::kRight ||
              kind == HandleKind::kBottom || kind == HandleKind::kLeft) &&
             input.capabilities.sides);
        handles_[index] = enabled
                              ? HandleGeometry{kind, geometry[index].second, 6.0F, 12.0F,
                                               input.active}
                              : HandleGeometry{};
    }
    return true;
}

void EditingOverlay::clear() noexcept {
    input_ = {};
    outline_ = {};
    guides_.clear();
    for (auto& handle : handles_) handle = {};
}

const HandleGeometry& EditingOverlay::handle(HandleKind kind) const noexcept {
    for (const auto& handle : handles_) {
        if (handle.kind == kind) return handle;
    }
    static const HandleGeometry empty{};
    return empty;
}

HandleKind EditingOverlay::hitTest(ScreenPoint point, float hitSlop) const noexcept {
    for (const auto& handle : handles_) {
        if (handle.kind == HandleKind::kNone) continue;
        const float radius = std::max(handle.hitRadius, hitSlop);
        if (distanceSquared(point, handle.center) <= radius * radius) return handle.kind;
    }
    return HandleKind::kNone;
}

} // namespace canvas::render
