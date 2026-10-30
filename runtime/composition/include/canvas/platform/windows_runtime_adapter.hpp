#pragma once

#include "canvas/runtime/diagnostics.hpp"
#include <cstdint>
#include <memory>

namespace canvas::runtime { class RuntimeComposition; }
namespace canvas::platform {

struct WindowsFrameStatus final {
  bool needsPaint = false;
  bool canonicalRequested = false;
  bool previewRendered = false;
  std::uint64_t canonicalWorkId = 0;
  std::uint64_t previewWorkId = 0;
  std::uint64_t pendingCanonicalHandoff = 0;
  bool available = false;
};

// Borrowed owner and HWND must outlive the adapter. All methods run on the
// Composition/window owner thread; shutdown before destroying either owner.
class WindowsRuntimeAdapter final : public runtime::PlatformDiagnostics {
 public:
  WindowsRuntimeAdapter(runtime::RuntimeComposition&, void* hwnd);
  ~WindowsRuntimeAdapter() override;
  WindowsRuntimeAdapter(const WindowsRuntimeAdapter&) = delete;
  WindowsRuntimeAdapter& operator=(const WindowsRuntimeAdapter&) = delete;
  [[nodiscard]] bool bind(std::uint32_t width, std::uint32_t height, float scale) noexcept;
  [[nodiscard]] bool resize(std::uint32_t width, std::uint32_t height, float scale) noexcept;
  [[nodiscard]] bool loseSurface() noexcept;
  [[nodiscard]] bool recoverSurface(std::uint32_t width, std::uint32_t height, float scale) noexcept;
  // Startup only, before binding other document resources: the existing font
  // catalog loader replaces the Runtime resource manifest. Fonts are not
  // installed into the operating system. A successful load is one-shot.
  [[nodiscard]] bool loadHostFonts(const wchar_t* cjkPath, const wchar_t* catalogRoot) noexcept;
  [[nodiscard]] bool prepareFrame(bool forceCanonical = false) noexcept;
  [[nodiscard]] WindowsFrameStatus frameStatus() const noexcept;
  [[nodiscard]] runtime::PlatformDiagnosticsSnapshot readPlatformDiagnostics() const noexcept override;
  // Call after the window's real platform paint/presentation opportunity.
  [[nodiscard]] bool present() noexcept;
  void reposition() noexcept;
  [[nodiscard]] bool shutdown() noexcept;
 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace canvas::platform
