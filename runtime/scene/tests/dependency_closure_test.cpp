#include "canvas/scene/scene_dependency_graph.hpp"

#include <cassert>

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
    return 0;
}
