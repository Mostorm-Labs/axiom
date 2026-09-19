#include "common/camera_controller.hpp"

#include <cassert>
#include <cmath>
#include <limits>

namespace {

using axiom::demo::CameraController;
using canvas::foundation::WorldPoint;
using canvas::render::CameraGeneration;
using canvas::render::CameraState;
using canvas::render::SurfaceMetrics;

bool close(float left, float right) {
    return std::fabs(left - right) < 1.0e-4F;
}

void roundTripIncludesRotationAndSurfaceCenter() {
    const SurfaceMetrics metrics{800.0F, 600.0F, 1600, 1200, 2.0F, 1.0F};
    const CameraController controller{
        CameraState{WorldPoint{30.0F, -10.0F}, 2.5F, 0.35F, CameraGeneration{7}}};
    const WorldPoint world{72.0F, 19.0F};
    const auto view = controller.worldToView(world, metrics);
    assert(view.hasValue());
    const auto restored = controller.viewToWorld(view.value(), metrics);
    assert(restored.hasValue());
    assert(close(restored.value().x, world.x));
    assert(close(restored.value().y, world.y));
}

void panAndAnchoredZoomArePerViewAndMonotonic() {
    const SurfaceMetrics metrics{640.0F, 480.0F, 640, 480, 1.0F, 1.0F};
    CameraController controller{
        CameraState{WorldPoint{10.0F, 20.0F}, 1.0F, 0.0F, CameraGeneration{10}}};
    assert(controller.panByViewDelta(WorldPoint{40.0F, -20.0F}));
    assert(controller.state().generation == CameraGeneration{11});
    assert(close(controller.state().worldCenter.x, -30.0F));
    assert(close(controller.state().worldCenter.y, 40.0F));

    const WorldPoint anchor{137.0F, 211.0F};
    const auto before = controller.viewToWorld(anchor, metrics);
    assert(before.hasValue());
    assert(controller.zoomAtViewPoint(2.0F, anchor, metrics));
    assert(controller.state().generation == CameraGeneration{12});
    const auto after = controller.viewToWorld(anchor, metrics);
    assert(after.hasValue());
    assert(close(after.value().x, before.value().x));
    assert(close(after.value().y, before.value().y));
}

void invalidCommandsFailWithoutMutation() {
    CameraController controller{
        CameraState{WorldPoint{1.0F, 2.0F}, 1.0F, 0.0F, CameraGeneration{5}}};
    const auto original = controller.state();
    const SurfaceMetrics invalidMetrics{};
    assert(!controller.zoomAtViewPoint(2.0F, WorldPoint{}, invalidMetrics));
    assert(!controller.panByViewDelta(
        WorldPoint{std::numeric_limits<float>::infinity(), 0.0F}));
    assert(!controller.zoomAtViewPoint(0.0F, WorldPoint{},
                                       SurfaceMetrics{100, 100, 100, 100, 1, 1}));
    assert(controller.state() == original);
}

} // namespace

int main() {
    roundTripIncludesRotationAndSurfaceCenter();
    panAndAnchoredZoomArePerViewAndMonotonic();
    invalidCommandsFailWithoutMutation();
    return 0;
}
