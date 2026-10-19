#pragma once

#include "canvas/semantic/canonical_commit_record.hpp"
#include "canvas/semantic/operation.hpp"

#include <cstdint>
#include <optional>

namespace canvas::interaction {

enum class AxisProgress : std::uint8_t { kUnavailable, kPending, kFailed, kConfirmed };

struct AxisOperationStatus final {
  AxisProgress localDurable = AxisProgress::kUnavailable;
  AxisProgress cloudSynced = AxisProgress::kUnavailable;
};

// External Data/Axis owns durable custody and ACK state. This thin host port
// delivers existing canonical commit facts; it is not a persistence engine.
class LocalOperationSink {
 public:
  virtual ~LocalOperationSink() = default;
  virtual void applied(const semantic::Operation&,
                       const semantic::CanonicalCommitRecord&) noexcept = 0;
  [[nodiscard]] virtual AxisOperationStatus status(
      const semantic::OperationId&, const semantic::CanonicalCommitStamp&) const noexcept = 0;
};

struct OperationOutcomeSnapshot final {
  std::optional<semantic::CanonicalCommitRecord> applied;
  semantic::SemanticGeneration presentedGeneration{};
  AxisOperationStatus axis{};
};

}  // namespace canvas::interaction
