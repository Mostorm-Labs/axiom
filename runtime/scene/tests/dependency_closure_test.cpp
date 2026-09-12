#include "canvas/scene/scene_dependency_graph.hpp"

#include <cassert>
#include "canvas/semantic/object_record.hpp"

int main() {
    using namespace canvas;
    scene::SceneDependencyGraph graph;
    graph.addHierarchy(foundation::ObjectId::fromUint64(1), foundation::ObjectId::fromUint64(2));
    graph.addHierarchy(foundation::ObjectId::fromUint64(2), foundation::ObjectId::fromUint64(3));
    graph.addRelation(foundation::ObjectId::fromUint64(2), foundation::ObjectId::fromUint64(9));
    const auto closure = graph.closure({foundation::ObjectId::fromUint64(1)});
    assert((closure == std::vector<foundation::ObjectId>{foundation::ObjectId::fromUint64(1),
                                                          foundation::ObjectId::fromUint64(2),
                                                          foundation::ObjectId::fromUint64(3),
                                                          foundation::ObjectId::fromUint64(9)}));

    graph.addRelation(foundation::ObjectId::fromUint64(3), foundation::ObjectId::fromUint64(1));
    graph.addHierarchy(foundation::ObjectId::fromUint64(3), foundation::ObjectId::fromUint64(3));
    const auto cyclic = graph.closure({foundation::ObjectId::fromUint64(1)});
    assert(cyclic.size() == 4U);

    graph.removeHierarchy(foundation::ObjectId::fromUint64(2), foundation::ObjectId::fromUint64(3));
    graph.removeRelation(foundation::ObjectId::fromUint64(3), foundation::ObjectId::fromUint64(1));
    const auto reparented = graph.closure({foundation::ObjectId::fromUint64(1)});
    assert((reparented == std::vector<foundation::ObjectId>{foundation::ObjectId::fromUint64(1),
                                                             foundation::ObjectId::fromUint64(2),
                                                             foundation::ObjectId::fromUint64(9)}));

    graph.addHierarchy(foundation::ObjectId::fromUint64(1), foundation::ObjectId::fromUint64(4));
    graph.addHierarchy(foundation::ObjectId::fromUint64(1), foundation::ObjectId::fromUint64(4));
    graph.removeHierarchy(foundation::ObjectId::fromUint64(1), foundation::ObjectId::fromUint64(4));
    assert(graph.closure({foundation::ObjectId::fromUint64(1)}) == reparented);

    semantic::ObjectRecord image;
    image.id = foundation::ObjectId::fromUint64(50);
    image.kind = semantic::ObjectKind::kImage;
    image.content = semantic::ImageContent{.resource_id = semantic::ResourceId{
        foundation::ObjectId::fromUint64(900)}};
    semantic::ObjectRecord connector;
    connector.id = foundation::ObjectId::fromUint64(51);
    connector.kind = semantic::ObjectKind::kConnector;
    connector.content = semantic::ConnectorContent{
        .start = semantic::ConnectorEndpoint{semantic::AttachedEndpoint{
            image.id, semantic::AutoPerimeterAnchor{}}}};
    const auto resource_id = std::get<semantic::ImageContent>(image.content).resource_id.value;
    graph.addRelation(resource_id, image.id);
    graph.addRelation(image.id, connector.id);
    const auto resourceChain = graph.closure({resource_id});
    assert((resourceChain == std::vector<foundation::ObjectId>{image.id, connector.id, resource_id}));
    graph.removeRelation(image.id, connector.id);
    const auto resourceImageOnly = graph.closure({resource_id});
    assert((resourceImageOnly == std::vector<foundation::ObjectId>{image.id, resource_id}));

    // A disjoint cold component must never enter an active closure.
    const auto coldRoot = foundation::ObjectId::fromUint64(7000);
    const auto coldLeaf = foundation::ObjectId::fromUint64(7001);
    graph.addHierarchy(coldRoot, coldLeaf);
    graph.addRelation(coldLeaf, foundation::ObjectId::fromUint64(7002));
    assert((graph.closure({resource_id}) ==
            std::vector<foundation::ObjectId>{image.id, resource_id}));

    // Creation inserts a dependent edge; deletion removes it without a stale closure.
    const auto createdDependent = foundation::ObjectId::fromUint64(8001);
    graph.addRelation(image.id, createdDependent);
    const auto afterCreate = graph.closure({resource_id});
    assert((afterCreate == std::vector<foundation::ObjectId>{image.id, createdDependent, resource_id}));
    graph.removeRelation(image.id, createdDependent);
    assert((graph.closure({resource_id}) ==
            std::vector<foundation::ObjectId>{image.id, resource_id}));
    return 0;
}
