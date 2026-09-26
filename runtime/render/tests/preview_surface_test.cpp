#include "canvas/render/preview_surface.hpp"
#include "canvas/render/skia_renderer.hpp"
#include "canvas/render/skia_surface_provider.hpp"

#include <cassert>

int main() {
    using namespace canvas::render;
    using canvas::ink::BrushPreviewDelta;
    RasterSkiaSurfaceProvider provider;
    assert(provider.resize(64U, 64U).code == BackendSubmissionCode::kAccepted);
    SkiaRenderer renderer;
    PreviewSurfaceController controller(renderer, provider);
    const auto generation = provider.generation();
    assert(controller.begin(1U, 7U, 1U, generation));

    BrushPreviewDelta delta;
    delta.revision = 1U;
    delta.outline = {{4.0, 4.0}, {20.0, 4.0}, {20.0, 20.0}, {4.0, 20.0}};
    assert(controller.update(delta));
    assert(controller.state().dirty);
    assert(controller.renderIfDirty());
    const auto firstPresent = provider.presentCount();
    assert(firstPresent == 1U);
    assert(!controller.state().dirty);
    // Same revision and viewport is a no-op, including repeated render ticks.
    assert(controller.update(delta));
    assert(controller.renderIfDirty());
    assert(provider.presentCount() == firstPresent);

    // A viewport change is independently dirty even when geometry revision is stable.
    assert(controller.update(delta, 2.0F));
    assert(controller.state().dirty);
    assert(controller.renderIfDirty());
    assert(provider.presentCount() == firstPresent + 1U);

    // Pinch/viewport changes must re-render retained preview geometry even
    // when no new BrushPreviewDelta revision was published.
    assert(controller.updateViewport(3.0F, 12.0F, -4.0F));
    assert(controller.state().dirty);
    assert(controller.renderIfDirty());
    assert(provider.presentCount() == firstPresent + 2U);

    // A second active BrushSession must add a contour instead of replacing
    // the first session's retained preview geometry.
    assert(controller.begin(1U, 8U, 1U, generation));
    BrushPreviewDelta secondDelta;
    secondDelta.revision = 1U;
    secondDelta.outline = {{30.0, 30.0}, {44.0, 30.0}, {44.0, 44.0}, {30.0, 44.0}};
    assert(controller.updateForSession(8U, secondDelta, 3.0F, 12.0F, -4.0F));
    assert(controller.geometry().contours.size() == 2U);
    assert(controller.geometry().contours[0].outline.size() == delta.outline.size());
    assert(controller.geometry().contours[1].outline.size() == secondDelta.outline.size());
    assert(controller.renderIfDirty());
    assert(controller.retireSession(8U, generation));
    assert(controller.active());
    assert(controller.geometry().contours.size() == 1U);

    // A mismatched handoff identity must retain preview state.
    assert(!controller.clearAfterCanonicalVisible(1U, 8U, 1U, generation));
    assert(controller.active());
    assert(controller.clearAfterCanonicalVisible(1U, 7U, 1U, generation));
    assert(!controller.active());
    assert(controller.state().submittedRevision == controller.state().contentRevision);

    // A stale generation is rejected without presenting.
    RasterSkiaSurfaceProvider staleProvider;
    assert(staleProvider.resize(64U, 64U).code == BackendSubmissionCode::kAccepted);
    PreviewSurfaceController stale(renderer, staleProvider);
    assert(stale.begin(1U, 9U, 1U, staleProvider.generation()));
    auto staleDelta = delta;
    staleDelta.revision = 2U;
    assert(stale.update(staleDelta));
    assert(staleProvider.advanceGeneration().code == BackendSubmissionCode::kAccepted);
    assert(!stale.renderIfDirty());
    assert(stale.state().dirty);
    return 0;
}
