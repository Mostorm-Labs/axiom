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
    void insert(const ObjectRecord& record);
    void erase(const ObjectRecord& record);

    [[nodiscard]] std::vector<ObjectId> children(
        const std::optional<ObjectId>& parent_id) const;
    [[nodiscard]] bool equals(const ObjectIndex& other) const noexcept;

  private:
    struct ChildEntry final {
        OrderKey order_key;
        ObjectId id;

        auto operator<=>(const ChildEntry&) const = default;
    };

    using ParentScope = std::optional<ObjectId>;
    using ChildSet = std::set<ChildEntry>;

    std::map<ParentScope, ChildSet> children_by_parent_;
};

} // namespace canvas::semantic::internal
