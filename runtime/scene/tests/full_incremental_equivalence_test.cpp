#include "canvas/scene/full_scene_compiler.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"

#include <cassert>
#include <algorithm>
#include <string>
#include <vector>

namespace {
canvas::semantic::ObjectRecord makeRecord(canvas::semantic::ObjectKind kind, std::uint64_t id) {
    canvas::semantic::ObjectRecord object;
    object.id = canvas::semantic::ObjectId::fromUint64(id);
    object.kind = kind;
    object.kind_version = 1;
    object.placement.order_key = canvas::semantic::OrderKey(std::vector<std::uint8_t>{static_cast<std::uint8_t>(id)});
    object.transform.tx = static_cast<double>(id);
    switch (kind) {
    case canvas::semantic::ObjectKind::kShape: object.content = canvas::semantic::ShapeContent{1, 10.0, 20.0}; break;
    case canvas::semantic::ObjectKind::kImage: object.content = canvas::semantic::ImageContent{.resource_id = {canvas::semantic::ObjectId::fromUint64(90)}, .width = 11.0, .height = 12.0}; break;
    case canvas::semantic::ObjectKind::kVectorPath: object.content = canvas::semantic::VectorPathContent{.geometry = {.fill_rule = canvas::semantic::FillRule::kEvenOdd, .commands = {canvas::semantic::MoveTo{.point={1.0,2.0}}, canvas::semantic::LineTo{.end={3.0,4.0}}}}}; break;
    case canvas::semantic::ObjectKind::kRichText: object.content = canvas::semantic::RichTextContent{.document = {.paragraphs = {{.id = canvas::semantic::ObjectId::fromUint64(100), .runs = {{.text = "hello"}}}}}}; break;
    case canvas::semantic::ObjectKind::kVectorStroke: object.content = canvas::semantic::VectorStrokeContent{.stroke = {.deterministic_seed = 7, .data = canvas::semantic::VectorStrokeData{.samples = {{.position={1.0,2.0}}}}}}; break;
    case canvas::semantic::ObjectKind::kDabStroke: object.content = canvas::semantic::DabStrokeContent{.stroke = {.deterministic_seed = 8, .data = canvas::semantic::DabStrokeData{.dabs = {{.center={2.0,3.0}, .size=4.0}}}}}; break;
    case canvas::semantic::ObjectKind::kConnector: object.content = canvas::semantic::ConnectorContent{.start = {.value = canvas::semantic::FreePointEndpoint{.point={1.0,2.0}}}, .end = {.value = canvas::semantic::AttachedEndpoint{.target_object_id = canvas::semantic::ObjectId::fromUint64(1)}}}; break;
    case canvas::semantic::ObjectKind::kSticky: object.content = canvas::semantic::StickyContent{13.0, 14.0}; break;
    case canvas::semantic::ObjectKind::kGroup: object.content = canvas::semantic::GroupContent{}; break;
    }
    return object;
}
}

