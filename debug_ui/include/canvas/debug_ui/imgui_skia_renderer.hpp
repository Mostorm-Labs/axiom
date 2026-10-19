#pragma once

#include <cstdint>
#include <string_view>

struct ImDrawData;
class SkImage;
class SkSurface;

namespace canvas::debug_ui {
struct DebugSnapshot;
class ImGuiSkiaRenderer final {
 public:
  [[nodiscard]] static constexpr std::string_view backendName() noexcept { return "ImGuiSkiaRenderer/reference"; }
  [[nodiscard]] std::uint64_t render(const DebugSnapshot& snapshot) noexcept;
#if defined(CANVAS_DEBUG_UI_HAS_SKIA)
  [[nodiscard]] bool render(ImDrawData*, SkSurface*, SkImage*) noexcept;
#endif
};
}  // namespace canvas::debug_ui
