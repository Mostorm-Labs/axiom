#pragma once

#include "canvas/foundation/object_id.hpp"

#include <span>
#include <unordered_map>
#include <vector>

namespace canvas::scene {

class SceneDependencyGraph final {
  public:
    void addHierarchy(foundation::ObjectId parent, foundation::ObjectId child);
    void addRelation(foundation::ObjectId source, foundation::ObjectId dependent);
    [[nodiscard]] std::vector<foundation::ObjectId> closure(std::span<const foundation::ObjectId> roots) const;
    [[nodiscard]] std::vector<foundation::ObjectId> closure(std::initializer_list<foundation::ObjectId> roots) const;
  private:
    std::unordered_map<foundation::ObjectId, std::vector<foundation::ObjectId>, foundation::ObjectIdHash> edges_;
};
} // namespace canvas::scene

namespace canvas { using scene::SceneDependencyGraph; }
