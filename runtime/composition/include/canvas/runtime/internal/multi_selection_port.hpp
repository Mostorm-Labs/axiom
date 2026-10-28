#pragma once

#include "canvas/foundation/object_id.hpp"
#include "canvas/semantic/property_value.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace canvas::runtime {
class RuntimeComposition;

enum class MultiSelectionExclusion : std::uint8_t { kNotApplicable };
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
  // Writes remain unavailable until the typed command path is implemented.
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
  bool operator==(const MultiSelectionStamp&) const = default;
};
struct MultiSelectionProjection final {
  bool available = false;
  MultiSelectionStamp stamp{};
  std::vector<foundation::ObjectId> roots;
  std::map<std::uint32_t, MultiSelectionPropertyProjection> properties;
};

// Internal, borrowed capability. Composition outlives this port and its host
// consumers must detach before closing or destroying the composition.
class MultiSelectionPort final {
 public:
  explicit MultiSelectionPort(RuntimeComposition& owner) noexcept : owner_(owner) {}
  [[nodiscard]] MultiSelectionProjection read() const noexcept;
 private:
  RuntimeComposition& owner_;
};
}  // namespace canvas::runtime
