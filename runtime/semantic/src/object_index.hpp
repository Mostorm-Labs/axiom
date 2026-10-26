#pragma once

#include "canvas/semantic/object_record.hpp"

#include <compare>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace canvas::semantic::internal {

// Private derived hierarchy acceleration. This class never owns records and
// can be reconstructed solely from the canonical ObjectStore record set.
class ObjectIndex final {
  public:
    struct ParentEntry final { ObjectId id{}; Placement placement{}; };
    void insert(const ObjectRecord& record);
    void erase(const ObjectRecord& record);

    [[nodiscard]] std::vector<ObjectId> children(
        const std::optional<ObjectId>& parent_id) const;
    [[nodiscard]] std::vector<ObjectId> connectorsReferencing(const ObjectId& target) const;
    [[nodiscard]] bool equals(const ObjectIndex& other) const noexcept;

    struct ChildEntry final {
        OrderKey order_key;
        ObjectId id;

        auto operator<=>(const ChildEntry&) const = default;
    };

    using ParentScope = std::optional<ObjectId>;
    using ChildSet = std::set<ChildEntry>;
    using ParentPatch = std::map<ParentScope, ChildSet>;
    [[nodiscard]] ParentPatch prepareParentChanges(
        const std::vector<ParentEntry>& before, const std::vector<ParentEntry>& after) const;
    void publishParentChanges(ParentPatch&&) noexcept;
    using ConnectorPatch = std::map<ObjectId, std::set<ObjectId>>;
    [[nodiscard]] ConnectorPatch prepareConnectorChanges(
        const std::vector<ObjectRecord>& before, const std::vector<ObjectRecord>& after) const;
    void publishConnectorChanges(ConnectorPatch&&) noexcept;

  private:
    std::map<ParentScope, ChildSet> children_by_parent_;
    std::map<ObjectId, std::set<ObjectId>> connectors_by_target_;
};

} // namespace canvas::semantic::internal
