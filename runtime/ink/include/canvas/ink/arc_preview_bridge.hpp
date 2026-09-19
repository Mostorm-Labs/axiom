#pragma once

#include "canvas/ink/preview_model.hpp"

#include <cstdint>

namespace canvas::ink {

class PreviewPresentationPort {
 public:
  virtual ~PreviewPresentationPort() = default;
  virtual bool present(const PreviewSnapshot& snapshot) noexcept = 0;
};

enum class PreviewDisposition : std::uint8_t { kPresented, kCanonicalOnly, kRejected };

class ArcPreviewBridge final {
 public:
  explicit ArcPreviewBridge(PreviewPresentationPort& port) noexcept : port_(port) {}
  PreviewDisposition present(const PreviewSnapshot& snapshot) noexcept;
  [[nodiscard]] bool canonicalOnly() const noexcept { return canonicalOnly_; }

 private:
  PreviewPresentationPort& port_;
  bool canonicalOnly_ = false;
};

}  // namespace canvas::ink
