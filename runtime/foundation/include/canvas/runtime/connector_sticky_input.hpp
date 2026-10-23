#pragma once

#include "canvas/semantic/object_record.hpp"

#include <cstdint>
#include <optional>

namespace canvas::runtime {

enum class ConnectorCommandState : std::uint8_t { kRejected, kApplied };
enum class ConnectorFailureReason : std::uint8_t {
  kNone, kWrongThread, kNotReady, kInvalidRequest, kObjectRejected,
  kInvalidTarget, kNoRouteInDomain, kBudgetExceeded, kInvalidEndpoint,
  kCancelled,
};
struct ConnectorCreateRequest final {
  semantic::ObjectId objectId{};
  semantic::ConnectorContent content{};
  semantic::Placement placement{};
  semantic::Transform2D transform{};
};
struct ConnectorCommandReceipt final {
  ConnectorCommandState state = ConnectorCommandState::kRejected;
  ConnectorFailureReason reason = ConnectorFailureReason::kInvalidRequest;
  semantic::ObjectId objectId{};
  std::uint64_t semanticGeneration = 0;
  bool historyChanged = false;
};
struct StickyCreateRequest final {
  semantic::ObjectId objectId{};
  semantic::StickyContent content{200.0, 200.0};
  semantic::Placement placement{};
  semantic::Transform2D transform{};
};
struct StickyCommandReceipt final {
  ConnectorCommandState state = ConnectorCommandState::kRejected;
  ConnectorFailureReason reason = ConnectorFailureReason::kInvalidRequest;
  semantic::ObjectId objectId{};
  std::uint64_t semanticGeneration = 0;
  bool historyChanged = false;
};
struct ConnectorSnapshot final {
  bool available = false;
  semantic::ObjectId objectId{};
  semantic::ConnectorContent content{};
};
struct StickySnapshot final {
  bool available = false;
  semantic::ObjectId objectId{};
  semantic::StickyContent content{};
};
}
