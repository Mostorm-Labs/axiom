#include "common/camera_controller.hpp"

#include <cmath>
#include <limits>

namespace axiom::demo {
namespace {

using canvas::foundation::Error;
using canvas::foundation::ErrorCode;
using canvas::foundation::Result;
using canvas::foundation::WorldPoint;
using canvas::render::SurfaceMetrics;

bool finitePoint(WorldPoint point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

bool finiteState(const canvas::render::CameraState& state) noexcept {
    return finitePoint(state.worldCenter) && std::isfinite(state.zoom) && state.zoom > 0.0F &&
           std::isfinite(state.rotationRadians);
}

Error invalid(const char* message) {
    return Error{ErrorCode::kInvalidArgument, message};
}

} // namespace

bool CameraController::validMetrics(const SurfaceMetrics& metrics) const noexcept {
    return std::isfinite(metrics.logicalWidth) && std::isfinite(metrics.logicalHeight) &&
           metrics.logicalWidth > 0.0F && metrics.logicalHeight > 0.0F &&
           metrics.physicalWidth > 0U && metrics.physicalHeight > 0U &&
           std::isfinite(metrics.devicePixelRatio) && metrics.devicePixelRatio > 0.0F &&
           std::isfinite(metrics.displayScale) && metrics.displayScale > 0.0F;
}

canvas::render::CameraState CameraController::state() const noexcept {
    return canvas::render::CameraState{center_, zoom_, rotationRadians_, generation_};
}

bool CameraController::advanceGeneration() noexcept {
    if (generation_.value() == std::numeric_limits<std::uint64_t>::max()) {
        return false;
    }
    generation_ = canvas::render::CameraGeneration{generation_.value() + 1U};
    return true;
}

Result<WorldPoint> CameraController::worldToView(
    WorldPoint world, const SurfaceMetrics& metrics) const {
    if (!finiteState(state()) || !finitePoint(world) || !validMetrics(metrics)) {
        return Result<WorldPoint>::failure(invalid("camera transform input is invalid"));
    }
    const float cosine = std::cos(rotationRadians_);
    const float sine = std::sin(rotationRadians_);
    const float dx = world.x - center_.x;
    const float dy = world.y - center_.y;
    const WorldPoint result{
        metrics.logicalWidth * 0.5F + zoom_ * (cosine * dx + sine * dy),
        metrics.logicalHeight * 0.5F + zoom_ * (-sine * dx + cosine * dy),
    };
    if (!finitePoint(result)) {
        return Result<WorldPoint>::failure(invalid("camera transform overflow"));
    }
    return Result<WorldPoint>::success(result);
}

Result<WorldPoint> CameraController::viewToWorld(
    WorldPoint view, const SurfaceMetrics& metrics) const {
    if (!finiteState(state()) || !finitePoint(view) || !validMetrics(metrics)) {
        return Result<WorldPoint>::failure(invalid("camera transform input is invalid"));
    }
    const float cosine = std::cos(rotationRadians_);
    const float sine = std::sin(rotationRadians_);
    const float dx = (view.x - metrics.logicalWidth * 0.5F) / zoom_;
    const float dy = (view.y - metrics.logicalHeight * 0.5F) / zoom_;
    const WorldPoint result{
        center_.x + cosine * dx - sine * dy,
        center_.y + sine * dx + cosine * dy,
    };
    if (!finitePoint(result)) {
        return Result<WorldPoint>::failure(invalid("camera transform overflow"));
    }
    return Result<WorldPoint>::success(result);
}

bool CameraController::panByViewDelta(WorldPoint delta) noexcept {
    if (!finiteState(state()) || !finitePoint(delta)) {
        return false;
    }
    const float cosine = std::cos(rotationRadians_);
    const float sine = std::sin(rotationRadians_);
    const WorldPoint next{
        center_.x - (cosine * delta.x - sine * delta.y) / zoom_,
        center_.y - (sine * delta.x + cosine * delta.y) / zoom_,
    };
    if (!finitePoint(next)) {
        return false;
    }
    const auto old = center_;
    center_ = next;
    if (!advanceGeneration()) {
        center_ = old;
        return false;
    }
    return true;
}

bool CameraController::zoomAtViewPoint(float zoom,
                                       WorldPoint viewAnchor,
                                       const SurfaceMetrics& metrics) noexcept {
    if (!finiteState(state()) || !std::isfinite(zoom) || zoom < 0.01F || zoom > 1000.0F ||
        !finitePoint(viewAnchor) || !validMetrics(metrics)) {
        return false;
    }
    const auto worldAnchor = viewToWorld(viewAnchor, metrics);
    if (!worldAnchor) {
        return false;
    }
    const float oldZoom = zoom_;
    zoom_ = zoom;
    const auto transformed = viewToWorld(viewAnchor, metrics);
    if (!transformed) {
        zoom_ = oldZoom;
        return false;
    }
    const auto oldCenter = center_;
    center_.x += worldAnchor.value().x - transformed.value().x;
    center_.y += worldAnchor.value().y - transformed.value().y;
    if (!finitePoint(center_) || !advanceGeneration()) {
        zoom_ = oldZoom;
        center_ = oldCenter;
        return false;
    }
    return true;
}

} // namespace axiom::demo
