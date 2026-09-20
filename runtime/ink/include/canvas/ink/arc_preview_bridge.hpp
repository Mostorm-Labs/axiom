#pragma once

#include "canvas/ink/preview_model.hpp"

#include <cstdint>
#include <unordered_map>

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
  PreviewDisposition presentKeyed(const PreviewSnapshot& snapshot) noexcept;
  [[nodiscard]] bool canonicalOnly() const noexcept { return canonicalOnly_; }
  [[nodiscard]] bool canonicalOnly(std::uint64_t strokeId) const noexcept;

 private:
  PreviewPresentationPort& port_;
  bool canonicalOnly_ = false;
  std::unordered_map<std::uint64_t, bool> keyedCanonicalOnly_;
};

}  // namespace canvas::ink
