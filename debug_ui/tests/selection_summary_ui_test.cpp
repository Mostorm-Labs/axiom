#include "canvas/debug_ui/builtin_panels.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"
#include "canvas/debug_ui/snapshot_assembler.hpp"
#include "canvas/runtime/runtime_composition.hpp"
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
    DebugControlRouter router(&facade, nullptr, nullptr, nullptr);
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
            g.NavActivateId = g.NavActivateDownId = ImGui::GetID(activate);
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
    ImGui::DestroyContext();
    assert(c.close());
    std::cout << "PASS read-only summary UI, typed disabled actions and stale selection fence\n";
}
