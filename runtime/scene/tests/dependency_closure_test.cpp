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
    return 0;
}
