#include "incremental_runtime_full_oracle_adapter.hpp"

#include "canvas/scene/full_scene_compiler.hpp"

namespace canvas::testing {

FullOracleDigest compileFullOracle(const semantic::SemanticReadView& view) noexcept {
    const auto compiled = canvas::scene::FullSceneCompiler::compile(view);
    if (!compiled) {
        return {};
    }
    std::uint64_t digest = 1469598103934665603ULL;
    for (const auto& record : compiled.value().records) {
        for (const auto byte : record.objectId.bytes) {
            digest ^= byte;
            digest *= 1099511628211ULL;
        }
        digest ^= static_cast<std::uint64_t>(record.kind);
        digest *= 1099511628211ULL;
        digest ^= static_cast<std::uint64_t>(record.worldBounds.left != 0.0F);
        digest *= 1099511628211ULL;
    }
    return FullOracleDigest{compiled.value().generation.value(),
                            compiled.value().records.size(), digest, true};
}

} // namespace canvas::testing
