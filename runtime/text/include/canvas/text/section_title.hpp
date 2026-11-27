#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace canvas::text {

inline constexpr std::size_t kSectionTitleMaxCharacters = 50;
inline constexpr float kSectionTitleInsetX = 6;
inline constexpr float kSectionTitleInsetY = 2;
inline constexpr float kSectionTitleRadius = 4;

inline std::size_t sectionTitleLineBreak(std::string_view s) noexcept {
    if (s.starts_with("\r\n")) return 2;
    if (s.starts_with('\r') || s.starts_with('\n') || s.starts_with('\v') || s.starts_with('\f')) return 1;
    if (s.starts_with("\xC2\x85")) return 2;
    if (s.starts_with("\xE2\x80\xA8") || s.starts_with("\xE2\x80\xA9")) return 3;
    return 0;
}

// Authoring policy. Existing Section/1 operations and snapshots retain their original UTF-8 metadata.
inline bool validSectionTitleEdit(std::string_view title) noexcept {
    if (title.size() > 4 * kSectionTitleMaxCharacters) return false;
    std::size_t characters = 0;
    for (std::size_t i = 0; i < title.size(); ++i) {
        if (sectionTitleLineBreak(title.substr(i))) return false;
        if ((static_cast<unsigned char>(title[i]) & 0xc0U) != 0x80U) ++characters;
    }
    return characters <= kSectionTitleMaxCharacters;
}

inline std::string sectionTitleDisplayLine(std::string_view title) {
    std::string line;
    line.reserve(title.size());
    for (std::size_t i = 0; i < title.size();) {
        if (const auto length = sectionTitleLineBreak(title.substr(i))) {
            line.push_back(' ');
            i += length;
        } else {
            line.push_back(title[i++]);
        }
    }
    return line;
}

} // namespace canvas::text
