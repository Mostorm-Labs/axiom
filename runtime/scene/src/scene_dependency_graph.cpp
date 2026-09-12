#include "canvas/scene/scene_dependency_graph.hpp"

#include <algorithm>
#include <unordered_set>

namespace canvas::scene {
void SceneDependencyGraph::addHierarchy(foundation::ObjectId parent, foundation::ObjectId child) { edges_[parent].push_back(child); }
void SceneDependencyGraph::addRelation(foundation::ObjectId source, foundation::ObjectId dependent) { edges_[source].push_back(dependent); }
std::vector<foundation::ObjectId> SceneDependencyGraph::closure(std::span<const foundation::ObjectId> roots) const {
    std::vector<foundation::ObjectId> result(roots.begin(), roots.end());
    std::unordered_set<foundation::ObjectId, foundation::ObjectIdHash> seen(result.begin(), result.end());
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto it = edges_.find(result[i]); if (it == edges_.end()) continue;
        for (const auto id : it->second) if (seen.insert(id).second) result.push_back(id);
    }
    std::sort(result.begin(), result.end());
    return result;
}
std::vector<foundation::ObjectId> SceneDependencyGraph::closure(std::initializer_list<foundation::ObjectId> roots) const { return closure(std::span<const foundation::ObjectId>(roots.begin(), roots.size())); }
} // namespace canvas::scene
