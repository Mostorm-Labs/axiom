#pragma once

#include "canvas/debug_ui/snapshot.hpp"
#include "canvas/debug_ui/control_router.hpp"
#include "canvas/debug_ui/input_capture.hpp"
#include "canvas/debug_ui/panels.hpp"
#include "canvas/runtime/runtime_facade.hpp"
#include "canvas/runtime/diagnostics.hpp"
#include "canvas/runtime/debug_control.hpp"
#include "canvas/runtime/telemetry.hpp"
#include "canvas/runtime/surface_debug_control.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

struct ImDrawData;
class SkImage;
class SkSurface;

namespace canvas::debug_ui {

enum class DebugPanel : std::uint8_t {
    kOverview = 0, kInput, kCanvas, kArcPreview, kSurface, kBrush, kTelemetry, kInspection,
    kCount,
};

struct DebugUiContext final {
    MutexCopySnapshotChannel* snapshots = nullptr;
    InputCaptureGate* input = nullptr;
    const canvas::runtime::IAxiomDiagnostics* diagnostics = nullptr;
    const canvas::runtime::IArcDiagnostics* arcDiagnostics = nullptr;
    const canvas::runtime::IPlatformDiagnostics* platformDiagnostics = nullptr;
    const canvas::runtime::ITelemetry* telemetry = nullptr;
    DebugControlRouter* router = nullptr;
};

struct PanelState final {
    DebugPanel panel = DebugPanel::kOverview;
    bool available = true;
    std::string_view label{};
};

class DebugController final {
  public:
    explicit DebugController(DebugUiContext context) : context_(context) {}
    [[nodiscard]] DebugSnapshot snapshot() const { return context_.snapshots->read(); }
    [[nodiscard]] std::array<PanelState, static_cast<std::size_t>(DebugPanel::kCount)> panels() const;

  private:
    DebugUiContext context_;
};

// Common panel construction. Product and engineering controls are submitted
// through the router; the panel never owns runtime truth or owner pointers.
[[nodiscard]] bool buildImGuiPanels(const DebugSnapshot& snapshot, int selectedTool,
                                    DebugControlRouter* router);

class ImGuiSkiaRenderer final {
  public:
    [[nodiscard]] static constexpr std::string_view backendName() noexcept {
        return "ImGuiSkiaRenderer/reference";
    }
    [[nodiscard]] std::uint64_t render(const DebugSnapshot& snapshot) noexcept {
        return snapshot.stamp.sequence;
    }
#if defined(CANVAS_DEBUG_UI_HAS_SKIA)
    [[nodiscard]] bool render(ImDrawData* drawData, SkSurface* surface,
                               SkImage* fontTexture) noexcept;
#endif
};

}  // namespace canvas::debug_ui
