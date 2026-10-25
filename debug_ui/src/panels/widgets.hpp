#pragma once

#include "imgui.h"
#include <algorithm>

namespace canvas::debug_ui::panel_detail {
// Ordinary ImGui items with larger hit targets; wrapping works in the 410px Web panel.
inline bool flowButton(const char* label, bool& first, bool selected = false) {
    const float available = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    const float width = (std::min)((std::max)(76.0F,
        ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0F), available);
    if (!first) {
        const float next = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x + ImGui::GetStyle().ItemSpacing.x;
        if (next + width <= ImGui::GetWindowContentRegionMax().x) ImGui::SameLine();
    }
    first = false;
    if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    const bool pressed = ImGui::Button(label, ImVec2(width, 32.0F));
    if (selected) ImGui::PopStyleColor();
    return pressed;
}
}
