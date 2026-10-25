#pragma once
#include <cstdint>
namespace canvas::runtime {
enum class ConnectorRouteStatus : std::uint8_t {
    kPending, kReady, kNoRouteInDomain, kBudgetExceeded, kInvalidEndpoint,
    kInvalidGeometry, kDegenerate, kCancelled
};
struct ConnectorRouteWork final {
    std::uint32_t examined = 0, obstacles = 0, expanded = 0, predicates = 0, retained = 0;
    bool operator==(const ConnectorRouteWork&) const = default;
};
inline const char* connectorRouteStatusName(ConnectorRouteStatus s) noexcept {
    switch (s) {
        case ConnectorRouteStatus::kPending: return "Pending";
        case ConnectorRouteStatus::kReady: return "Ready";
        case ConnectorRouteStatus::kNoRouteInDomain: return "No route in search domain";
        case ConnectorRouteStatus::kBudgetExceeded: return "Routing budget exceeded";
        case ConnectorRouteStatus::kInvalidEndpoint: return "Invalid endpoint";
        case ConnectorRouteStatus::kInvalidGeometry: return "Invalid geometry";
        case ConnectorRouteStatus::kDegenerate: return "Degenerate";
        case ConnectorRouteStatus::kCancelled: return "Cancelled";
    }
    return "Unknown";
}
}
