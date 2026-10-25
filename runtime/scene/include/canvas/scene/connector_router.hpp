#pragma once
#include "canvas/scene/derived_geometry.hpp"
#include <memory>
#include <span>
namespace canvas::scene {
// Owner-thread job. The immutable source span must outlive the job; cancel
// before replacing it. No background callbacks or canonical mutations.
class ConnectorRouteJob final {
public:
    ConnectorRouteJob(const semantic::ObjectRecord&, std::span<const semantic::ObjectRecord>);
    ~ConnectorRouteJob();
    ConnectorRouteJob(ConnectorRouteJob&&) noexcept;
    ConnectorRouteJob& operator=(ConnectorRouteJob&&) noexcept;
    ConnectorRouteJob(const ConnectorRouteJob&) = delete;
    ConnectorRouteJob& operator=(const ConnectorRouteJob&) = delete;
    // One quantum: at most 1024 examined records/expansions and 8192 predicates.
    runtime::ConnectorRouteStatus advance();
    void cancel() noexcept;
    [[nodiscard]] const ConnectorGeometry& result() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
