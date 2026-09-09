#pragma once

#include "canvas/semantic/object_store.hpp"

#include <map>
#include <optional>
#include <set>
#include <vector>

namespace canvas::semantic {

class StagedObjectView;
namespace internal {
[[nodiscard]] std::vector<ObjectId> connectorsReferencing(
    const StagedObjectView& staged, const ObjectId& target);
[[nodiscard]] std::map<ObjectId, std::vector<ObjectId>> referenceConnectorReverseRelation(
    const StagedObjectView& staged);
[[nodiscard]] bool usesIndexedConnectorLookup(const StagedObjectView& staged);
}

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
    friend std::vector<ObjectId> internal::connectorsReferencing(
        const StagedObjectView&, const ObjectId&);
    friend std::map<ObjectId, std::vector<ObjectId>>
    internal::referenceConnectorReverseRelation(const StagedObjectView&);
    friend bool internal::usesIndexedConnectorLookup(const StagedObjectView&);
    static bool childBefore(const ObjectRecord& left, const ObjectRecord& right);

    const ObjectStore& base_;
    std::map<ObjectId, ObjectRecord> creates_;
    std::map<ObjectId, ObjectRecord> replacements_;
    std::set<ObjectId> deletes_;
};

} // namespace canvas::semantic
