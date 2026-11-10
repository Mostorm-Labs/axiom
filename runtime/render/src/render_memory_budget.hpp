#pragma once
#include "canvas/render/render_cache.hpp"

namespace canvas::render::internal {
// Build policy stays private. Explicit RenderCacheOptions retain their own
// conservative defaults and may request smaller budgets in tests/tools.
inline RenderCacheOptions configuredRenderCacheOptions() noexcept {
    RenderCacheOptions options;
    constexpr std::size_t mib = 1024U * 1024U;
    options.maxBytes = std::size_t(AXIOM_RENDER_CACHE_MIB) * mib;
    options.sharedMaxBytes = std::size_t(AXIOM_RENDER_SHARED_MIB) * mib;
    options.maxEntryBytes = std::size_t(AXIOM_RENDER_ENTRY_MIB) * mib;
    return options;
}
}
