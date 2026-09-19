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

}  // namespace canvas::ink
