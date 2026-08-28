#pragma once

#include "canvas/semantic/object_store.hpp"

#include <map>
#include <optional>
#include <set>
#include <vector>

namespace canvas::semantic {

class StagedObjectView final {
  public:
    explicit StagedObjectView(const ObjectStore& base) : base_(base) {}

    [[nodiscard]] bool contains(const ObjectId& id) const noexcept;
    [[nodiscard]] const ObjectRecord* find(const ObjectId& id) const noexcept;
    [[nodiscard]] std::vector<ObjectRecord> allObjects() const;
    [[nodiscard]] std::vector<ObjectRecord> children(
        const std::optional<ObjectId>& parent_id) const;
    [[nodiscard]] std::vector<ObjectRecord> projection() const { return allObjects(); }

    [[nodiscard]] bool stageCreate(ObjectRecord record);
    [[nodiscard]] bool stageReplace(ObjectRecord record);
    [[nodiscard]] bool stageDelete(const ObjectId& id);

  private:
    [[nodiscard]] std::vector<ObjectRecord> materialize() const;
    static bool childBefore(const ObjectRecord& left, const ObjectRecord& right);

    const ObjectStore& base_;
    std::map<ObjectId, ObjectRecord> creates_;
    std::map<ObjectId, ObjectRecord> replacements_;
    std::set<ObjectId> deletes_;
};

} // namespace canvas::semantic
