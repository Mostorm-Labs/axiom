#include "canvas/runtime/internal/multi_selection_port.hpp"

#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/semantic/normalizer.hpp"
#include "canvas/semantic/validator.hpp"

#include <array>
#include <new>
#include <thread>
#include <utility>

namespace canvas::runtime {
MultiSelectionProjection MultiSelectionPort::read() const noexcept {
  MultiSelectionProjection result;
  if (std::this_thread::get_id() != owner_.ownerThread_ ||
      owner_.state_ != RuntimeCompositionState::kReady || !owner_.runtime_) return result;
  try {
    auto& runtime = *owner_.runtime_;
    const auto before = runtime.selectionSummary();
    if (before.status != SelectionSummaryStatus::kAvailable) return result;
    result.stamp = {owner_.runtimeInstanceId_, owner_.resources_.lifetimeGeneration,
                    owner_.attachmentEpoch_, before.selectionRevision,
                    before.semanticGeneration, before.sceneGeneration, before.sceneRevision};
    result.roots = before.roots;
    constexpr std::array<std::uint32_t, 8> fields = {1U, 2U, 3U, 4U, 0x100U, 0x101U, 0x200U, 0x201U};
    for (const auto field : fields) {
      MultiSelectionPropertyProjection property;
      property.total = before.roots.size();
      std::optional<semantic::PropertyValue> first;
      for (const auto id : before.roots) {
        const auto* record = runtime.semanticObjects_.find(id);
        if (!record) return {};
        if (!semantic::propertyAppliesToObjectKind(record->kind, field)) {
          property.excluded.push_back({id, MultiSelectionExclusion::kNotApplicable});
          continue;
        }
        property.eligible.push_back(id);
        std::optional<semantic::PropertyValue> value;
        for (const auto& entry : record->properties.entries)
          if (entry.field_id == field) { value = entry.value; break; }
        if (!value) value = semantic::propertyDefaultValue(field);
        if (!value) return {};
        if (!first) first = *value;
        else if (*first != *value) property.mixed = true;
      }
      property.partial = !property.eligible.empty() && !property.excluded.empty();
      if (!property.mixed) property.uniformValue = std::move(first);
      result.properties.emplace(field, std::move(property));
    }
    const auto after = runtime.selectionSummary();
    if (after.status != SelectionSummaryStatus::kAvailable ||
        after.roots != before.roots || after.selectionRevision != before.selectionRevision ||
        after.semanticGeneration != before.semanticGeneration ||
        after.sceneGeneration != before.sceneGeneration || after.sceneRevision != before.sceneRevision ||
        owner_.resources_.lifetimeGeneration != result.stamp.lifetimeGeneration ||
        owner_.attachmentEpoch_ != result.stamp.attachmentEpoch) return {};
    result.available = true;
    return result;
  } catch (const std::bad_alloc&) {
    return {};
  }
}
}  // namespace canvas::runtime
