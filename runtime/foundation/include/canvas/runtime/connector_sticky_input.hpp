#pragma once

#include "canvas/semantic/object_record.hpp"
#include "canvas/runtime/connector_route.hpp"

#include <cstdint>
#include <optional>

namespace canvas::runtime {

enum class ConnectorCommandState : std::uint8_t { kRejected, kApplied, kPending };
enum class ConnectorFailureReason : std::uint8_t {
  kNone, kWrongThread, kNotReady, kInvalidRequest, kObjectRejected,
  kInvalidTarget, kNoRouteInDomain, kBudgetExceeded, kInvalidEndpoint,
  kCancelled, kStale, kLocked,
};
struct ConnectorToolDefaults final {
  semantic::ConnectorRouting routing = semantic::ConnectorRouting::kStraight;
  bool startArrow = false;
  bool endArrow = false;
};
struct ConnectorEndpointRequest final {
  semantic::ObjectId objectId{};
  bool atStart = false;
  semantic::ConnectorEndpoint endpoint{};
  std::uint64_t expectedSemanticGeneration = 0;
};
struct ConnectorDecorationRequest final {
  semantic::ObjectId objectId{};
  bool startArrow = false, endArrow = false;
  std::uint64_t expectedSemanticGeneration = 0;
};
struct ConnectorCreateRequest final {
  semantic::ObjectId objectId{};
  semantic::ConnectorContent content{};
  semantic::Placement placement{};
  semantic::Transform2D transform{};
  std::optional<bool> startArrow = std::nullopt, endArrow = std::nullopt;
};
struct ConnectorRoutingRequest final {
  // Zero object id changes only the preset for subsequent canvas connectors.
  semantic::ObjectId objectId{};
  semantic::ConnectorRouting routing = semantic::ConnectorRouting::kStraight;
  std::uint64_t expectedSemanticGeneration = 0;
};
struct ConnectorCommandReceipt final {
  ConnectorCommandState state = ConnectorCommandState::kRejected;
  ConnectorFailureReason reason = ConnectorFailureReason::kInvalidRequest;
  semantic::ObjectId objectId{};
  std::uint64_t semanticGeneration = 0;
  bool historyChanged = false;
  std::uint64_t pendingSerial = 0;
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
  std::uint64_t pendingSerial = 0;
};
struct ConnectorSnapshot final {
  bool available = false;
  semantic::ObjectId objectId{};
  semantic::ConnectorContent content{};
  ConnectorRouteStatus routeStatus = ConnectorRouteStatus::kPending;
  ConnectorRouteWork work{};
  std::uint64_t semanticGeneration = 0;
  std::uint64_t sceneRevision = 0;
  bool editingEndpoint = false;
  std::optional<ConnectorCommandReceipt> lastEndpointEdit;
  bool startArrow = false, endArrow = false;
  std::optional<semantic::Transform2D> worldTransform;
  std::vector<semantic::Vec2> displayRoute;
  std::uint32_t algorithmRevision = 1;
  // Derived polyline in connector-local coordinates. Diagnostic chords carry
  // a non-Ready status; they never claim obstacle avoidance.
  std::vector<semantic::Vec2> route;
};
struct StickySnapshot final {
  bool available = false;
  semantic::ObjectId objectId{};
  semantic::StickyContent content{};
};
}
