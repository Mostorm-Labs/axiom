#include "canvas/runtime/internal/runtime_composition_core.hpp"

#include <algorithm>
#include <stdexcept>

using namespace canvas;
namespace {
void require(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
// A second finger can claim the viewport before a newly sealed brush has had
// its first canonical frame. Keep that frame eligible until the camera moves.
void stationaryPinchAfterBrush() {
    runtime::RuntimeCompositionCore core;
    require(core.openDocument(semantic::DocumentId{foundation::ObjectId::fromUint64(42)}) &&
                core.bindSurface(256, 128) && core.registerSource(7) &&
                core.setPlatformPresentationDeferred(true),
            "brush/pinch setup");
    auto& facade = core.controlFacade();
    runtime::CanvasControlRequest request{};
    request.clientId = 17;
    request.requestId = 1;
    request.target = facade.mountedCanvasTargets().front();
    request.payload.kind = runtime::CanvasControlPayloadKind::kSelectPreset;
    request.payload.preset = {"round-handdrawn", 4};
    require(facade.submitCanvasControl(request).state == runtime::ProductControlState::kQueued &&
                core.ownerPump(),
            "select v4 brush");
    const auto acknowledge = [&](const runtime::CanonicalWork& work) {
        return core.receivePlatformPresented({work,
                                              render::PresentOutcome::kPresented,
                                              render::PresentEvidenceKind::kPlatformQualified,
                                              {}});
    };
    runtime::RuntimeFrameOpportunity frame;
    require(core.serviceFrameOpportunity(false, frame) && frame.canonicalWork &&
                acknowledge(*frame.canonicalWork),
            "qualify initial empty canvas");
    if (core.cpuPendingFacts().capability)
        require(core.bindCpuPendingExecutor(1), "enable ordered Brush commits");
    std::uint64_t sequence = 0;
    const auto pointer =
        [&](unsigned id, input::PointerTool tool, input::PointerPhase phase, float x, float y) {
            input::PlatformPointerSample s;
            s.source = 7;
            s.pointer = id;
            s.capabilities.tool = tool;
            s.phase = phase;
            s.x = x;
            s.y = y;
            s.sequence = ++sequence;
            s.timestampNs = sequence * 8'000'000;
            require(core.submit({{s}}), "brush/pinch input");
        };
    using Phase = input::PointerPhase;
    using Tool = input::PointerTool;
    pointer(1, Tool::kPen, Phase::kDown, 40, 60);
    pointer(1, Tool::kPen, Phase::kMove, 55, 60);
    pointer(1, Tool::kPen, Phase::kUp, 70, 60);
    pointer(2, Tool::kFinger, Phase::kDown, 80, 40);
    pointer(3, Tool::kFinger, Phase::kDown, 160, 40);
    pointer(2, Tool::kFinger, Phase::kMove, 80, 40);
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        require(core.serviceFrameOpportunity(false, frame), "stationary handoff opportunity");
        require(!frame.navigationPending && !frame.navigationPresented,
                "stationary two-finger claim preempted the sealed brush with navigation");
        if (frame.previewWork)
            require(core.finishFrameOpportunity({frame.previewWork, true}), "submit brush preview");
        if (frame.canonicalWork && core.semanticObjectCount() == 1)
            break;
    }
    require(frame.canonicalWork &&
                frame.canonicalWork->frame.sceneGeneration == core.readback().semanticGeneration &&
                frame.pendingCanonicalHandoff == 1,
            "new stroke missing an exact submitted handoff");
    const auto work = *frame.canonicalWork;
    std::vector<std::uint8_t> pixels(256 * 128 * 4);
    require(core.readCanonicalRgba(pixels), "read brush handoff pixels");
    const auto offset = (60 * 256 + 55) * 4;
    require(!std::equal(pixels.begin() + offset, pixels.begin() + offset + 4, pixels.begin()),
            "empty canonical brush frame");

    pointer(2, Tool::kFinger, Phase::kMove, 65, 40);
    require(core.serviceFrameOpportunity(false, frame) && frame.navigationPending &&
                !frame.canonicalWork && core.lastSubmittedCanonicalWork() == work,
            "first real pinch move lost navigation or duplicated pending canonical work");
    require(acknowledge(work) && core.semanticObjectCount() == 1,
            "original brush receipt lost during pinch");
}
} // namespace
int main() {
    stationaryPinchAfterBrush();
}
