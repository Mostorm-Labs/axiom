#pragma once

#include "canvas/semantic/canonical_commit_record.hpp"
#include "canvas/semantic/document_runtime_state.hpp"
#include "canvas/semantic/operation.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/semantic_error.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/stateful_validation.hpp"
#include "g1_06_projection.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace canvas::verification::g1_07 {

enum class Provider : std::uint8_t { kReference = 0, kIndexed };
enum class BaselineKind : std::uint8_t { kEmpty = 0, kSnapshot };

enum class FailureClass : std::uint8_t {
    kNone = 0,
    kTraceInvalid,
    kSnapshotDecodeFailed,
    kSnapshotBootstrapFailed,
    kPositionInvalid,
    kApplyRejected,
    kCommitBlocked,
    kReplayFailed,
};

struct ReplayBaseline final {
    BaselineKind kind = BaselineKind::kEmpty;
    canvas::semantic::SemanticGeneration generation{};
    canvas::semantic::RuntimeEpoch runtime_epoch{};
    canvas::semantic::CommitOrdinal commit_ordinal{};
    std::optional<canvas::semantic::SemanticSnapshot> snapshot;
};

struct ReplayTrace final {
    canvas::semantic::DocumentId document_id{};
    std::uint32_t schema_version = 1U;
    ReplayBaseline baseline{};
    std::vector<canvas::semantic::Operation> operations;
};

struct TraceDecodeResult final {
    std::optional<ReplayTrace> trace;
    FailureClass failure = FailureClass::kNone;
    std::string detail;
    [[nodiscard]] bool ok() const noexcept { return trace.has_value() && failure == FailureClass::kNone; }
};

struct StepObservation final {
    std::string provider;
    std::string command = "step";
    std::size_t trace_operation_count = 0U;
    std::optional<std::size_t> requested_index;
    bool success = false;
    FailureClass failure = FailureClass::kNone;
    std::size_t operation_index = 0U;
    std::size_t resulting_position = 0U;
    canvas::semantic::OperationId operation_id{};
    canvas::semantic::ApplyDisposition disposition = canvas::semantic::ApplyDisposition::kRejected;
    canvas::semantic::StatefulIssue stateful_issue = canvas::semantic::StatefulIssue::kNone;
    canvas::semantic::CommitBlockReason commit_block_reason = canvas::semantic::CommitBlockReason::kNone;
    canvas::semantic::SemanticGeneration before_generation{};
    canvas::semantic::SemanticGeneration after_generation{};
    canvas::semantic::RuntimeEpoch runtime_epoch{};
    canvas::semantic::CommitOrdinal commit_ordinal{};
    std::optional<canvas::semantic::CanonicalCommitRecord> commit_record;
    std::string projection_json;
    std::uint64_t digest = 0U;
};

struct RunObservation final {
    std::string provider;
    std::string command = "run";
    std::size_t trace_operation_count = 0U;
    std::optional<std::size_t> requested_position;
    bool success = false;
    FailureClass failure = FailureClass::kNone;
    std::size_t applied = 0U;
    std::size_t already_applied = 0U;
    std::optional<std::size_t> failure_index;
    std::optional<canvas::semantic::OperationId> failure_operation_id;
    canvas::semantic::ApplyDisposition failure_disposition = canvas::semantic::ApplyDisposition::kRejected;
    std::size_t resulting_position = 0U;
    canvas::semantic::DocumentRuntimeState state_after = canvas::semantic::DocumentRuntimeState::kConstructed;
    canvas::semantic::SemanticGeneration semantic_generation{};
    canvas::semantic::RuntimeEpoch runtime_epoch{};
    canvas::semantic::CommitOrdinal commit_ordinal{};
    std::string projection_json;
    std::uint64_t digest = 0U;
};

struct ObjectObservation final {
    std::string provider;
    std::string command = "object";
    std::size_t trace_operation_count = 0U;
    std::optional<std::size_t> requested_position;
    std::string object_id_hex;
    bool success = false;
    FailureClass failure = FailureClass::kNone;
    bool found = false;
    std::size_t resulting_position = 0U;
    canvas::semantic::SemanticGeneration semantic_generation{};
    canvas::semantic::RuntimeEpoch runtime_epoch{};
    canvas::semantic::CommitOrdinal commit_ordinal{};
    std::string object_json;
    std::string projection_json;
    std::uint64_t digest = 0U;
};

struct ProjectionObservation final {
    std::string provider;
    std::string command = "projection";
    std::size_t trace_operation_count = 0U;
    std::optional<std::size_t> requested_position;
    bool success = false;
    FailureClass failure = FailureClass::kNone;
    std::size_t resulting_position = 0U;
    canvas::semantic::SemanticGeneration semantic_generation{};
    canvas::semantic::RuntimeEpoch runtime_epoch{};
    canvas::semantic::CommitOrdinal commit_ordinal{};
    std::string projection_json;
    std::uint64_t digest = 0U;
};

[[nodiscard]] TraceDecodeResult decodeTraceJson(std::string_view json_text);
[[nodiscard]] TraceDecodeResult readTraceFile(const std::string& path);
[[nodiscard]] const char* failureClassName(FailureClass value) noexcept;
[[nodiscard]] const char* dispositionName(canvas::semantic::ApplyDisposition value) noexcept;
[[nodiscard]] const char* statefulIssueName(canvas::semantic::StatefulIssue value) noexcept;
[[nodiscard]] const char* commitBlockReasonName(canvas::semantic::CommitBlockReason value) noexcept;
[[nodiscard]] std::string writeStepJson(const StepObservation& value);
[[nodiscard]] std::string writeRunJson(const RunObservation& value);
[[nodiscard]] std::string writeObjectJson(const ObjectObservation& value);
[[nodiscard]] std::string writeProjectionJson(const ProjectionObservation& value);

class ReplayInspector final {
  public:
    ReplayInspector(ReplayTrace trace, Provider provider);

    [[nodiscard]] const ReplayTrace& trace() const noexcept { return trace_; }
    [[nodiscard]] Provider provider() const noexcept { return provider_; }
    [[nodiscard]] StepObservation step(std::size_t index) const;
    [[nodiscard]] ProjectionObservation seek(std::size_t position) const;
    [[nodiscard]] RunObservation run() const;
    [[nodiscard]] ObjectObservation object(
        std::size_t position, const canvas::foundation::ObjectId& object_id) const;
    [[nodiscard]] ProjectionObservation projection(std::size_t position) const;

  private:
    ReplayTrace trace_;
    Provider provider_;
};

} // namespace canvas::verification::g1_07
