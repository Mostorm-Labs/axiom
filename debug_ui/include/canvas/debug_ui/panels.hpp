#pragma once
#include "canvas/debug_ui/snapshot.hpp"
#include <array>
#include <cstddef>
#include <string_view>
namespace canvas::debug_ui {
enum class PanelCapability : std::uint8_t { kOverview=0,kInput,kCanvas,kArcPreview,kSurface,kBrush,kTelemetry,kInspection,kCount };
struct PanelDescriptor final { PanelCapability capability; std::string_view title; Capability required; };
inline constexpr std::array<PanelDescriptor, static_cast<std::size_t>(PanelCapability::kCount)> kPanelDescriptors{{
 {PanelCapability::kOverview,"Overview",Capability::kTelemetry},{PanelCapability::kInput,"Input",Capability::kInput},{PanelCapability::kCanvas,"Canvas",Capability::kCanonicalSurface},{PanelCapability::kArcPreview,"Arc Preview",Capability::kArcPreviewSurface},{PanelCapability::kSurface,"Surface",Capability::kSurfaceMode},{PanelCapability::kBrush,"Brush",Capability::kCanonicalSurface},{PanelCapability::kTelemetry,"Telemetry",Capability::kTelemetry},{PanelCapability::kInspection,"Inspection",Capability::kInspection}}};
inline bool panelAvailable(const DebugSnapshot& snapshot, PanelCapability panel) noexcept { const auto& descriptor=kPanelDescriptors[static_cast<std::size_t>(panel)]; return snapshot.capability(descriptor.required)==CapabilityState::kAvailable; }
} // namespace canvas::debug_ui
