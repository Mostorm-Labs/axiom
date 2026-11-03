#include "incremental_runtime_full_materialization_bridge.hpp"

#include "canvas/scene/full_scene_compiler.hpp"

#include <new>

namespace canvas::internal {

namespace {
// A synchronous, borrowed projection input, not another document owner. Only
// the selected records are exposed to the existing full evaluator.
class ProjectionInput final : public semantic::ObjectStore {
    std::span<const semantic::ObjectRecord> records_;
public:
    explicit ProjectionInput(std::span<const semantic::ObjectRecord> records) : records_(records) {}
    std::size_t size() const noexcept override { return records_.size(); }
    const semantic::ObjectRecord* find(const semantic::ObjectId& id) const noexcept override {
        for (const auto& r : records_) if (r.id == id) return &r;
        return nullptr;
    }
    bool contains(const semantic::ObjectId& id) const noexcept override { return find(id) != nullptr; }
    std::vector<semantic::ObjectRecord> allObjects() const override { return {records_.begin(), records_.end()}; }
    std::vector<semantic::ObjectRecord> children(const std::optional<semantic::ObjectId>& parent) const override {
        std::vector<semantic::ObjectRecord> result;
        for (const auto& r : records_) if (r.placement.parent_id == parent) result.push_back(r);
        return result;
    }
};
}
foundation::Result<FullMaterializedScene> materializeSceneRecords(
    std::span<const semantic::ObjectRecord> records, semantic::SemanticGeneration generation,
    text::RichTextLayoutService* textService, bool deferRoutes) {
    const ProjectionInput input(records);
    return materializeFullScene(semantic::SemanticReadView(input, generation), textService, deferRoutes);
}

foundation::Result<FullMaterializedScene> materializeFullScene(
    const semantic::SemanticReadView& view, text::RichTextLayoutService* textService, bool deferRoutes) {
    try {
        const auto compiled = scene::FullSceneCompiler::compile(view,textService,deferRoutes);
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
                .derivedGeometry = source.derivedGeometry,
                .geometryBounds = source.geometryBounds,
                .visualBounds = source.visualBounds,
                .worldBounds = source.worldBounds,
                .referenceGeometryDigest = source.referenceGeometryDigest,
                .directDependencies = source.directDependencies,
                .geometryChunks = source.geometryChunks,
                .textLayout = source.textLayout,
                .hierarchyOrder = source.hierarchyOrder,
            });
        }
        return foundation::Result<FullMaterializedScene>::success(std::move(result));
    } catch (const std::bad_alloc&) {
        return foundation::Result<FullMaterializedScene>::failure(
            {foundation::ErrorCode::kOutOfMemory, "Unable to materialize FullSceneCompiler recovery"});
    }
}

} // namespace canvas::internal
