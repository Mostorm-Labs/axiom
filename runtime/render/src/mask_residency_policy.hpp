#pragma once
#include "canvas/render/render_cache.hpp"
#include <algorithm>
namespace canvas::render::internal {
struct MaskResidencyPolicy final {
 std::size_t drawing=0,exact=0;
 static constexpr std::size_t singlePlan=8U*1024U*1024U;
};
inline MaskResidencyPolicy maskResidencyPolicy(const RenderCacheOptions& options)noexcept{
#if defined(AXIOM_MPS_AGGREGATE_8)
 constexpr std::size_t maximum=8U*1024U*1024U;
#else
 constexpr std::size_t maximum=32U*1024U*1024U;
#endif
 const auto exact=std::min(options.maxEntryBytes,options.maxBytes/4);
 return {std::min(maximum,exact),exact};
}
}
