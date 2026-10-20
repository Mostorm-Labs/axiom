#include "canvas/debug_ui/builtin_panels.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"
#include "imgui.h"

#include <cmath>
#include <array>
#include <utility>

namespace canvas::debug_ui {
namespace {
void tools(DebugPanelContext& context) {
    const auto& snapshot = context.snapshot;
    ImGui::SeparatorText("Tools");
    ImGui::Text("Product: %s", panel_detail::availabilityLabel(snapshot.product.availability));
    if (!context.controls.hasRuntimeOwner())
        ImGui::TextUnformatted("Tool controls: Unsupported (Runtime owner unbound)");
    if(ImGui::Button("Text tool")){(void)context.controls.setTool(canvas::runtime::CanvasToolKind::kText);context.ui.markControlSubmitted();}
    const auto text=context.controls.textEditor();
    if(text.phase!=canvas::runtime::TextPhase::kClosed){
        double width=text.width;if(ImGui::InputDouble("Text wrap width",&width)){(void)context.controls.setTextWidth(width);context.ui.markControlSubmitted();}
        ImGui::Text("Text phase %u / IME %llu",static_cast<unsigned>(text.phase),static_cast<unsigned long long>(text.composition));
        ImGui::SeparatorText("Text format");
        const std::array<std::pair<const char*,double>,4> sizes{{{"Title",32},{"Heading",24},{"Subheading",20},{"Body",16}}};
        const double currentSize=text.format.fontSize.value_or(16); int sizeIndex=3; for(int i=0;i<4;++i)if(std::abs(currentSize-sizes[i].second)<0.1)sizeIndex=i;
        if(ImGui::BeginCombo("Size",sizes[sizeIndex].first)){for(int i=0;i<4;++i){if(ImGui::Selectable(sizes[i].first,i==sizeIndex)){canvas::runtime::TextFormatPatch p;p.fontSize=sizes[i].second;(void)context.controls.setTextFormat(p);context.ui.markControlSubmitted();}if(i==sizeIndex)ImGui::SetItemDefaultFocus();}ImGui::EndCombo();}
        const auto color=text.format.color.value_or(std::array<float,4>{0,0,0,1});
        int colorIndex=color[2]>0.5F?1:0;const char* colors[]={"Black","Blue"};if(ImGui::Combo("Color",&colorIndex,colors,2)){canvas::runtime::TextFormatPatch p;p.color=colorIndex==0?std::array<float,4>{0,0,0,1}:std::array<float,4>{0.12F,0.42F,0.95F,1};(void)context.controls.setTextFormat(p);context.ui.markControlSubmitted();}
        bool bold=text.format.bold.value_or(false),italic=text.format.italic.value_or(false),underline=text.format.underline.value_or(false),strike=text.format.strikethrough.value_or(false),bullet=text.format.bulleted.value_or(false);
        if(ImGui::Checkbox("Bold",&bold)){canvas::runtime::TextFormatPatch p;p.bold=bold;(void)context.controls.setTextFormat(p);context.ui.markControlSubmitted();}
        ImGui::SameLine();if(ImGui::Checkbox("Italic",&italic)){canvas::runtime::TextFormatPatch p;p.italic=italic;(void)context.controls.setTextFormat(p);context.ui.markControlSubmitted();}
        ImGui::SameLine();if(ImGui::Checkbox("Underline",&underline)){canvas::runtime::TextFormatPatch p;p.underline=underline;(void)context.controls.setTextFormat(p);context.ui.markControlSubmitted();}
        ImGui::SameLine();if(ImGui::Checkbox("Strike",&strike)){canvas::runtime::TextFormatPatch p;p.strikethrough=strike;(void)context.controls.setTextFormat(p);context.ui.markControlSubmitted();}
        if(ImGui::Checkbox("Bulleted list",&bullet)){canvas::runtime::TextFormatPatch p;p.bulleted=bullet;(void)context.controls.setTextFormat(p);context.ui.markControlSubmitted();}
        const char* align[] = {"Left","Center","Right"};int alignIndex=static_cast<int>(text.format.alignment.value_or(1))-1;if(alignIndex<0||alignIndex>2)alignIndex=0;
        if(ImGui::Combo("Align",&alignIndex,align,3)){canvas::runtime::TextFormatPatch p;p.alignment=static_cast<std::uint32_t>(alignIndex+1);(void)context.controls.setTextFormat(p);context.ui.markControlSubmitted();}
        if(!text.fonts.empty()){int fontIndex=0;for(std::size_t i=0;i<text.fonts.size();++i)if(text.format.font==text.fonts[i].id)fontIndex=static_cast<int>(i);std::vector<const char*> names;for(const auto& f:text.fonts)names.push_back(f.name.c_str());if(ImGui::Combo("Font",&fontIndex,names.data(),static_cast<int>(names.size()))){canvas::runtime::TextFormatPatch p;p.font=text.fonts[fontIndex].id;(void)context.controls.setTextFormat(p);context.ui.markControlSubmitted();}}
    }
    bool selectionMode = snapshot.product.value.selection.enabled;
    ImGui::BeginDisabled(!panel_detail::canSubmitProduct(context));
    if (ImGui::Checkbox("Selection mode", &selectionMode)) {
        (void)context.controls.setSelectionMode(selectionMode);
        context.ui.markControlSubmitted();
    }
    const auto eraserId = snapshot.product.value.tool.eraserId;
    if (ImGui::Selectable("Object Eraser", eraserId == 1U)) {
        (void)context.controls.setEraser(1U);
        context.ui.markControlSubmitted();
    }
    if (ImGui::Selectable("Partial Eraser", eraserId == 2U)) {
        (void)context.controls.setEraser(2U);
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    ImGui::SeparatorText("Create");
    const std::array<std::pair<const char*, canvas::runtime::CanvasCreationPreset>, 5> presets{{
        {"Rectangle", canvas::runtime::CanvasCreationPreset::kRectangle},
        {"Ellipse", canvas::runtime::CanvasCreationPreset::kEllipse},
        {"Triangle", canvas::runtime::CanvasCreationPreset::kTriangle},
        {"Line", canvas::runtime::CanvasCreationPreset::kLine},
        {"Arrow", canvas::runtime::CanvasCreationPreset::kArrow},
    }};
    std::size_t selectedPreset = 0U;
    const auto runtimePreset = static_cast<canvas::runtime::CanvasCreationPreset>(
        snapshot.product.value.control.creationPreset);
    for (std::size_t i = 0; i < presets.size(); ++i) {
        if (panel_detail::hasReadback(snapshot.product.availability)
                ? presets[i].second == runtimePreset
                : presets[i].second == context.ui.creationPresetDraft()) {
            selectedPreset = i;
            break;
        }
    }
    ImGui::BeginDisabled(!panel_detail::canSubmitProduct(context));
    if (ImGui::BeginCombo("Creation", presets[selectedPreset].first)) {
        for (std::size_t i = 0; i < presets.size(); ++i) {
            const bool selected = i == selectedPreset;
            if (ImGui::Selectable(presets[i].first, selected)) {
                const auto receipt = context.controls.setCreationPreset(presets[i].second);
                if (receipt.state == canvas::runtime::ProductControlState::kApplied) {
                    context.ui.setCreationPresetDraft(presets[i].second);
                }
                context.ui.markControlSubmitted();
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (panel_detail::hasReadback(snapshot.product.availability)) {
        if (!snapshot.product.value.control.creationMode) {
            ImGui::Text("Creation mode: disabled (last preset: %s)",
                        presets[selectedPreset].first);
        } else {
            ImGui::Text("Creation mode: %s", presets[selectedPreset].first);
        }
        ImGui::Text("resolved eraserId: %u / selection: %s",
                    eraserId,
                    snapshot.product.value.selection.enabled ? "enabled" : "disabled");
        ImGui::Text("legacy toolId (diagnostic): %u", snapshot.product.value.tool.toolId);
    }
}
void view(DebugPanelContext& context) {
    const auto& snapshot = context.snapshot;
    ImGui::SeparatorText("View");
    if (panel_detail::hasReadback(snapshot.product.availability)) {
        const auto& camera = snapshot.product.value.camera;
        ImGui::Text("camera scale %.3f / translation %.3f, %.3f",
                    camera.scale,
                    camera.translationX,
                    camera.translationY);
        ImGui::Text("view generation %llu",
                    static_cast<unsigned long long>(snapshot.stamp.viewGeneration));
    } else
        ImGui::Text("Camera: %s", panel_detail::availabilityLabel(snapshot.product.availability));
    if (panel_detail::hasReadback(snapshot.axiom.availability))
        ImGui::Text("camera generation %llu",
                    static_cast<unsigned long long>(snapshot.axiom.value.camera.generation));
    else
        ImGui::Text("Camera generation: %s",
                    panel_detail::availabilityLabel(snapshot.axiom.availability));
    auto& draft = context.ui.cameraDraft();
    ImGui::TextUnformatted("Camera input drafts (UI-local)");
    ImGui::InputFloat("Pan delta X", &draft.panDeltaX);
    ImGui::InputFloat("Pan delta Y", &draft.panDeltaY);
    const bool available = panel_detail::canSubmitProduct(context);
    ImGui::BeginDisabled(!available || !std::isfinite(draft.panDeltaX) ||
                         !std::isfinite(draft.panDeltaY));
    if (ImGui::Button("Pan by")) {
        (void)context.controls.panBy(draft.panDeltaX, draft.panDeltaY);
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    ImGui::InputFloat("Zoom anchor X", &draft.zoomAnchorX);
    ImGui::InputFloat("Zoom anchor Y", &draft.zoomAnchorY);
    ImGui::InputFloat("Zoom scale delta", &draft.zoomScaleDelta);
    ImGui::BeginDisabled(!available || !std::isfinite(draft.zoomAnchorX) ||
                         !std::isfinite(draft.zoomAnchorY) ||
                         !std::isfinite(draft.zoomScaleDelta) || draft.zoomScaleDelta <= 0.0F);
    if (ImGui::Button("Zoom At")) {
        (void)context.controls.zoomAt(draft.zoomAnchorX, draft.zoomAnchorY, draft.zoomScaleDelta);
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!available);
    if (ImGui::Button("Fit Content")) {
        (void)context.controls.fitToContent();
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!available || snapshot.product.value.selection.selectedObjectCount == 0);
    if (ImGui::Button("Fit Selection")) {
        (void)context.controls.fitToSelection();
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!available || snapshot.product.value.selection.primaryObject == 0);
    if (ImGui::Button("Fit Primary Object")) {
        (void)context.controls.fitPrimaryObject();
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    if (!context.controls.hasRuntimeOwner())
        ImGui::TextUnformatted("View controls: Unsupported (Runtime owner unbound)");
}
void history(DebugPanelContext& context) {
    const auto& snapshot = context.snapshot;
    ImGui::SeparatorText("History");
    ImGui::Text("History: %s", panel_detail::availabilityLabel(snapshot.product.availability));
    const bool available = panel_detail::canSubmitProduct(context);
    ImGui::BeginDisabled(!available || !snapshot.product.value.history.canUndo);
    if (ImGui::Button("Undo (Ctrl+Z)")) {
        (void)context.controls.undo();
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!available || !snapshot.product.value.history.canRedo);
    if (ImGui::Button("Redo (Ctrl+Y)")) {
        (void)context.controls.redo();
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
}
void feature(DebugPanelContext&) {
    ImGui::SeparatorText("Feature");
    ImGui::TextUnformatted("Unsupported / Not implemented (successor feature slot)");
}
} // namespace
void registerControlDebugPanels(PanelRegistry& registry) noexcept {
    (void)registry.add({WorkspaceId::kControl, PanelSlot::kTools, 0, nullptr, tools});
    (void)registry.add({WorkspaceId::kControl, PanelSlot::kView, 0, nullptr, view});
    (void)registry.add({WorkspaceId::kControl, PanelSlot::kHistory, 0, nullptr, history});
    (void)registry.add({WorkspaceId::kControl, PanelSlot::kFeature, 0, nullptr, feature});
}
} // namespace canvas::debug_ui