int main() {
    canvas::semantic::ReferenceObjectStore store;
    std::vector<canvas::semantic::ObjectRecord> expected;
    for (std::uint8_t kind = 1; kind <= 9; ++kind) {
        expected.push_back(makeRecord(static_cast<canvas::semantic::ObjectKind>(kind), kind));
        assert(canvas::semantic::internal::ObjectStoreMutator::insertFresh(store, expected.back()));
    }
    expected[8].placement.parent_id = expected[0].id;
    assert(canvas::semantic::internal::ObjectStoreMutator::replaceExisting(store, expected[8]));
    const canvas::semantic::SemanticReadView view(store, canvas::semantic::SemanticGeneration(4));
    const auto first = canvas::FullSceneCompiler::compile(view);
    const auto second = canvas::FullSceneCompiler::compile(view);
    assert(first.hasValue() && second.hasValue());
    assert(first.value() == second.value());
    assert(first.value().generation == canvas::semantic::SemanticGeneration(4));
    assert(first.value().records.size() == 9);
    assert(first.value().records[0].worldBounds.right == 11.0F);
    for (const auto& object : expected) {
        const auto* row = first.value().find(object.id);
        assert(row != nullptr);
        assert(row->kind == object.kind);
        assert(row->referenceGeometryDigest.find(std::to_string(static_cast<unsigned>(object.kind))) != std::string::npos);
    }
    const auto* group = first.value().find(expected[8].id);
    assert(group->placement.parent_id && *group->placement.parent_id == expected[0].id);
    assert(group->placement.order_key == expected[8].placement.order_key);
    assert(group->transform.tx == expected[8].transform.tx);
    assert(group->geometryBounds == canvas::foundation::WorldRect{});
    assert(group->visualBounds == canvas::foundation::WorldRect{});
    assert(group->worldBounds == canvas::foundation::WorldRect{});
    assert(first.value().find(canvas::semantic::ObjectId::fromUint64(2))->directDependencies == std::vector<canvas::semantic::ObjectId>{canvas::semantic::ObjectId::fromUint64(90)});
    assert(first.value().find(canvas::semantic::ObjectId::fromUint64(7))->directDependencies == std::vector<canvas::semantic::ObjectId>{canvas::semantic::ObjectId::fromUint64(1)});
    auto reversed = expected;
    std::reverse(reversed.begin(), reversed.end());
    canvas::semantic::ReferenceObjectStore reordered;
    for (const auto& object : reversed) assert(canvas::semantic::internal::ObjectStoreMutator::insertFresh(reordered, object));
    const auto permutation = canvas::FullSceneCompiler::compile(canvas::semantic::SemanticReadView(reordered, canvas::semantic::SemanticGeneration(4)));
    assert(permutation.hasValue());
    assert(permutation.value() == first.value());

    auto path_a = makeRecord(canvas::semantic::ObjectKind::kVectorPath, 40);
    auto path_b = path_a;
    auto& path_geometry = std::get<canvas::semantic::VectorPathContent>(path_b.content).geometry;
    std::get<canvas::semantic::LineTo>(path_geometry.commands[1]).end.x = 99.0;
    canvas::semantic::ReferenceObjectStore path_store_a;
    canvas::semantic::ReferenceObjectStore path_store_b;
    assert(canvas::semantic::internal::ObjectStoreMutator::insertFresh(path_store_a, path_a));
    assert(canvas::semantic::internal::ObjectStoreMutator::insertFresh(path_store_b, path_b));
    const auto path_projection_a = canvas::FullSceneCompiler::compile(canvas::semantic::SemanticReadView(path_store_a, canvas::semantic::SemanticGeneration(1)));
    const auto path_projection_b = canvas::FullSceneCompiler::compile(canvas::semantic::SemanticReadView(path_store_b, canvas::semantic::SemanticGeneration(1)));
    assert(path_projection_a.value().records[0].referenceGeometryDigest != path_projection_b.value().records[0].referenceGeometryDigest);

    auto image_a = makeRecord(canvas::semantic::ObjectKind::kImage, 41);
    auto image_b = image_a;
    std::get<canvas::semantic::ImageContent>(image_b.content).resource_id.value = canvas::semantic::ObjectId::fromUint64(91);
    canvas::semantic::ReferenceObjectStore image_store_a;
    canvas::semantic::ReferenceObjectStore image_store_b;
    assert(canvas::semantic::internal::ObjectStoreMutator::insertFresh(image_store_a, image_a));
    assert(canvas::semantic::internal::ObjectStoreMutator::insertFresh(image_store_b, image_b));
    const auto image_projection_a = canvas::FullSceneCompiler::compile(canvas::semantic::SemanticReadView(image_store_a, canvas::semantic::SemanticGeneration(1)));
    const auto image_projection_b = canvas::FullSceneCompiler::compile(canvas::semantic::SemanticReadView(image_store_b, canvas::semantic::SemanticGeneration(1)));
    assert(image_projection_a.value().records[0].referenceGeometryDigest != image_projection_b.value().records[0].referenceGeometryDigest);
    return 0;
}
