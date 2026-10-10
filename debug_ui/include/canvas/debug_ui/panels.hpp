#pragma once
#include "canvas/debug_ui/snapshot.hpp"
#include <array>
#include <cstddef>
#include <string_view>
namespace canvas::debug_ui {
enum class PanelCapability : std::uint8_t { kOverview=0,kInput,kCanvas,kArcPreview,kSurface,kBrush,kTelemetry,kInspection,kCount };
enum class PanelDomain : std::uint8_t { kTelemetry, kArc, kPlatform, kProduct, kUnsupported };
struct PanelDescriptor final { PanelCapability capability; std::string_view title; PanelDomain domain; };
inline constexpr std::array<PanelDescriptor, static_cast<std::size_t>(PanelCapability::kCount)> kPanelDescriptors{{
 {PanelCapability::kOverview,"Overview",PanelDomain::kTelemetry},{PanelCapability::kInput,"Input",PanelDomain::kArc},{PanelCapability::kCanvas,"Canvas",PanelDomain::kPlatform},{PanelCapability::kArcPreview,"Arc Preview",PanelDomain::kArc},{PanelCapability::kSurface,"Surface",PanelDomain::kPlatform},{PanelCapability::kBrush,"Brush",PanelDomain::kProduct},{PanelCapability::kTelemetry,"Telemetry",PanelDomain::kTelemetry},{PanelCapability::kInspection,"Inspection",PanelDomain::kUnsupported}}};
inline bool panelAvailable(const DebugSnapshot& snapshot, PanelCapability panel) noexcept {
  const auto& descriptor = kPanelDescriptors[static_cast<std::size_t>(panel)];
  switch (descriptor.domain) {
  case PanelDomain::kProduct: return snapshot.product.availability == DebugAvailability::kAvailable;
  case PanelDomain::kArc: return snapshot.arc.availability == DebugAvailability::kAvailable;
  case PanelDomain::kPlatform: return snapshot.platform.availability == DebugAvailability::kAvailable;
  case PanelDomain::kTelemetry: return snapshot.telemetry.availability == DebugAvailability::kAvailable;
  case PanelDomain::kUnsupported: return false;
  }
  return false;
}
} // namespace canvas::debug_ui
