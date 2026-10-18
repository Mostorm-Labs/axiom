#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace canvas::minimal_runtime_host {

// Keeps the Win32 presentation boundary from repeating expensive whole-surface
// readbacks and preview/canonical compositing when the underlying runtime
// content has not changed. The cache is UI-thread owned and contains no
// runtime or platform state.
class WindowsPresentationCache final {
 public:
  [[nodiscard]] bool shouldReadCanonical(std::uint64_t surfaceGeneration,
                                         std::uint64_t frameSerial,
                                         std::size_t byteCount) const noexcept {
    return !canonicalValid_ || canonicalSurfaceGeneration_ != surfaceGeneration ||
        canonicalFrameSerial_ != frameSerial || canonicalPixels_.size() != byteCount;
  }

  void markCanonicalRead(std::uint64_t surfaceGeneration,
                         std::uint64_t frameSerial,
                         std::vector<std::uint8_t> pixels) {
    canonicalSurfaceGeneration_ = surfaceGeneration;
    canonicalFrameSerial_ = frameSerial;
    canonicalPixels_ = std::move(pixels);
    canonicalValid_ = true;
  }

  [[nodiscard]] const std::vector<std::uint8_t>& canonicalPixels() const noexcept {
    return canonicalPixels_;
  }

  [[nodiscard]] bool shouldReadPreview(std::uint64_t surfaceGeneration,
                                       std::uint64_t contentRevision,
                                       std::size_t byteCount) const noexcept {
    return !previewValid_ || previewSurfaceGeneration_ != surfaceGeneration ||
        previewContentRevision_ != contentRevision || previewPixels_.size() != byteCount;
  }

  void markPreviewRead(std::uint64_t surfaceGeneration,
                       std::uint64_t contentRevision,
                       std::vector<std::uint8_t> pixels) {
    previewSurfaceGeneration_ = surfaceGeneration;
    previewContentRevision_ = contentRevision;
    previewPixels_ = std::move(pixels);
    previewValid_ = true;
  }

  [[nodiscard]] const std::vector<std::uint8_t>& previewPixels() const noexcept {
    return previewPixels_;
  }

  void clearPreview() noexcept {
    previewValid_ = false;
    previewPixels_.clear();
  }

  [[nodiscard]] bool shouldRecompose(std::uint64_t surfaceGeneration,
                                     std::uint64_t frameSerial,
                                     std::uint64_t previewSurfaceGeneration,
                                     std::uint64_t previewContentRevision,
                                     bool previewActive) const noexcept {
    return !compositeValid_ || compositeSurfaceGeneration_ != surfaceGeneration ||
        compositeFrameSerial_ != frameSerial ||
        compositePreviewSurfaceGeneration_ != previewSurfaceGeneration ||
        compositePreviewContentRevision_ != previewContentRevision ||
        compositePreviewActive_ != previewActive;
  }

  void markComposite(std::uint64_t surfaceGeneration,
                     std::uint64_t frameSerial,
                     std::uint64_t previewSurfaceGeneration,
                     std::uint64_t previewContentRevision,
                     bool previewActive,
                     std::vector<std::uint8_t> pixels) {
    compositeSurfaceGeneration_ = surfaceGeneration;
    compositeFrameSerial_ = frameSerial;
    compositePreviewSurfaceGeneration_ = previewSurfaceGeneration;
    compositePreviewContentRevision_ = previewContentRevision;
    compositePreviewActive_ = previewActive;
    compositePixels_ = std::move(pixels);
    compositeValid_ = true;
  }

  [[nodiscard]] const std::vector<std::uint8_t>& compositePixels() const noexcept {
    return compositePixels_;
  }

  void reset() noexcept {
    canonicalValid_ = false;
    previewValid_ = false;
    compositeValid_ = false;
    canonicalPixels_.clear();
    previewPixels_.clear();
    compositePixels_.clear();
  }

 private:
  bool canonicalValid_ = false;
  bool previewValid_ = false;
  bool compositeValid_ = false;
  std::uint64_t canonicalSurfaceGeneration_ = 0;
  std::uint64_t canonicalFrameSerial_ = 0;
  std::uint64_t previewSurfaceGeneration_ = 0;
  std::uint64_t previewContentRevision_ = 0;
  std::uint64_t compositeSurfaceGeneration_ = 0;
  std::uint64_t compositeFrameSerial_ = 0;
  std::uint64_t compositePreviewSurfaceGeneration_ = 0;
  std::uint64_t compositePreviewContentRevision_ = 0;
  bool compositePreviewActive_ = false;
  std::vector<std::uint8_t> canonicalPixels_;
  std::vector<std::uint8_t> previewPixels_;
  std::vector<std::uint8_t> compositePixels_;
};

}  // namespace canvas::minimal_runtime_host
