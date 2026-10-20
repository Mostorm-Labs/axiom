#pragma once
#include "canvas/semantic/document_id.hpp"
#include "canvas/semantic/object_record.hpp"

#include <cmath>
#include <limits>
#include <vector>
namespace canvas::semantic {
// Returns empty on invalid state or unavailable codec; never hashes native memory.
[[nodiscard]] std::vector<std::uint8_t> richTextStateDigest(DocumentId, const ObjectRecord&);
[[nodiscard]] inline bool validRichTextLayoutState(std::uint32_t version,
                                                   const RichTextContent& content) noexcept {
    for (const auto& p : content.document.paragraphs) {
        if (version < 3 && p.style.bulleted) return false;
        for (const auto& r : p.runs) if (version < 3 && r.style.strikethrough) return false;
    }
    if (version == 1)
        return !content.layout;
    if ((version != 2 && version != 3) || !content.layout)
        return false;
    const auto width = content.layout->wrap_width;
    return std::isfinite(width) &&
           width >= static_cast<double>(std::numeric_limits<float>::min()) &&
           width <= static_cast<double>(std::numeric_limits<float>::max());
}
} // namespace canvas::semantic
