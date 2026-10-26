#pragma once
#include "../../semantic/src/qualification_checkpoint.hpp"

namespace canvas::qualification {
inline void historyTransition(const std::shared_ptr<State>& state, const void* owner,
                              semantic::OperationId operation, semantic::DocumentId document,
                              std::size_t size, std::size_t cursor) noexcept {
  Event event{};
  event.point = Point::kHistDone; event.owner = owner;
  event.operation = operation; event.document = document;
  event.historySize = size; event.historyCursor = cursor;
  event.preparedResources = kHistoryEntry | kPublished;
  (void)notify(state.get(), event);
}
inline bool historyCheckpoint(const std::shared_ptr<State>& state, const void* owner,
                              Point point, const semantic::Operation& operation,
                              std::size_t size, std::size_t cursor) noexcept {
  Event event{};
  event.point = point; event.owner = owner;
  event.operation = operation.id; event.document = operation.document_id;
  event.historySize = size; event.historyCursor = cursor;
  if (point == Point::kHistReady) event.preparedResources = kHistoryEntry;
  if (point == Point::kHistDone) event.preparedResources = kHistoryEntry | kPublished;
  return notify(state.get(), event);
}
} // namespace canvas::qualification
