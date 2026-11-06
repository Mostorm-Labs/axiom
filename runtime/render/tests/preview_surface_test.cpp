#include "canvas/render/preview_surface.hpp"
#include "canvas/render/skia_renderer.hpp"
#include "canvas/render/skia_surface_provider.hpp"

#include <cassert>
#include <cstdlib>

namespace {
void roundSweepPreviewKeepsCapsJoinsAndCancel() {
    using namespace canvas::render;
    TransparentOverlaySkiaSurfaceProvider provider;
    assert(provider.resize(128, 128).code == BackendSubmissionCode::kAccepted);
    SkiaRenderer renderer;
    PreviewSurfaceController controller(renderer, provider);
    assert(controller.begin(1, 41, 1, provider.generation()));
    canvas::ink::BrushPreviewDelta delta;
    delta.revision = 1;
    delta.roundStroke = canvas::ink::RoundStrokePreview{{{24, 32}, {70, 32}, {70, 80}}, 12};
    PreviewStyleOverride style;
    style.red = style.green = style.blue = style.alpha = style.opacityMultiplier = 1;
    assert(controller.update(delta));
    assert(controller.renderIfDirty(style));
    std::vector<std::uint8_t> pixels(128 * 128 * 4);
    assert(provider.readbackRgba(pixels).code == BackendSubmissionCode::kAccepted);
    const auto alpha = [&](unsigned x, unsigned y) { return pixels[(y * 128 + x) * 4 + 3]; };
    assert(alpha(15, 32) > 240); // beyond the first center, inside its round cap
    assert(alpha(70, 89) > 240); // beyond the last center
    assert(alpha(77, 25) > 240); // outside the bend, inside its round join
    assert(alpha(5, 32) == 0 && alpha(90, 12) == 0);
    assert(controller.cancel());
    assert(controller.renderIfDirty(style));
    assert(provider.readbackRgba(pixels).code == BackendSubmissionCode::kAccepted);
    assert(alpha(15, 32) == 0 && alpha(70, 89) == 0);
}
void fadingTrailHasNoJointDots() {
    using namespace canvas::render;
    for(bool diagonal:{false,true}){
    TransparentOverlaySkiaSurfaceProvider provider;
    assert(provider.resize(128,128).code==BackendSubmissionCode::kAccepted);
    SkiaRenderer renderer;PreviewSurfaceController controller(renderer,provider);
    assert(controller.begin(1,42,1,provider.generation()));
    canvas::ink::BrushPreviewDelta delta;delta.revision=1;delta.hasPaint=true;delta.red=1;delta.green=delta.blue=0;
    delta.fadingTrail.emplace();
    for(double x:{16.,40.,64.,88.,112.})delta.fadingTrail->points.push_back({x,diagonal?x:64,12,.4F});
    assert(controller.update(delta));assert(controller.renderIfDirty());
    std::vector<std::uint8_t> rgba(128*128*4);
    assert(provider.readbackRgba(rgba).code==BackendSubmissionCode::kAccepted);
    for(unsigned x=28;x<100;++x){const auto alpha=rgba[((diagonal?x:64)*128+x)*4+3];assert(std::abs(int(alpha)-102)<=3);}
    }
}
} // namespace

int main() {
    roundSweepPreviewKeepsCapsJoinsAndCancel();
    fadingTrailHasNoJointDots();
    using namespace canvas::render;
    using canvas::ink::BrushPreviewDelta;
    TransparentOverlaySkiaSurfaceProvider provider;
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

    // Retiring the final session must hide the overlay without submitting an
    // empty frame. A synchronous platform provider may block on that present
    // (for example while waiting for D3D12 composition/vsync).
    const auto presentBeforeFinalRetire = provider.presentCount();
    assert(controller.retireSession(7U, generation));
    assert(!controller.active());
    assert(!provider.overlayVisible());
    assert(provider.presentCount() == presentBeforeFinalRetire);

    // A mismatched handoff identity must retain preview state.
    assert(!controller.clearAfterCanonicalVisible(1U, 8U, 1U, generation));

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
