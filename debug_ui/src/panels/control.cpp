#include "canvas/debug_ui/builtin_panels.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"
#include "widgets.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

namespace canvas::debug_ui {
namespace {
using Tool = canvas::runtime::CanvasToolKind;
using Preset = canvas::runtime::CanvasCreationPreset;
void textFormat(DebugPanelContext& c) {
    const auto text = c.controls.textEditor();
    if (text.phase == canvas::runtime::TextPhase::kClosed && text.formatObject.isZero()) return;
    ImGui::SeparatorText("Text format");
    ImGui::BeginDisabled(!panel_detail::canSubmitProduct(c) || c.snapshot.product.value.control.selectedLocked ||
                        c.snapshot.product.value.selection.selectedObjectCount>1);
    const std::array<std::pair<const char*, double>, 4> sizes{{{"Title",32},{"Heading",24},{"Subheading",20},{"Body",16}}};
    const double currentSize = text.format.fontSize.value_or(16);
    char customSize[48]{};
    (void)std::snprintf(customSize, sizeof(customSize), "Custom (%.4g)", currentSize);
    std::string sizeLabel = customSize;
    for (const auto& size : sizes) if (std::abs(currentSize - size.second) < 0.001) sizeLabel = size.first;
    if (ImGui::BeginCombo("Size", sizeLabel.c_str())) {
        for (const auto& size : sizes) if (ImGui::Selectable(size.first, sizeLabel == size.first)) {
            canvas::runtime::TextFormatPatch patch; patch.fontSize = size.second;
            (void)c.controls.setTextFormat(patch); c.ui.markControlSubmitted();
        }
        ImGui::EndCombo();
    }
    std::string fontLabel = text.format.font ? "Unavailable font" : "Default / mixed font";
    for (const auto& font : text.fonts) if (text.format.font == font.id) fontLabel = font.name;
    ImGui::BeginDisabled(text.fonts.empty());
    if (ImGui::BeginCombo("Font", fontLabel.c_str())) {
        for (const auto& font : text.fonts) if (ImGui::Selectable(font.name.c_str(), text.format.font == font.id)) {
            canvas::runtime::TextFormatPatch patch; patch.font = font.id;
            (void)c.controls.setTextFormat(patch); c.ui.markControlSubmitted();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    const std::array<std::array<float,4>,2> colors{{{0,0,0,1},{0.12F,0.42F,0.95F,1}}};
    const auto color = text.format.color.value_or(colors[0]);
    const char* colorNames[] = {"Black", "Blue"};
    const char* colorLabel = "Custom color";
    for (std::size_t i=0; i<colors.size(); ++i) if (color == colors[i]) colorLabel = colorNames[i];
    if (ImGui::BeginCombo("Color", colorLabel)) {
        for (std::size_t i=0; i<colors.size(); ++i) if (ImGui::Selectable(colorNames[i], color == colors[i])) {
            canvas::runtime::TextFormatPatch patch; patch.color = colors[i];
            (void)c.controls.setTextFormat(patch); c.ui.markControlSubmitted();
        }
        ImGui::EndCombo();
    }
    bool first=true;
    const auto effect = [&](const char* label, const std::optional<bool>& value,
                            std::optional<bool> canvas::runtime::TextFormatPatch::* field) {
        if (panel_detail::flowButton(label, first, value.value_or(false))) {
            canvas::runtime::TextFormatPatch patch; patch.*field = !value.value_or(false);
            (void)c.controls.setTextFormat(patch); c.ui.markControlSubmitted();
        }
    };
    effect("Bold", text.format.bold, &canvas::runtime::TextFormatPatch::bold);
    effect("Italic", text.format.italic, &canvas::runtime::TextFormatPatch::italic);
    effect("Underline", text.format.underline, &canvas::runtime::TextFormatPatch::underline);
    effect("Strike", text.format.strikethrough, &canvas::runtime::TextFormatPatch::strikethrough);
    effect("Bulleted list", text.format.bulleted, &canvas::runtime::TextFormatPatch::bulleted);
    const char* align[] = {"Left", "Center", "Right"};
    int alignment = static_cast<int>(text.format.alignment.value_or(1)) - 1;
    if (alignment < 0 || alignment > 2) alignment = 0;
    if (ImGui::Combo("Align", &alignment, align, 3)) {
        canvas::runtime::TextFormatPatch patch; patch.alignment = static_cast<std::uint32_t>(alignment + 1);
        (void)c.controls.setTextFormat(patch); c.ui.markControlSubmitted();
    }
    const auto kind = c.snapshot.product.value.control.selectedKind;
    if (text.phase != canvas::runtime::TextPhase::kClosed &&
        kind != static_cast<std::uint32_t>(canvas::semantic::ObjectKind::kShape) &&
        kind != static_cast<std::uint32_t>(canvas::semantic::ObjectKind::kSticky)) {
        double width = text.width;
        if (ImGui::InputDouble("Text wrap width", &width)) {
            (void)c.controls.setTextWidth(width); c.ui.markControlSubmitted();
        }
    }
    if (text.mixedFormat) ImGui::TextWrapped("Mixed selection: active effects apply to part of the selection.");
    if (c.ui.advancedControls()) ImGui::Text("Text phase %u / IME %llu", static_cast<unsigned>(text.phase),
        static_cast<unsigned long long>(text.composition));
    ImGui::EndDisabled();
}
void tools(DebugPanelContext& c) {
    const auto& state = c.snapshot.product.value;
    const bool creating = state.control.creationMode;
    const auto preset = static_cast<Preset>(state.control.creationPreset);
    const auto tool = c.controls.canvasTool();
    ImGui::SeparatorText("Tools");
    ImGui::BeginDisabled(!panel_detail::canSubmitProduct(c) || c.controls.imageCrop().active);
    bool first=true;
    const auto chooseTool = [&](const char* label, Tool value, bool selected) {
        if (panel_detail::flowButton(label, first, selected)) {
            (void)c.controls.setTool(value); c.ui.markControlSubmitted();
        }
    };
    using SelectionMode=canvas::runtime::CanvasSelectionMode;
    const auto chooseSelection=[&](const char* label,SelectionMode mode) {
        if(panel_detail::flowButton(label,first,state.selection.enabled&&!creating&&state.control.selectionAcquisitionMode==mode)) {
            if(mode==SelectionMode::kPoint) (void)c.controls.setTool(Tool::kSelect);
            else (void)c.controls.setSelectionAcquisitionMode(mode);
            c.ui.markControlSubmitted();
        }
    };
    chooseSelection("Select",SelectionMode::kPoint);
    chooseSelection("Rectangle select",SelectionMode::kRectangle);
    chooseSelection("Freeform lasso",SelectionMode::kFreeform);
    chooseTool("Hand",Tool::kPan,tool==Tool::kPan&&!creating);
    chooseTool("Ink", Tool::kInk, tool == Tool::kInk && !creating && !state.selection.enabled);
    chooseTool("Eraser", Tool::kEraser, tool == Tool::kEraser && !creating);
    chooseTool("Text tool", Tool::kText, tool == Tool::kText && !creating);
    if (preset <= Preset::kTriangle && panel_detail::hasReadback(c.snapshot.product.availability))
        c.ui.setCreationPresetDraft(preset);
    const bool shape = creating && preset <= Preset::kTriangle;
    if (panel_detail::flowButton("Shape", first, shape)) {
        (void)c.controls.setCreationPreset(c.ui.creationPresetDraft()); c.ui.markControlSubmitted();
    }
    if (panel_detail::flowButton("Sticky", first, creating && preset == Preset::kSticky)) {
        (void)c.controls.setCreationPreset(Preset::kSticky); c.ui.markControlSubmitted();
    }
    const bool connector = creating && (preset == Preset::kLine || preset == Preset::kArrow);
    if (panel_detail::flowButton("Connector", first, connector)) {
        const auto receipt = c.controls.setConnectorRouting(canvas::semantic::ConnectorRouting::kOrthogonal, false);
        if (receipt.state == canvas::runtime::ConnectorCommandState::kApplied)
            (void)c.controls.setCreationPreset(preset == Preset::kArrow ? Preset::kArrow : Preset::kLine);
        c.ui.markControlSubmitted();
    }
    if (shape) {
        const char* labels[] = {"Rectangle", "Ellipse", "Triangle"};
        int index = static_cast<int>(preset);
        if (ImGui::Combo("Creation", &index, labels, 3)) {
            (void)c.controls.setCreationPreset(static_cast<Preset>(index)); c.ui.markControlSubmitted();
        }
    }
    if (connector) {
        int index = preset == Preset::kArrow ? 1 : 0;
        if (ImGui::Combo("Connector style", &index, "No arrow\0Arrow\0")) {
            const auto receipt = c.controls.setConnectorRouting(canvas::semantic::ConnectorRouting::kOrthogonal, false);
            if (receipt.state == canvas::runtime::ConnectorCommandState::kApplied)
                (void)c.controls.setCreationPreset(index == 1 ? Preset::kArrow : Preset::kLine);
            c.ui.markControlSubmitted();
        }
    }
    if ((!creating && tool == Tool::kEraser) || c.ui.advancedControls()) {
        bool eraserFirst=true;
        for (const auto& mode : std::array<std::pair<const char*,std::uint32_t>,2>{{{"Object Eraser",1},{"Partial Eraser",2}}}) {
            if (panel_detail::flowButton(mode.first, eraserFirst, state.tool.eraserId == mode.second)) {
                (void)c.controls.setEraser(mode.second); c.ui.markControlSubmitted();
            }
        }
    }
    ImGui::EndDisabled();
    if(state.control.selectionQueryStatus==canvas::runtime::SelectionQueryStatus::kPartial) {
        ImGui::TextWrapped("Selection unchanged: region query has unsupported object providers.");
        for(std::size_t i=0;i<state.control.regionUnsupportedKindCount;++i)
            ImGui::Text("Unsupported object kind: %u",state.control.regionUnsupportedKinds[i]);
    } else if(state.control.selectionQueryStatus==canvas::runtime::SelectionQueryStatus::kFailed||
              state.control.selectionQueryStatus==canvas::runtime::SelectionQueryStatus::kStale) {
        ImGui::TextWrapped("Selection query failed or became stale; selection was preserved.");
    }
    if (!c.controls.hasRuntimeOwner()) ImGui::TextWrapped("Runtime unavailable");
    if (c.controls.hasRuntimeOwner() && !c.controls.connectorDefaultsReady()) {
        ImGui::TextWrapped("Connector defaults unavailable");
        ImGui::BeginDisabled(!panel_detail::canSubmitProduct(c));
        bool retryFirst=true;
        if (panel_detail::flowButton("Retry connector setup", retryFirst)) {
            c.controls.prepareConnectorDefaults(c.snapshot.stamp.runtimeGeneration, true);
            c.ui.markControlSubmitted();
        }
        ImGui::EndDisabled();
    }
    if (state.selection.selectedObjectCount != 0) {
        ImGui::SeparatorText("Selected object");
        ImGui::BeginDisabled(!panel_detail::canSubmitProduct(c));
        bool objectFirst=true;
        const bool sectionSelected=state.control.selectedKind==static_cast<std::uint32_t>(canvas::semantic::ObjectKind::kSection);
        ImGui::BeginDisabled(sectionSelected||state.selection.selectedObjectCount>1);
        if (panel_detail::flowButton(state.control.selectedLocked ? "Unlock" : "Lock", objectFirst)) {
            (void)c.controls.selectedAction(canvas::runtime::CanvasControlPayloadKind::kSelectedLock,
                                            !state.control.selectedLocked); c.ui.markControlSubmitted();
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(state.control.selectedLocked);
        if (panel_detail::flowButton(sectionSelected?"Release selected Section":"Delete selected", objectFirst)) {
            (void)c.controls.selectedAction(canvas::runtime::CanvasControlPayloadKind::kDeleteSelected); c.ui.markControlSubmitted();
        }
        ImGui::BeginDisabled(sectionSelected||state.selection.selectedObjectCount>1);
        if (panel_detail::flowButton("Bring front", objectFirst)) {
            (void)c.controls.selectedAction(canvas::runtime::CanvasControlPayloadKind::kSelectedOrder, true); c.ui.markControlSubmitted();
        }
        ImGui::EndDisabled();
        const auto selectedConnector = c.controls.selectedConnector();
        if (selectedConnector.available) {
            if (selectedConnector.content.routing == canvas::semantic::ConnectorRouting::kStraight &&
                panel_detail::flowButton("Convert to orthogonal", objectFirst)) {
                (void)c.controls.setConnectorRouting(canvas::semantic::ConnectorRouting::kOrthogonal, true); c.ui.markControlSubmitted();
            }
            ImGui::Text("Route: %s", canvas::runtime::connectorRouteStatusName(selectedConnector.routeStatus));
            if (c.ui.advancedControls()) ImGui::Text("Search: %u states / %u predicates",
                selectedConnector.work.expanded, selectedConnector.work.predicates);
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
    }
    textFormat(c);
    if (c.ui.advancedControls()) {
        ImGui::SeparatorText("Test creation");
        ImGui::BeginDisabled(!panel_detail::canSubmitProduct(c));
        bool selected = state.selection.enabled;
        if (ImGui::Checkbox("Selection mode", &selected)) {
            (void)c.controls.setSelectionMode(selected); c.ui.markControlSubmitted();
        }
        if (ImGui::Button("Create Sticky")) { (void)c.controls.createSticky(); c.ui.markControlSubmitted(); }
        if (ImGui::Button("Create Connector")) { (void)c.controls.createConnector(); c.ui.markControlSubmitted(); }
        ImGui::Text("legacy toolId (diagnostic): %u", state.tool.toolId);
        ImGui::EndDisabled();
    }
    const auto& message = c.controls.connectorStickyStatus();
    if (!message.empty()) ImGui::TextWrapped("%s", message.c_str());
}
void images(DebugPanelContext& context) {
    auto& controls = context.controls;
    const auto selected = controls.selectedImage();
    const auto crop = controls.imageCrop();
    ImGui::SeparatorText("Image");
    ImGui::BeginDisabled(!controls.hasRuntimeOwner() || crop.active);
    bool imageFirst=true;
    if (panel_detail::flowButton("Import image...", imageFirst)) { controls.requestImageFile(false); context.ui.markControlSubmitted(); }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!selected.editable || crop.active);
    if (selected.available && panel_detail::flowButton("Replace selected image...", imageFirst)) { controls.requestImageFile(true); context.ui.markControlSubmitted(); }
    if (selected.available && panel_detail::flowButton("Crop selected image", imageFirst)) { controls.beginImageCrop(); context.ui.markControlSubmitted(); }
    ImGui::EndDisabled();
    if (selected.available)
        ImGui::Text("Image %.0f x %.0f / source %.0f x %.0f", selected.content.width, selected.content.height,
                    selected.content.intrinsic_width, selected.content.intrinsic_height);

    if (crop.active) {
        auto& draft = context.ui.imageDraft();
        const auto rect = crop.content.source_rect.value_or(canvas::semantic::NormalizedRect{0,0,1,1});
        if (draft.token != crop.cropToken ||
            (!ImGui::IsAnyItemActive() &&
             (draft.rect[0] != rect.x || draft.rect[1] != rect.y || draft.rect[2] != rect.width ||
              draft.rect[3] != rect.height || draft.width != crop.content.width || draft.height != crop.content.height ||
              draft.scale != static_cast<float>(crop.contentScale)))) {
            draft.token = crop.cropToken; draft.rect[0] = rect.x; draft.rect[1] = rect.y;
            draft.rect[2] = rect.width; draft.rect[3] = rect.height;
            draft.width = crop.content.width; draft.height = crop.content.height; draft.scale = static_cast<float>(crop.contentScale);
        }
        bool changed = false;
        ImGui::TextWrapped("Live crop preview. Done creates one Undo step; Cancel discards it.");
        float scale = draft.scale;
        if (ImGui::SliderFloat("Content zoom", &scale, 0.25F, 4.0F, "%.2fx")) {
            canvas::runtime::ImageCropUpdateRequest request{};
            request.cropToken=crop.cropToken; request.contentScale=scale;
            controls.updateImageCrop(request); draft.scale=scale;
            context.ui.markControlSubmitted();
        }
        if (context.ui.advancedControls()) {
        changed = ImGui::InputDouble("Crop X", &draft.rect[0], .01, .1, "%.4f") || changed;
        changed = ImGui::InputDouble("Crop Y", &draft.rect[1], .01, .1, "%.4f") || changed;
        changed = ImGui::InputDouble("Crop width", &draft.rect[2], .01, .1, "%.4f") || changed;
        changed = ImGui::InputDouble("Crop height", &draft.rect[3], .01, .1, "%.4f") || changed;
        changed = ImGui::InputDouble("Display width", &draft.width) || changed;
        changed = ImGui::InputDouble("Display height", &draft.height) || changed;
        if (changed) {
            controls.updateImageCrop({crop.cropToken,
                {draft.rect[0], draft.rect[1], draft.rect[2], draft.rect[3]}, draft.width, draft.height,std::nullopt});
            context.ui.markControlSubmitted();
        }
        }
        bool cropFirst=true;
        if (panel_detail::flowButton("Crop Done", cropFirst)) { controls.finishImageCrop(true); context.ui.markControlSubmitted(); }
        if (panel_detail::flowButton("Crop Cancel", cropFirst) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            controls.finishImageCrop(false); context.ui.markControlSubmitted();
        }
    }
    if (!controls.imageStatus().empty()) ImGui::TextWrapped("%s", controls.imageStatus().c_str());
}
void view(DebugPanelContext& context) {
    const auto& snapshot = context.snapshot;
    ImGui::SeparatorText("View");
    if (context.ui.advancedControls()) {
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
    }
    const bool available = panel_detail::canSubmitProduct(context);
    ImGui::BeginDisabled(!available);
    bool first = true;
    if (panel_detail::flowButton("Fit Content", first)) {
        (void)context.controls.fitToContent();
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!available || snapshot.product.value.selection.selectedObjectCount == 0);
    if (panel_detail::flowButton("Fit Selection", first)) {
        (void)context.controls.fitToSelection();
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    if (context.ui.advancedControls()) {
    ImGui::BeginDisabled(!available || snapshot.product.value.selection.primaryObject == 0);
    if (ImGui::Button("Fit Primary Object")) {
        (void)context.controls.fitPrimaryObject();
        context.ui.markControlSubmitted();
    }
    ImGui::EndDisabled();
    }
    if (!context.controls.hasRuntimeOwner())
        ImGui::TextUnformatted("View controls: Unsupported (Runtime owner unbound)");
}
void sections(DebugPanelContext& c) {
    using A=canvas::runtime::SectionAction;
    ImGui::SeparatorText("Section");
    const auto section=c.controls.sectionSnapshot();
    const auto submit=[&](canvas::runtime::SectionIntent intent) {
        intent.expectedSemanticGeneration=section.semanticGeneration;
        intent.selectionRevision=section.selectionRevision;
        (void)c.controls.sectionIntent(intent);c.ui.markControlSubmitted();
    };
    ImGui::BeginDisabled(!panel_detail::canSubmitProduct(c)||section.busy);
    bool first=true;
    if(panel_detail::flowButton("Create Section",first))submit({});
    ImGui::BeginDisabled(section.selectedRoots.empty());
    if(panel_detail::flowButton("Wrap selection",first)) {
        canvas::runtime::SectionIntent intent;intent.action=A::kWrap;intent.objects=section.selectedRoots;submit(intent);
    }
    static std::optional<canvas::foundation::ObjectId> moveTarget;
    static canvas::runtime::CanvasTargetKey moveTargetBinding{};
    if(moveTargetBinding!=section.target){moveTarget.reset();moveTargetBinding=section.target;}
    if(moveTarget&&panel_detail::flowButton("Move into target Section",first)) {
        canvas::runtime::SectionIntent intent;intent.action=A::kReparent;intent.objects=section.selectedRoots;intent.parentId=moveTarget;submit(intent);
    }
    if(panel_detail::flowButton("Move to root",first)) {
        canvas::runtime::SectionIntent intent;intent.action=A::kReparent;intent.objects=section.selectedRoots;submit(intent);
    }
    ImGui::EndDisabled();
    if(section.available) {
        ImGui::Text("Members: %zu",section.directChildren.size());
        if(ImGui::Button("Use as move target"))moveTarget=section.objectId;
        if(ImGui::Button("Select children")) {
            canvas::runtime::SectionIntent intent;intent.action=A::kSelectChildren;intent.objectId=section.objectId;submit(intent);
        }
        ImGui::SameLine();
        if(ImGui::Button("Release Section")) {
            canvas::runtime::SectionIntent intent;intent.action=A::kRelease;intent.objectId=section.objectId;submit(intent);
        }
        static canvas::foundation::ObjectId editingId;
        static std::array<char,4096> title{};
        static canvas::semantic::Rect2 boundary{};
        static std::uint64_t generation=0;
        static canvas::runtime::CanvasTargetKey editingBinding{};
        if(editingBinding!=section.target||editingId!=section.objectId||generation!=section.semanticGeneration) {
            editingBinding=section.target;
            editingId=section.objectId;generation=section.semanticGeneration;
            title.fill(0);
            if(section.content.title.size()<title.size())std::copy(section.content.title.begin(),section.content.title.end(),title.begin());
            boundary=section.content.boundary;
        }
        ImGui::BeginDisabled(section.content.title.size()>=title.size());
        ImGui::InputText("Section title",title.data(),title.size());
        if(ImGui::Button("Rename Section")) {
            canvas::runtime::SectionIntent intent;intent.action=A::kRename;intent.objectId=section.objectId;intent.content.title=title.data();submit(intent);
        }
        ImGui::EndDisabled();
        ImGui::InputDouble("Boundary X",&boundary.x);ImGui::InputDouble("Boundary Y",&boundary.y);
        ImGui::InputDouble("Boundary width",&boundary.width);ImGui::InputDouble("Boundary height",&boundary.height);
        if(ImGui::Button("Resize boundary")) {
            canvas::runtime::SectionIntent intent;intent.action=A::kResize;intent.objectId=section.objectId;intent.content.boundary=boundary;submit(intent);
        }
        if(!section.projectionReady)ImGui::TextUnformatted("Section projection pending");
    }
    ImGui::EndDisabled();
}
void history(DebugPanelContext& c) {
    ImGui::SeparatorText("History");
    const bool enabled = panel_detail::canSubmitProduct(c);
    bool first=true;
    ImGui::BeginDisabled(!enabled || !c.snapshot.product.value.history.canUndo);
    if (panel_detail::flowButton("Undo (Ctrl+Z)", first)) { (void)c.controls.undo(); c.ui.markControlSubmitted(); }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!enabled || !c.snapshot.product.value.history.canRedo);
    if (panel_detail::flowButton("Redo (Ctrl+Y)", first)) { (void)c.controls.redo(); c.ui.markControlSubmitted(); }
    ImGui::EndDisabled();
}
} // namespace
void registerControlDebugPanels(PanelRegistry& registry) noexcept {
    (void)registry.add({WorkspaceId::kControl, PanelSlot::kTools, 0, nullptr, tools});
    (void)registry.add({WorkspaceId::kControl, PanelSlot::kTools, 10, nullptr, images});
    (void)registry.add({WorkspaceId::kControl, PanelSlot::kTools, 20, nullptr, sections});
    (void)registry.add({WorkspaceId::kControl, PanelSlot::kView, 0, nullptr, view});
    (void)registry.add({WorkspaceId::kControl, PanelSlot::kHistory, 0, nullptr, history});
}
} // namespace canvas::debug_ui
