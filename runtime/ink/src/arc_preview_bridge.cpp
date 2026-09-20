#include "canvas/ink/arc_preview_bridge.hpp"

namespace canvas::ink {

PreviewDisposition ArcPreviewBridge::present(const PreviewSnapshot& snapshot) noexcept {
  if (snapshot.strokeId == 0 || snapshot.revision == 0) return PreviewDisposition::kRejected;
  if (canonicalOnly_) return PreviewDisposition::kCanonicalOnly;
  if (!port_.present(snapshot)) {
    canonicalOnly_ = true;
    return PreviewDisposition::kCanonicalOnly;
  }
  return PreviewDisposition::kPresented;
}

PreviewDisposition ArcPreviewBridge::presentKeyed(const PreviewSnapshot& snapshot) noexcept {
  if (snapshot.strokeId == 0 || snapshot.revision == 0) return PreviewDisposition::kRejected;
  if (keyedCanonicalOnly_[snapshot.strokeId]) return PreviewDisposition::kCanonicalOnly;
  if (!port_.present(snapshot)) {
    keyedCanonicalOnly_[snapshot.strokeId] = true;
    return PreviewDisposition::kCanonicalOnly;
  }
  return PreviewDisposition::kPresented;
}

bool ArcPreviewBridge::canonicalOnly(std::uint64_t strokeId) const noexcept {
  const auto it = keyedCanonicalOnly_.find(strokeId);
  return it != keyedCanonicalOnly_.end() && it->second;
}

}  // namespace canvas::ink
