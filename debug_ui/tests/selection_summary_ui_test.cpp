#include "canvas/debug_ui/builtin_panels.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"
#include "canvas/debug_ui/snapshot_assembler.hpp"
#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/internal/multi_selection_port.hpp"
#include "imgui.h"
#include "imgui_internal.h"

#include <cassert>
#include <iostream>

int main() {
    using namespace canvas;
    using namespace debug_ui;
    const auto id = [](std::uint64_t n) { return foundation::ObjectId::fromUint64(n); };
    runtime::RuntimeComposition c;
    assert(c.open() && c.bindSurface(800, 600));
    assert(c.multiSelectionPort().setPermissionMode(runtime::MultiSelectionPermissionMode::kLocalSingleUser));
    auto& facade = c.controlFacade();
    semantic::ObjectRecord section;
    section.id = id(3);
    section.kind = semantic::ObjectKind::kSection;
    section.kind_version = 1;
    section.content = semantic::SectionContent{{0, 0, 400, 300}, "Summary", 8, 20};
    section.placement.order_key = semantic::OrderKey({3});
    std::vector<semantic::ObjectRecord> records{section};
    for (unsigned n : {1U, 2U}) {
        semantic::ObjectRecord shape;
        shape.id = id(n);
        shape.kind = semantic::ObjectKind::kShape;
        shape.kind_version = 1;
        shape.placement = {section.id, semantic::OrderKey({static_cast<std::uint8_t>(n)})};
        shape.content = semantic::ShapeContent{1, 40, 30};
        shape.transform.tx = 50 * n;
        shape.transform.ty = 60;
        shape.properties.entries = {{3U, n == 1 ? 0.5F : 0.75F}};
        records.push_back(shape);
    }
    semantic::Operation seed{semantic::OperationId{id(7000)},
                             semantic::DocumentId{id(1)},
                             1,
                             1,
                             semantic::InsertObjectsOp{records}};
    assert(c.applyExternalOperation(seed, semantic::ApplySource::kRemoteSync).disposition ==
           semantic::ApplyDisposition::kApplied);
    runtime::SectionIntent choose;
    choose.action = runtime::SectionAction::kSelectChildren;
    choose.objectId = section.id;
    assert(facade.submitSectionIntent(choose, 1, 7004).state ==
           runtime::ProductControlState::kQueued);
    assert(c.ownerPump());
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1000, 1800};
    io.DeltaTime = 1.0F / 60;
    unsigned char* pixels = nullptr;
    int w = 0, h = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    PanelRegistry panels;
    registerBuiltinPanels(panels);
    DebugUiSessionState ui;
    ui.setAdvancedControls(true);
    DebugControlRouter router(&facade, nullptr, nullptr, nullptr, &c.multiSelectionPort());
    DebugSnapshotAssembler assembler;
    const auto snapshot = [&] { return assembler.capture({&facade}); };
    const auto render = [&](const DebugSnapshot& s, const char* activate = nullptr) {
        router.beginFrame(s);
        ui.beginFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowSize({1000, 1800}, ImGuiCond_Always);
        ImGui::Begin("Summary oracle");
        ImGui::LogToBuffer();
        auto& g = *ImGui::GetCurrentContext();
        if (activate) {
            if (std::string(activate) == "ApplyOpacity") {
                ImGui::PushID(3);
                g.NavActivateId = g.NavActivateDownId = ImGui::GetID("Apply");
                ImGui::PopID();
            } else {
                g.NavActivateId = g.NavActivateDownId = ImGui::GetID(activate);
            }
            g.NavInputSource = ImGuiInputSource_Keyboard;
        }
        DebugPanelContext context{s, router, ui};
        for (const auto& panel : panels.contributions(WorkspaceId::kControl))
            if (panel.slot == PanelSlot::kTools)
                panel.render(context);
        std::string text = g.LogBuffer.c_str();
        ImGui::LogFinish();
        ImGui::End();
        ImGui::Render();
        g.NavActivateId = g.NavActivateDownId = 0;
        ImGui::ClearActiveID();
        return text;
    };
    auto s = snapshot();
    assert(s.selectionSummary.count == 2);
    const auto before = c.readback();
    const auto history = s.product.value.history;
    // A consumer must obey the supplied typed capability even when the legacy
    // count/lock fields would otherwise allow the action.
    s.selectionSummary
        .actions[static_cast<std::size_t>(runtime::SelectionAction::kDelete)] = {
        runtime::SelectionCapabilityState::kUnavailable, runtime::SelectionReason::kBusy};
    (void)render(s);
    (void)render(s, "Delete selected");
    assert(c.ownerPump());
    assert(c.semanticObjectCount() == 3 &&
           c.readback().semanticGeneration == before.semanticGeneration);
    router.beginFrame(snapshot());
    const auto projection = router.multiSelectionProjection();
    assert(projection.available && projection.properties.at(3U).mixed);
    const auto multiText = render(snapshot());
    assert(multiText.find("Multi-selection properties") != std::string::npos);
    assert(multiText.find("Opacity: 2/2 mixed") != std::string::npos);
    ui.multiSelectionDraft().opacity = 0.6F;
    (void)render(snapshot(), "ApplyOpacity");
    assert(c.ownerPump());
    const auto outcome = router.multiSelectionOutcome();
    assert(outcome && outcome->applied && outcome->changed.size() == 2U);
    assert(facade.undo(3, facade.readRuntimeState().identity.runtimeGeneration, 7778).state ==
           runtime::ProductControlState::kQueued);
    assert(c.ownerPump());
    const auto text = render(snapshot());
    assert(text.find("Selected roots: 2") != std::string::npos);
    assert(text.find("Shape: 2") != std::string::npos);
    assert(text.find("Opacity: mixed") != std::string::npos);
    assert(text.find("Not implemented") != std::string::npos);
    assert(text.find("Bounds") != std::string::npos);
    const auto stale = snapshot();
    choose.action = runtime::SectionAction::kSelect;
    assert(facade.submitSectionIntent(choose, 2, 7004).state ==
           runtime::ProductControlState::kQueued);
    assert(c.ownerPump());
    router.beginFrame(stale);
    assert(router.selectedAction(runtime::CanvasControlPayloadKind::kDeleteSelected).state ==
           runtime::ProductControlState::kRejected);
    assert(c.ownerPump());
    assert(c.semanticObjectCount() == 3);
    assert(facade.readRuntimeState().history.canUndo == history.canUndo);
    assert(render(snapshot()).find("Selected roots: 1") != std::string::npos);
    choose.action = runtime::SectionAction::kSelectChildren;
    assert(facade.submitSectionIntent(choose, 4, 7004).state == runtime::ProductControlState::kQueued);
    assert(c.ownerPump());
    auto groupSnapshot = snapshot();
    assert(groupSnapshot.selectionSummary.count == 2);
    assert(groupSnapshot.selectionSummary.capability(runtime::SelectionAction::kGroup).available());
    (void)render(groupSnapshot);
    (void)render(groupSnapshot, "Group selected");
    assert(c.ownerPump());
    assert(c.semanticObjectCount() == 4);
    auto grouped = snapshot();
    assert(grouped.selectionSummary.count == 1);
    assert(grouped.selectionSummary.capability(runtime::SelectionAction::kUngroup).available());
    (void)render(grouped);
    (void)render(grouped, "Ungroup selected");
    assert(c.ownerPump());
    assert(c.semanticObjectCount() == 3);
    assert(snapshot().selectionSummary.count == 2);
    (void)render(snapshot());
    (void)render(snapshot(), "Group selected");
    assert(c.ownerPump());
    const auto selectedGroup = snapshot().selectionSummary.primary;
    assert(!selectedGroup.isZero());
    (void)render(snapshot());
    (void)render(snapshot(), "Enter group");
    assert(c.ownerPump());
    assert(snapshot().selectionSummary.groupScope == selectedGroup);
    assert(facade.setSelectionMode(true, 5, facade.readRuntimeState().identity.runtimeGeneration, 7004).state ==
           runtime::ProductControlState::kQueued);
    assert(c.ownerPump() && c.registerSource(7004));
    const auto view = facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front());
    input::PlatformPointerBatch down;
    down.samples.push_back({7004, 1, 1, 1000000, 70 * view.cameraScale + view.cameraTranslationX,
                            75 * view.cameraScale + view.cameraTranslationY, 0, 0, 0, {}, {},
                            input::SampleProvenance::kConfirmedCurrent, input::PointerPhase::kDown});
    assert(c.submit(down));
    auto up=down;up.samples.front().sequence=2;up.samples.front().timestampNs=2000000;
    up.samples.front().phase=input::PointerPhase::kUp;
    assert(c.submit(up));
    assert(snapshot().selectionSummary.primary == id(1));
    (void)render(snapshot());
    (void)render(snapshot(), "Leave group");
    assert(c.ownerPump());
    assert(!snapshot().selectionSummary.groupScope);
    ImGui::DestroyContext();
    assert(c.close());
    std::cout << "PASS read-only summary UI, typed disabled actions and stale selection fence\n";
}
