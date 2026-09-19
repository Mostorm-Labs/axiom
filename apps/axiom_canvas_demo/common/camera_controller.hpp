#pragma once

#include "canvas/foundation/result.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/render/frame_state.hpp"

namespace axiom::demo {

class CameraController final {
  public:
    explicit CameraController(canvas::render::CameraState state) noexcept
        : center_(state.worldCenter),
          zoom_(state.zoom),
          rotationRadians_(state.rotationRadians),
          generation_(state.generation) {}

    [[nodiscard]] canvas::render::CameraState state() const noexcept;

    [[nodiscard]] canvas::foundation::Result<canvas::foundation::WorldPoint> worldToView(
        canvas::foundation::WorldPoint world,
        const canvas::render::SurfaceMetrics& metrics) const;
    [[nodiscard]] canvas::foundation::Result<canvas::foundation::WorldPoint> viewToWorld(
        canvas::foundation::WorldPoint view,
        const canvas::render::SurfaceMetrics& metrics) const;

    [[nodiscard]] bool panByViewDelta(canvas::foundation::WorldPoint delta) noexcept;
    [[nodiscard]] bool zoomAtViewPoint(float zoom,
                                       canvas::foundation::WorldPoint viewAnchor,
                                       const canvas::render::SurfaceMetrics& metrics) noexcept;

  private:
    [[nodiscard]] bool validMetrics(const canvas::render::SurfaceMetrics& metrics) const noexcept;
    [[nodiscard]] bool advanceGeneration() noexcept;

    canvas::foundation::WorldPoint center_{};
    float zoom_ = 1.0F;
    float rotationRadians_ = 0.0F;
    canvas::render::CameraGeneration generation_{};
};

} // namespace axiom::demo
