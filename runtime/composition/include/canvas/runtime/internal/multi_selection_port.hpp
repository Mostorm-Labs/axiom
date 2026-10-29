#pragma once

#include "canvas/foundation/object_id.hpp"
#include "canvas/runtime/multi_selection.hpp"
#include "canvas/semantic/property_value.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace canvas::runtime {
class RuntimeComposition;

// Internal, borrowed capability. Composition outlives this port and its host
// consumers must detach before closing or destroying the composition.
class MultiSelectionPort final {
 public:
  explicit MultiSelectionPort(RuntimeComposition& owner) noexcept : owner_(owner) {}
  [[nodiscard]] bool setPermissionMode(MultiSelectionPermissionMode mode) noexcept;
  [[nodiscard]] MultiSelectionProjection read() const noexcept;
  [[nodiscard]] std::optional<std::vector<MultiSelectionRootPropertyValue>> readPropertyValues(
      std::uint32_t, const MultiSelectionStamp&, const std::vector<foundation::ObjectId>&) const noexcept;
  [[nodiscard]] MultiSelectionSubmitReceipt submitProperty(
      std::uint32_t field, const semantic::PropertyValue& value,
      const MultiSelectionStamp& stamp, const std::vector<foundation::ObjectId>& eligible,
      std::uint64_t client, std::uint64_t request) noexcept;
  [[nodiscard]] MultiSelectionSubmitReceipt submitStrokeWidth(
      double width, const MultiSelectionStamp& stamp,
      const std::vector<foundation::ObjectId>& eligible,
      std::uint64_t client, std::uint64_t request) noexcept;
  [[nodiscard]] std::optional<MultiSelectionOutcome> receipt(
      std::uint64_t client, std::uint64_t request) const noexcept;
 private:
  [[nodiscard]] MultiSelectionSubmitReceipt submitPropertyImpl(
      std::uint32_t field, const semantic::PropertyValue& value, std::optional<double> strokeWidth,
      const MultiSelectionStamp& stamp, const std::vector<foundation::ObjectId>& eligible,
      std::uint64_t client, std::uint64_t request) noexcept;
  RuntimeComposition& owner_;
  MultiSelectionPermissionMode permissionMode_ = MultiSelectionPermissionMode::kUnconfigured;
  std::uint64_t permissionRevision_ = 1;
  std::map<std::pair<std::uint64_t, std::uint64_t>, std::shared_ptr<MultiSelectionOutcome>> outcomes_;
  std::map<std::uint64_t, std::uint64_t> outcomeHighWater_;
};
}  // namespace canvas::runtime
