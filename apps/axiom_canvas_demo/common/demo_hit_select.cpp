#include "common/demo_hit_select.hpp"

#include <limits>

namespace axiom::demo {
namespace {

std::uint64_t lowId(const canvas::foundation::ObjectId& id) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value |= static_cast<std::uint64_t>(id.bytes[index]) << (index * 8U);
    }
    return value;
}

} // namespace

canvas::foundation::Result<canvas::HitTestResult> SceneHitTestPort::hitTest(
    canvas::WorldPoint worldPoint) const {
    return scene_.hitTest(canvas::HitTestRequest{
        .worldPoint = worldPoint,
        .tolerance = 0.0F,
        .filter = canvas::HitTestFilter{},
        .maximumResults = 1U,
    });
}

canvas::foundation::Result<DemoHitSelection> DemoHitSelectHarness::selectAtViewPoint(
    canvas::foundation::WorldPoint viewPoint,
    const CameraController& camera,
    const canvas::render::SurfaceMetrics& metrics) {
    const auto worldPoint = camera.viewToWorld(viewPoint, metrics);
    if (!worldPoint) {
        return canvas::foundation::Result<DemoHitSelection>::failure(worldPoint.error());
    }
    const auto hit = port_.hitTest(worldPoint.value());
    if (!hit) {
        return canvas::foundation::Result<DemoHitSelection>::failure(hit.error());
    }
    if (transient_.selectionGeneration == std::numeric_limits<std::uint64_t>::max()) {
        return canvas::foundation::Result<DemoHitSelection>::failure(
            {canvas::foundation::ErrorCode::kOutOfMemory,
             "demo selection generation exhausted"});
    }
    ++transient_.selectionGeneration;
    if (hit.value().frontToBack.empty()) {
        transient_.selectedObject.reset();
        transient_.overlayDigest = "overlay:none";
    } else {
        transient_.selectedObject = hit.value().frontToBack.front();
        transient_.overlayDigest = "overlay:selection:" +
                                   std::to_string(lowId(*transient_.selectedObject));
    }
    return canvas::foundation::Result<DemoHitSelection>::success(
        DemoHitSelection{.hit = hit.value()});
}

} // namespace axiom::demo
