#pragma once

#include "common/camera_controller.hpp"
#include "common/demo_transient_state.hpp"
#include "canvas/foundation/result.hpp"
#include "canvas/scene/scene.hpp"

namespace axiom::demo {

class SceneHitTestPort final {
  public:
    explicit SceneHitTestPort(canvas::Scene& scene) noexcept : scene_(scene) {}
    [[nodiscard]] canvas::foundation::Result<canvas::HitTestResult> hitTest(
        canvas::WorldPoint worldPoint) const;

  private:
    canvas::Scene& scene_;
};

struct DemoHitSelection final {
    canvas::HitTestResult hit;
};

class DemoHitSelectHarness final {
  public:
    explicit DemoHitSelectHarness(SceneHitTestPort& port) noexcept : port_(port) {}

    [[nodiscard]] canvas::foundation::Result<DemoHitSelection> selectAtViewPoint(
        canvas::foundation::WorldPoint viewPoint,
        const CameraController& camera,
        const canvas::render::SurfaceMetrics& metrics);
    [[nodiscard]] const DemoTransientSnapshot& transient() const noexcept { return transient_; }

  private:
    SceneHitTestPort& port_;
    DemoTransientSnapshot transient_;
};

} // namespace axiom::demo
