#pragma once

#include "canvas/foundation/object_id.hpp"
#include "canvas/runtime/canvas_control_types.hpp"
#include "canvas/semantic/property_value.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace canvas::runtime {

enum class MultiSelectionExclusion : std::uint8_t { kNotApplicable };
enum class MultiSelectionAvailability : std::uint8_t { kReady, kUnavailable, kPermissionUnknown };
// Hosts must explicitly establish local scope; missing external context fails closed.
enum class MultiSelectionPermissionMode : std::uint8_t {
  kUnconfigured, kLocalSingleUser, kContextUnavailable
};
struct MultiSelectionExcludedRoot final {
  foundation::ObjectId object{};
  MultiSelectionExclusion reason = MultiSelectionExclusion::kNotApplicable;
};
struct MultiSelectionPropertyProjection final {
  std::size_t total = 0;
  std::vector<foundation::ObjectId> eligible;
  std::vector<MultiSelectionExcludedRoot> excluded;
  bool mixed = false;
  bool partial = false;
  // A missing permission fact is not authorization to mutate.
  MultiSelectionAvailability availability = MultiSelectionAvailability::kPermissionUnknown;
  bool editAvailable = false;
  std::optional<semantic::PropertyValue> uniformValue;
};
struct MultiSelectionStamp final {
  std::uint64_t runtimeInstance = 0;
  std::uint64_t lifetimeGeneration = 0;
  std::uint64_t attachmentEpoch = 0;
  std::uint64_t selectionRevision = 0;
  std::uint64_t semanticGeneration = 0;
  std::uint64_t sceneGeneration = 0;
  std::uint64_t sceneRevision = 0;
  std::uint64_t permissionRevision = 0;
  bool operator==(const MultiSelectionStamp&) const = default;
};
struct MultiSelectionProjection final {
  bool available = false;
  MultiSelectionStamp stamp{};
  std::vector<foundation::ObjectId> roots;
  std::map<std::uint32_t, MultiSelectionPropertyProjection> properties;
};
struct MultiSelectionSubmitReceipt final {
  bool queued = false;
  CanvasControlReceiptState state = CanvasControlReceiptState::kRejected;
  CanvasControlError error = CanvasControlError::kNone;
};
struct MultiSelectionOutcome final {
  bool applied = false;
  bool terminal = false;
  CanvasControlReceiptState state = CanvasControlReceiptState::kQueued;
  CanvasControlError error = CanvasControlError::kNone;
  std::vector<foundation::ObjectId> changed;
  std::vector<foundation::ObjectId> unchanged;
  std::vector<MultiSelectionExcludedRoot> skipped;
};


}  // namespace canvas::runtime
