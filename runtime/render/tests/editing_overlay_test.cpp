#include "canvas/render/editing_overlay.hpp"
#include "canvas/render/render_view_runtime.hpp"

#include "canvas/foundation/object_id.hpp"

#include <cassert>

namespace {
using canvas::foundation::ObjectId;
using canvas::foundation::WorldPoint;
using canvas::foundation::WorldRect;
using canvas::render::CameraGeneration;
using canvas::render::CameraState;
using canvas::render::EditingOverlay;
using canvas::render::EditingOverlayInput;
using canvas::render::FrameState;
using canvas::render::HandleKind;
using canvas::render::HandleCapabilities;
using canvas::render::MetricsGeneration;
using canvas::render::RenderViewRuntime;
using canvas::render::SurfaceGeneration;
using canvas::render::SurfaceMetrics;
using canvas::render::SnapGuideGeometry;
using canvas::render::SnapGuideAxis;
using canvas::render::ViewId;

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }

FrameState frame(ViewId view, CameraGeneration camera, float zoom, float dpr = 1.0F,
                 std::uint32_t physicalWidth = 800, std::uint32_t physicalHeight = 600) {
    return FrameState{view,
                      CameraState{WorldPoint{0.0F, 0.0F}, zoom, 0.0F, camera},
                      WorldRect{-100.0F, -100.0F, 100.0F, 100.0F},
                      SurfaceMetrics{800.0F, 600.0F, physicalWidth, physicalHeight, dpr, dpr},
                      canvas::semantic::SemanticGeneration(4),
                      canvas::foundation::SceneRevision(4),
                      SurfaceGeneration(5),
                      MetricsGeneration(6),
                      canvas::render::FrameId(7)};
}

void two_views_keep_overlay_state_independent() {
    RenderViewRuntime first(frame(ViewId(1), CameraGeneration(10), 1.0F));
    RenderViewRuntime second(frame(ViewId(2), CameraGeneration(20), 2.0F));
    const EditingOverlayInput input{id(11), WorldRect{-10.0F, -5.0F, 10.0F, 5.0F}, {}, true,
                                    true, true};
    assert(first.editingOverlay().update(input));
    assert(first.editingOverlay().selectedObject() == id(11));
    assert(second.editingOverlay().selectedObject().isZero());
    assert(first.editingOverlay().updateCount() == 1);
    assert(second.editingOverlay().updateCount() == 0);
}

void chrome_is_screen_space_stable_and_hit_slop_is_larger() {
    RenderViewRuntime normal(frame(ViewId(1), CameraGeneration(10), 1.0F));
    RenderViewRuntime zoomed(frame(ViewId(1), CameraGeneration(11), 4.0F));
    const EditingOverlayInput input{id(12), WorldRect{-10.0F, -10.0F, 10.0F, 10.0F}, {}, true,
                                    true, true};
    assert(normal.editingOverlay().update(input));
    assert(zoomed.editingOverlay().update(input));
    const auto normalHandle = normal.editingOverlay().handle(HandleKind::kTopLeft);
    const auto zoomedHandle = zoomed.editingOverlay().handle(HandleKind::kTopLeft);
    assert(normalHandle.visualRadius == zoomedHandle.visualRadius);
    assert(normalHandle.hitRadius > normalHandle.visualRadius);
    assert(normal.editingOverlay().selectionOutline().visible);
    assert(zoomed.editingOverlay().selectionOutline().visible);
    assert(normal.editingOverlay().selectionOutline().corners !=
           zoomed.editingOverlay().selectionOutline().corners);
    assert(normal.editingOverlay().hitTest(normalHandle.center, normalHandle.hitRadius - 0.5F) ==
           HandleKind::kTopLeft);
}

void chrome_is_stable_across_device_pixel_ratios() {
    RenderViewRuntime low(frame(ViewId(4), CameraGeneration(13), 1.0F, 1.0F, 800, 600));
    RenderViewRuntime high(frame(ViewId(5), CameraGeneration(14), 1.0F, 2.0F, 1600, 1200));
    const EditingOverlayInput input{id(14), WorldRect{-10.0F, -10.0F, 10.0F, 10.0F}, {}, true,
                                    false, false};
    assert(low.editingOverlay().update(input));
    assert(high.editingOverlay().update(input));
    assert(low.editingOverlay().handle(HandleKind::kRight).visualRadius ==
           high.editingOverlay().handle(HandleKind::kRight).visualRadius);
    assert(low.editingOverlay().handle(HandleKind::kRight).hitRadius ==
           high.editingOverlay().handle(HandleKind::kRight).hitRadius);
}

void unsupported_handle_capabilities_do_not_create_hit_targets() {
    RenderViewRuntime view(frame(ViewId(3), CameraGeneration(12), 1.0F));
    EditingOverlayInput input{id(13), WorldRect{-10.0F, -10.0F, 10.0F, 10.0F}, {}, true,
                              false, false, HandleCapabilities{false, true, false}};
    assert(view.editingOverlay().update(input));
    assert(view.editingOverlay().handle(HandleKind::kTopLeft).kind == HandleKind::kNone);
    assert(view.editingOverlay().handle(HandleKind::kTop).kind == HandleKind::kTop);
    assert(view.editingOverlay().handle(HandleKind::kRotation).kind == HandleKind::kNone);
}

void guides_are_transient_screen_space_overlay_geometry() {
    RenderViewRuntime view(frame(ViewId(6), CameraGeneration(15), 1.0F));
    const std::array guides{
        SnapGuideGeometry{SnapGuideAxis::kVertical, {120.0F, 10.0F}, {120.0F, 240.0F}},
        SnapGuideGeometry{SnapGuideAxis::kHorizontal, {20.0F, 90.0F}, {220.0F, 90.0F}}};
    EditingOverlayInput input{id(15), WorldRect{-10.0F, -10.0F, 10.0F, 10.0F}, {}, true,
                              false, true, HandleCapabilities{}, guides};
    assert(view.editingOverlay().update(input));
    assert(view.editingOverlay().guides().size() == 2U);
    assert(view.editingOverlay().guides()[0].axis == SnapGuideAxis::kVertical);
    assert(view.editingOverlay().guides()[1].start.y == 90.0F);
    view.editingOverlay().clear();
    assert(view.editingOverlay().guides().empty());
}
} // namespace

int main() {
    two_views_keep_overlay_state_independent();
    chrome_is_screen_space_stable_and_hit_slop_is_larger();
    chrome_is_stable_across_device_pixel_ratios();
    unsupported_handle_capabilities_do_not_create_hit_targets();
    guides_are_transient_screen_space_overlay_geometry();
    return 0;
}
