#pragma once

#include "canvas/debug_ui/control_router.hpp"
#include "canvas/debug_ui/snapshot.hpp"
#include "canvas/debug_ui/workspace.hpp"

#include <cstdint>

namespace canvas::debug_ui {

class DebugUiSessionState;

enum class PanelSlot : std::uint8_t {
  kRuntimeIdentity = 0, kCurrentProductState, kHealth, kLivePerformance, kRecentActivity,
  kTools, kInk, kView, kHistory, kFeature,
  kSelection, kObject, kInteraction, kRelationship,
  kDocument, kScene, kArc, kRender, kSurface, kResources, kFeatureDiagnostics, kAdvancedActions,
  kLiveMetrics, kInputArc, kRendering, kMemoryCache, kTrace, kGPUTiming, kFeatureMetrics,
  kSmoke, kFeatureScenario, kStress, kEvidence,
};

struct DebugPanelContext final {
  const DebugSnapshot& snapshot;
  DebugControlRouter& controls;
  DebugUiSessionState& ui;
};

using PanelVisible = bool (*)(const DebugSnapshot&) noexcept;
using PanelRender = void (*)(DebugPanelContext&);

struct PanelContribution final {
  WorkspaceId workspace = WorkspaceId::kDashboard;
  PanelSlot slot = PanelSlot::kRuntimeIdentity;
  int order = 0;
  PanelVisible visible = nullptr;
  PanelRender render = nullptr;
};

[[nodiscard]] constexpr bool panelSlotBelongsTo(WorkspaceId workspace, PanelSlot slot) noexcept {
  const auto value = static_cast<std::uint8_t>(slot);
  switch (workspace) {
    case WorkspaceId::kDashboard: return value <= static_cast<std::uint8_t>(PanelSlot::kRecentActivity);
    case WorkspaceId::kControl: return value >= static_cast<std::uint8_t>(PanelSlot::kTools) && value <= static_cast<std::uint8_t>(PanelSlot::kFeature);
    case WorkspaceId::kInspect: return value >= static_cast<std::uint8_t>(PanelSlot::kSelection) && value <= static_cast<std::uint8_t>(PanelSlot::kRelationship);
    case WorkspaceId::kRuntime: return value >= static_cast<std::uint8_t>(PanelSlot::kDocument) && value <= static_cast<std::uint8_t>(PanelSlot::kAdvancedActions);
    case WorkspaceId::kPerformance: return value >= static_cast<std::uint8_t>(PanelSlot::kLiveMetrics) && value <= static_cast<std::uint8_t>(PanelSlot::kFeatureMetrics);
    case WorkspaceId::kScenarios: return value >= static_cast<std::uint8_t>(PanelSlot::kSmoke) && value <= static_cast<std::uint8_t>(PanelSlot::kEvidence);
    case WorkspaceId::kCount: return false;
  }
  return false;
}

}  // namespace canvas::debug_ui
