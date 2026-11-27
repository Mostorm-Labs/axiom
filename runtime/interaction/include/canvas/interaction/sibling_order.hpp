#pragma once

#include "canvas/semantic/order_key.hpp"

namespace canvas::interaction {
// Advance the ordering cursor past the last sibling. An empty cursor starts a new list.
inline bool nextSiblingOrderKey(std::vector<std::uint8_t>& key) {
    if (key.empty()) {
        key.push_back(128);
        return true;
    }
    for (std::size_t n=key.size(); n>0; --n) {
        if (key[n-1]==255) continue;
        ++key[n-1];
        key.resize(n);
        return true;
    }
    if (key.size()==semantic::OrderKey::kMaxSize) return false;
    key.push_back(128);
    return true;
}
} // namespace canvas::interaction
