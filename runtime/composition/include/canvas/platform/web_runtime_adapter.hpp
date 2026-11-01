#pragma once

#include <cstdint>
#include <memory>

namespace canvas::runtime { class RuntimeComposition; }
namespace canvas::platform {

enum class WebFrameResult : std::uint8_t { kRejected = 0, kIdle = 1, kPending = 2, kRetry = 3 };
struct WebFrameStatus final {
  bool pending = false;
  std::uint64_t canonicalWorkId = 0;
  std::uint64_t previewWorkId = 0;
};

// The application owns the two WebGL2 contexts and their canvas elements.
// The adapter borrows them and the Composition; all calls are owner-thread only.
// Callback state and both contexts must outlive the Composition's providers.
class WebRuntimeAdapter final {
 public:
  using PreviewVisibilityCallback = void (*)(void*, bool);
  explicit WebRuntimeAdapter(runtime::RuntimeComposition& owner,
                             PreviewVisibilityCallback visibility = nullptr,
                             void* visibilityContext = nullptr);
  ~WebRuntimeAdapter();
  WebRuntimeAdapter(const WebRuntimeAdapter&) = delete;
  WebRuntimeAdapter& operator=(const WebRuntimeAdapter&) = delete;
  // Contexts must be distinct and have framebuffer sizes matching width/height.
  [[nodiscard]] bool bind(std::int32_t canonicalContext, std::int32_t previewContext,
                          float logicalWidth, float logicalHeight,
                          std::uint32_t width, std::uint32_t height, float dpr) noexcept;
  // Startup-only application catalog; like the Windows adapter, this replaces
  // the resource manifest and must precede other document resource binding.
  [[nodiscard]] bool loadHostFonts(const char* cjkPath, const char* catalogRoot) noexcept;
  // The application first resizes both canvases, then calls bind again.
  [[nodiscard]] bool loseSurface() noexcept;
  [[nodiscard]] WebFrameResult prepareFrame(bool forceCanonical = false) noexcept;
  // Poll only after yielding to the browser; pending means the GPU fence has not completed.
  [[nodiscard]] WebFrameResult pollFrame() noexcept;
  [[nodiscard]] WebFrameStatus frameStatus() const noexcept;
  [[nodiscard]] bool previewActive() const noexcept;
  [[nodiscard]] std::uint64_t presentedCount() const noexcept;
  [[nodiscard]] bool shutdown() noexcept;
 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace canvas::platform
