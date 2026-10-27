#pragma once
#include "canvas/debug_ui/builtin_panels.hpp"
#include "widgets.hpp"

#include <array>
#include <cstdio>
#include <utility>

namespace canvas::debug_ui::panel_detail {
inline void renderSelectionSummary(const runtime::SelectionSummary& summary, bool advanced) {
    ImGui::SeparatorText("Selection summary");
    if (summary.status != runtime::SelectionSummaryStatus::kAvailable) {
        ImGui::TextWrapped("Selection summary: %s", runtime::selectionReasonName(summary.reason));
        return;
    }
    ImGui::Text("Selected roots: %zu", summary.count);
    if (summary.count == 0)
        return;
    const auto kindName = [](std::uint32_t kind) {
        switch (kind) {
        case 1:
            return "Shape";
        case 2:
            return "Image";
        case 3:
            return "VectorPath";
        case 4:
            return "RichText";
        case 5:
            return "VectorStroke";
        case 6:
            return "DabStroke";
        case 7:
            return "Connector";
        case 8:
            return "Sticky";
        case 9:
            return "Group";
        case 10:
            return "Section";
        default:
            return "Unknown kind";
        }
    };
    bool first = true;
    for (const auto& kind : summary.kindCounts) {
        if (!first)
            ImGui::SameLine();
        ImGui::Text("%s: %zu", kindName(kind.kind), kind.count);
        first = false;
    }
    char primary[33]{};
    for (std::size_t i = 0; i < summary.primary.bytes.size(); ++i)
        (void)std::snprintf(
            primary + i * 2, 3, "%02x", static_cast<unsigned>(summary.primary.bytes[i]));
    ImGui::Text("Primary: %s", primary);
    if (summary.bounds.status == runtime::SelectionSummaryStatus::kAvailable &&
        summary.bounds.world) {
        const auto& b = *summary.bounds.world;
        ImGui::Text("Bounds: [%.2f, %.2f] - [%.2f, %.2f]", b.left, b.top, b.right, b.bottom);
    } else
        ImGui::Text("Bounds: %s", runtime::selectionReasonName(summary.bounds.reason));
    for (const auto& property : summary.properties) {
        const char* name = property.fieldId == 1   ? "Visibility"
                           : property.fieldId == 2 ? "Lock"
                                                   : "Opacity";
        using P = runtime::SelectionPropertyState;
        if (property.state == P::kUniform && property.value) {
            if (const auto* value = std::get_if<bool>(&*property.value))
                ImGui::Text("%s: %s", name, *value ? "true" : "false");
            else if (const auto* scalar = std::get_if<float>(&*property.value))
                ImGui::Text("%s: %.3f", name, *scalar);
        } else if (property.state == P::kMixed)
            ImGui::Text("%s: mixed", name);
        else if (property.state == P::kPartial)
            ImGui::Text("%s: partial (%zu/%zu roots%s)",
                        name,
                        property.applicableRoots,
                        summary.count,
                        property.applicableValuesMixed ? ", mixed" : "");
        else
            ImGui::Text(
                "%s: unavailable (%s)", name, runtime::selectionReasonName(property.reason));
    }
    if (advanced) {
        ImGui::Text("Selection revision %llu / semantic %llu / Scene %llu@%llu",
                    static_cast<unsigned long long>(summary.selectionRevision),
                    static_cast<unsigned long long>(summary.semanticGeneration),
                    static_cast<unsigned long long>(summary.sceneGeneration),
                    static_cast<unsigned long long>(summary.sceneRevision));
        for (const auto& item : std::array<std::pair<const char*, runtime::SelectionAction>, 6>{
                 {{"Group", runtime::SelectionAction::kGroup},
                  {"Ungroup", runtime::SelectionAction::kUngroup},
                  {"Wrap in Section shortcut", runtime::SelectionAction::kWrapInSection},
                  {"Bulk properties", runtime::SelectionAction::kBulkProperties},
                  {"Snap", runtime::SelectionAction::kSnap},
                  {"Alignment", runtime::SelectionAction::kAlignment}}})
            ImGui::TextDisabled(
                "%s: %s",
                item.first,
                runtime::selectionReasonName(summary.capability(item.second).reason));
    }
}
inline bool selectionActionButton(DebugPanelContext& c,
                                  const char* label,
                                  bool& first,
                                  runtime::SelectionAction action) {
    const auto cap = c.snapshot.selectionSummary.capability(action);
    ImGui::BeginDisabled(!canSubmitProduct(c) || !cap.available());
    const bool clicked = flowButton(label, first);
    ImGui::EndDisabled();
    if (!cap.available() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", runtime::selectionReasonName(cap.reason));
    return clicked;
}
} // namespace canvas::debug_ui::panel_detail
