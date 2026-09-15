#include "incremental_runtime_full_materialization_bridge.hpp"

#include "canvas/scene/full_scene_compiler.hpp"

#include <new>

namespace canvas::internal {

foundation::Result<FullMaterializedScene> materializeFullScene(
    const semantic::SemanticReadView& view) {
    try {
        const auto compiled = scene::FullSceneCompiler::compile(view);
        if (!compiled) return foundation::Result<FullMaterializedScene>::failure(compiled.error());
        FullMaterializedScene result;
        result.generation = compiled.value().generation;
        result.records.reserve(compiled.value().records.size());
        for (const auto& source : compiled.value().records) {
            result.records.push_back(FullMaterializedScene::Record{
                .objectId = source.objectId,
                .kind = source.kind,
                .kindVersion = source.kindVersion,
                .placement = source.placement,
                .transform = source.transform,
                .properties = source.properties,
                .content = source.content,
                .eraseMasks = source.eraseMasks,
                .geometryBounds = source.geometryBounds,
                .visualBounds = source.visualBounds,
                .worldBounds = source.worldBounds,
                .referenceGeometryDigest = source.referenceGeometryDigest,
                .directDependencies = source.directDependencies,
            });
        }
        return foundation::Result<FullMaterializedScene>::success(std::move(result));
    } catch (const std::bad_alloc&) {
        return foundation::Result<FullMaterializedScene>::failure(
            {foundation::ErrorCode::kOutOfMemory, "Unable to materialize FullSceneCompiler recovery"});
    }
}

} // namespace canvas::internal
