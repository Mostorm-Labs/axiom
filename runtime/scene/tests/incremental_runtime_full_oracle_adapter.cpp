#include "incremental_runtime_full_oracle_adapter.hpp"

#include "canvas/scene/full_scene_compiler.hpp"

namespace canvas::testing {

FullOracleScene compileFullOracle(const semantic::SemanticReadView& view) noexcept {
    const auto compiled = canvas::scene::FullSceneCompiler::compile(view);
    if (!compiled) {
        return {};
    }
    FullOracleScene result;
    result.generation = compiled.value().generation;
    result.records.reserve(compiled.value().records.size());
    for (const auto& record : compiled.value().records) {
        result.records.push_back(FullOracleRecord{
            record.objectId, record.kind, record.kindVersion, record.placement,
            record.transform, record.properties, record.content, record.eraseMasks,
            record.geometryBounds, record.visualBounds, record.worldBounds,
            record.referenceGeometryDigest, record.directDependencies});
    }
    result.valid = true;
    return result;
}

} // namespace canvas::testing
