#include "canvas/ink/brush_preview_handoff.hpp"

namespace canvas::ink {

bool BrushPreviewHandoff::publish(std::uint64_t session, BrushPreviewDelta delta) noexcept {
    if (session == 0U || delta.revision == 0U || capacity_ == 0U) return false;
    const auto it = previews_.find(session);
    if (it == previews_.end() && previews_.size() >= capacity_) {
        // Evict the oldest revision; terminal state is never stored here.
        auto oldest = previews_.begin();
        for (auto cursor = previews_.begin(); cursor != previews_.end(); ++cursor)
            if (cursor->second.revision < oldest->second.revision) oldest = cursor;
        previews_.erase(oldest);
    }
    if (it == previews_.end() || it->second.revision < delta.revision) previews_[session] = std::move(delta);
    return true;
}

bool BrushPreviewHandoff::seal(std::uint64_t session, BrushCommitIntent intent) noexcept {
    if (session == 0U || intent.session != session || intent.revision == 0U) return false;
    terminals_[session] = Terminal{std::move(intent), false};
    return true;
}

bool BrushPreviewHandoff::cancel(std::uint64_t session) noexcept {
    if (session == 0U) return false;
    terminals_[session] = Terminal{{session, 0U, 0U, {}, {}}, true};
    return true;
}

bool BrushPreviewHandoff::acknowledge(CanonicalVisibleReceipt receipt) noexcept {
    const auto terminal = terminals_.find(receipt.session);
    if (terminal == terminals_.end() || terminal->second.cancelled ||
        terminal->second.intent.revision != receipt.revision) return false;
    terminals_.erase(terminal);
    previews_.erase(receipt.session);
    return true;
}

} // namespace canvas::ink
