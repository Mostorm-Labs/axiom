#include "canvas/scene/full_scene_compiler.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "object_store_mutator.hpp"

#include <cassert>
#include <vector>

int main() {
    canvas::semantic::ReferenceObjectStore store;
    canvas::semantic::ObjectRecord object;
    object.id = canvas::semantic::ObjectId::fromUint64(1);
    object.kind = canvas::semantic::ObjectKind::kShape;
    object.kind_version = 1;
    object.placement.order_key = canvas::semantic::OrderKey(std::vector<std::uint8_t>{1});
    object.content = canvas::semantic::ShapeContent{1, 10.0, 20.0};
    assert(canvas::semantic::internal::ObjectStoreMutator::insertFresh(store, object));
    const canvas::semantic::SemanticReadView view(store, canvas::semantic::SemanticGeneration(4));
    const auto first = canvas::FullSceneCompiler::compile(view);
    const auto second = canvas::FullSceneCompiler::compile(view);
    assert(first.hasValue() && second.hasValue());
    assert(first.value() == second.value());
    assert(first.value().generation == canvas::semantic::SemanticGeneration(4));
    assert(first.value().records.size() == 1);
    assert(first.value().records[0].worldBounds.right == 10.0F);
    return 0;
}
