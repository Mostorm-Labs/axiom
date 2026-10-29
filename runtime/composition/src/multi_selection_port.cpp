#include "canvas/runtime/internal/multi_selection_port.hpp"

#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/canvas_runtime_facade.hpp"
#include "canvas/semantic/normalizer.hpp"
#include "canvas/semantic/validator.hpp"
#include "multi_selection_queue.hpp"

#include <algorithm>
#include <array>
#include <new>
#include <limits>
#include <thread>
#include <utility>

namespace canvas::runtime {
bool MultiSelectionPort::setPermissionMode(MultiSelectionPermissionMode mode) noexcept {
  if (std::this_thread::get_id() != owner_.ownerThread_ ||
      owner_.state_ != RuntimeCompositionState::kReady ||
      (mode != MultiSelectionPermissionMode::kUnconfigured &&
       mode != MultiSelectionPermissionMode::kLocalSingleUser &&
       mode != MultiSelectionPermissionMode::kContextUnavailable)) return false;
  if (permissionMode_ == mode) return true;
  if (permissionRevision_ == std::numeric_limits<std::uint64_t>::max()) return false;
  permissionMode_ = mode;
  ++permissionRevision_;
  owner_.runtime_->multiSelectionPermissionAllowed_ = mode == MultiSelectionPermissionMode::kLocalSingleUser;
  owner_.runtime_->multiSelectionPermissionRevision_ = permissionRevision_;
  return true;
}
MultiSelectionProjection MultiSelectionPort::read() const noexcept {
  MultiSelectionProjection result;
  if (std::this_thread::get_id() != owner_.ownerThread_ ||
      owner_.state_ != RuntimeCompositionState::kReady || !owner_.runtime_) return result;
  try {
    auto& runtime = *owner_.runtime_;
    // Hierarchy normalization may coalesce an ancestor and child, but a raw
    // selected object outside the active scope must never silently disappear.
    for (const auto id : runtime.selection_.selected())
      if (runtime.selection_.resolveHit(id, runtime.semanticObjects_).isZero()) return {};
    const auto before = runtime.selectionSummary();
    if (before.status != SelectionSummaryStatus::kAvailable) return result;
    result.stamp = {owner_.runtimeInstanceId_, owner_.resources_.lifetimeGeneration,
                    owner_.attachmentEpoch_, before.selectionRevision,
                    before.semanticGeneration, before.sceneGeneration, before.sceneRevision, permissionRevision_};
    result.roots = before.roots;
    auto* text = runtime.interaction_ ? runtime.interaction_->textSession() : nullptr;
    const bool writeReady = runtime.textReady() && !runtime.explicitIntentActive_ &&
        (!text || text->read().phase == TextPhase::kClosed);
    constexpr std::array<std::uint32_t, 8> fields = {1U, 2U, 3U, 4U, 0x100U, 0x101U, 0x200U, 0x201U};
    for (const auto field : fields) {
      MultiSelectionPropertyProjection property;
      property.total = before.roots.size();
      bool writable = writeReady;
      bool allSolidStrokes = field == 0x101U;
      std::optional<semantic::PropertyValue> first;
      for (const auto id : before.roots) {
        const auto* record = runtime.semanticObjects_.find(id);
        if (!record) return {};
        if (!semantic::propertyAppliesToObjectKind(record->kind, field)) {
          property.excluded.push_back({id, MultiSelectionExclusion::kNotApplicable});
          continue;
        }
        property.eligible.push_back(id);
        writable = writable && runtime.textWritable(id);
        std::optional<semantic::PropertyValue> value;
        for (const auto& entry : record->properties.entries)
          if (entry.field_id == field) { value = entry.value; break; }
        if (!value) value = semantic::propertyDefaultValue(field);
        if (!value) return {};
        if (field == 0x101U) {
          const auto* stroke = std::get_if<semantic::StrokeStyleValue>(&*value);
          allSolidStrokes = allSolidStrokes && stroke && std::holds_alternative<semantic::SolidStroke>(*stroke);
        }
        if (!first) first = *value;
        else if (*first != *value) property.mixed = true;
      }
      property.partial = !property.eligible.empty() && !property.excluded.empty();
      if (!property.mixed) property.uniformValue = std::move(first);
      // Local mode is explicit. Any missing or unreadable contextual permission
      // remains unknown and cannot authorize a write.
      const bool permissionKnownAllow = permissionMode_ == MultiSelectionPermissionMode::kLocalSingleUser;
      property.availability = !permissionKnownAllow ? MultiSelectionAvailability::kPermissionUnknown
          : writable && !property.eligible.empty() ? MultiSelectionAvailability::kReady
                                                   : MultiSelectionAvailability::kUnavailable;
      property.editAvailable = permissionKnownAllow && writable && !property.eligible.empty();
      property.strokeWidthAvailable = allSolidStrokes && !property.eligible.empty();
      result.properties.emplace(field, std::move(property));
    }
    const auto after = runtime.selectionSummary();
    if (after.status != SelectionSummaryStatus::kAvailable ||
        after.roots != before.roots || after.selectionRevision != before.selectionRevision ||
        after.semanticGeneration != before.semanticGeneration ||
        after.sceneGeneration != before.sceneGeneration || after.sceneRevision != before.sceneRevision ||
        owner_.resources_.lifetimeGeneration != result.stamp.lifetimeGeneration ||
        owner_.attachmentEpoch_ != result.stamp.attachmentEpoch ||
        permissionRevision_ != result.stamp.permissionRevision) return {};
    result.available = true;
    return result;
  } catch (const std::bad_alloc&) {
    return {};
  }
}

std::optional<std::vector<MultiSelectionRootPropertyValue>> MultiSelectionPort::readPropertyValues(
    std::uint32_t field, const MultiSelectionStamp& stamp,
    const std::vector<foundation::ObjectId>& eligible) const noexcept {
  try {
    const auto projection = read();
    const auto property = projection.properties.find(field);
    if (!projection.available || projection.stamp != stamp || property == projection.properties.end() ||
        !property->second.editAvailable || property->second.eligible != eligible || eligible.empty()) return {};
    std::vector<MultiSelectionRootPropertyValue> values;
    values.reserve(eligible.size());
    for (const auto id : eligible) {
      const auto* record = owner_.runtime_->semanticObjects_.find(id);
      if (!record) return {};
      std::optional<semantic::PropertyValue> value;
      for (const auto& entry : record->properties.entries)
        if (entry.field_id == field) { value = entry.value; break; }
      if (!value) value = semantic::propertyDefaultValue(field);
      if (!value) return {};
      values.push_back({id, std::move(*value)});
    }
    const auto after = read();
    if (!after.available || after.stamp != stamp) return {};
    return values;
  } catch (const std::bad_alloc&) {
    return {};
  }
}

MultiSelectionSubmitReceipt MultiSelectionPort::submitProperty(
    std::uint32_t field, const semantic::PropertyValue& value, const MultiSelectionStamp& stamp,
    const std::vector<foundation::ObjectId>& eligible, std::uint64_t client,
    std::uint64_t request) noexcept {
  return submitPropertyImpl(field, value, std::nullopt, stamp, eligible, client, request);
}
MultiSelectionSubmitReceipt MultiSelectionPort::submitStrokeWidth(
    double width, const MultiSelectionStamp& stamp, const std::vector<foundation::ObjectId>& eligible,
    std::uint64_t client, std::uint64_t request) noexcept {
  return submitPropertyImpl(0x101U, semantic::PropertyValue{false}, width, stamp, eligible, client, request);
}
MultiSelectionSubmitReceipt MultiSelectionPort::submitPropertyImpl(
    std::uint32_t field, const semantic::PropertyValue& value, std::optional<double> strokeWidth,
    const MultiSelectionStamp& stamp, const std::vector<foundation::ObjectId>& eligible,
    std::uint64_t client, std::uint64_t request) noexcept {
  if (std::this_thread::get_id() != owner_.ownerThread_ || client == 0U || request == 0U ||
      !owner_.controlFacade_) return {};
  bool highWaterChanged = false;
  bool insertedClient = false;
  std::uint64_t previousHighWater = 0;
  const auto restoreHighWater = [&]() noexcept {
    if (!highWaterChanged) return;
    if (insertedClient) outcomeHighWater_.erase(client);
    else outcomeHighWater_.find(client)->second = previousHighWater;
    highWaterChanged = false;
  };
  try {
    const auto projection = read();
    const auto property = projection.properties.find(field);
    if (projection.available && property != projection.properties.end() &&
        property->second.availability == MultiSelectionAvailability::kPermissionUnknown)
      return {false, CanvasControlReceiptState::kRejected, CanvasControlError::kResourceUnavailable};
    if (!projection.available || projection.stamp != stamp || property == projection.properties.end() ||
        !property->second.editAvailable || property->second.eligible != eligible || eligible.empty() ||
        (strokeWidth && !property->second.strokeWidthAvailable) ||
        (!strokeWidth && property->second.uniformValue && *property->second.uniformValue == value)) return {};
    const auto key = std::make_pair(client, request);
    if (outcomes_.contains(key)) return {false, CanvasControlReceiptState::kDuplicate, CanvasControlError::kNone};
    const auto priorHighWater = outcomeHighWater_.find(client);
    if (priorHighWater != outcomeHighWater_.end() && request <= priorHighWater->second)
      return {false, CanvasControlReceiptState::kExpired, CanvasControlError::kNone};
    if (priorHighWater == outcomeHighWater_.end() && outcomeHighWater_.size() >= 1024U)
      return {false, CanvasControlReceiptState::kQueueFull, CanvasControlError::kQueueFull};
    if (outcomes_.size() >= 1024U) {
      const auto old = std::find_if(outcomes_.begin(), outcomes_.end(),
                                    [](const auto& item) { return item.second->terminal; });
      if (old == outcomes_.end()) return {};
      outcomes_.erase(old);
    }
    auto outcome = std::make_shared<MultiSelectionOutcome>();
    std::vector<std::pair<foundation::ObjectId, semantic::PropertyValue>> changedValues;
    changedValues.reserve(eligible.size());
    for (const auto id : eligible) {
      const auto* record = owner_.runtime_->semanticObjects_.find(id);
      if (!record) return {};
      std::optional<semantic::PropertyValue> prior;
      for (const auto& entry : record->properties.entries)
        if (entry.field_id == field) { prior = entry.value; break; }
      if (!prior) prior = semantic::propertyDefaultValue(field);
      if (!prior) return {};
      auto next = value;
      if (strokeWidth) {
        const auto* style = std::get_if<semantic::StrokeStyleValue>(&*prior);
        const auto* solid = style ? std::get_if<semantic::SolidStroke>(style) : nullptr;
        if (!solid) return {};
        auto updated = *solid;
        updated.width = *strokeWidth;
        next = semantic::StrokeStyleValue{std::move(updated)};
      }
      if (*prior != next) {
        outcome->changed.push_back(id);
        changedValues.emplace_back(id, std::move(next));
      }
      else outcome->unchanged.push_back(id);
    }
    if (outcome->changed.empty()) return {};
    outcome->skipped = property->second.excluded;
    auto work = std::make_shared<MultiSelectionControlWork>();
    work->key = {client, request};
    work->target = owner_.controlFacade_->target();
    const auto sequence = owner_.controlFacade_->controlSequence();
    work->deadlineSequence = sequence <= std::numeric_limits<std::uint64_t>::max() - 120U
        ? sequence + 120U : std::numeric_limits<std::uint64_t>::max();
    work->apply = [this, field, changedValues = std::move(changedValues), stamp, eligible, outcome]() -> CanvasControlError {
      const auto current = read();
      const auto item = current.properties.find(field);
      if (!current.available || current.stamp != stamp || item == current.properties.end() ||
          item->second.eligible != eligible) return CanvasControlError::kStaleTarget;
      if (!item->second.editAvailable) return CanvasControlError::kBusy;
      bool intentStarted = false;
      try {
        semantic::PatchPropertiesOp patch;
        patch.patches.reserve(outcome->changed.size());
        for (const auto& [id, next] : changedValues)
          patch.patches.push_back({id, field, semantic::PropertyPatchAction::kSet, next});
        auto& runtime = *owner_.runtime_;
        const auto operationId = runtime.allocateOperationId();
        if (operationId.isZero() || !runtime.beginExplicitIntent()) return CanvasControlError::kBusy;
        intentStarted = true;
        const semantic::Operation operation{operationId, runtime.documentId_, 1U, 1U, std::move(patch)};
        if (!runtime.prepareExplicitIntent(std::span<const semantic::Operation>(&operation, 1U)))
          return CanvasControlError::kInvalidValue;
        if (!runtime.commitExplicitIntent()) return CanvasControlError::kBusy;
        return CanvasControlError::kNone;
      } catch (const std::bad_alloc&) {
        if (intentStarted) owner_.runtime_->abortExplicitIntent();
        return CanvasControlError::kResourceUnavailable;
      }
    };
    work->finish = [outcome](CanvasControlReceiptState state, CanvasControlError error) noexcept {
      outcome->applied = state == CanvasControlReceiptState::kApplied;
      outcome->terminal = true;
      outcome->state = state;
      outcome->error = error;
      if (!outcome->applied) outcome->changed.clear();
    };
    outcomes_.emplace(key, outcome);
    const auto [highWater, inserted] = outcomeHighWater_.try_emplace(client, 0U);
    insertedClient = inserted;
    previousHighWater = highWater->second;
    highWater->second = request;
    highWaterChanged = true;
    const auto admitted = owner_.controlFacade_->submitMultiSelection(std::move(work));
    if (admitted.state != CanvasControlReceiptState::kQueued) {
      outcomes_.erase(key);
      restoreHighWater();
      return {false, admitted.state, admitted.error};
    }
    return {true, admitted.state, admitted.error};
  } catch (const std::bad_alloc&) {
    outcomes_.erase(std::make_pair(client, request));
    restoreHighWater();
    return {};
  }
}

std::optional<MultiSelectionOutcome> MultiSelectionPort::receipt(
    std::uint64_t client, std::uint64_t request) const noexcept {
  if (std::this_thread::get_id() != owner_.ownerThread_) return {};
  try {
    const auto found = outcomes_.find(std::make_pair(client, request));
    if (found == outcomes_.end()) {
      const auto highWater = outcomeHighWater_.find(client);
      if (highWater == outcomeHighWater_.end() || request == 0U || request > highWater->second) return {};
      MultiSelectionOutcome expired;
      expired.terminal = true;
      expired.state = CanvasControlReceiptState::kExpired;
      return expired;
    }
    auto result = *found->second;
    if (!result.terminal) result.changed.clear();
    return result;
  } catch (const std::bad_alloc&) {
    return {};
  }
}
}  // namespace canvas::runtime
